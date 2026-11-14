#pragma once
// Sector-based level: convex polygons with floor/ceiling heights, joined by
// portals where two sectors share an edge. Loaded from a small text format
// (see assets/maps/README.md).
#include <optional>
#include <string>
#include <vector>

#include "world/geom.h"

namespace tg {

enum class Tex : unsigned char { Brick, Metal, Stone, Tech, Wood, Tile, Panel, Dirt, Slime, Sky, Count };
std::optional<Tex> texFromName(const std::string& name);

enum class Mover : unsigned char { None, Door, Lift };
enum class KeyColor : unsigned char { None, Red, Blue };

struct Sector {
    float floor = 0, ceil = 3;
    float light = 0.8f;  // 0..1
    Tex floorTex = Tex::Tile, ceilTex = Tex::Panel, wallTex = Tex::Brick;
    bool exit = false;     // standing here ends the level
    float damage = 0;      // health per second while on the floor
    // Moving sectors. A door's ceiling starts at its floor and opens to
    // openCeil; a lift's floor lowers from topFloor to lowFloor and back.
    Mover mover = Mover::None;
    KeyColor key = KeyColor::None;  // doors: key needed to open
    float openCeil = 0;
    float topFloor = 0, lowFloor = 0;
    // Corners, counter-clockwise (interior on the left of each edge).
    std::vector<Vec2> verts;
    // neighbor[i] is the sector across edge verts[i] -> verts[i+1], or -1.
    std::vector<int> neighbor;

    size_t edgeCount() const { return verts.size(); }
    Vec2 a(size_t i) const { return verts[i]; }
    Vec2 b(size_t i) const { return verts[(i + 1) % verts.size()]; }
    bool sky() const { return ceilTex == Tex::Sky; }
    bool contains(Vec2 p) const;
};

enum class ThingType : unsigned char { Player, Grunt, Brute, Medkit, Ammo, Armor, RedKey, BlueKey };

struct Thing {
    ThingType type;
    Vec2 pos;
    float angle = 0;  // radians, counter-clockwise from +x
};

struct Map {
    std::string name;
    std::vector<Sector> sectors;
    std::vector<Thing> things;

    // Sector containing p; `hint` is checked first (and its neighbours).
    int sectorAt(Vec2 p, int hint = -1) const;
    const Thing* playerStart() const;
};

struct MapError {
    int line = 0;
    std::string message;
};

// Parses map text. On failure returns nullopt and fills `err`.
std::optional<Map> parseMap(const std::string& text, MapError& err);
std::optional<Map> loadMap(const std::string& path, MapError& err);

}  // namespace tg
