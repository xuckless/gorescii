#include "game/game.h"

#include <algorithm>
#include <cmath>

#include "world/trace.h"

namespace tg::game {
namespace {

constexpr float kPi = 3.14159265f;

struct EnemyDef {
    int health;
    float speed, radius, height;
    float keepAway;          // stops approaching inside this distance
    float range;             // max attack distance
    float cooldownLo, cooldownHi;
};

const EnemyDef& def(ThingType t) {
    static const EnemyDef grunt{30, 2.6f, 0.35f, 1.7f, 3.0f, 22.0f, 1.2f, 2.2f};
    static const EnemyDef brute{140, 2.0f, 0.5f, 2.2f, 1.2f, 18.0f, 1.8f, 2.8f};
    return t == ThingType::Brute ? brute : grunt;
}

bool isEnemy(ThingType t) { return t == ThingType::Grunt || t == ThingType::Brute; }
bool isPickup(ThingType t) {
    return t == ThingType::Medkit || t == ThingType::Ammo || t == ThingType::Armor || t == ThingType::RedKey ||
           t == ThingType::BlueKey;
}

constexpr float kDoorSpeed = 2.5f, kLiftSpeed = 2.0f;
constexpr float kDoorWait = 4.0f, kLiftWait = 3.0f;
constexpr float kUseReach = 1.6f;

// Distance along a ray (origin o, unit dir d) to a circle, or -1.
float rayCircle(Vec2 o, Vec2 d, Vec2 c, float r) {
    Vec2 oc = c - o;
    float t = dot(oc, d);
    float perp2 = dot(oc, oc) - t * t;
    if (perp2 > r * r) return -1;
    float th = std::sqrt(r * r - perp2);
    float hit = t - th;
    if (hit < 0) hit = t + th;
    return hit > 0 ? hit : -1;
}

}  // namespace

Game::Game(Map map, uint32_t seed, Loadout carry)
    : map_(std::move(map)), rng_(seed), health_(carry.health), armor_(carry.armor), ammo_(carry.ammo),
      weapon_(carry.weapon) {
    const Thing* start = map_.playerStart();
    player_.pos = start->pos;
    player_.angle = start->angle;
    player_.sector = map_.sectorAt(player_.pos);
    player_.z = map_.sectors[static_cast<size_t>(player_.sector)].floor;
    viewZ_ = player_.z + Body::kEye;

    for (const Thing& t : map_.things) {
        int sector = map_.sectorAt(t.pos);
        if (isEnemy(t.type)) {
            const EnemyDef& d = def(t.type);
            Enemy e;
            e.type = t.type;
            e.health = d.health;
            e.body.pos = t.pos;
            e.body.angle = t.angle;
            e.body.sector = sector;
            e.body.radius = d.radius;
            e.body.height = d.height;
            e.body.z = map_.sectors[static_cast<size_t>(sector)].floor;
            e.timer = rand01() * 0.3f;
            e.id = nextId_++;
            enemies_.push_back(e);
        } else if (isPickup(t.type)) {
            pickups_.push_back({t.type, t.pos, sector, false, nextId_++});
        }
    }
}

float Game::rand01() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }
int Game::randRange(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng_); }

void Game::say(std::string text) {
    messages_.push_back({std::move(text), 2.5f});
    while (messages_.size() > 4) messages_.pop_front();
}

