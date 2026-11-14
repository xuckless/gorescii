#include "ui/hud.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>
#include <vector>

namespace tg::ui {
namespace {

using gfx::Rgb;

constexpr Rgb kBarBg{18, 18, 24};
constexpr Rgb kLabel{140, 140, 150};
constexpr Rgb kText{230, 230, 220};
constexpr Rgb kDim{110, 110, 120};
constexpr Rgb kGreen{90, 220, 110}, kYellow{240, 200, 70}, kRed{240, 70, 60}, kBlue{100, 160, 255};

Rgb levelColor(int v) { return v > 60 ? kGreen : v > 30 ? kYellow : kRed; }

// Writes UTF-8 text, one cell per code point.
int put(gfx::CellGrid& s, int x, int y, std::string_view text, Rgb fg, const Rgb* bg = nullptr) {
    if (y < 0 || y >= s.rows) return x;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        char32_t cp;
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        if (len == 1) cp = c;
        else if (len == 2) cp = ((c & 0x1F) << 6) | (text[i + 1] & 0x3F);
        else if (len == 3) cp = ((c & 0x0F) << 12) | ((text[i + 1] & 0x3F) << 6) | (text[i + 2] & 0x3F);
        else cp = ((c & 0x07) << 18) | ((text[i + 1] & 0x3F) << 12) | ((text[i + 2] & 0x3F) << 6) | (text[i + 3] & 0x3F);
        i += static_cast<size_t>(len);
        if (x >= 0 && x < s.cols) {
            gfx::Cell& cell = s.at(x, y);
            cell.glyph = cp;
            cell.fg = fg;
            if (bg) cell.bg = *bg;
        }
        ++x;
    }
    return x;
}

int width(std::string_view text) {
    int n = 0;
    for (unsigned char c : text) n += (c & 0xC0) != 0x80;
    return n;
}

int meter(gfx::CellGrid& s, int x, int y, int value, int max, int cells, Rgb color) {
    int filled = std::clamp((value * cells + max - 1) / max, 0, cells);
    x = put(s, x, y, "[", kDim, &kBarBg);
    for (int i = 0; i < cells; ++i) x = put(s, x, y, i < filled ? "█" : "·", i < filled ? color : kDim, &kBarBg);
    return put(s, x, y, "]", kDim, &kBarBg);
}

std::string clock(float seconds) {
    char buf[16];
    int t = static_cast<int>(seconds);
    std::snprintf(buf, sizeof buf, "%02d:%02d", t / 60, t % 60);
    return buf;
}

// Centred box with a title and lines of text, drawn over the scene.
void dialog(gfx::CellGrid& s, int sceneRows, std::string_view title, Rgb titleColor, const std::vector<std::string>& lines) {
    int inner = width(title);
    for (const auto& l : lines) inner = std::max(inner, width(l));
    int w = inner + 6, h = static_cast<int>(lines.size()) + 4;
    int x0 = (s.cols - w) / 2, y0 = std::max(0, (sceneRows - h) / 2);
    const Rgb bg{10, 10, 14};
    for (int y = 0; y < h; ++y) {
        std::string row;
        for (int x = 0; x < w; ++x) {
            bool top = y == 0, bot = y == h - 1, left = x == 0, right = x == w - 1;
            row += top && left ? "┌" : top && right ? "┐" : bot && left ? "└" : bot && right ? "┘"
                 : top || bot ? "─" : left || right ? "│" : " ";
        }
        put(s, x0, y0 + y, row, kDim, &bg);
    }
    put(s, x0 + (w - width(title)) / 2, y0 + 1, title, titleColor, &bg);
    for (size_t i = 0; i < lines.size(); ++i)
        put(s, x0 + (w - width(lines[i])) / 2, y0 + 3 + static_cast<int>(i), lines[i], kText, &bg);
}

}  // namespace

