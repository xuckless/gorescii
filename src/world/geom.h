#pragma once
#include <algorithm>
#include <cmath>

namespace tg {

struct Vec2 {
    float x = 0, y = 0;
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    friend bool operator==(Vec2, Vec2) = default;
};

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }
inline Vec2 normalize(Vec2 a) {
    float l = length(a);
    return l > 0 ? a * (1.0f / l) : Vec2{};
}
// > 0 when p is left of the directed line a->b.
inline float side(Vec2 a, Vec2 b, Vec2 p) { return cross(b - a, p - a); }

inline Vec2 closestOnSegment(Vec2 a, Vec2 b, Vec2 p) {
    Vec2 ab = b - a;
    float t = dot(p - a, ab) / std::max(dot(ab, ab), 1e-12f);
    t = std::fmax(0.0f, std::fmin(1.0f, t));
    return a + ab * t;
}

}  // namespace tg
