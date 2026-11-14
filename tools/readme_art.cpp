// Renders the animated SVGs in docs/readme from the real engine, so the
// README shows actual frames. Usage: gorescii_readme_art <out-dir>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "game/game.h"
#include "gfx/present.h"
#include "ui/view.h"
#include "world/map.h"

namespace {

using namespace tg;

constexpr float kCharW = 7.2f, kRowH = 14.0f, kFont = 12.0f;
const char* kMono = "'JetBrains Mono', ui-monospace, SFMono-Regular, Menlo, Consolas, 'DejaVu Sans Mono', monospace";
const char* kSans = "-apple-system, 'Segoe UI', Inter, Roboto, 'Helvetica Neue', Arial, sans-serif";

std::string hex(gfx::Rgb c) {
    auto q = [](uint8_t v) { return "0123456789abcdef"[std::min(15, (v + 8) / 17)]; };
    return std::string("#") + q(c.r) + q(c.g) + q(c.b);
}

std::string escape(char32_t cp) {
    if (cp == U'<') return "&lt;";
    if (cp == U'>') return "&gt;";
    if (cp == U'&') return "&amp;";
    std::string s;
    gfx::appendUtf8(s, cp);
    return s;
}

// Rows of a cell grid as <text> elements with one <tspan> per colour run.
std::string gridText(const gfx::CellGrid& g, float x0, float y0, int c0 = 0, int r0 = 0, int cols = -1, int rows = -1,
                     const std::function<gfx::Rgb(int, int, const gfx::Cell&)>& colorOf = nullptr) {
    if (cols < 0) cols = g.cols;
    if (rows < 0) rows = g.rows;
    std::ostringstream o;
    for (int r = 0; r < rows; ++r) {
        o << "<text x=\"" << x0 << "\" y=\"" << y0 + (r + 0.78f) * kRowH << "\" textLength=\"" << cols * kCharW
          << "\" lengthAdjust=\"spacing\">";
        std::string cur, run;
        auto flush = [&] {
            if (!run.empty()) o << "<tspan fill=\"" << cur << "\">" << run << "</tspan>";
            run.clear();
        };
        for (int c = 0; c < cols; ++c) {
            const gfx::Cell& cell = g.at(c0 + c, r0 + r);
            std::string col = hex(colorOf ? colorOf(c0 + c, r0 + r, cell) : cell.fg);
            if (cell.glyph != U' ' && col != cur) {
                flush();
                cur = col;
            }
            run += cell.glyph == U' ' ? std::string("\xc2\xa0") : escape(cell.glyph);  // nbsp keeps width
        }
        flush();
        o << "</text>\n";
    }
    return o.str();
}

std::string windowFrame(float w, float h, const std::string& title) {
    std::ostringstream o;
    o << "<rect x=\"0.5\" y=\"0.5\" width=\"" << w - 1 << "\" height=\"" << h - 1
      << "\" rx=\"10\" fill=\"#16161e\" stroke=\"#2f3449\"/>\n";
    o << "<path d=\"M10 0.5h" << w - 20 << "a9.5 9.5 0 0 1 9.5 9.5v20.5h-" << w - 1
      << "v-20.5a9.5 9.5 0 0 1 9.5-9.5z\" fill=\"#1a1b26\"/>\n";
    for (int i = 0; i < 3; ++i)
        o << "<circle cx=\"" << 18 + i * 14 << "\" cy=\"15.5\" r=\"4\" fill=\"" << (i == 0 ? "#f7768e" : i == 1 ? "#e0af68" : "#9ece6a")
          << "\" opacity=\"0.8\"/>";
    o << "<text x=\"" << w / 2 << "\" y=\"20\" text-anchor=\"middle\" fill=\"#8089b3\" style=\"font-family:" << kMono
      << ";font-size:12px\">" << title << "</text>\n";
    return o.str();
}

std::string header(float w, float h, const std::string& label, const std::string& extraCss) {
    std::ostringstream o;
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w << "\" height=\"" << h << "\" viewBox=\"0 0 " << w << " " << h
      << "\" role=\"img\" aria-label=\"" << label << "\">\n<title>" << label << "</title>\n<style>\n"
      << "text { font-family: " << kMono << "; font-size: " << kFont << "px; white-space: pre; }\n"
      << ".sans { font-family: " << kSans << "; font-size: 12px; }\n"
      << "* { transform-box: fill-box; }\n"
      << extraCss << "</style>\n";
    return o.str();
}

bool save(const std::string& path, const std::string& text) {
    std::ofstream f(path);
    f << text;
    std::printf("wrote %s (%zu KB)\n", path.c_str(), text.size() / 1024);
    return static_cast<bool>(f);
}

std::optional<Map> load(const char* name) {
    MapError err;
    auto m = loadMap(std::string(GORESCII_ASSET_DIR) + "/maps/" + name, err);
    if (!m) std::fprintf(stderr, "%s: %s\n", name, err.message.c_str());
    return m;
}

// Frames cycle with negative delays so the first frame shows immediately.
std::string frameCss(int n, float each) {
    std::ostringstream o;
    float total = n * each;
    float pct = 100.0f / n;
    o << ".f { opacity: 0; animation: show " << total << "s infinite; }\n"
      << "@keyframes show { 0%, " << pct << "% { opacity: 1; } " << pct + 0.01f << "%, 100% { opacity: 0; } }\n";
    for (int i = 0; i < n; ++i) o << ".f" << i << " { animation-delay: " << i * each - total << "s; }\n";
    o << "@media (prefers-reduced-motion: reduce) { .f { animation: none; } .f0 { opacity: 1; } }\n";
    return o.str();
}

// --- play.svg: a short scripted run through the Outpost corridor -----------

bool writePlay(const std::string& dir) {
    auto map = load("outpost.map");
    if (!map) return false;
    const int cols = 78, rows = 24;
    game::Game g(*map, 5);
    ui::View view;
    ui::HudInfo hud;
    hud.levelCount = 2;

    std::vector<std::string> frames;
    const float dt = 1.0f / 60, each = 0.18f;
    float t = 0, nextShot = 0;
    const int count = 18;
    while (static_cast<int>(frames.size()) < count) {
        game::Controls c;
        // Walk into the start room toward the corridor, tracking the nearest
        // grunt, and open fire once it is in range.
        c.forward = t < 1.3f ? 0.45f : 0.0f;
        c.fire = t > 0.9f;
        const game::Enemy* target = nullptr;
        for (auto& e : g.enemies())
            if (e.alive() && (!target || length(e.body.pos - g.player().pos) < length(target->body.pos - g.player().pos)))
                target = &e;
        if (target) {
            Vec2 to = target->body.pos - g.player().pos;
            float diff = std::remainder(std::atan2(to.y, to.x) - g.player().angle, 2 * 3.14159265f);
            c.look = std::clamp(diff, -0.025f, 0.025f);
        }
        g.update(c, dt);
        t += dt;
        if (t >= nextShot) {
            nextShot += each;
            view.draw(g, cols, rows, 4, 2.0, hud);
            std::ostringstream f;
            f << "<g class=\"f f" << frames.size() << "\">\n" << gridText(view.grid, 16, 40) << "</g>\n";
            frames.push_back(f.str());
        }
    }
    float w = cols * kCharW + 32, h = rows * kRowH + 56;
    std::ostringstream o;
    o << header(w, h, "Gorescii running in a terminal: walking down a corridor and shooting a grunt, all drawn in coloured ASCII",
                frameCss(count, each));
    o << windowFrame(w, h, "gorescii — Outpost");
    o << "<rect x=\"8\" y=\"" << 40 + (rows - ui::kStatusRows) * kRowH << "\" width=\"" << w - 16 << "\" height=\""
      << ui::kStatusRows * kRowH << "\" fill=\"#121218\"/>\n";
    for (auto& f : frames) o << f;
    o << "</svg>\n";
    return save(dir + "/play.svg", o.str());
}

// --- shader.svg: pixels -> edges -> glyphs ---------------------------------

bool writeShader(const std::string& dir) {
    auto map = load("outpost.map");
    if (!map) return false;
    for (Thing& th : map->things)
        if (th.type == ThingType::Player) th.pos = {12.6f, 4.0f}, th.angle = 0.25f;
    game::Game g(*map, 1);
    const int cols = 20, rows = 11, cw = 4, ch = 8;
    ui::View view;
    ui::HudInfo hud;
    view.draw(g, cols, rows + ui::kStatusRows, cw, 2.0, hud);
    gfx::CellGrid edges = view.scene;
    gfx::AsciiShader fillOnly;
    fillOnly.settings.edges = false;
    gfx::CellGrid fills;
    fills.resize(view.scene.cols, view.scene.rows);
    fillOnly.apply(view.fb, cw, ch, fills);

    const float pw = cols * kCharW, ph = rows * kRowH, gap = 30, top = 66, left = 16;
    float w = left * 2 + pw * 3 + gap * 2, h = top + ph + 52;
    std::ostringstream css;
    // A highlight hops across tiles in the same spot on every panel.
    const int hops = 8;
    css << ".hl { animation: hop " << hops * 1.2f << "s steps(1) infinite; }\n@keyframes hop {\n";
    int path[hops][2] = {{3, 2}, {9, 3}, {15, 2}, {6, 6}, {10, 8}, {17, 7}, {2, 9}, {13, 5}};
    for (int i = 0; i < hops; ++i)
        css << "  " << i * 100.0f / hops << "% { transform: translate(" << path[i][0] * kCharW << "px, " << path[i][1] * kRowH
            << "px); }\n";
    css << "}\n@media (prefers-reduced-motion: reduce) { .hl { animation: none; } }\n";
    std::ostringstream o;
    o << header(w, h, "One frame three ways: rendered pixels, cells where edges win, and the final coloured ASCII", css.str());
    o << windowFrame(w, h, "pixels → edges → glyphs");

    // Panel 1: the framebuffer as 2x2-pixel blocks, merged into runs.
    float sx = pw / view.fb.width, sy = ph / view.fb.height;
    o << "<g>\n";
    for (int y = 0; y < view.fb.height; y += 2) {
        int x = 0;
        while (x < view.fb.width) {
            std::string col = hex(view.fb.at(x, y));
            int x1 = x + 2;
            while (x1 < view.fb.width && hex(view.fb.at(x1, y)) == col) x1 += 2;
            o << "<rect x=\"" << left + x * sx << "\" y=\"" << top + y * sy << "\" width=\"" << (x1 - x) * sx + 0.3f
              << "\" height=\"" << 2 * sy + 0.3f << "\" fill=\"" << col << "\"/>";
            x = x1;
        }
    }
    o << "\n</g>\n";
    // Panel 2: edge cells bright, fill cells dim.
    o << gridText(edges, left + pw + gap, top, 0, 0, cols, rows, [&](int x, int y, const gfx::Cell& c) {
        bool edge = c.glyph != fills.at(x, y).glyph;
        return edge ? gfx::Rgb{255, 158, 100} : gfx::Rgb{70, 74, 100};
    });
    // Panel 3: the real output.
    o << gridText(view.scene, left + 2 * (pw + gap), top, 0, 0, cols, rows);

    const char* labels[3] = {"Render", "Find edges", "Pick glyphs"};
    for (int i = 0; i < 3; ++i) {
        float px = left + i * (pw + gap);
        o << "<text class=\"sans\" x=\"" << px + pw / 2 << "\" y=\"54\" text-anchor=\"middle\" fill=\"#a9b1d6\">" << labels[i]
          << "</text>\n";
        o << "<rect class=\"hl\" x=\"" << px << "\" y=\"" << top << "\" width=\"" << kCharW << "\" height=\"" << kRowH
          << "\" fill=\"none\" stroke=\"#e0af68\" stroke-width=\"1.5\" rx=\"1.5\"/>\n";
        if (i < 2)
            o << "<path d=\"M" << px + pw + 8 << " " << top + ph / 2 << " h14 m-5 -5 l5 5 l-5 5\" fill=\"none\" stroke=\"#565f89\" stroke-width=\"1.6\"/>\n";
    }
    o << "<text class=\"sans\" x=\"" << w / 2 << "\" y=\"" << h - 16
      << "\" text-anchor=\"middle\" fill=\"#565f89\">each cell is a 4×8 tile: brightness picks the fill, edges pick | _ / \\</text>\n";
    o << "</svg>\n";
    return save(dir + "/shader.svg", o.str());
}

// --- map.svg: Pumping Station from above, with the route a bot takes -------

bool writeMap(const std::string& dir) {
    auto map = load("pumping-station.map");
    if (!map) return false;
    // Rotate so the level lies wide: screen x = map y, screen y = map x.
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (auto& s : map->sectors)
        for (Vec2 v : s.verts) minX = std::min(minX, v.x), maxX = std::max(maxX, v.x), minY = std::min(minY, v.y), maxY = std::max(maxY, v.y);
    const float scale = 13.0f, left = 24, top = 48;
    auto P = [&](Vec2 v) { return Vec2{left + (v.y - minY) * scale, top + (maxX - v.x) * scale}; };
    float w = left * 2 + (maxY - minY) * scale, h = top + (maxX - minX) * scale + 56;

    std::ostringstream o;
    std::string css =
        ".lift { animation: lift 2.4s ease-in-out infinite alternate; }\n"
        "@keyframes lift { from { fill: #3b4261; } to { fill: #7aa2f7; } }\n"
        ".key { animation: glow 1.2s ease-in-out infinite alternate; }\n"
        "@keyframes glow { from { opacity: 0.55; } to { opacity: 1; } }\n"
        "@media (prefers-reduced-motion: reduce) { * { animation: none !important; } }\n";
    o << header(w, h, "Top-down plan of the Pumping Station level with doors, a lift, two keycards and the route to the exit", css);
    o << windowFrame(w, h, "assets/maps/pumping-station.map");
    for (auto& s : map->sectors) {
        std::string fill = "#24283b";
        float k = std::clamp((s.mover == Mover::Lift ? s.topFloor : s.floor) / 3.0f, 0.0f, 1.0f);
        int base = 36 + static_cast<int>(k * 28);
        char buf[16];
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x", base, base + 4, base + 22);
        fill = buf;
        std::string cls;
        if (s.exit) fill = "#9ece6a";
        if (s.mover == Mover::Door) fill = s.key == KeyColor::Red ? "#f7768e" : s.key == KeyColor::Blue ? "#7aa2f7" : "#e0af68";
        if (s.mover == Mover::Lift) cls = " class=\"lift\"";
        o << "<polygon" << cls << " points=\"";
        for (Vec2 v : s.verts) {
            Vec2 p = P(v);
            o << p.x << "," << p.y << " ";
        }
        o << "\" fill=\"" << fill << "\" stroke=\"#16161e\" stroke-width=\"1\"/>\n";
    }
    // Solid walls drawn on top for a crisp outline.
    o << "<g stroke=\"#565f89\" stroke-width=\"1.4\" stroke-linecap=\"round\">\n";
    for (auto& s : map->sectors)
        for (size_t e = 0; e < s.edgeCount(); ++e)
            if (s.neighbor[e] < 0) {
                Vec2 a = P(s.a(e)), b = P(s.b(e));
                o << "<line x1=\"" << a.x << "\" y1=\"" << a.y << "\" x2=\"" << b.x << "\" y2=\"" << b.y << "\"/>";
            }
    o << "\n</g>\n";
    for (const Thing& t : map->things) {
        Vec2 p = P(t.pos);
        const char* col = nullptr;
        float r = 2.6f;
        std::string cls;
        switch (t.type) {
            case ThingType::Grunt: col = "#f7768e"; break;
            case ThingType::Brute: col = "#f7768e", r = 4.2f; break;
            case ThingType::RedKey: col = "#ff5f7e", r = 4.0f, cls = " class=\"key\""; break;
            case ThingType::BlueKey: col = "#7dcfff", r = 4.0f, cls = " class=\"key\""; break;
            case ThingType::Medkit:
            case ThingType::Ammo:
            case ThingType::Armor: col = "#a9b1d6", r = 1.8f; break;
            default: break;
        }
        if (col) o << "<circle" << cls << " cx=\"" << p.x << "\" cy=\"" << p.y << "\" r=\"" << r << "\" fill=\"" << col << "\"/>";
    }
    // The route from the beatable-level test, walked by a dot.
    const Vec2 route[] = {{2, 4},   {9.2f, 4}, {12, 4},   {18.5f, 9.2f}, {18.5f, 11}, {18.5f, 13}, {15, 16}, {11.2f, 15},
                          {8, 15},  {3, 19},   {9.2f, 15}, {12, 15},     {13, 11},    {15, -3.2f}, {15, -7}, {15, -11}};
    std::ostringstream d;
    for (size_t i = 0; i < std::size(route); ++i) {
        Vec2 p = P(route[i]);
        d << (i ? " L" : "M") << p.x << " " << p.y;
    }
    o << "\n<path d=\"" << d.str() << "\" fill=\"none\" stroke=\"#9ece6a\" stroke-width=\"1.2\" stroke-dasharray=\"3 4\" opacity=\"0.55\"/>\n";
    o << "<circle r=\"4.5\" fill=\"#9ece6a\" stroke=\"#16161e\" stroke-width=\"1.5\">"
         "<animateMotion dur=\"14s\" repeatCount=\"indefinite\" calcMode=\"paced\" path=\""
      << d.str() << "\"/></circle>\n";
    // Legend.
    struct Item {
        const char* color;
        const char* label;
    } items[] = {{"#9ece6a", "you / exit"}, {"#e0af68", "door"}, {"#f7768e", "red door"}, {"#7aa2f7", "blue door, lift"}, {"#f7768e", "enemies"}};
    float lx = left, ly = h - 22;
    for (auto& it : items) {
        o << "<circle cx=\"" << lx + 4 << "\" cy=\"" << ly - 4 << "\" r=\"4\" fill=\"" << it.color << "\"/>";
        o << "<text class=\"sans\" x=\"" << lx + 12 << "\" y=\"" << ly << "\" fill=\"#8089b3\">" << it.label << "</text>";
        lx += 22 + std::strlen(it.label) * 6.2f;
    }
    o << "\n</svg>\n";
    return save(dir + "/map.svg", o.str());
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "docs/readme";
    bool ok = writePlay(dir) && writeShader(dir) && writeMap(dir);
    return ok ? 0 : 1;
}
