#pragma once
// Plays sound effects through the system's command-line audio player
// (afplay, paplay or aplay) so the game needs no audio library.
#include <array>
#include <string>
#include <vector>

#include "game/sound.h"

namespace tg::audio {

class Player {
public:
    Player() = default;
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    // Finds a player program and writes the effects to a temp directory.
    // Returns false (and stays silent) if either fails.
    bool init();
    bool available() const { return !backend_.empty(); }
    const std::string& backend() const { return backend_; }

    // volume 0..1. Drops the sound if too many are already playing.
    void play(game::Sfx id, float volume);
    // Collects finished child processes; call once per frame.
    void reap();

private:
    std::string backend_, backendPath_, dir_;
    std::array<std::string, static_cast<size_t>(game::Sfx::Count)> files_;
    std::vector<int> children_;
};

}  // namespace tg::audio