void drawHud(gfx::CellGrid& s, const game::Game& g, const HudInfo& info) {
    const int sceneRows = s.rows - kStatusRows;

    // Status bar.
    for (int y = sceneRows; y < s.rows; ++y)
        for (int x = 0; x < s.cols; ++x) s.at(x, y) = gfx::Cell{U' ', kText, kBarBg};
    if (sceneRows >= 0 && sceneRows < s.rows)
        for (int x = 0; x < s.cols; ++x) s.at(x, sceneRows) = gfx::Cell{U'▔', Rgb{60, 60, 70}, kBarBg};
    int y = s.rows - 1;
    bool narrow = s.cols < 100;
    int x = 1;
    x = put(s, x, y, "HEALTH ", kLabel, &kBarBg);
    char num[32];
    std::snprintf(num, sizeof num, "%3d ", g.health());
    x = put(s, x, y, num, levelColor(g.health()), &kBarBg);
    if (!narrow) x = meter(s, x, y, g.health(), 100, 10, levelColor(g.health())) + 1;
    x = put(s, x + 1, y, "ARMOR ", kLabel, &kBarBg);
    std::snprintf(num, sizeof num, "%3d ", g.armor());
    x = put(s, x, y, num, kBlue, &kBarBg);
    if (!narrow) x = meter(s, x, y, g.armor(), 100, 10, kBlue) + 1;
    x = put(s, x + 1, y, "AMMO ", kLabel, &kBarBg);
    std::snprintf(num, sizeof num, "%3d", g.ammo());
    x = put(s, x, y, num, g.ammo() > 10 ? kText : kRed, &kBarBg);
    bool pistol = g.weapon() == game::Weapon::Pistol;
    x = put(s, x + 3, y, "1 PISTOL", pistol ? kYellow : kDim, &kBarBg);
    x = put(s, x + 2, y, "2 SCATTERGUN", pistol ? kDim : kYellow, &kBarBg);
    if (g.hasKey(KeyColor::Red)) x = put(s, x + 2, y, "▮", kRed, &kBarBg);
    if (g.hasKey(KeyColor::Blue)) x = put(s, x + (g.hasKey(KeyColor::Red) ? 1 : 2), y, "▮", kBlue, &kBarBg);
    if (info.levelCount > 1)
        std::snprintf(num, sizeof num, "LEVEL %d/%d  KILLS %d/%d  %s", info.level, info.levelCount, g.kills(),
                      g.totalEnemies(), clock(g.elapsed()).c_str());
    else
        std::snprintf(num, sizeof num, "KILLS %d/%d  %s", g.kills(), g.totalEnemies(), clock(g.elapsed()).c_str());
    int right = s.cols - width(num) - 1;
    if (right > x + 1) put(s, right, y, num, kLabel, &kBarBg);

    // Crosshair.
    if (sceneRows > 0 && g.state() == game::State::Playing) {
        gfx::Cell& c = s.at(s.cols / 2, sceneRows / 2);
        c.glyph = U'+';
        c.fg = Rgb{255, 255, 255};
    }

    // Messages, newest last, top-left.
    int my = 0;
    for (const auto& m : g.messages()) put(s, 1, my++, m.text, kYellow);
    if (!info.toast.empty()) put(s, 1, my++, info.toast, kBlue);
    if (info.showHelp && g.state() == game::State::Playing) {
        put(s, 1, my++, "WASD move  ←→ / mouse turn  Space / click fire  E use  1 2 Q weapons  Esc pause", kDim);
    }

    if (g.state() == game::State::Dead) {
        dialog(s, sceneRows, "YOU DIED", kRed, {"Enter  try again", "Ctrl-C  quit"});
    } else if (g.state() == game::State::Won) {
        std::snprintf(num, sizeof num, "Kills %d/%d   Time %s", g.kills(), g.totalEnemies(), clock(g.elapsed()).c_str());
        std::string title = g.map().name.empty() ? std::string("LEVEL") : g.map().name;
        for (char& ch : title) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        title += " CLEARED";
        bool last = !info.hasNextLevel;
        std::vector<std::string> lines{num, ""};
        if (last && info.levelCount > 1) lines.insert(lines.begin(), "All levels complete!");
        lines.push_back(last ? "Enter  play again from the start" : "Enter  next level");
        lines.push_back("Ctrl-C  quit");
        dialog(s, sceneRows, title, kGreen, lines);
    } else if (info.paused) {
        dialog(s, sceneRows, "PAUSED", kText,
               {"Esc  resume        X  quit", "", "C  colour   V  edge style   B  background   M  sound"});
    }
}

void flashFrame(gfx::Framebuffer& fb, const game::Game& g) {
    float dmg = g.damageFlash() * 0.55f, pick = g.pickupFlash() * 0.35f, muzzle = g.muzzleFlash() * 0.12f;
    if (dmg <= 0 && pick <= 0 && muzzle <= 0) return;
    for (auto& c : fb.color) {
        float r = c.r, gg = c.g, b = c.b;
        r = r + (255 - r) * muzzle, gg = gg + (255 - gg) * muzzle * 0.8f, b = b + (255 - b) * muzzle * 0.4f;
        r = r * (1 - dmg) + 200 * dmg, gg *= 1 - dmg, b *= 1 - dmg;
        r = r * (1 - pick) + 230 * pick, gg = gg * (1 - pick) + 200 * pick, b *= 1 - pick;
        c = {static_cast<uint8_t>(std::min(255.0f, r)), static_cast<uint8_t>(std::min(255.0f, gg)),
             static_cast<uint8_t>(std::min(255.0f, b))};
    }
}

}  // namespace tg::ui