void Game::update(const Controls& c, float dt) {
    for (auto& m : messages_) m.time -= dt;
    while (!messages_.empty() && messages_.front().time <= 0) messages_.pop_front();
    damageFlash_ = std::max(0.0f, damageFlash_ - dt * 1.5f);
    pickupFlash_ = std::max(0.0f, pickupFlash_ - dt * 2.0f);
    fireAnim_ = std::max(0.0f, fireAnim_ - dt);
    if (state_ != State::Playing) {
        // Keep the world alive behind the death / win screen, but the player is out.
        for (Enemy& e : enemies_) updateEnemy(e, dt);
        updateMovers(dt);
        updateProjectiles(dt);
        return;
    }
    elapsed_ += dt;

    if (c.use) {
        int target = moverAhead(player_.pos, player_.angle, player_.sector, kUseReach);
        const Sector& here = map_.sectors[static_cast<size_t>(player_.sector)];
        if (target < 0 && here.mover == Mover::Lift) target = player_.sector;
        if (target >= 0) activate(target, true);
    }
    updateMovers(dt);
    stepPlayer(map_, player_, c, dt);
    updateWeapon(c, dt);
    for (Enemy& e : enemies_) updateEnemy(e, dt);
    separateBodies();
    updateProjectiles(dt);
    updatePickups();

    const Sector& s = map_.sectors[static_cast<size_t>(player_.sector)];
    if (s.damage > 0 && player_.onGround) {
        slimeAcc_ += s.damage * dt;
        if (slimeAcc_ >= 1) {
            int n = static_cast<int>(slimeAcc_);
            slimeAcc_ -= n;
            damagePlayer(n);
        }
    }
    if (s.exit && state_ == State::Playing) {
        state_ = State::Won;
        say(map_.name.empty() ? std::string("Level cleared.") : map_.name + " cleared.");
        sound(Sfx::Win);
    }

    float target = player_.z + Body::kEye;
    // Rise smoothly up steps; drop immediately when falling.
    viewZ_ = target > viewZ_ ? viewZ_ + (target - viewZ_) * std::min(1.0f, dt * 14.0f) : target;
    float speed = length(player_.vel);
    bobPhase_ += speed * dt * 1.6f;
    float amp = player_.onGround ? std::min(speed / 8.0f, 1.0f) * 0.06f : 0.0f;
    bob_ = std::sin(bobPhase_ * kPi) * amp;
}

void Game::damagePlayer(int amount) {
    if (state_ != State::Playing || amount <= 0) return;
    int absorbed = std::min(armor_, amount / 3);
    armor_ -= absorbed;
    health_ -= amount - absorbed;
    damageFlash_ = std::min(1.0f, damageFlash_ + amount / 20.0f);
    if (health_ <= 0) {
        health_ = 0;
        state_ = State::Dead;
        say("You died.");
        sound(Sfx::PlayerDeath);
    } else {
        sound(Sfx::PlayerHurt);
    }
}

void Game::updateWeapon(const Controls& c, float dt) {
    if (c.weaponSlot == 1) weapon_ = Weapon::Pistol;
    if (c.weaponSlot == 2) weapon_ = Weapon::Scattergun;
    if (c.nextWeapon) weapon_ = weapon_ == Weapon::Pistol ? Weapon::Scattergun : Weapon::Pistol;

    fireCooldown_ = std::max(0.0f, fireCooldown_ - dt);
    noAmmoMsg_ = std::max(0.0f, noAmmoMsg_ - dt);
    if (!c.fire || fireCooldown_ > 0) return;
    int cost = weapon_ == Weapon::Pistol ? 1 : 2;
    if (ammo_ < cost) {
        if (noAmmoMsg_ <= 0) {
            say("Out of ammo.");
            sound(Sfx::Click);
        }
        noAmmoMsg_ = 1.5f;
        return;
    }
    ammo_ -= cost;
    fireAnim_ = 0.12f;
    sound(weapon_ == Weapon::Pistol ? Sfx::Pistol : Sfx::Scattergun);
    if (weapon_ == Weapon::Pistol) {
        fireCooldown_ = 0.38f;
        fireHitscan(0.02f, 1, 10, 18);
    } else {
        fireCooldown_ = 0.85f;
        fireHitscan(0.10f, 7, 6, 10);
    }
    alert(player_.pos, 20.0f);
}

void Game::fireHitscan(float spread, int pellets, int dmgLo, int dmgHi) {
    for (int p = 0; p < pellets; ++p) {
        float a = player_.angle + (rand01() * 2 - 1) * spread;
        Vec2 dir{std::cos(a), std::sin(a)};
        Enemy* best = nullptr;
        float bestT = 1e9f;
        for (Enemy& e : enemies_) {
            if (!e.alive()) continue;
            float t = rayCircle(player_.pos, dir, e.body.pos, e.body.radius + 0.1f);
            if (t > 0 && t < bestT) {
                bestT = t;
                best = &e;
            }
        }
        if (!best) continue;
        // Auto-aim vertically (no pitch) but walls and steps still block.
        Vec2 hit = player_.pos + dir * bestT;
        float targetZ = best->body.z + best->body.height * 0.6f;
        if (lineOfSight(map_, player_.sector, player_.pos, eyeZ(), hit, targetZ)) damageEnemy(*best, randRange(dmgLo, dmgHi));
    }
}

