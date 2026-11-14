#pragma once
// Procedurally generated textures (no image assets).
#include <vector>

#include "gfx/framebuffer.h"
#include "world/map.h"

namespace tg::render {

struct Texture {
    static constexpr int kLevels = 4;  // 64, 32, 16, 8
    int size = 64;                     // square, power of two
    std::vector<gfx::Rgb> texels;      // level 0
    std::vector<gfx::Rgb> mips[kLevels];  // box-filtered; mips[0] == texels

    gfx::Rgb sample(int u, int v) const { return texels[static_cast<size_t>((v & (size - 1)) * size + (u & (size - 1)))]; }
    // u, v in level-0 texels; `level` 0..kLevels-1.
    gfx::Rgb sample(int u, int v, int level) const {
        int s = size >> level;
        u >>= level;
        v >>= level;
        return mips[level][static_cast<size_t>((v & (s - 1)) * s + (u & (s - 1)))];
    }
    void buildMips();
};

// Mip level for a surface where one screen pixel spans `texelsPerPixel` texels.
int mipLevel(float texelsPerPixel);

// Texels per world metre for walls, floors and ceilings.
constexpr float kTexelsPerMetre = 32.0f;

const Texture& texture(Tex t);
// Self-lit textures ignore distance falloff.
bool emissive(Tex t);

}  // namespace tg::render
