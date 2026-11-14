#include "world/map.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace tg {
namespace {

constexpr float kEps = 1e-4f;

constexpr std::array<const char*, static_cast<size_t>(Tex::Count)> kTexNames = {
    "brick", "metal", "stone", "tech", "wood", "tile", "panel", "dirt", "slime", "sky"};

bool near(Vec2 a, Vec2 b) { return std::abs(a.x - b.x) < kEps && std::abs(a.y - b.y) < kEps; }

float signedArea(const std::vector<Vec2>& v) {
    float a = 0;
    for (size_t i = 0; i < v.size(); ++i) a += cross(v[i], v[(i + 1) % v.size()]);
    return a * 0.5f;
}

// CCW polygon is convex when no corner turns right (collinear points allowed).
bool convex(const std::vector<Vec2>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        Vec2 a = v[i], b = v[(i + 1) % v.size()], c = v[(i + 2) % v.size()];
        if (side(a, b, c) < -kEps) return false;
    }
    return true;
}

bool parseVec(const std::string& tok, Vec2& out) {
    auto comma = tok.find(',');
    if (comma == std::string::npos) return false;
    try {
        size_t used = 0;
        out.x = std::stof(tok.substr(0, comma), &used);
        if (used != comma) return false;
        std::string ys = tok.substr(comma + 1);
        out.y = std::stof(ys, &used);
        return used == ys.size();
    } catch (...) {
        return false;
    }
}

