#pragma once
#include <cstdint>

#include "world/geom.h"

namespace tg::game {

enum class Sfx : uint8_t {
    Pistol,
    Scattergun,
    Click,       // dry fire
    EnemyShot,
    Fireball,
    Claw,
    Pain,
    EnemyDeath,
    PlayerHurt,
    PlayerDeath,
    Pickup,
    Door,
    Lift,
    Win,
    Count
};

struct SoundEvent {
    Sfx id;
    Vec2 pos;
    bool positional = false;  // false: plays at full volume (player's own sounds)
};

}  // namespace tg::game
