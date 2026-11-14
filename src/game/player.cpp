#include "game/player.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tg::game {
namespace {

constexpr float kGravity = 18.0f;
constexpr float kWalkSpeed = 5.0f, kRunSpeed = 8.5f;
constexpr float kAccel = 60.0f, kFriction = 10.0f;
constexpr float kTurnSpeed = 2.6f;

// Sectors near `start`, to collide against: the current one plus two portal hops.
void nearby(const Map& map, int start, std::vector<int>& out) {
    out.clear();
    out.push_back(start);
    for (size_t i = 0; i < out.size() && i < 64; ++i) {
        if (i > 0 && out.size() > 32) break;
        const Sector& s = map.sectors[static_cast<size_t>(out[i])];
        for (int n : s.neighbor)
            if (n >= 0 && std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n);
        if (i >= 8) break;
    }
}

}  // namespace

bool blocks(const Sector& dest, float z, float height) {
    if (dest.floor - z > Body::kStep) return true;
    return dest.ceil - std::max(dest.floor, z) < height;
}

void moveAndSlide(const Map& map, Body& b, Vec2 delta) {
    // Sub-step so fast moves cannot tunnel through thin walls.
    float dist = length(delta);
    int steps = std::max(1, static_cast<int>(std::ceil(dist / (b.radius * 0.5f))));
    Vec2 stepDelta = delta * (1.0f / steps);
    std::vector<int> near;
    for (int st = 0; st < steps; ++st) {
        Vec2 p = b.pos + stepDelta;
        nearby(map, b.sector, near);
        // A few relaxation passes resolve corners.
        for (int pass = 0; pass < 3; ++pass) {
            for (int si : near) {
                const Sector& s = map.sectors[static_cast<size_t>(si)];
                for (size_t e = 0; e < s.edgeCount(); ++e) {
                    Vec2 a = s.a(e), c = s.b(e);
                    bool inside = side(a, c, p) > 0;  // p on this sector's side
                    bool solid = s.neighbor[e] < 0;
                    if (!solid) {
                        const Sector& other = map.sectors[static_cast<size_t>(s.neighbor[e])];
                        const Sector& dest = inside ? other : s;
                        if (!blocks(dest, b.z, b.height)) continue;
                    }
                    Vec2 q = closestOnSegment(a, c, p);
                    Vec2 away = p - q;
                    float d = length(away);
                    if (d >= b.radius) continue;
                    Vec2 n = d > 1e-5f ? away * (1.0f / d) : normalize(Vec2{a.y - c.y, c.x - a.x});
                    if (solid && !inside) n = n * -1.0f;  // never get pushed out of the level
                    p += n * (b.radius - d + 1e-4f);
                }
            }
        }
        int ns = map.sectorAt(p, b.sector);
        if (ns < 0) break;  // would leave the map: stop here
        // Remove velocity into whatever we hit so we slide instead of sticking.
        Vec2 actual = p - b.pos, wanted = stepDelta;
        if (dot(wanted, wanted) > 1e-12f && dot(actual, actual) < dot(wanted, wanted) * 0.999f) {
            Vec2 lost = wanted - actual;
            float ll = length(lost);
            if (ll > 1e-6f) {
                Vec2 ln = lost * (1.0f / ll);
                float into = dot(b.vel, ln);
                if (into > 0) b.vel -= ln * into;
            }
        }
        b.pos = p;
        b.sector = ns;
    }
}

void stepPlayer(const Map& map, Body& b, const Controls& c, float dt) {
    b.angle += c.turn * kTurnSpeed * (c.run ? 1.3f : 1.0f) * dt + c.look;
    b.angle = std::remainder(b.angle, 2.0f * 3.14159265f);

    Vec2 fwd{std::cos(b.angle), std::sin(b.angle)}, right{fwd.y, -fwd.x};
    Vec2 wish = fwd * c.forward + right * c.strafe;
    float wl = length(wish);
    if (wl > 1) wish = wish * (1.0f / wl);
    float maxSpeed = c.run ? kRunSpeed : kWalkSpeed;

    // Accelerate toward the wished velocity; friction otherwise (weaker in the air).
    float control = b.onGround ? 1.0f : 0.25f;
    Vec2 target = wish * maxSpeed;
    Vec2 diff = target - b.vel;
    float dl = length(diff);
    float rate = (wl > 0.01f ? kAccel : kFriction * std::max(1.0f, length(b.vel))) * control * dt;
    b.vel += dl > rate ? diff * (rate / dl) : diff;

    moveAndSlide(map, b, b.vel * dt);
    applyGravity(map, b, dt);
}

void applyGravity(const Map& map, Body& b, float dt) {
    const Sector& s = map.sectors[static_cast<size_t>(b.sector)];
    if (b.z > s.floor + 1e-3f) {
        b.vz -= kGravity * dt;
        b.z += b.vz * dt;
        b.onGround = false;
    }
    if (b.z <= s.floor) {
        b.z = s.floor;  // also steps us up onto stairs
        b.vz = 0;
        b.onGround = true;
    }
    if (b.z + b.height > s.ceil) b.z = std::max(s.floor, s.ceil - b.height);
}

}  // namespace tg::game
