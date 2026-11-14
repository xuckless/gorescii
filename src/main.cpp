#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <string>
#include <vector>
#include <unistd.h>

#include "game/game.h"
#include "gfx/ascii.h"
#include "gfx/present.h"
#include "render/renderer.h"
#include "term/input.h"
#include "ui/hud.h"
#include "term/terminal.h"
#include "version.h"
#include "world/map.h"

namespace {

using namespace tg;

struct Options {
    std::string mapPath = std::string(TEXTGME_ASSET_DIR) + "/maps/outpost.map";
    int cellW = 4;
    bool mouse = true;
    gfx::EdgeStyle style = gfx::EdgeStyle::Lines;
    float mouseSens = 1.0f;
    bool snapshot = false;
    int snapCols = 120, snapRows = 40;
    bool snapAt = false;
    float atX = 0, atY = 0, atAngle = 0;
};

void usage() {
    std::puts(
        "usage: textgme [--map FILE] [--cell-width N] [--mouse-sens X] [--no-mouse]\n"
        "               [--style lines|clean|off]\n"
        "               [--snapshot COLSxROWS] [--at X,Y,DEG]\n"
        "  --snapshot  render one frame as plain text to stdout and exit");
}

bool parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (a == "--map") {
            const char* v = next();
            if (!v) return false;
            o.mapPath = v;
        } else if (a == "--cell-width") {
            const char* v = next();
            if (!v) return false;
            o.cellW = std::clamp(std::atoi(v), 1, 8);
        } else if (a == "--style") {
            const char* v = next();
            if (!v) return false;
            std::string sv = v;
            if (sv == "lines") o.style = gfx::EdgeStyle::Lines;
            else if (sv == "clean") o.style = gfx::EdgeStyle::Clean;
            else if (sv == "off") o.style = gfx::EdgeStyle::Off;
            else return false;
        } else if (a == "--no-mouse") {
            o.mouse = false;
        } else if (a == "--mouse-sens") {
            const char* v = next();
            if (!v) return false;
            o.mouseSens = std::clamp(static_cast<float>(std::atof(v)), 0.05f, 10.0f);
        } else if (a == "--snapshot") {
            const char* v = next();
            if (!v || std::sscanf(v, "%dx%d", &o.snapCols, &o.snapRows) != 2) return false;
            o.snapshot = true;
        } else if (a == "--at") {
            const char* v = next();
            if (!v || std::sscanf(v, "%f,%f,%f", &o.atX, &o.atY, &o.atAngle) != 3) return false;
            o.snapAt = true;
        } else if (a == "--version") {
            std::printf("textgme %s\n", version());
            std::exit(0);
        } else {
            return false;
        }
    }
    return true;
}

struct View {
    gfx::Framebuffer fb;
    gfx::CellGrid scene, grid;
    gfx::AsciiShader shader;
    render::Renderer renderer;

    std::vector<render::SpriteInstance> sprites;

    void draw(const game::Game& g, int cols, int rows, int cellW, double cellAspect, const ui::HudInfo& hud) {
        int cellH = std::max(1, static_cast<int>(std::lround(cellW * cellAspect)));
        int sceneRows = std::max(1, rows - ui::kStatusRows);
        if (grid.cols != cols || grid.rows != rows) grid.resize(cols, rows);
        if (scene.cols != cols || scene.rows != sceneRows) scene.resize(cols, sceneRows);
        if (fb.width != cols * cellW || fb.height != sceneRows * cellH) fb.resize(cols * cellW, sceneRows * cellH);
        renderer.render(g.map(), g.camera(), fb);
        g.collectSprites(sprites);
        renderer.drawSprites(sprites, fb);
        game::WeaponView wv = g.weaponView();
        float px = fb.height / 48.0f;
        renderer.drawOverlay(render::sprite(wv.sprite), fb.width * (0.5f + wv.bobX), fb.height * (1.0f + wv.bobY) + px,
                             px, wv.flash ? 1.3f : 0.9f, fb);
        ui::flashFrame(fb, g);
        shader.apply(fb, cellW, cellH, scene);
        std::copy(scene.cells.begin(), scene.cells.end(), grid.cells.begin());
        ui::drawHud(grid, g, hud);
    }

