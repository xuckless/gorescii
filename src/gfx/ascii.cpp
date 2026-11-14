#include "gfx/ascii.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tg::gfx {
namespace {

void gaussianKernel(float sigma, std::vector<float>& k) {
    int r = std::max(1, static_cast<int>(std::ceil(sigma * 2.5f)));
    k.assign(static_cast<size_t>(2 * r + 1), 0.0f);
    float sum = 0;
    for (int i = -r; i <= r; ++i) {
        float v = std::exp(-(i * i) / (2 * sigma * sigma));
        k[static_cast<size_t>(i + r)] = v;
        sum += v;
    }
    for (float& v : k) v /= sum;
}

// Separable blur with clamped edges; the interior skips the clamping.
void blur(const std::vector<float>& src, std::vector<float>& dst, std::vector<float>& tmp, int w, int h,
          const std::vector<float>& k) {
    const int r = static_cast<int>(k.size() / 2);
    const float* kc = k.data() + r;
    tmp.resize(src.size());
    dst.resize(src.size());
    for (int y = 0; y < h; ++y) {
        const float* row = &src[static_cast<size_t>(y) * w];
        float* out = &tmp[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x) {
            float acc = 0;
            if (x >= r && x < w - r) {
                for (int i = -r; i <= r; ++i) acc += row[x + i] * kc[i];
            } else {
                for (int i = -r; i <= r; ++i) acc += row[std::clamp(x + i, 0, w - 1)] * kc[i];
            }
            out[x] = acc;
        }
    }
    std::fill(dst.begin(), dst.end(), 0.0f);
    for (int y = 0; y < h; ++y) {
        float* out = &dst[static_cast<size_t>(y) * w];
        for (int i = -r; i <= r; ++i) {
            const float* in = &tmp[static_cast<size_t>(std::clamp(y + i, 0, h - 1)) * w];
            const float kv = kc[i];
            for (int x = 0; x < w; ++x) out[x] += in[x] * kv;  // row-wise: vectorises
        }
    }
}

}  // namespace

int AsciiShader::directionBucket(float gx, float gy) {
    if (gx == 0 && gy == 0) return -1;
    float theta = std::atan2(gy, gx);
    float a = std::abs(theta) / std::numbers::pi_v<float>;  // 0..1
    // Gradient points across the edge, so a horizontal gradient means a vertical line.
    if (a < 0.125f || a > 0.875f) return 0;      // |
    if (a > 0.375f && a < 0.625f) return 1;      // _
    if (a <= 0.375f) return theta > 0 ? 2 : 3;   // gradient down-right -> '/'
    return theta > 0 ? 3 : 2;                    // gradient down-left  -> '\'
}

void AsciiShader::computeEdges(const Framebuffer& fb) {
    const int w = fb.width, h = fb.height;
    const size_t n = static_cast<size_t>(w) * h;
    const AsciiSettings& s = settings;

    // Difference of Gaussians, thresholded to a binary edge map.
    gaussianKernel(s.dogSigma, kernel_);
    blur(lum_, blurA_, tmp_, w, h, kernel_);
    gaussianKernel(s.dogSigma * s.dogK, kernel_);
    blur(lum_, blurB_, tmp_, w, h, kernel_);
    dog_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        float d = blurA_[i] - s.dogTau * blurB_[i];
        dog_[i] = d >= s.dogThreshold ? 1.0f : 0.0f;
    }

    dir_.assign(n, -1);
    if (s.depthEdges) {
        logDepth_.resize(n);
        for (size_t i = 0; i < n; ++i) logDepth_[i] = std::log(std::max(fb.depth[i], 1e-3f));
    }
    const bool useSurface = s.surfaceEdges && fb.surface.size() == n;
    const float depthT = s.depthThreshold * 4;  // Sobel weights sum to 4
    // 3x3 Sobel at index i of a row-major w-wide buffer (interior pixels only).
    auto sobel = [w](const float* p, size_t i, float& gx, float& gy) {
        const float a = p[i - w - 1], b = p[i - w], c = p[i - w + 1];
        const float d = p[i - 1], f = p[i + 1];
        const float g = p[i + w - 1], hh = p[i + w], k = p[i + w + 1];
        gx = (c + 2 * f + k) - (a + 2 * d + g);
        gy = (g + 2 * hh + k) - (a + 2 * b + c);
    };
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            float gx, gy;
            sobel(dog_.data(), i, gx, gy);
            float mag = gx * gx + gy * gy;
            bool edge = mag > 0.5f;

            if (s.depthEdges) {
                float zx, zy;
                sobel(logDepth_.data(), i, zx, zy);
                float zmag = zx * zx + zy * zy;
                if (zmag > depthT * depthT && zmag * 16 > mag) {
                    gx = zx;
                    gy = zy;
                    edge = true;
                }
            }
            if (useSurface && !edge) {
                const uint32_t* sf = fb.surface.data();
                const uint32_t self = sf[i];
                auto df = [&](size_t j) { return sf[j] != self ? 1.0f : 0.0f; };
                // Sobel over "neighbour is a different surface"; opposite signs
                // map to the same glyph, so the sign doesn't matter.
                float sx = (df(i - w + 1) + 2 * df(i + 1) + df(i + w + 1)) - (df(i - w - 1) + 2 * df(i - 1) + df(i + w - 1));
                float sy = (df(i + w - 1) + 2 * df(i + w) + df(i + w + 1)) - (df(i - w - 1) + 2 * df(i - w) + df(i - w + 1));
                if (sx != 0 || sy != 0) {
                    gx = sx;
                    gy = sy;
                    edge = true;
                }
            }
            if (edge) dir_[i] = static_cast<signed char>(directionBucket(gx, gy));
        }
    }
}

