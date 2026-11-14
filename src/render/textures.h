#pragma once
// Procedurally generated textures (no image assets).
#include <vector>

#include "gfx/framebuffer.h"
#include "world/map.h"

namespace tg::render {

struct Texture {
    int size = 64;  // square, power of two
    std::vector<gfx::Rgb> texels;
    gfx::Rgb sample(int u, int v) const { return texels[static_cast<size_t>((v & (size - 1)) * size + (u & (size - 1)))]; }
};

// Texels per world metre for walls, floors and ceilings.
constexpr float kTexelsPerMetre = 32.0f;

const Texture& texture(Tex t);
// Self-lit textures ignore distance falloff.
bool emissive(Tex t);

}  // namespace tg::render
