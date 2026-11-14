#pragma once
// Game state and simulation, independent of the terminal.
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "game/player.h"
#include "game/sound.h"
#include "render/renderer.h"
#include "world/map.h"

namespace tg::game {

enum class Weapon : uint8_t { Pistol, Scattergun };
enum class State : uint8_t { Playing, Dead, Won };

struct Enemy {
    enum class AI : uint8_t { Idle, Chase, Attack, Pain, Dead };
    ThingType type = ThingType::Grunt;
    Body body;
    int health = 0;
    AI ai = AI::Idle;
    float timer = 0;     // time left in Attack / Pain, or until next sight check
    float cooldown = 0;  // until the next attack may start
    bool fired = false;  // this attack's shot has gone off
    float stuck = 0;
    float wander = 0;
    Vec2 wanderDir;
    float flash = 0;     // hit flash
    uint32_t id = 0;
    bool alive() const { return ai != AI::Dead; }
};

struct Pickup {
    ThingType type;
    Vec2 pos;
    int sector = -1;
    bool taken = false;
    uint32_t id = 0;
};

struct Projectile {
    Vec2 pos;
    float z = 0;
    Vec2 vel;
    int sector = -1;
    int damage = 0;
    uint32_t id = 0;
};

// A door or lift that is currently moving or waiting.
struct ActiveMover {
    int sector = -1;
    int dir = 0;      // +1 up (door opening / lift rising), -1 down
    float wait = 0;   // > 0 while parked at the far end
};

// What the player carries from one level to the next.
struct Loadout {
    int health = 100, armor = 0, ammo = 50;
    Weapon weapon = Weapon::Pistol;
};

struct Message {
    std::string text;
    float time = 0;
};

struct WeaponView {
    render::SpriteId sprite;
    float bobX = 0, bobY = 0;  // fractions of screen height
    bool flash = false;
};

class Game {
public:
    explicit Game(Map map, uint32_t seed = 1, Loadout carry = {});
    Loadout loadout() const { return {health_, armor_, ammo_, weapon_}; }

    void update(const Controls& c, float dt);
    render::Camera camera() const;
    void collectSprites(std::vector<render::SpriteInstance>& out) const;
    WeaponView weaponView() const;

    const Map& map() const { return map_; }
    const Body& player() const { return player_; }
    State state() const { return state_; }
    int health() const { return health_; }
    int armor() const { return armor_; }
    int ammo() const { return ammo_; }
    Weapon weapon() const { return weapon_; }
    int kills() const { return kills_; }
    int totalEnemies() const { return static_cast<int>(enemies_.size()); }
    float elapsed() const { return elapsed_; }
    float damageFlash() const { return damageFlash_; }
    float pickupFlash() const { return pickupFlash_; }
    float muzzleFlash() const { return fireAnim_ > 0 ? 1.0f : 0.0f; }
    const std::deque<Message>& messages() const { return messages_; }
    // Sounds triggered since the last call.
    std::vector<SoundEvent> takeSounds() { return std::exchange(sounds_, {}); }
    bool hasKey(KeyColor k) const { return k == KeyColor::Red ? redKey_ : k == KeyColor::Blue ? blueKey_ : true; }

    // Exposed for tests.
    std::vector<Enemy>& enemies() { return enemies_; }
    void damagePlayer(int amount);

private:
    float rand01();
    int randRange(int lo, int hi);
    void say(std::string text);
    void sound(Sfx id) { sounds_.push_back({id, player_.pos, false}); }
    void sound(Sfx id, Vec2 at) { sounds_.push_back({id, at, true}); }

    void updateWeapon(const Controls& c, float dt);
    void fireHitscan(float spreadRad, int pellets, int dmgLo, int dmgHi);
    void damageEnemy(Enemy& e, int amount);
    void alert(Vec2 pos, float radius);
    void updateEnemy(Enemy& e, float dt);
    void enemyAttack(Enemy& e);
    void updateProjectiles(float dt);
    void updatePickups();
    void separateBodies();
    // Starts a door or lift. `byPlayer` controls key checks and messages.
    bool activate(int sector, bool byPlayer);
    int moverAhead(Vec2 pos, float angle, int sector, float reach) const;
    bool occupied(int sector) const;
    void updateMovers(float dt);
    float eyeZ() const { return player_.z + Body::kEye; }

    Map map_;
    Body player_;
    std::vector<Enemy> enemies_;
    std::vector<Pickup> pickups_;
    std::vector<Projectile> projectiles_;
    std::vector<ActiveMover> movers_;
    bool redKey_ = false, blueKey_ = false;
    std::deque<Message> messages_;
    std::vector<SoundEvent> sounds_;
    std::mt19937 rng_;
    uint32_t nextId_ = 1;

    State state_ = State::Playing;
    int health_, armor_, ammo_;
    Weapon weapon_;
    float fireCooldown_ = 0, fireAnim_ = 0, noAmmoMsg_ = 0;
    float damageFlash_ = 0, pickupFlash_ = 0, slimeAcc_ = 0;
    int kills_ = 0;
    float elapsed_ = 0;

    float viewZ_ = 0;  // smoothed eye height (stairs, landings)
    float bobPhase_ = 0;
    float bob_ = 0;
};

}  // namespace tg::game
