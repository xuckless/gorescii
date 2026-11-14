#pragma once
// Pixel-art sprites, defined in code as palette-indexed rows.
#include <cstdint>
#include <vector>

#include "gfx/framebuffer.h"

namespace tg::render {

struct Image {
    int w = 0, h = 0;
    std::vector<gfx::Rgb> px;
    std::vector<uint8_t> opaque;
    bool solid(int x, int y) const { return opaque[static_cast<size_t>(y * w + x)] != 0; }
    gfx::Rgb at(int x, int y) const { return px[static_cast<size_t>(y * w + x)]; }
};

enum class SpriteId : uint8_t {
    Grunt,
    GruntFire,
    GruntDead,
    Brute,
    BruteFire,
    BruteDead,
    Medkit,
    Ammo,
    Armor,
    Pistol,
    PistolFire,
    Scattergun,
    ScattergunFire,
    Fireball,
    KeyRed,
    KeyBlue,
    Count
};

const Image& sprite(SpriteId id);

}  // namespace tg::render
