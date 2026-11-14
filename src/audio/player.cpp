#include "audio/player.h"

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "audio/synth.h"

extern char** environ;

namespace tg::audio {
namespace {

constexpr size_t kMaxVoices = 6;

std::string findInPath(const char* name) {
    const char* path = std::getenv("PATH");
    if (!path) return {};
    std::string p = path;
    size_t start = 0;
    while (start <= p.size()) {
        size_t end = p.find(':', start);
        std::string dir = p.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty()) {
            std::string full = dir + "/" + name;
            if (access(full.c_str(), X_OK) == 0) return full;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

}  // namespace

bool Player::init() {
    for (const char* name : {"afplay", "paplay", "aplay"}) {
        std::string full = findInPath(name);
        if (!full.empty()) {
            backend_ = name;
            backendPath_ = full;
            break;
        }
    }
    if (backend_.empty()) return false;

    const char* tmp = std::getenv("TMPDIR");
    std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/textgme-sfx-XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    if (!mkdtemp(buf.data())) {
        backend_.clear();
        return false;
    }
    dir_ = buf.data();
    for (size_t i = 0; i < files_.size(); ++i) {
        std::string wav = encodeWav(synthesize(static_cast<game::Sfx>(i)));
        std::string path = dir_ + "/" + std::to_string(i) + ".wav";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) continue;
        std::fwrite(wav.data(), 1, wav.size(), f);
        std::fclose(f);
        files_[i] = path;
    }
    return true;
}

Player::~Player() {
    for (int pid : children_) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }
    for (const std::string& f : files_)
        if (!f.empty()) unlink(f.c_str());
    if (!dir_.empty()) rmdir(dir_.c_str());
}

void Player::play(game::Sfx id, float volume) {
    if (backend_.empty() || volume <= 0.01f) return;
    reap();
    if (children_.size() >= kMaxVoices) return;
    const std::string& file = files_[static_cast<size_t>(id)];
    if (file.empty()) return;

    std::vector<std::string> args{backend_};
    char vol[32];
    if (backend_ == "afplay") {
        std::snprintf(vol, sizeof vol, "%.2f", volume);
        args.insert(args.end(), {"-v", vol});
    } else if (backend_ == "paplay") {
        std::snprintf(vol, sizeof vol, "--volume=%d", static_cast<int>(volume * 65536));
        args.push_back(vol);
    } else {
        args.push_back("-q");  // aplay has no volume flag
    }
    args.push_back(file);
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    // Detach the child from the terminal entirely.
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = 0;
    if (posix_spawn(&pid, backendPath_.c_str(), &fa, nullptr, argv.data(), environ) == 0) children_.push_back(pid);
    posix_spawn_file_actions_destroy(&fa);
}

void Player::reap() {
    for (size_t i = 0; i < children_.size();) {
        if (waitpid(children_[i], nullptr, WNOHANG) != 0) {
            children_[i] = children_.back();
            children_.pop_back();
        } else {
            ++i;
        }
    }
}

}  // namespace tg::audio
