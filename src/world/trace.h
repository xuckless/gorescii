#pragma once
#include "world/map.h"

namespace tg {

// Follows the segment (from, z0) -> (to, z1) through portals starting in
// `sector`. Returns the fraction of the way it gets before something solid
// (a wall, or a floor/ceiling step it passes outside of); 1 means clear.
float traceSegment(const Map& map, int sector, Vec2 from, float z0, Vec2 to, float z1);

inline bool lineOfSight(const Map& map, int sector, Vec2 from, float z0, Vec2 to, float z1) {
    return traceSegment(map, sector, from, z0, to, z1) >= 1.0f;
}

}  // namespace tg
