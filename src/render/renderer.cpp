#include "render/renderer.h"

#include <algorithm>
#include <cmath>

#include "render/textures.h"

namespace tg::render {
namespace {

constexpr float kNear = 0.05f;
constexpr float kSkyDepth = 1e6f;
constexpr int kMaxPortalDepth = 64;

gfx::Rgb lit(gfx::Rgb c, float k) {
    auto f = [&](uint8_t v) { return static_cast<uint8_t>(std::min(255.0f, v * k)); };
    return {f(c.r), f(c.g), f(c.b)};
}

uint32_t mix(uint32_t h, int32_t v) {
    h ^= static_cast<uint32_t>(v) + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}
int32_t q(float v) { return static_cast<int32_t>(std::lround(v * 100.0f)); }

}  // namespace

uint32_t Renderer::flatId(const Sector& s, bool ceiling) {
    // Keyed by height and texture so coplanar neighbouring sectors don't seam.
    uint32_t h = mix(ceiling ? 2u : 1u, q(ceiling ? s.ceil : s.floor));
    return mix(h, static_cast<int32_t>(ceiling ? s.ceilTex : s.floorTex)) | 1u;
}

uint32_t Renderer::wallId(Vec2 a, Vec2 b) {
    // Keyed by the infinite line, so collinear split edges read as one wall.
    Vec2 n = normalize(Vec2{b.y - a.y, a.x - b.x});
    if (n.x < -1e-4f || (std::abs(n.x) <= 1e-4f && n.y < 0)) n = n * -1.0f;
    uint32_t h = mix(3u, q(n.x));
    h = mix(h, q(n.y));
    return mix(h, q(dot(n, a))) | 1u;
}

float Renderer::shadeFor(float light, float dist) {
    float fall = std::clamp(1.3f - dist * 0.055f, 0.12f, 1.0f);
    return light * fall * 1.15f;
}

void Renderer::render(const Map& map, const Camera& cam, gfx::Framebuffer& fb) {
    map_ = &map;
    cam_ = cam;
    fb_ = &fb;
    const int w = fb.width, h = fb.height;
    focal_ = (w * 0.5f) / std::tan(cam.fovDeg * 3.14159265f / 360.0f);
    horizon_ = h * 0.5f;
    fwd_ = {std::cos(cam.angle), std::sin(cam.angle)};
    right_ = {fwd_.y, -fwd_.x};
    yTop_.assign(static_cast<size_t>(w), 0);
    yBot_.assign(static_cast<size_t>(w), h);
    rayDir_.resize(static_cast<size_t>(w));
    for (int x = 0; x < w; ++x) rayDir_[static_cast<size_t>(x)] = fwd_ + right_ * ((x + 0.5f - w * 0.5f) / focal_);

    fb.clear();
    int start = cam.sector >= 0 ? cam.sector : map.sectorAt(cam.pos);
    if (start < 0) return;
    drawSector(start, 0, w, 0);
}

void Renderer::drawSky(int x, int yStart, int yEnd) {
    // Panorama: hue drifts with view angle, darker toward the horizon.
    float ang = std::atan2(rayDir_[static_cast<size_t>(x)].y, rayDir_[static_cast<size_t>(x)].x);
    float band = 0.5f + 0.5f * std::sin(ang * 3.0f) * std::sin(ang * 7.0f + 1.3f);
    for (int y = yStart; y < yEnd; ++y) {
        float t = std::clamp(y / std::max(horizon_, 1.0f), 0.0f, 1.0f);  // 0 top .. 1 horizon
        float cloud = band * std::clamp(1.0f - std::abs(t - 0.55f) * 4.0f, 0.0f, 1.0f);
        gfx::Rgb c{static_cast<uint8_t>(40 + 150 * t + 50 * cloud), static_cast<uint8_t>(20 + 60 * t + 35 * cloud),
                   static_cast<uint8_t>(35 + 30 * t + 20 * cloud)};
        size_t i = static_cast<size_t>(y) * fb_->width + x;
        fb_->color[i] = c;
        fb_->depth[i] = kSkyDepth;
        fb_->surface[i] = 0;
    }
}

void Renderer::drawFlat(int x, int yStart, int yEnd, const Sector& s, bool ceiling) {
    if (yStart >= yEnd) return;
    if (ceiling && s.sky()) {
        drawSky(x, yStart, yEnd);
        return;
    }
    float height = ceiling ? s.ceil - cam_.z : cam_.z - s.floor;
    if (height <= 0) return;
    const Texture& tex = texture(ceiling ? s.ceilTex : s.floorTex);
    const uint32_t id = flatId(s, ceiling);
    bool glow = emissive(ceiling ? s.ceilTex : s.floorTex);
    Vec2 ray = rayDir_[static_cast<size_t>(x)];
    for (int y = yStart; y < yEnd; ++y) {
        float dy = ceiling ? horizon_ - (y + 0.5f) : (y + 0.5f) - horizon_;
        if (dy <= 0) continue;
        float dist = height * focal_ / dy;
        Vec2 p = cam_.pos + ray * dist;
        // Flats are seen at a grazing angle, so bias the level by the slope.
        float tpp = kTexelsPerMetre * dist / focal_ * std::max(1.0f, dist / std::max(height, 0.1f) * 0.5f);
        gfx::Rgb c = tex.sample(static_cast<int>(std::floor(p.x * kTexelsPerMetre)),
                                static_cast<int>(std::floor(-p.y * kTexelsPerMetre)), mipLevel(tpp));
        size_t i = static_cast<size_t>(y) * fb_->width + x;
        fb_->color[i] = lit(c, glow ? 1.0f : shadeFor(s.light, dist));
        fb_->depth[i] = dist;
        fb_->surface[i] = id;
    }
}

void Renderer::drawSector(int sectorIndex, int x0, int x1, int depth) {
    if (depth > kMaxPortalDepth || x0 >= x1) return;
    const Sector& s = map_->sectors[static_cast<size_t>(sectorIndex)];
    const float camZ = cam_.z;
    const int w = fb_->width;

    for (size_t e = 0; e < s.edgeCount(); ++e) {
        // Only the inward face is visible (interior is on the left of a->b).
        if (side(s.a(e), s.b(e), cam_.pos) <= 0) continue;
        // Seen from inside a CCW polygon, b is on screen-left of a: project b..a.
        Vec2 a = s.b(e), b = s.a(e);

        Vec2 ra = a - cam_.pos, rb = b - cam_.pos;
        float da = dot(ra, fwd_), db = dot(rb, fwd_);
        float xa = dot(ra, right_), xb = dot(rb, right_);
        float edgeLen = length(b - a);
        float ua = 0, ub = edgeLen;
        if (da < kNear && db < kNear) continue;
        if (da < kNear) {
            float t = (kNear - da) / (db - da);
            xa += (xb - xa) * t;
            ua += (ub - ua) * t;
            da = kNear;
        } else if (db < kNear) {
            float t = (kNear - db) / (da - db);
            xb += (xa - xb) * t;
            ub += (ua - ub) * t;
            db = kNear;
        }
        float sxa = w * 0.5f + xa / da * focal_;
        float sxb = w * 0.5f + xb / db * focal_;
        if (sxa >= sxb) continue;
        int cx0 = std::max(x0, static_cast<int>(std::ceil(sxa - 0.5f)));
        int cx1 = std::min(x1, static_cast<int>(std::ceil(sxb - 0.5f)));
        if (cx0 >= cx1) continue;

        int nIndex = s.neighbor[e];
        const Sector* n = nIndex >= 0 ? &map_->sectors[static_cast<size_t>(nIndex)] : nullptr;
        bool bothSky = n && s.sky() && n->sky();
        const Texture& wallTex = texture(s.wallTex);
        const Texture& nWallTex = n ? texture(n->wallTex) : wallTex;
        Vec2 dir = b - a;
        const uint32_t id = wallId(a, b);
        float contrast = std::abs(dir.x) > std::abs(dir.y) ? 0.92f : 1.08f;

        float invA = 1.0f / da, invB = 1.0f / db;
        float uA = ua * invA, uB = ub * invB;
        for (int x = cx0; x < cx1; ++x) {
            int top = yTop_[static_cast<size_t>(x)], bot = yBot_[static_cast<size_t>(x)];
            if (top >= bot) continue;
            float t = std::clamp((x + 0.5f - sxa) / (sxb - sxa), 0.0f, 1.0f);
            float inv = invA + (invB - invA) * t;
            float dist = 1.0f / inv;
            float u = (uA + (uB - uA) * t) * dist;
            int tu = static_cast<int>(std::floor(u * kTexelsPerMetre));
            float k = shadeFor(s.light, dist) * contrast;
            const int mip = mipLevel(kTexelsPerMetre * dist / focal_);

            auto proj = [&](float z) { return horizon_ - (z - camZ) * focal_ * inv; };
            float yc = proj(s.ceil), yf = proj(s.floor);
            int ycI = std::clamp(static_cast<int>(std::ceil(yc - 0.5f)), top, bot);
            int yfI = std::clamp(static_cast<int>(std::ceil(yf - 0.5f)), top, bot);

            drawFlat(x, top, ycI, s, true);
            drawFlat(x, yfI, bot, s, false);

            auto wallSpan = [&](int y0, int y1, const Texture& tex) {
                for (int y = y0; y < y1; ++y) {
                    float z = camZ + (horizon_ - (y + 0.5f)) * dist / focal_;
                    int tv = static_cast<int>(std::floor(-z * kTexelsPerMetre));
                    size_t i = static_cast<size_t>(y) * w + x;
                    fb_->color[i] = lit(tex.sample(tu, tv, mip), k);
                    fb_->depth[i] = dist;
                    fb_->surface[i] = id;
                }
            };

            if (!n) {
                wallSpan(ycI, yfI, wallTex);
                yTop_[static_cast<size_t>(x)] = yBot_[static_cast<size_t>(x)] = 0;
                continue;
            }
            // Upper wall where the neighbour's ceiling is lower, lower wall where its floor is higher.
            float nyc = proj(std::min(n->ceil, s.ceil)), nyf = proj(std::max(n->floor, s.floor));
            int nycI = std::clamp(static_cast<int>(std::ceil(nyc - 0.5f)), ycI, yfI);
            int nyfI = std::clamp(static_cast<int>(std::ceil(nyf - 0.5f)), nycI, yfI);
            if (bothSky) {
                drawSky(x, ycI, nycI);  // keep the sky continuous across open-air portals
            } else {
                wallSpan(ycI, nycI, nWallTex);
            }
            wallSpan(nyfI, yfI, nWallTex);
            yTop_[static_cast<size_t>(x)] = bothSky ? top : nycI;
            yBot_[static_cast<size_t>(x)] = nyfI;
        }
        if (n) drawSector(nIndex, cx0, cx1, depth + 1);
    }
}

void Renderer::drawSprites(std::span<const SpriteInstance> sprites, gfx::Framebuffer& fb) {
    const int w = fb.width, h = fb.height;
    for (const SpriteInstance& sp : sprites) {
        if (!sp.image || sp.image->w == 0) continue;
        Vec2 rel = sp.pos - cam_.pos;
        float d = dot(rel, fwd_);
        if (d < 0.2f) continue;
        const Image& img = *sp.image;
        float scale = focal_ / d;
        float worldW = sp.height * img.w / img.h;
        float cx = w * 0.5f + dot(rel, right_) * scale;
        float halfW = worldW * 0.5f * scale;
        float top = horizon_ - (sp.z + sp.height - cam_.z) * scale;
        float bottom = horizon_ - (sp.z - cam_.z) * scale;
        int x0 = std::max(0, static_cast<int>(std::ceil(cx - halfW - 0.5f)));
        int x1 = std::min(w, static_cast<int>(std::ceil(cx + halfW - 0.5f)));
        int y0 = std::max(0, static_cast<int>(std::ceil(top - 0.5f)));
        int y1 = std::min(h, static_cast<int>(std::ceil(bottom - 0.5f)));
        float k = sp.fullbright ? 1.2f : shadeFor(sp.light, d);
        for (int x = x0; x < x1; ++x) {
            int tx = std::clamp(static_cast<int>((x + 0.5f - (cx - halfW)) / (2 * halfW) * img.w), 0, img.w - 1);
            for (int y = y0; y < y1; ++y) {
                size_t i = static_cast<size_t>(y) * w + x;
                if (fb.depth[i] <= d) continue;
                int ty = std::clamp(static_cast<int>((y + 0.5f - top) / (bottom - top) * img.h), 0, img.h - 1);
                if (!img.solid(tx, ty)) continue;
                fb.color[i] = lit(img.at(tx, ty), k);
                fb.depth[i] = d;
                fb.surface[i] = sp.id | 0x80000000u;
            }
        }
    }
}

void Renderer::drawOverlay(const Image& img, float centerX, float bottomY, float pixel, float light,
                           gfx::Framebuffer& fb) {
    float left = centerX - img.w * pixel * 0.5f, top = bottomY - img.h * pixel;
    int x0 = std::max(0, static_cast<int>(left)), x1 = std::min(fb.width, static_cast<int>(left + img.w * pixel));
    int y0 = std::max(0, static_cast<int>(top)), y1 = std::min(fb.height, static_cast<int>(bottomY));
    for (int y = y0; y < y1; ++y) {
        int ty = std::clamp(static_cast<int>((y + 0.5f - top) / pixel), 0, img.h - 1);
        for (int x = x0; x < x1; ++x) {
            int tx = std::clamp(static_cast<int>((x + 0.5f - left) / pixel), 0, img.w - 1);
            if (!img.solid(tx, ty)) continue;
            size_t i = static_cast<size_t>(y) * fb.width + x;
            fb.color[i] = lit(img.at(tx, ty), light);
            fb.depth[i] = 0.3f;
            fb.surface[i] = 0xfffffffeu;
        }
    }
}

}  // namespace tg::render