void Game::damageEnemy(Enemy& e, int amount) {
    e.health -= amount;
    e.flash = 0.12f;
    if (e.health <= 0) {
        if (e.ai != Enemy::AI::Dead) sound(Sfx::EnemyDeath, e.body.pos);
        e.ai = Enemy::AI::Dead;
        ++kills_;
        return;
    }
    if (e.flash <= 0.0f || e.ai != Enemy::AI::Pain) sound(Sfx::Pain, e.body.pos);
    if (rand01() < (e.type == ThingType::Brute ? 0.25f : 0.6f)) {
        e.ai = Enemy::AI::Pain;
        e.timer = 0.25f;
    } else if (e.ai == Enemy::AI::Idle) {
        e.ai = Enemy::AI::Chase;
    }
}

void Game::alert(Vec2 pos, float radius) {
    for (Enemy& e : enemies_) {
        if (e.ai != Enemy::AI::Idle) continue;
        float d = length(e.body.pos - pos);
        if (d > radius) continue;
        // Close enough to hear through walls, or in sight.
        if (d < radius * 0.5f ||
            lineOfSight(map_, e.body.sector, e.body.pos, e.body.z + e.body.height * 0.85f, pos, eyeZ())) {
            e.ai = Enemy::AI::Chase;
            e.cooldown = 0.5f + rand01();
        }
    }
}

void Game::updateEnemy(Enemy& e, float dt) {
    e.flash = std::max(0.0f, e.flash - dt);
    if (!e.alive()) {
        applyGravity(map_, e.body, dt);
        return;
    }
    const EnemyDef& d = def(e.type);
    Vec2 toPlayer = player_.pos - e.body.pos;
    float dist = length(toPlayer);
    float eye = e.body.z + e.body.height * 0.85f;
    bool playerAlive = state_ == State::Playing;
    e.cooldown = std::max(0.0f, e.cooldown - dt);
    e.timer -= dt;

    switch (e.ai) {
        case Enemy::AI::Idle:
            if (e.timer <= 0) {
                e.timer = 0.3f;
                if (playerAlive && dist < 28 && lineOfSight(map_, e.body.sector, e.body.pos, eye, player_.pos, eyeZ())) {
                    e.ai = Enemy::AI::Chase;
                    e.cooldown = 0.6f + rand01();
                }
            }
            break;
        case Enemy::AI::Pain:
            if (e.timer <= 0) e.ai = Enemy::AI::Chase;
            break;
        case Enemy::AI::Attack:
            e.body.angle = std::atan2(toPlayer.y, toPlayer.x);
            if (!e.fired && e.timer <= 0.2f) {
                e.fired = true;
                if (playerAlive) enemyAttack(e);
            }
            if (e.timer <= 0) {
                e.ai = Enemy::AI::Chase;
                e.cooldown = d.cooldownLo + rand01() * (d.cooldownHi - d.cooldownLo);
            }
            break;
        case Enemy::AI::Chase: {
            if (!playerAlive) break;
            if (e.cooldown <= 0 && dist < d.range && e.timer <= 0) {
                e.timer = 0.15f;  // re-check sight at most this often
                if (lineOfSight(map_, e.body.sector, e.body.pos, eye, player_.pos, eyeZ())) {
                    e.ai = Enemy::AI::Attack;
                    e.timer = 0.45f;
                    e.fired = false;
                    break;
                }
            }
            Vec2 dir = dist > 1e-3f ? toPlayer * (1.0f / dist) : Vec2{};
            if (e.wander > 0) {
                e.wander -= dt;
                dir = e.wanderDir;
            } else if (dist < d.keepAway) {
                dir = Vec2{-dir.y, dir.x} * (e.id % 2 ? 0.6f : -0.6f);  // circle-strafe instead of crowding
            }
            Vec2 before = e.body.pos;
            e.body.vel = dir * d.speed;
            moveAndSlide(map_, e.body, e.body.vel * dt);
            e.body.angle = std::atan2(toPlayer.y, toPlayer.x);
            // Stuck on geometry: pick a random direction for a moment.
            float moved = length(e.body.pos - before);
            e.stuck = moved < d.speed * dt * 0.3f ? e.stuck + dt : 0;
            if (e.stuck > 0.2f) {
                int door = moverAhead(e.body.pos, std::atan2(dir.y, dir.x), e.body.sector, e.body.radius + 1.0f);
                if (door >= 0 && map_.sectors[static_cast<size_t>(door)].mover == Mover::Door) activate(door, false);
            }
            if (e.stuck > 0.4f && e.wander <= 0) {
                float a = rand01() * 2 * kPi;
                e.wanderDir = {std::cos(a), std::sin(a)};
                e.wander = 0.7f;
                e.stuck = 0;
            }
            break;
        }
        case Enemy::AI::Dead: break;
    }
    applyGravity(map_, e.body, dt);
}

