#include "game/game.h"

#include <cmath>

namespace tg::game {

Game::Game(Map map) : map_(std::move(map)) {
    const Thing* start = map_.playerStart();
    player_.pos = start->pos;
    player_.angle = start->angle;
    player_.sector = map_.sectorAt(player_.pos);
    player_.z = map_.sectors[static_cast<size_t>(player_.sector)].floor;
    viewZ_ = player_.z + Body::kEye;
}

void Game::update(const Controls& c, float dt) {
    stepPlayer(map_, player_, c, dt);

    float target = player_.z + Body::kEye;
    // Rise smoothly up steps; drop immediately when falling.
    viewZ_ = target > viewZ_ ? viewZ_ + (target - viewZ_) * std::min(1.0f, dt * 14.0f) : target;
    float speed = length(player_.vel);
    bobPhase_ += speed * dt * 1.6f;
    float amp = player_.onGround ? std::min(speed / 8.0f, 1.0f) * 0.06f : 0.0f;
    bob_ = std::sin(bobPhase_ * 3.14159265f) * amp;
}

render::Camera Game::camera() const {
    render::Camera cam;
    cam.pos = player_.pos;
    cam.angle = player_.angle;
    cam.sector = player_.sector;
    const Sector& s = map_.sectors[static_cast<size_t>(player_.sector)];
    cam.z = std::min(viewZ_ + bob_, s.ceil - 0.1f);
    return cam;
}

}  // namespace tg::game
