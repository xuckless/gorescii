#pragma once
// Portal renderer for convex sectors: walks from the camera's sector through
// portals, narrowing per-column clip bounds, drawing walls, floors and sky
// into a colour+depth framebuffer.
#include <vector>

#include <span>

#include "gfx/framebuffer.h"
#include "render/sprites.h"
#include "world/map.h"

namespace tg::render {

struct Camera {
    Vec2 pos;
    float z = 1.6f;      // eye height (absolute)
    float angle = 0;     // radians, CCW from +x
    int sector = -1;
    float fovDeg = 90;   // horizontal
};

// A billboard standing in the world.
struct SpriteInstance {
    const Image* image = nullptr;
    Vec2 pos;
    float z = 0;          // bottom
    float height = 1.7f;  // world height; width follows the image aspect
    float light = 1.0f;   // sector light where it stands
    bool fullbright = false;
    uint32_t id = 0;      // unique per object so the ASCII pass outlines it
};

class Renderer {
public:
    void render(const Map& map, const Camera& cam, gfx::Framebuffer& fb);

    // Draws sprites into the last rendered frame, depth-tested against it.
    void drawSprites(std::span<const SpriteInstance> sprites, gfx::Framebuffer& fb);
    // Draws a screen-space image (the held weapon) with no depth test.
    // `pixel` is framebuffer pixels per image pixel.
    void drawOverlay(const Image& img, float centerX, float bottomY, float pixel, float light, gfx::Framebuffer& fb);

    // Projection used for the last frame, for drawing sprites in the same space.
    float focal() const { return focal_; }
    float horizon() const { return horizon_; }

    // Brightness multiplier for a surface at `dist` metres in light `light`.
    static float shadeFor(float light, float dist);

private:
    void drawSector(int sector, int x0, int x1, int depth);
    void drawFlat(int x, int yStart, int yEnd, const Sector& s, bool ceiling);
    static uint32_t flatId(const Sector& s, bool ceiling);
    static uint32_t wallId(Vec2 a, Vec2 b);
    void drawSky(int x, int yStart, int yEnd);

    const Map* map_ = nullptr;
    Camera cam_;  // copied: callers often pass a temporary
    gfx::Framebuffer* fb_ = nullptr;
    float focal_ = 1, horizon_ = 0;
    Vec2 fwd_, right_;
    std::vector<int> yTop_, yBot_;   // drawable rows per column: [top, bot)
    std::vector<Vec2> rayDir_;       // per column, scaled so dot(ray, fwd) == 1
};

}  // namespace tg::render
