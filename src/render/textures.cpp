#include "render/textures.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace tg::render {
namespace {

using gfx::Rgb;

uint32_t hash(int x, int y, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(seed) * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
float rnd(int x, int y, int seed) { return (hash(x, y, seed) & 0xffff) / 65535.0f; }

// Tileable value noise with period `period` cells over a 64 texture.
float noise(float x, float y, int period, int seed) {
    int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    float fx = x - x0, fy = y - y0;
    auto r = [&](int ix, int iy) { return rnd(((ix % period) + period) % period, ((iy % period) + period) % period, seed); };
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    float a = r(x0, y0) + (r(x0 + 1, y0) - r(x0, y0)) * sx;
    float b = r(x0, y0 + 1) + (r(x0 + 1, y0 + 1) - r(x0, y0 + 1)) * sx;
    return a + (b - a) * sy;
}
float fbm(int u, int v, int seed) {
    float s = 0, amp = 0.5f;
    int period = 4;
    for (int o = 0; o < 4; ++o) {
        s += amp * noise(u * period / 64.0f, v * period / 64.0f, period, seed + o);
        amp *= 0.5f;
        period *= 2;
    }
    return s;
}

Rgb shade(Rgb c, float k) {
    auto f = [&](uint8_t v) { return static_cast<uint8_t>(std::clamp(v * k, 0.0f, 255.0f)); };
    return {f(c.r), f(c.g), f(c.b)};
}

Texture make(Tex t) {
    Texture tex;
    const int n = tex.size;
    tex.texels.resize(static_cast<size_t>(n * n));
    for (int v = 0; v < n; ++v) {
        for (int u = 0; u < n; ++u) {
            float grain = 0.8f + 0.4f * fbm(u, v, static_cast<int>(t) * 17);
            Rgb c{};
            switch (t) {
                case Tex::Brick: {
                    int row = v / 16, off = (row % 2) * 16;
                    bool mortar = (v % 16) < 2 || ((u + off) % 32) < 2;
                    float tint = 0.85f + 0.3f * rnd((u + off) / 32, row, 3);
                    c = mortar ? Rgb{95, 70, 58} : shade(Rgb{150, 60, 40}, tint * grain);
                    break;
                }
                case Tex::Metal: {
                    bool seam = u % 32 == 0 || v % 32 == 0;
                    bool rivet = (u % 32 == 4 || u % 32 == 28) && (v % 32 == 4 || v % 32 == 28);
                    c = seam ? Rgb{80, 86, 92} : rivet ? Rgb{200, 205, 210} : shade(Rgb{110, 118, 125}, grain);
                    break;
                }
                case Tex::Stone: {
                    // Irregular blocks: jittered grid with dark gaps.
                    int bx = u / 21, by = v / 16;
                    float k = 0.7f + 0.5f * rnd(bx, by, 9);
                    bool gap = (v % 16) < 1 || ((u + by * 7) % 21) < 1;
                    c = gap ? Rgb{70, 68, 60} : shade(Rgb{125, 120, 105}, k * grain);
                    break;
                }
                case Tex::Tech: {
                    bool frame = u % 32 < 2 || v % 32 < 2;
                    bool light = (u % 32 > 12 && u % 32 < 20) && (v % 32 > 22 && v % 32 < 26);
                    c = frame ? Rgb{45, 52, 60} : light ? Rgb{120, 230, 255} : shade(Rgb{60, 70, 85}, grain);
                    break;
                }
                case Tex::Wood: {
                    bool joint = u % 16 == 0;
                    float ring = 0.85f + 0.25f * std::sin(v * 0.35f + fbm(u, v, 5) * 6.0f);
                    c = joint ? Rgb{45, 28, 15} : shade(Rgb{125, 80, 45}, ring * (0.9f + 0.2f * rnd(u / 16, 0, 2)));
                    break;
                }
                case Tex::Tile: {
                    bool grout = u % 16 == 0 || v % 16 == 0;
                    bool dark = ((u / 16) + (v / 16)) % 2;
                    c = grout ? Rgb{85, 85, 88} : shade(dark ? Rgb{105, 105, 110} : Rgb{135, 135, 140}, grain);
                    break;
                }
                case Tex::Panel: {
                    bool grid = u % 32 < 2 || v % 32 < 2;
                    c = grid ? Rgb{70, 68, 62} : shade(Rgb{95, 92, 85}, grain);
                    break;
                }
                case Tex::Dirt: c = shade(Rgb{105, 85, 60}, 0.6f + 0.7f * fbm(u, v, 21)); break;
                case Tex::Slime: {
                    float f = fbm(u, v, 33);
                    c = shade(Rgb{60, 220, 50}, 0.6f + 0.6f * f);
                    break;
                }
                case Tex::Sky: {
                    // Only used as a fallback; the renderer draws the sky panorama.
                    c = Rgb{90, 60, 50};
                    break;
                }
                case Tex::Count: break;
            }
            tex.texels[static_cast<size_t>(v * n + u)] = c;
        }
    }
    tex.buildMips();
    return tex;
}

}  // namespace

void Texture::buildMips() {
    mips[0] = texels;
    for (int l = 1; l < kLevels; ++l) {
        int s = size >> l, ps = s * 2;
        const auto& prev = mips[l - 1];
        mips[l].resize(static_cast<size_t>(s * s));
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x) {
                int r = 0, g = 0, b = 0;
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        Rgb c = prev[static_cast<size_t>((y * 2 + dy) * ps + x * 2 + dx)];
                        r += c.r, g += c.g, b += c.b;
                    }
                mips[l][static_cast<size_t>(y * s + x)] = {static_cast<uint8_t>(r / 4), static_cast<uint8_t>(g / 4),
                                                            static_cast<uint8_t>(b / 4)};
            }
    }
}

int mipLevel(float texelsPerPixel) {
    if (texelsPerPixel <= 1.0f) return 0;
    int l = static_cast<int>(std::log2(texelsPerPixel));
    return std::clamp(l, 0, Texture::kLevels - 1);
}

const Texture& texture(Tex t) {
    static const std::array<Texture, static_cast<size_t>(Tex::Count)> all = [] {
        std::array<Texture, static_cast<size_t>(Tex::Count)> a;
        for (size_t i = 0; i < a.size(); ++i) a[i] = make(static_cast<Tex>(i));
        return a;
    }();
    return all[static_cast<size_t>(t)];
}

bool emissive(Tex t) { return t == Tex::Slime; }

}  // namespace tg::render
