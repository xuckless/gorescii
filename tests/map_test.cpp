#include "check.h"
#include "world/map.h"

using namespace tg;

TEST(map_portals_are_linked_both_ways) {
    MapError err;
    auto m = parseMap(
        "sector floor=0 ceil=3 : 0,0 2,0 2,2 0,2\n"
        "sector floor=0 ceil=3 : 2,2 2,0 4,0 4,2\n"  // clockwise on purpose
        "thing player 1,1\n",
        err);
    CHECK(m.has_value());
    if (!m) return;
    int links = 0;
    for (size_t s = 0; s < 2; ++s)
        for (int n : m->sectors[s].neighbor) links += n >= 0;
    CHECK(links == 2);
    CHECK(m->sectorAt({3, 1}) == 1);
    CHECK(m->sectorAt({1, 1}, 1) == 0);
    CHECK(m->sectorAt({9, 9}) == -1);
}

TEST(map_rejects_concave_sector) {
    MapError err;
    auto m = parseMap("sector : 0,0 4,0 4,4 2,1 0,4\nthing player 1,1\n", err);
    CHECK(!m && err.line == 1);
}

TEST(map_rejects_t_junction) {
    MapError err;
    auto m = parseMap(
        "sector : 0,0 4,0 4,4 0,4\n"
        "sector : 4,1 6,1 6,3 4,3\n"
        "thing player 1,1\n",
        err);
    CHECK(!m);
    CHECK(err.message.find("matching corners") != std::string::npos);
}

TEST(outpost_map_loads) {
    MapError err;
    auto m = loadMap(std::string(TEXTGME_ASSET_DIR) + "/maps/outpost.map", err);
    if (!m) std::fprintf(stderr, "  line %d: %s\n", err.line, err.message.c_str());
    CHECK(m.has_value());
    if (!m) return;
    CHECK(m->sectors.size() == 14);
    CHECK(m->playerStart() != nullptr);
    bool hasExit = false;
    for (auto& s : m->sectors) hasExit |= s.exit;
    CHECK(hasExit);
    // Every sector is reachable from the start through portals.
    std::vector<bool> seen(m->sectors.size(), false);
    std::vector<int> stack{m->sectorAt(m->playerStart()->pos)};
    while (!stack.empty()) {
        int s = stack.back();
        stack.pop_back();
        if (s < 0 || seen[static_cast<size_t>(s)]) continue;
        seen[static_cast<size_t>(s)] = true;
        for (int n : m->sectors[static_cast<size_t>(s)].neighbor) stack.push_back(n);
    }
    for (bool b : seen) CHECK(b);
}

#include "world/trace.h"

TEST(trace_through_portals_and_steps) {
    MapError err;
    auto m = parseMap(
        "sector floor=0 ceil=3 : 0,0 4,0 4,4 0,4\n"
        "sector floor=0 ceil=3 : 4,0 8,0 8,4 4,4\n"
        "sector floor=2.5 ceil=3 : 8,0 10,0 10,4 8,4\n"  // tall step
        "thing player 1,1\n",
        err);
    CHECK(m.has_value());
    if (!m) return;
    CHECK(lineOfSight(*m, 0, {1, 2}, 1.6f, {7, 2}, 1.6f));           // through one portal
    CHECK(!lineOfSight(*m, 0, {1, 2}, 1.6f, {12, 2}, 1.6f));         // out through a wall
    CHECK(!lineOfSight(*m, 0, {1, 2}, 1.0f, {9, 2}, 2.7f));          // hits the step face
    CHECK(lineOfSight(*m, 0, {1, 2}, 2.8f, {9, 2}, 2.8f));           // over the step
    float t = traceSegment(*m, 0, {1, 2}, 1.6f, {21, 2}, 1.6f);       // wall at x=8 (step)
    CHECK(t > 0.34f && t < 0.36f);
}