void AsciiShader::apply(const Framebuffer& fb, int cellW, int cellH, CellGrid& grid) {
    const AsciiSettings& s = settings;
    const int w = fb.width, h = fb.height;
    const size_t n = static_cast<size_t>(w) * h;
    lum_.resize(n);
    for (size_t i = 0; i < n; ++i) lum_[i] = luminance(fb.color[i]);

    if (s.edges) computeEdges(fb);

    const int rampLen = static_cast<int>(s.ramp.size());
    const int perCell = cellW * cellH;
    const int need = std::max(1, static_cast<int>(std::ceil(s.edgeCoverage * (cellW + cellH))));

    for (int cy = 0; cy < grid.rows; ++cy) {
        for (int cx = 0; cx < grid.cols; ++cx) {
            float lsum = 0, rs = 0, gs = 0, bs = 0;
            int votes[4] = {0, 0, 0, 0};
            for (int py = 0; py < cellH; ++py) {
                int y = cy * cellH + py;
                if (y >= h) break;
                for (int px = 0; px < cellW; ++px) {
                    int x = cx * cellW + px;
                    if (x >= w) break;
                    size_t idx = static_cast<size_t>(y) * w + x;
                    lsum += lum_[idx];
                    Rgb c = fb.color[idx];
                    rs += c.r;
                    gs += c.g;
                    bs += c.b;
                    if (s.edges && dir_[idx] >= 0) ++votes[dir_[idx]];
                }
            }
            float inv = 1.0f / perCell;
            float l = std::pow(std::clamp(lsum * inv * s.exposure, 0.0f, 1.0f), s.attenuation);

            Cell& cell = grid.at(cx, cy);
            int q = std::clamp(static_cast<int>(l * rampLen), 0, rampLen - 1);
            cell.glyph = s.ramp[static_cast<size_t>(q)];
            if (s.edges) {
                int best = 0;
                for (int k = 1; k < 4; ++k)
                    if (votes[k] > votes[best]) best = k;
                if (votes[best] >= need && l > 0.02f) cell.glyph = s.edgeGlyphs[best];
            }

            float r = rs * inv, g = gs * inv, b = bs * inv;
            float m = std::max({r, g, b, 1.0f});
            float scale = 1.0f + s.colorNormalize * (255.0f / m - 1.0f);
            // Monochrome keeps some floor brightness so dim glyphs stay legible.
            float mono = 0.35f + 0.65f * l;
            auto mix = [&](float v, uint8_t t) {
                float colored = std::min(255.0f, v * scale);
                return static_cast<uint8_t>(std::clamp(s.colorBlend * colored + (1 - s.colorBlend) * t * mono, 0.0f, 255.0f));
            };
            cell.fg = {mix(r, s.tint.r), mix(g, s.tint.g), mix(b, s.tint.b)};
            cell.bg = {static_cast<uint8_t>(r * s.backgroundBlend), static_cast<uint8_t>(g * s.backgroundBlend),
                       static_cast<uint8_t>(b * s.backgroundBlend)};
        }
    }
}

}  // namespace tg::gfx