void Game::enemyAttack(Enemy& e) {
    Vec2 toPlayer = player_.pos - e.body.pos;
    float dist = length(toPlayer);
    if (e.type == ThingType::Grunt) {
        sound(Sfx::EnemyShot, e.body.pos);
        float hitChance = std::clamp(0.85f - dist * 0.03f - length(player_.vel) * 0.03f, 0.2f, 0.85f);
        if (rand01() < hitChance) damagePlayer(randRange(3, 9));
        return;
    }
    if (dist < e.body.radius + player_.radius + 0.9f) {
        sound(Sfx::Claw, e.body.pos);
        damagePlayer(randRange(10, 18));  // claws
        return;
    }
    Projectile p;
    Vec2 dir = toPlayer * (1.0f / std::max(dist, 1e-3f));
    p.pos = e.body.pos + dir * (e.body.radius + 0.2f);
    p.z = e.body.z + e.body.height * 0.6f;
    p.vel = dir * 9.0f;
    p.sector = map_.sectorAt(p.pos, e.body.sector);
    p.damage = randRange(12, 20);
    p.id = nextId_++;
    if (p.sector >= 0) {
        projectiles_.push_back(p);
        sound(Sfx::Fireball, e.body.pos);
    }
}

void Game::updateProjectiles(float dt) {
    for (size_t i = 0; i < projectiles_.size();) {
        Projectile& p = projectiles_[i];
        Vec2 next = p.pos + p.vel * dt;
        // Aim drifts toward the player's chest height as it flies.
        float targetZ = player_.z + 1.1f;
        float nz = p.z + std::clamp(targetZ - p.z, -1.0f, 1.0f) * dt * 1.5f;
        bool dead = traceSegment(map_, p.sector, p.pos, p.z, next, nz) < 1.0f;
        if (!dead) {
            p.pos = next;
            p.z = nz;
            p.sector = map_.sectorAt(p.pos, p.sector);
            dead = p.sector < 0;
        }
        if (!dead && state_ == State::Playing && length(p.pos - player_.pos) < player_.radius + 0.25f &&
            p.z > player_.z && p.z < player_.z + player_.height) {
            damagePlayer(p.damage);
            dead = true;
        }
        if (dead) {
            projectiles_[i] = projectiles_.back();
            projectiles_.pop_back();
        } else {
            ++i;
        }
    }
}

void Game::updatePickups() {
    for (Pickup& p : pickups_) {
        if (p.taken || length(p.pos - player_.pos) > 0.9f) continue;
        switch (p.type) {
            case ThingType::Medkit:
                if (health_ >= 100) continue;
                health_ = std::min(100, health_ + 25);
                say("Picked up a medkit.");
                break;
            case ThingType::Ammo:
                if (ammo_ >= 200) continue;
                ammo_ = std::min(200, ammo_ + 20);
                say("Picked up some ammo.");
                break;
            case ThingType::RedKey:
                redKey_ = true;
                say("Picked up the red key.");
                break;
            case ThingType::BlueKey:
                blueKey_ = true;
                say("Picked up the blue key.");
                break;
            case ThingType::Armor:
                if (armor_ >= 100) continue;
                armor_ = std::min(100, armor_ + 50);
                say("Picked up armour.");
                break;
            default: continue;
        }
        p.taken = true;
        pickupFlash_ = 0.4f;
        sound(Sfx::Pickup);
    }
}

