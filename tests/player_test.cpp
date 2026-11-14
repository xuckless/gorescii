#include "check.h"
#include "game/player.h"

using namespace tg;
using namespace tg::game;

namespace {
Map testMap() {
    MapError err;
    // Room, a step up of 0.5 (ok), and beyond it a ledge 1.5 higher (too tall).
    auto m = parseMap(
        "sector floor=0   ceil=3 : 0,0 4,0 4,4 0,4\n"
        "sector floor=0.5 ceil=5 : 4,0 6,0 6,4 4,4\n"
        "sector floor=2.0 ceil=5 : 6,0 8,0 8,4 6,4\n"
        "thing player 1,2 0\n",
        err);
    return *m;
}
Body spawn(const Map& m) {
    Body b;
    b.pos = {1, 2};
    b.sector = m.sectorAt(b.pos);
    return b;
}
void run(const Map& m, Body& b, Controls c, float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 60) stepPlayer(m, b, c, 1.0f / 60);
}
}  // namespace

TEST(player_walks_up_a_step_and_stops_at_a_ledge) {
    Map m = testMap();
    Body b = spawn(m);
    Controls c;
    c.forward = 1;
    run(m, b, c, 3.0f);
    CHECK(b.sector == 1);
    CHECK(std::abs(b.z - 0.5f) < 1e-3f);
    CHECK(b.pos.x < 6.0f - Body{}.radius + 0.01f);
    CHECK(b.pos.x > 5.5f);
}

TEST(player_stops_at_solid_wall_and_slides) {
    Map m = testMap();
    Body b = spawn(m);
    b.angle = 3.14159265f * 0.75f;  // up-left into the west wall
    Controls c;
    c.forward = 1;
    run(m, b, c, 2.0f);
    CHECK(b.pos.x >= Body{}.radius - 0.01f);
    CHECK(b.pos.y > 3.0f);  // slid north along the wall
    CHECK(b.sector == 0);
}

TEST(player_falls_off_ledge_under_gravity) {
    Map m = testMap();
    Body b;
    b.pos = {7, 2};
    b.sector = 2;
    b.z = 2.0f;
    b.angle = 3.14159265f;  // west, off the ledge
    Controls c;
    c.forward = 1;
    run(m, b, c, 0.4f);
    CHECK(b.sector == 1);
    run(m, b, c, 1.0f);
    CHECK(std::abs(b.z - 0.5f) < 1e-3f || b.sector == 0);
    CHECK(b.onGround);
}

TEST(outpost_start_to_hall_walkthrough) {
    MapError err;
    auto m = loadMap(std::string(GORESCII_ASSET_DIR) + "/maps/outpost.map", err);
    CHECK(m.has_value());
    if (!m) return;
    Body b;
    b.pos = m->playerStart()->pos;
    b.angle = 0;  // east, down the corridor and up the steps
    b.sector = m->sectorAt(b.pos);
    Controls c;
    c.forward = 1;
    c.run = true;
    for (float t = 0; t < 5 && b.pos.x < 18.5f; t += 1.0f / 60) stepPlayer(*m, b, c, 1.0f / 60);
    CHECK(b.pos.x >= 18.5f);
    CHECK(std::abs(b.z - 1.2f) < 1e-3f);
    CHECK(m->sectors[static_cast<size_t>(b.sector)].sky());
}
