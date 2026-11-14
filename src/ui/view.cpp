#include "ui/view.h"

#include <algorithm>
#include <cmath>

namespace tg::ui {

void View::draw(const game::Game& g, int cols, int rows, int cellW, double cellAspect, const HudInfo& hud) {
    int cellH = std::max(1, static_cast<int>(std::lround(cellW * cellAspect)));
    int sceneRows = std::max(1, rows - kStatusRows);
    if (grid.cols != cols || grid.rows != rows) grid.resize(cols, rows);
    if (scene.cols != cols || scene.rows != sceneRows) scene.resize(cols, sceneRows);
    if (fb.width != cols * cellW || fb.height != sceneRows * cellH) fb.resize(cols * cellW, sceneRows * cellH);
    renderer.render(g.map(), g.camera(), fb);
    g.collectSprites(sprites_);
    renderer.drawSprites(sprites_, fb);
    game::WeaponView wv = g.weaponView();
    float px = fb.height / 48.0f;
    renderer.drawOverlay(render::sprite(wv.sprite), fb.width * (0.5f + wv.bobX), fb.height * (1.0f + wv.bobY) + px, px,
                         wv.flash ? 1.3f : 0.9f, fb);
    flashFrame(fb, g);
    shader.apply(fb, cellW, cellH, scene);
    std::copy(scene.cells.begin(), scene.cells.end(), grid.cells.begin());
    drawHud(grid, g, hud);
}

std::string View::cycleColor() {
    auto& st = shader.settings;
    if (st.colorBlend > 0.5f) {
        st.colorBlend = 0;
        st.tint = {80, 255, 120};
        return "Colour: green phosphor";
    }
    if (st.tint.r < 200) {
        st.tint = {255, 180, 60};
        return "Colour: amber";
    }
    st.colorBlend = 1;
    st.tint = {255, 255, 255};
    return "Colour: full";
}

void View::setStyle(gfx::EdgeStyle st) {
    style_ = st;
    gfx::applyEdgeStyle(shader.settings, st);
}

std::string View::cycleEdges() {
    setStyle(style_ == gfx::EdgeStyle::Clean   ? gfx::EdgeStyle::Lines
             : style_ == gfx::EdgeStyle::Lines ? gfx::EdgeStyle::Off
                                               : gfx::EdgeStyle::Clean);
    return std::string("Edges: ") + gfx::edgeStyleName(style_);
}

std::string View::toggleBackground() {
    auto& b = shader.settings.backgroundBlend;
    b = b > 0 ? 0.0f : 0.3f;
    return b > 0 ? "Background fill on" : "Background fill off";
}

}  // namespace tg::ui
