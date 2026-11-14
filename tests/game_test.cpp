#include "check.h"
#include "game/game.h"

using namespace tg;
using namespace tg::game;

namespace {
Map arena(const char* extra) {
    MapError err;
    std::string text =
        "sector floor=0 ceil=4 : 0,0 20,0 20,6 0,6\n"
        "sector floor=0 ceil=4 damage=20 : 20,0 24,0 24,6 20,6\n"
        "sector floor=0 ceil=4 exit : 24,0 26,0 26,6 24,6\n"
        "thing player 2,3 0\n";
    text += extra;
    auto m = parseMap(text, err);
    return *m;
}
void tick(Game& g, Controls c, float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 60) g.update(c, 1.0f / 60);
}
}  // namespace

TEST(shooting_kills_a_grunt_in_front) {
    Game g(arena("thing grunt 8,3 180\n"));
    Controls c;
    c.fire = true;
    tick(g, c, 3.0f);
    CHECK(g.kills() == 1);
    CHECK(g.ammo() < 50);
}

TEST(walls_stop_shots) {
    MapError err;
    auto m = parseMap(
        "sector : 0,0 4,0 4,4 0,4\n"
        "sector : 6,0 10,0 10,4 6,4\n"  // separate room, no portal
        "thing player 2,2 0\nthing grunt 8,2 180\n",
        err);
    CHECK(m.has_value());
    Game g(*m);
    Controls c;
    c.fire = true;
    tick(g, c, 2.0f);
    CHECK(g.kills() == 0);
}

TEST(grunt_spots_and_shoots_player) {
    Game g(arena("thing grunt 10,3 180\n"), 7);
    tick(g, Controls{}, 8.0f);
    CHECK(g.enemies()[0].ai != Enemy::AI::Idle);
    CHECK(g.health() < 100);
}

TEST(armor_absorbs_a_third) {
    Game g(arena("thing armor 2.2,3\n"));
    tick(g, Controls{}, 0.1f);
    CHECK(g.armor() == 50);
    g.damagePlayer(30);
    CHECK(g.health() == 80);
    CHECK(g.armor() == 40);
}

TEST(medkit_only_taken_when_hurt) {
    Game g(arena("thing medkit 2.2,3\n"));
    tick(g, Controls{}, 0.1f);
    std::vector<render::SpriteInstance> sprites;
    g.collectSprites(sprites);
    CHECK(sprites.size() == 1);  // still there at full health
    g.damagePlayer(40);
    tick(g, Controls{}, 0.1f);
    CHECK(g.health() == 85);
}

TEST(slime_hurts_and_exit_wins) {
    Game g(arena(""));
    Controls c;
    c.forward = 1;
    for (float t = 0; t < 6 && g.state() == State::Playing; t += 1.0f / 60) g.update(c, 1.0f / 60);
    CHECK(g.state() == State::Won);
    CHECK(g.health() < 100);
}
