#pragma once
// Game state and simulation, independent of the terminal.
#include "game/player.h"
#include "render/renderer.h"
#include "world/map.h"

namespace tg::game {

class Game {
public:
    explicit Game(Map map);

    void update(const Controls& c, float dt);
    render::Camera camera() const;

    const Map& map() const { return map_; }
    const Body& player() const { return player_; }

private:
    Map map_;
    Body player_;
    float viewZ_ = 0;    // smoothed eye height (stairs, landings)
    float bobPhase_ = 0;
    float bob_ = 0;
};

}  // namespace tg::game