void Game::separateBodies() {
    for (Enemy& e : enemies_) {
        if (!e.alive()) continue;
        Vec2 d = player_.pos - e.body.pos;
        float r = player_.radius + e.body.radius, l = length(d);
        if (l < r && l > 1e-4f) moveAndSlide(map_, player_, d * ((r - l) / l));
    }
    for (size_t i = 0; i < enemies_.size(); ++i) {
        for (size_t j = i + 1; j < enemies_.size(); ++j) {
            Enemy &a = enemies_[i], &b = enemies_[j];
            if (!a.alive() || !b.alive()) continue;
            Vec2 d = a.body.pos - b.body.pos;
            float r = a.body.radius + b.body.radius, l = length(d);
            if (l >= r || l < 1e-4f) continue;
            Vec2 push = d * ((r - l) / l * 0.5f);
            moveAndSlide(map_, a.body, push);
            moveAndSlide(map_, b.body, push * -1.0f);
        }
    }
}

render::Camera Game::camera() const {
    render::Camera cam;
    cam.pos = player_.pos;
    cam.angle = player_.angle;
    cam.sector = player_.sector;
    const Sector& s = map_.sectors[static_cast<size_t>(player_.sector)];
    float z = state_ == State::Dead ? player_.z + 0.35f : viewZ_ + bob_;  // on the floor when dead
    cam.z = std::min(z, s.ceil - 0.1f);
    return cam;
}

void Game::collectSprites(std::vector<render::SpriteInstance>& out) const {
    using render::SpriteId, render::sprite;
    out.clear();
    auto lightAt = [&](int sector) { return sector >= 0 ? map_.sectors[static_cast<size_t>(sector)].light : 1.0f; };
    for (const Enemy& e : enemies_) {
        bool brute = e.type == ThingType::Brute;
        render::SpriteInstance s;
        s.pos = e.body.pos;
        s.z = e.body.z;
        s.light = lightAt(e.body.sector);
        s.id = e.id;
        if (!e.alive()) {
            s.image = &sprite(brute ? SpriteId::BruteDead : SpriteId::GruntDead);
            s.height = brute ? 0.6f : 0.4f;
        } else {
            bool firing = e.ai == Enemy::AI::Attack && e.timer < 0.3f && e.timer > 0.05f;
            s.image = &sprite(brute ? (firing ? SpriteId::BruteFire : SpriteId::Brute)
                                    : (firing ? SpriteId::GruntFire : SpriteId::Grunt));
            s.height = e.body.height;
            s.fullbright = e.flash > 0 || firing;
        }
        out.push_back(s);
    }
    for (const Pickup& p : pickups_) {
        if (p.taken) continue;
        render::SpriteInstance s;
        s.pos = p.pos;
        s.z = map_.sectors[static_cast<size_t>(p.sector)].floor;
        s.light = lightAt(p.sector);
        s.id = p.id;
        bool key = p.type == ThingType::RedKey || p.type == ThingType::BlueKey;
        s.image = &sprite(p.type == ThingType::Medkit   ? SpriteId::Medkit
                          : p.type == ThingType::Ammo   ? SpriteId::Ammo
                          : p.type == ThingType::RedKey ? SpriteId::KeyRed
                          : p.type == ThingType::BlueKey ? SpriteId::KeyBlue
                                                         : SpriteId::Armor);
        s.height = p.type == ThingType::Armor ? 0.55f : key ? 0.32f : 0.42f;
        if (key) {
            s.z += 0.25f + 0.08f * std::sin(elapsed_ * 3.0f);  // keys hover and bob
            s.fullbright = true;
        }
        out.push_back(s);
    }
    for (const Projectile& p : projectiles_) {
        render::SpriteInstance s;
        s.image = &sprite(SpriteId::Fireball);
        s.pos = p.pos;
        s.z = p.z - 0.2f;
        s.height = 0.4f;
        s.fullbright = true;
        s.id = p.id;
        out.push_back(s);
    }
}