bool parseFloat(const std::string& s, float& out) {
    try {
        size_t used = 0;
        out = std::stof(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

std::optional<ThingType> thingFromName(const std::string& n) {
    if (n == "player") return ThingType::Player;
    if (n == "grunt") return ThingType::Grunt;
    if (n == "brute") return ThingType::Brute;
    if (n == "medkit") return ThingType::Medkit;
    if (n == "ammo") return ThingType::Ammo;
    if (n == "armor") return ThingType::Armor;
    return std::nullopt;
}

}  // namespace

std::optional<Tex> texFromName(const std::string& name) {
    for (size_t i = 0; i < kTexNames.size(); ++i)
        if (name == kTexNames[i]) return static_cast<Tex>(i);
    return std::nullopt;
}

bool Sector::contains(Vec2 p) const {
    for (size_t i = 0; i < verts.size(); ++i)
        if (side(a(i), b(i), p) < -kEps) return false;
    return true;
}

int Map::sectorAt(Vec2 p, int hint) const {
    if (hint >= 0 && hint < static_cast<int>(sectors.size())) {
        const Sector& h = sectors[static_cast<size_t>(hint)];
        if (h.contains(p)) return hint;
        for (int n : h.neighbor)
            if (n >= 0 && sectors[static_cast<size_t>(n)].contains(p)) return n;
    }
    for (size_t i = 0; i < sectors.size(); ++i)
        if (sectors[i].contains(p)) return static_cast<int>(i);
    return -1;
}

const Thing* Map::playerStart() const {
    for (const Thing& t : things)
        if (t.type == ThingType::Player) return &t;
    return nullptr;
}

std::optional<Map> parseMap(const std::string& text, MapError& err) {
    Map map;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    std::vector<int> sectorLines;
    auto fail = [&](int ln, std::string msg) {
        err = {ln, std::move(msg)};
        return std::nullopt;
    };

    while (std::getline(in, line)) {
        ++lineNo;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        std::istringstream ls(line);
        std::string kind;
        if (!(ls >> kind)) continue;

        if (kind == "name") {
            std::getline(ls >> std::ws, map.name);
        } else if (kind == "sector") {
            Sector s;
            std::string tok;
            bool corners = false;
            while (ls >> tok) {
                if (tok == ":") {
                    corners = true;
                    continue;
                }
                if (corners) {
                    Vec2 v;
                    if (!parseVec(tok, v)) return fail(lineNo, "bad corner '" + tok + "' (want x,y)");
                    s.verts.push_back(v);
                    continue;
                }
                if (tok == "exit") {
                    s.exit = true;
                    continue;
                }
                auto eq = tok.find('=');
                if (eq == std::string::npos) return fail(lineNo, "expected key=value, got '" + tok + "'");
                std::string key = tok.substr(0, eq), val = tok.substr(eq + 1);
                float f = 0;
                if (key == "floor" || key == "ceil" || key == "light" || key == "damage") {
                    if (!parseFloat(val, f)) return fail(lineNo, "bad number for " + key);
                    (key == "floor" ? s.floor : key == "ceil" ? s.ceil : key == "light" ? s.light : s.damage) = f;
                } else if (key == "wall" || key == "floortex" || key == "ceiltex") {
                    auto t = texFromName(val);
                    if (!t) return fail(lineNo, "unknown texture '" + val + "'");
                    (key == "wall" ? s.wallTex : key == "floortex" ? s.floorTex : s.ceilTex) = *t;
                } else {
                    return fail(lineNo, "unknown sector key '" + key + "'");
                }
            }
            if (s.verts.size() < 3) return fail(lineNo, "sector needs at least 3 corners after ':'");
            if (s.ceil <= s.floor) return fail(lineNo, "ceil must be above floor");
            float area = signedArea(s.verts);
            if (std::abs(area) < kEps) return fail(lineNo, "sector has no area");
            if (area < 0) std::reverse(s.verts.begin(), s.verts.end());
            if (!convex(s.verts)) return fail(lineNo, "sector is not convex; split it into convex pieces");
            map.sectors.push_back(std::move(s));
            sectorLines.push_back(lineNo);
        } else if (kind == "thing") {
            std::string type, pos;
            float angleDeg = 0;
            if (!(ls >> type >> pos)) return fail(lineNo, "thing needs: thing <type> x,y [angle]");
            auto t = thingFromName(type);
            if (!t) return fail(lineNo, "unknown thing '" + type + "'");
            Thing th{*t, {}, 0};
            if (!parseVec(pos, th.pos)) return fail(lineNo, "bad position '" + pos + "'");
            std::string ang;
            if (ls >> ang && !parseFloat(ang, angleDeg)) return fail(lineNo, "bad angle '" + ang + "'");
            th.angle = angleDeg * 3.14159265f / 180.0f;
            map.things.push_back(th);
        } else {
            return fail(lineNo, "unknown statement '" + kind + "'");
        }
    }

    if (map.sectors.empty()) return fail(0, "map has no sectors");

    // Portals: an edge a->b in one sector matches b->a in another.
    for (size_t si = 0; si < map.sectors.size(); ++si) {
        Sector& s = map.sectors[si];
        s.neighbor.assign(s.verts.size(), -1);
        for (size_t e = 0; e < s.verts.size(); ++e) {
            for (size_t sj = 0; sj < map.sectors.size() && s.neighbor[e] < 0; ++sj) {
                if (sj == si) continue;
                const Sector& o = map.sectors[sj];
                for (size_t f = 0; f < o.verts.size(); ++f) {
                    if (near(s.a(e), o.b(f)) && near(s.b(e), o.a(f))) {
                        s.neighbor[e] = static_cast<int>(sj);
                        break;
                    }
                }
            }
        }
    }

    // Catch T-junctions: a solid edge lying along another sector's edge means
    // the author forgot matching corners, which would leave an invisible wall.
    for (size_t si = 0; si < map.sectors.size(); ++si) {
        const Sector& s = map.sectors[si];
        for (size_t e = 0; e < s.verts.size(); ++e) {
            if (s.neighbor[e] >= 0) continue;
            Vec2 a = s.a(e), b = s.b(e), d = b - a;
            float len2 = dot(d, d);
            for (size_t sj = 0; sj < map.sectors.size(); ++sj) {
                if (sj == si) continue;
                const Sector& o = map.sectors[sj];
                for (size_t f = 0; f < o.verts.size(); ++f) {
                    Vec2 c = o.a(f), g = o.b(f);
                    if (std::abs(side(a, b, c)) > kEps * len2 || std::abs(side(a, b, g)) > kEps * len2) continue;
                    float t0 = dot(c - a, d) / len2, t1 = dot(g - a, d) / len2;
                    float lo = std::max(0.0f, std::min(t0, t1)), hi = std::min(1.0f, std::max(t0, t1));
                    if (hi - lo > 1e-3f)
                        return fail(sectorLines[si], "edge " + std::to_string(a.x) + "," + std::to_string(a.y) + " -> " +
                                                         std::to_string(b.x) + "," + std::to_string(b.y) +
                                                         " overlaps sector on line " + std::to_string(sectorLines[sj]) +
                                                         " without matching corners");
                }
            }
        }
    }

    const Thing* start = map.playerStart();
    if (!start) return fail(0, "map has no 'thing player'");
    for (const Thing& t : map.things)
        if (map.sectorAt(t.pos) < 0)
            return fail(0, "a thing at " + std::to_string(t.pos.x) + "," + std::to_string(t.pos.y) +
                               " is outside every sector");
    return map;
}

std::optional<Map> loadMap(const std::string& path, MapError& err) {
    std::ifstream f(path);
    if (!f) {
        err = {0, "cannot open " + path};
        return std::nullopt;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parseMap(ss.str(), err);
}

}  // namespace tg
