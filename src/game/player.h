#pragma once
#include "world/map.h"

namespace tg::game {

// What the player wants to do this tick, independent of the input device.
struct Controls {
    float forward = 0;  // -1..1
    float strafe = 0;   // -1..1, + is right
    float turn = 0;     // -1..1, + is left (counter-clockwise)
    float look = 0;     // radians to turn this tick (mouse), + is left
    bool run = false;
    bool fire = false;  // held
    bool use = false;   // pressed this tick
    int weaponSlot = 0; // 1-based slot pressed this tick, 0 = none
    bool nextWeapon = false;
};

struct Body {
    static constexpr float kStep = 0.65f;
    static constexpr float kEye = 1.6f;

    float radius = 0.32f;
    float height = 1.8f;
    Vec2 pos;
    Vec2 vel;
    float z = 0;   // feet height
    float vz = 0;
    float angle = 0;
    int sector = -1;
    bool onGround = true;
};

// True if a body of `height` standing at feet height `z` cannot cross into `dest`.
bool blocks(const Sector& dest, float z, float height);

// Moves `b` by `delta`, sliding along anything that blocks it.
void moveAndSlide(const Map& map, Body& b, Vec2 delta);

// Gravity, landing and ceiling clamp for one tick.
void applyGravity(const Map& map, Body& b, float dt);

// Applies controls, friction, gravity and collision for one tick.
void stepPlayer(const Map& map, Body& b, const Controls& c, float dt);

}  // namespace tg::game