    // C cycles colour / green phosphor / amber; V toggles edges; B background.
    std::string cycleColor() {
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
    gfx::EdgeStyle style = gfx::EdgeStyle::Lines;
    void setStyle(gfx::EdgeStyle st) {
        style = st;
        gfx::applyEdgeStyle(shader.settings, st);
    }
    // V cycles lines -> clean -> off.
    std::string cycleEdges() {
        setStyle(style == gfx::EdgeStyle::Lines   ? gfx::EdgeStyle::Clean
                 : style == gfx::EdgeStyle::Clean ? gfx::EdgeStyle::Off
                                                  : gfx::EdgeStyle::Lines);
        return std::string("Edges: ") + gfx::edgeStyleName(style);
    }
    std::string toggleBackground() {
        auto& b = shader.settings.backgroundBlend;
        b = b > 0 ? 0.0f : 0.3f;
        return b > 0 ? "Background fill on" : "Background fill off";
    }
};

// Mouse look: horizontal motion turns (a full window width = half a turn at
// sensitivity 1). Terminals cannot capture the pointer, so resting it in the
// outer edge zone keeps turning, until no mouse event has arrived for a while
// (the pointer probably left the window).
void applyMouse(game::Controls& c, const term::MouseState& m, double now, float sens) {
    constexpr float kEdge = 0.05f;
    constexpr double kEdgeTimeout = 2.0;
    c.look -= m.dx() * 3.14159265f * sens;
    if (m.hasPosition() && now - m.lastEventTime() < kEdgeTimeout) {
        if (m.x() < kEdge) c.turn += (kEdge - m.x()) / kEdge * sens;
        if (m.x() > 1 - kEdge) c.turn -= (m.x() - (1 - kEdge)) / kEdge * sens;
        c.turn = std::clamp(c.turn, -1.5f, 1.5f);
    }
    c.fire = c.fire || m.left();
    c.nextWeapon = c.nextWeapon || m.rightPressed() || m.wheel() != 0;
}

game::Controls readControls(const term::KeyState& k) {
    using term::Key, term::keyChar;
    auto axis = [&](bool pos, bool neg) { return (pos ? 1.0f : 0.0f) - (neg ? 1.0f : 0.0f); };
    game::Controls c;
    c.forward = axis(k.held(keyChar('w')) || k.held(Key::Up), k.held(keyChar('s')) || k.held(Key::Down));
    c.strafe = axis(k.held(keyChar('d')), k.held(keyChar('a')));
    c.turn = axis(k.held(Key::Left) || k.held(keyChar('j')), k.held(Key::Right) || k.held(keyChar('l')));
    c.run = !k.held(Key::Shift);  // always run; hold Shift to walk (needs key-release support)
    c.fire = k.held(Key::Space) || k.held(Key::Ctrl);
    c.use = k.pressed(keyChar('e'));
    if (k.pressed(keyChar('1'))) c.weaponSlot = 1;
    if (k.pressed(keyChar('2'))) c.weaponSlot = 2;
    c.nextWeapon = k.pressed(keyChar('q')) || k.pressed(Key::Tab);
    return c;
}

int runSnapshot(const Map& map, const Options& o) {
    game::Game g(map);
    if (o.snapAt) {
        Map moved = map;
        for (Thing& t : moved.things) {
            if (t.type != ThingType::Player) continue;
            t.pos = {o.atX, o.atY};
            t.angle = o.atAngle * 3.14159265f / 180.0f;
        }
        if (moved.sectorAt({o.atX, o.atY}) < 0) {
            std::fprintf(stderr, "--at position is outside the map\n");
            return 1;
        }
        g = game::Game(moved);
    }
    View view;
    view.setStyle(o.style);
    view.draw(g, o.snapCols, o.snapRows, o.cellW, 2.0, ui::HudInfo{});
    for (int y = 0; y < view.grid.rows; ++y) {
        std::string line;
        for (int x = 0; x < view.grid.cols; ++x) gfx::appendUtf8(line, view.grid.at(x, y).glyph);
        std::puts(line.c_str());
    }
    return 0;
}

int runInteractive(const Map& map, const Options& o) {
    term::Terminal tty;
    if (!tty.enter(o.mouse)) {
        std::fprintf(stderr, "textgme needs an interactive terminal (try --snapshot)\n");
        return 1;
    }
    term::KeyState keys(tty.hasKeyReleaseEvents());
    term::MouseState mouse;
    std::vector<term::MouseEvent> mouseEvents;
    gfx::Presenter presenter;
    View view;
    view.setStyle(o.style);
    game::Game g(map);
    std::string inbuf;
    bool paused = false;
    ui::HudInfo hud;
    double toastUntil = 0, helpUntil = 8.0;
    auto toast = [&](std::string t, double now, double secs = 2.0) {
        hud.toast = std::move(t);
        toastUntil = now + secs;
    };
    if (!tty.hasKeyReleaseEvents())
        toast("Tip: this terminal has no key-release events; movement follows key repeat. Try Ghostty/kitty/WezTerm.", 0, 8);

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    double last = 0;
    const double frameTime = 1.0 / 30.0;
    for (;;) {
        double now = std::chrono::duration<double>(clock::now() - t0).count();
        double dt = std::min(0.1, now - last);
        last = now;

        keys.beginFrame(now);
        mouse.beginFrame();
        tty.readAvailable(inbuf);
        bool quit = false;
        mouseEvents.clear();
        for (const auto& e : term::decode(inbuf, &mouseEvents)) {
            if (e.ctrl && e.key == term::keyChar('c')) quit = true;
            if (e.action == term::KeyAction::Press && g.state() == game::State::Playing) {
                if (e.key == term::Key::Escape) paused = !paused;
                if (paused && e.key == term::keyChar('x')) quit = true;
            }
            if (e.action == term::KeyAction::Press && !e.ctrl) {
                if (e.key == term::keyChar('c')) toast(view.cycleColor(), now);
                if (e.key == term::keyChar('v')) toast(view.cycleEdges(), now);
                if (e.key == term::keyChar('b')) toast(view.toggleBackground(), now);
            }
            if (e.key == term::Key::FocusOut) {
                keys.releaseAll();
                mouse.releaseAll();
                continue;
            }
            keys.apply(e, now);
        }
        if (quit) break;
        term::Size sz = tty.size();
        for (const auto& me : mouseEvents) {
            int mw = tty.mousePixels() ? sz.pixelW : sz.cols, mh = tty.mousePixels() ? sz.pixelH : sz.rows;
            mouse.apply(me, now, mw, mh);
        }

        if (g.state() != game::State::Playing && keys.pressed(term::Key::Enter)) {
            g = game::Game(map, static_cast<uint32_t>(now * 1000));
            helpUntil = now + 4.0;
        }
        if (!paused) {
            game::Controls controls = readControls(keys);
            if (o.mouse) applyMouse(controls, mouse, now, o.mouseSens);
            g.update(controls, static_cast<float>(dt));
        }
        if (now > toastUntil) hud.toast.clear();
        hud.paused = paused;
        hud.showHelp = now < helpUntil;

        if (tty.consumeResize()) presenter.invalidate();
        view.draw(g, sz.cols, sz.rows, o.cellW, sz.cellAspect(), hud);
        tty.write(presenter.frame(view.grid));

        double spent = std::chrono::duration<double>(clock::now() - t0).count() - now;
        int waitMs = static_cast<int>((frameTime - spent) * 1000);
        if (waitMs > 0) {
            pollfd p{STDIN_FILENO, POLLIN, 0};
            poll(&p, 1, waitMs);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options opts;
    if (!parseArgs(argc, argv, opts)) {
        usage();
        return 2;
    }
    MapError err;
    auto map = loadMap(opts.mapPath, err);
    if (!map) {
        std::fprintf(stderr, "%s:%d: %s\n", opts.mapPath.c_str(), err.line, err.message.c_str());
        return 1;
    }
    return opts.snapshot ? runSnapshot(*map, opts) : runInteractive(*map, opts);
}
