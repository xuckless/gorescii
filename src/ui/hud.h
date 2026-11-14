#pragma once
// Text-mode overlays drawn on top of the ASCII scene: status bar, messages,
// crosshair, and the pause / death / win screens.
#include <string>

#include "game/game.h"
#include "gfx/cells.h"

namespace tg::ui {

constexpr int kStatusRows = 2;

struct HudInfo {
    bool paused = false;
    bool showHelp = false;
    std::string toast;  // short-lived UI notice (look toggles, terminal tips)
    bool hasNextLevel = false;
    int level = 1, levelCount = 1;
};

void drawHud(gfx::CellGrid& screen, const game::Game& g, const HudInfo& info);

// Tints the rendered frame for damage / pickup / muzzle flashes.
void flashFrame(gfx::Framebuffer& fb, const game::Game& g);

}  // namespace tg::ui
