#include "world/trace.h"

#include <algorithm>

namespace tg {

float traceSegment(const Map& map, int sector, Vec2 from, float z0, Vec2 to, float z1) {
    if (sector < 0) return 0;
    Vec2 d = to - from;
    float tEnter = 0;
    for (int hop = 0; hop < 128; ++hop) {
        const Sector& s = map.sectors[static_cast<size_t>(sector)];
        // In a convex sector the segment leaves through exactly one edge.
        float bestT = 2;
        size_t bestE = 0;
        for (size_t e = 0; e < s.edgeCount(); ++e) {
            Vec2 a = s.a(e), b = s.b(e), ab = b - a;
            float denom = cross(d, ab);
            // Interior is on the left of a->b, so leaving means cross(d, ab) > 0.
            if (denom <= 0) continue;
            float t = cross(a - from, ab) / denom;
            float u = cross(a - from, d) / denom;
            if (u < -1e-4f || u > 1 + 1e-4f) continue;
            if (t >= tEnter - 1e-5f && t < bestT) {
                bestT = t;
                bestE = e;
            }
        }
        if (bestT >= 1.0f) return 1.0f;  // destination lies inside this sector
        int n = s.neighbor[bestE];
        if (n < 0) return std::max(0.0f, bestT);
        const Sector& ns = map.sectors[static_cast<size_t>(n)];
        float z = z0 + (z1 - z0) * bestT;
        float lo = std::max(s.floor, ns.floor), hi = std::min(s.ceil, ns.ceil);
        if (s.sky() && ns.sky()) hi = 1e9f;
        if (z <= lo || z >= hi) return std::max(0.0f, bestT);
        sector = n;
        tEnter = bestT;
    }
    return 0;
}

}  // namespace tg