WeaponView Game::weaponView() const {
    using render::SpriteId;
    WeaponView v;
    bool firing = fireAnim_ > 0;
    if (weapon_ == Weapon::Pistol) {
        v.sprite = firing ? SpriteId::PistolFire : SpriteId::Pistol;
    } else {
        v.sprite = firing ? SpriteId::ScattergunFire : SpriteId::Scattergun;
    }
    float speed = std::min(length(player_.vel) / 8.0f, 1.0f);
    v.bobX = std::sin(bobPhase_ * kPi * 0.5f) * 0.04f * speed;
    v.bobY = std::abs(std::cos(bobPhase_ * kPi * 0.5f)) * 0.03f * speed + (fireCooldown_ > 0.2f ? 0.02f : 0.0f);
    v.flash = firing;
    if (state_ == State::Dead) v.bobY = 1.0f;  // lowered off-screen
    return v;
}

int Game::moverAhead(Vec2 pos, float angle, int sector, float reach) const {
    // Walk along the facing direction and report the first door/lift sector.
    Vec2 dir{std::cos(angle), std::sin(angle)};
    int hint = sector;
    for (float t = 0.1f; t <= reach; t += 0.1f) {
        int s = map_.sectorAt(pos + dir * t, hint);
        if (s < 0) return -1;
        if (s != sector && map_.sectors[static_cast<size_t>(s)].mover != Mover::None) return s;
        hint = s;
    }
    return -1;
}

bool Game::occupied(int sector) const {
    const Sector& s = map_.sectors[static_cast<size_t>(sector)];
    auto inside = [&](const Body& b) {
        if (b.sector == sector) return true;
        if (!s.contains(b.pos)) {
            for (size_t e = 0; e < s.edgeCount(); ++e)
                if (length(closestOnSegment(s.a(e), s.b(e), b.pos) - b.pos) < b.radius && side(s.a(e), s.b(e), b.pos) > -b.radius)
                    return true;
            return false;
        }
        return true;
    };
    if (inside(player_)) return true;
    for (const Enemy& e : enemies_)
        if (e.alive() && inside(e.body)) return true;
    return false;
}

bool Game::activate(int sector, bool byPlayer) {
    Sector& s = map_.sectors[static_cast<size_t>(sector)];
    if (s.mover == Mover::None) return false;
    if (s.mover == Mover::Door && !hasKey(s.key)) {
        if (byPlayer) say(s.key == KeyColor::Red ? "You need the red key." : "You need the blue key.");
        return false;
    }
    for (ActiveMover& m : movers_) {
        if (m.sector != sector) continue;
        // Already moving: a closing door reopens, a parked one waits longer.
        if (s.mover == Mover::Door) {
            if (m.dir < 0) m.dir = 1;
            if (m.wait > 0) m.wait = kDoorWait;
        }
        return true;
    }
    movers_.push_back({sector, s.mover == Mover::Door ? 1 : -1, 0});
    Vec2 centre{};
    for (Vec2 v : s.verts) centre += v;
    sound(s.mover == Mover::Door ? Sfx::Door : Sfx::Lift, centre * (1.0f / static_cast<float>(s.verts.size())));
    return true;
}

void Game::updateMovers(float dt) {
    for (size_t i = 0; i < movers_.size();) {
        ActiveMover& m = movers_[i];
        Sector& s = map_.sectors[static_cast<size_t>(m.sector)];
        bool done = false;
        if (m.wait > 0) {
            m.wait -= dt;
            if (m.wait <= 0) m.dir = s.mover == Mover::Door ? -1 : 1;
        } else if (s.mover == Mover::Door) {
            if (m.dir > 0) {
                s.ceil = std::min(s.openCeil, s.ceil + kDoorSpeed * dt);
                if (s.ceil >= s.openCeil) m.wait = kDoorWait;
            } else if (occupied(m.sector)) {
                m.dir = 1;  // never crush: reopen
            } else {
                s.ceil = std::max(s.floor, s.ceil - kDoorSpeed * dt);
                done = s.ceil <= s.floor;
            }
        } else {
            if (m.dir < 0) {
                s.floor = std::max(s.lowFloor, s.floor - kLiftSpeed * dt);
                if (s.floor <= s.lowFloor) m.wait = kLiftWait;
            } else {
                s.floor = std::min(s.topFloor, s.floor + kLiftSpeed * dt);
                done = s.floor >= s.topFloor;
            }
        }
        if (done) {
            movers_[i] = movers_.back();
            movers_.pop_back();
        } else {
            ++i;
        }
    }
}

}  // namespace tg::game
