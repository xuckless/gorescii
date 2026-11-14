#pragma once
// One frame of the game as terminal cells: 3D render, sprites, held weapon,
// flashes, the ASCII pass and the HUD. Shared by the game and the README
// screenshot tool.
#include <string>
#include <vector>

#include "game/game.h"
#include "gfx/ascii.h"
#include "gfx/cells.h"
#include "render/renderer.h"
#include "ui/hud.h"

namespace tg::ui {

class View {
public:
    gfx::Framebuffer fb;
    gfx::CellGrid scene, grid;  // grid = scene + status rows
    gfx::AsciiShader shader;
    render::Renderer renderer;

    void draw(const game::Game& g, int cols, int rows, int cellW, double cellAspect, const HudInfo& hud);

    // C cycles colour / green phosphor / amber.
    std::string cycleColor();
    void setStyle(gfx::EdgeStyle st);
    // V cycles clean -> lines -> off.
    std::string cycleEdges();
    std::string toggleBackground();

private:
    gfx::EdgeStyle style_ = gfx::EdgeStyle::Clean;
    std::vector<render::SpriteInstance> sprites_;
};

}  // namespace tg::ui
