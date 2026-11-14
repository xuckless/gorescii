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

namespace {
// Room A | door | room B | lift, with an optional key on the door.
Map doorMap(const char* doorExtra, const char* things) {
    MapError err;
    std::string text = std::string(
        "sector floor=0 ceil=3 : 0,0 4,0 4,1.5 4,2.5 4,4 0,4\n"
        "sector floor=0 ceil=2.6 wall=metal door ") + doorExtra + " : 4,1.5 4.4,1.5 4.4,2.5 4,2.5\n"
        "sector floor=0 ceil=3 : 4.4,0 8,0 8,1 8,3 8,4 4.4,4 4.4,2.5 4.4,1.5\n"
        "sector floor=2 ceil=5 lift low=0 : 8,1 10,1 10,3 8,3\n"
        "thing player 3,2 0\n" + things;
    auto m = parseMap(text, err);
    if (!m) std::fprintf(stderr, "  door map: line %d %s\n", err.line, err.message.c_str());
    return *m;
}
}  // namespace

TEST(door_blocks_until_used_then_opens_and_closes) {
    Game g(doorMap("", ""));
    Controls walk;
    walk.forward = 1;
    tick(g, walk, 1.5f);
    CHECK(g.player().pos.x < 4.0f);  // shut door stopped us
    Controls use;
    use.use = true;
    g.update(use, 1.0f / 60);
    tick(g, Controls{}, 1.5f);
    tick(g, walk, 1.5f);
    CHECK(g.player().pos.x > 4.5f);  // through the open door
    // Walk on, then the door closes behind us after its wait.
    tick(g, Controls{}, 7.0f);
    CHECK(g.map().sectors[1].ceil <= g.map().sectors[1].floor + 1e-3f);
}

TEST(locked_door_needs_key) {
    Game g(doorMap("key=red", ""));
    Controls use;
    use.use = true;
    g.update(use, 1.0f / 60);
    tick(g, Controls{}, 1.0f);
    CHECK(g.map().sectors[1].ceil <= 1e-3f);
    bool told = false;
    for (auto& m : g.messages()) told |= m.text.find("red key") != std::string::npos;
    CHECK(told);

    Game k(doorMap("key=red", "thing redkey 3.3,2\n"));
    tick(k, Controls{}, 0.1f);
    CHECK(k.hasKey(KeyColor::Red));
    k.update(use, 1.0f / 60);
    tick(k, Controls{}, 1.0f);
    CHECK(k.map().sectors[1].ceil > 1.5f);
}

TEST(door_does_not_close_on_player) {
    Game g(doorMap("", ""));
    Controls use;
    use.use = true;
    g.update(use, 1.0f / 60);
    tick(g, Controls{}, 1.2f);
    Controls walk;
    walk.forward = 1;
    // Step into the doorway and stand there past the wait time.
    for (float t = 0; t < 3 && g.player().pos.x < 4.2f; t += 1.0f / 60) g.update(walk, 1.0f / 60);
    tick(g, Controls{}, 6.0f);
    CHECK(g.map().sectors[1].ceil > 1.5f);
}

TEST(lift_lowers_waits_and_rises) {
    Map lm = doorMap("", "");
    for (Thing& t : lm.things)
        if (t.type == ThingType::Player) t.pos = {7.2f, 2};
    Game l(lm);
    Controls use;
    use.use = true;
    l.update(use, 1.0f / 60);
    tick(l, Controls{}, 1.5f);
    CHECK(l.map().sectors[3].floor <= 0.01f);
    tick(l, Controls{}, 4.5f);
    CHECK(l.map().sectors[3].floor >= 1.99f);
}

TEST(loadout_carries_into_the_next_level) {
    Loadout carry{60, 25, 90, Weapon::Scattergun};
    Game g(arena(""), 1, carry);
    CHECK(g.health() == 60 && g.armor() == 25 && g.ammo() == 90 && g.weapon() == Weapon::Scattergun);
    Loadout out = g.loadout();
    CHECK(out.health == 60 && out.ammo == 90);
}

namespace {
struct Step {
    float x, y;
    bool use = false;     // on arrival, face `face` and press use
    float face = 0;       // degrees
    float wait = 0;       // seconds to stand still afterwards
};

// Drives the player through waypoints like a very patient speedrunner.
bool follow(Game& g, const std::vector<Step>& route, std::string& where) {
    const float dt = 1.0f / 60;
    for (size_t i = 0; i < route.size(); ++i) {
        const Step& s = route[i];
        float t = 0;
        for (;; t += dt) {
            if (t > 20) {
                char buf[128];
                std::snprintf(buf, sizeof buf, "stuck before step %zu at %.2f,%.2f z=%.2f", i, g.player().pos.x,
                              g.player().pos.y, g.player().z);
                where = buf;
                return false;
            }
            Vec2 to = Vec2{s.x, s.y} - g.player().pos;
            if (length(to) < 0.25f) break;
            float want = std::atan2(to.y, to.x);
            float diff = std::remainder(want - g.player().angle, 2 * 3.14159265f);
            Controls c;
            c.look = std::clamp(diff, -0.1f, 0.1f);
            c.forward = std::abs(diff) < 0.3f ? std::min(1.0f, length(to)) : 0;
            g.update(c, dt);
            if (g.state() != State::Playing) return g.state() == State::Won;
        }
        if (s.use) {
            float want = s.face * 3.14159265f / 180;
            for (int k = 0; k < 120; ++k) {
                Controls c;
                c.look = std::clamp(std::remainder(want - g.player().angle, 2 * 3.14159265f), -0.1f, 0.1f);
                g.update(c, dt);
            }
            Controls use;
            use.use = true;
            g.update(use, dt);
        }
        for (float w = 0; w < s.wait; w += dt) g.update(Controls{}, dt);
        if (g.state() == State::Won) return true;
    }
    where = "route ended without reaching the exit";
    return g.state() == State::Won;
}
}  // namespace

TEST(pumping_station_is_beatable) {
    MapError err;
    auto m = loadMap(std::string(TEXTGME_ASSET_DIR) + "/maps/pumping-station.map", err);
    CHECK(m.has_value());
    if (!m) return;
    std::erase_if(m->things, [](const Thing& t) { return t.type == ThingType::Grunt || t.type == ThingType::Brute; });
    Game g(*m);
    std::vector<Step> route = {
        {9.2f, 4, true, 0, 1.5f},      // open the hall door
        {12, 4},
        {18.5f, 9.2f, true, 90, 1.6f}, // call the lift down
        {18.5f, 11},                   // step on, wait for it to rise
        {18.5f, 11, false, 0, 5.0f},
        {18.5f, 13},
        {15, 16},                      // red key
        {11.2f, 15, true, 180, 1.5f},  // red door
        {8, 15},
        {3, 19},                       // blue key
        {9.2f, 15, true, 0, 1.5f},     // back out through the red door
        {12, 15},
        {13, 11},                      // drop off the platform
        {15, -3.2f, true, 270, 1.5f},  // blue door
        {15, -7},
        {15, -11},                     // exit pad
    };
    std::string where;
    bool won = follow(g, route, where);
    if (!won) std::fprintf(stderr, "  %s\n", where.c_str());
    CHECK(won);
    CHECK(g.hasKey(KeyColor::Red) && g.hasKey(KeyColor::Blue));
}
