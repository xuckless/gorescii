#pragma once
// Raw-mode terminal session: alternate screen, hidden cursor, kitty keyboard
// protocol when available, and restoration on exit or fatal signals.
#include <string>
#include <string_view>

namespace tg::term {

struct Size {
    int cols = 80;
    int rows = 24;
    // Pixel size of the whole window if the terminal reports it, else 0.
    int pixelW = 0;
    int pixelH = 0;
    // Height of a character cell divided by its width (~2 on most fonts).
    double cellAspect() const;
};

class Terminal {
public:
    Terminal() = default;
    ~Terminal();
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    // Switches the tty into game mode. Returns false if stdin/stdout is not a tty.
    bool enter(bool mouse = true);
    void leave();

    Size size() const;
    // True once after the window has been resized.
    bool consumeResize();
    // True when the terminal confirmed the kitty keyboard protocol.
    bool hasKeyReleaseEvents() const { return kitty_; }
    // True when mouse reports use pixel coordinates (SGR-pixel, mode 1016).
    bool mousePixels() const { return mousePixels_; }

    void write(std::string_view bytes) const;
    // Non-blocking: appends whatever input is waiting to `out`.
    void readAvailable(std::string& out) const;

private:
    void probe(bool mouse);
    bool active_ = false;
    bool kitty_ = false;
    bool mousePixels_ = false;
};

}  // namespace tg::term
