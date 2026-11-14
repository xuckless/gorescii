#include "term/terminal.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace tg::term {
namespace {

termios g_saved{};
std::atomic<bool> g_rawActive{false};
std::atomic<bool> g_resized{false};

constexpr std::string_view kEnterSeq =
    "\x1b[?1049h"  // alternate screen
    "\x1b[?25l"    // hide cursor
    "\x1b[?7l"     // no autowrap
    "\x1b[2J";
constexpr std::string_view kLeaveSeq =
    "\x1b[?1016l\x1b[?1006l\x1b[?1003l\x1b[?1004l"  // mouse and focus reporting off
    "\x1b]22;default\x1b\\"  // pointer shape back to normal
    "\x1b[<u"      // pop kitty keyboard flags (ignored if unsupported)
    "\x1b[0m"
    "\x1b[?7h"
    "\x1b[?25h"
    "\x1b[?1049l";

void writeAll(std::string_view s) {
    while (!s.empty()) {
        ssize_t n = ::write(STDOUT_FILENO, s.data(), s.size());
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return;
        }
        s.remove_prefix(static_cast<size_t>(n));
    }
}

// Async-signal-safe restore used by both the fatal signal handler and atexit.
void restoreTty() {
    if (!g_rawActive.exchange(false)) return;
    writeAll(kLeaveSeq);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
}

void onFatalSignal(int sig) {
    restoreTty();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void onResize(int) { g_resized = true; }

}  // namespace

double Size::cellAspect() const {
    if (pixelW > 0 && pixelH > 0 && cols > 0 && rows > 0) {
        double a = (static_cast<double>(pixelH) / rows) / (static_cast<double>(pixelW) / cols);
        if (a > 0.5 && a < 4.0) return a;
    }
    return 2.0;
}

Terminal::~Terminal() { leave(); }

bool Terminal::enter(bool mouse) {
    if (active_) return true;
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return false;
    if (tcgetattr(STDIN_FILENO, &g_saved) != 0) return false;

    termios raw = g_saved;
    raw.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    raw.c_cflag |= CS8;
    // ISIG stays on so Ctrl-C still interrupts (and the handler restores the tty).
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;

    g_rawActive = true;
    active_ = true;
    std::atexit(restoreTty);
    for (int sig : {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGSEGV, SIGABRT, SIGBUS}) std::signal(sig, onFatalSignal);
    std::signal(SIGWINCH, onResize);

    writeAll(kEnterSeq);
    writeAll("\x1b[?1004h");  // focus in/out reports
    if (mouse) {
        // Any-motion tracking with SGR encoding, and a crosshair pointer (OSC 22).
        writeAll("\x1b[?1003h\x1b[?1006h\x1b]22;crosshair\x1b\\");
    }
    probe(mouse);
    return true;
}

void Terminal::probe(bool mouse) {
    // Ask for the current kitty flags and whether SGR-pixel mouse mode exists
    // (DECRQM), then primary device attributes. Every terminal answers DA, so
    // anything that has not answered before it is unsupported.
    writeAll(mouse ? "\x1b[?u\x1b[?1016$p\x1b[c" : "\x1b[?u\x1b[c");
    std::string reply;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    bool sawKitty = false;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd p{STDIN_FILENO, POLLIN, 0};
        if (poll(&p, 1, 20) > 0) readAvailable(reply);
        if (reply.find("\x1b[?") != std::string::npos) {
            size_t pos = 0;
            while ((pos = reply.find("\x1b[?", pos)) != std::string::npos) {
                size_t end = reply.find_first_of("ucy", pos + 3);
                if (end == std::string::npos) break;
                if (reply[end] == 'u') sawKitty = true;
                if (reply[end] == 'c') {
                    if (sawKitty) {
                        // 1 disambiguate | 2 event types | 8 all keys as escapes
                        writeAll("\x1b[>11u");
                    }
                    // DECRPM: CSI ? 1016 ; Ps $ y with Ps 1 (set) or 2 (reset) = supported.
                    bool pixels = reply.find("\x1b[?1016;1$y") != std::string::npos ||
                                  reply.find("\x1b[?1016;2$y") != std::string::npos;
                    if (pixels && size().pixelW > 0) {
                        writeAll("\x1b[?1016h");
                        mousePixels_ = true;
                    }
                    kitty_ = sawKitty;
                    return;
                }
                pos = end + 1;
            }
        }
    }
}

void Terminal::leave() {
    if (!active_) return;
    restoreTty();
    active_ = false;
    kitty_ = false;
    mousePixels_ = false;
}

Size Terminal::size() const {
    Size s;
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        s.cols = ws.ws_col;
        s.rows = ws.ws_row;
        s.pixelW = ws.ws_xpixel;
        s.pixelH = ws.ws_ypixel;
    }
    return s;
}

bool Terminal::consumeResize() { return g_resized.exchange(false); }

void Terminal::write(std::string_view bytes) const { writeAll(bytes); }

void Terminal::readAvailable(std::string& out) const {
    char buf[4096];
    for (;;) {
        ssize_t n = ::read(STDIN_FILENO, buf, sizeof buf);
        if (n > 0) {
            out.append(buf, static_cast<size_t>(n));
            if (n < static_cast<ssize_t>(sizeof buf)) return;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        return;
    }
}

}  // namespace tg::term
