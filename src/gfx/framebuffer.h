#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace tg::gfx {

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    friend bool operator==(Rgb, Rgb) = default;
};

inline float luminance(Rgb c) { return (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f; }

// What the 3D renderer draws into: colour plus view depth per pixel.
struct Framebuffer {
    int width = 0, height = 0;
    std::vector<Rgb> color;
    std::vector<float> depth;  // view-space distance; large = far / sky
    // Which surface a pixel belongs to (0 = sky / nothing). The ASCII pass
    // draws edges where it changes, like a normals buffer in AcerolaFX.
    std::vector<uint32_t> surface;

    void resize(int w, int h) {
        width = w;
        height = h;
        color.assign(static_cast<size_t>(w) * h, Rgb{});
        depth.assign(static_cast<size_t>(w) * h, 1e9f);
        surface.assign(static_cast<size_t>(w) * h, 0);
    }
    void clear(Rgb c = {}) {
        std::fill(color.begin(), color.end(), c);
        std::fill(depth.begin(), depth.end(), 1e9f);
        std::fill(surface.begin(), surface.end(), 0u);
    }
    Rgb& at(int x, int y) { return color[static_cast<size_t>(y) * width + x]; }
    Rgb at(int x, int y) const { return color[static_cast<size_t>(y) * width + x]; }
};

}  // namespace tg::gfx
