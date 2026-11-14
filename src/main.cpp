#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <string>
#include <unistd.h>

#include "game/game.h"
#include "gfx/ascii.h"
#include "gfx/present.h"
#include "render/renderer.h"
#include "term/input.h"
#include "term/terminal.h"
#include "version.h"
#include "world/map.h"

namespace {

using namespace tg;

struct Options {
    std::string mapPath = std::string(TEXTGME_ASSET_DIR) + "/maps/outpost.map";
    int cellW = 4;
    bool snapshot = false;
    int snapCols = 120, snapRows = 40;
    bool snapAt = false;
    float atX = 0, atY = 0, atAngle = 0;
};

void usage() {
    std::puts(
        "usage: textgme [--map FILE] [--cell-width N]\n"
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
    gfx::CellGrid grid;
    gfx::AsciiShader shader;
    render::Renderer renderer;

    void draw(const Map& map, const render::Camera& cam, int cols, int rows, int cellW, double cellAspect) {
        int cellH = std::max(1, static_cast<int>(std::lround(cellW * cellAspect)));
        if (grid.cols != cols || grid.rows != rows) grid.resize(cols, rows);
        if (fb.width != cols * cellW || fb.height != rows * cellH) fb.resize(cols * cellW, rows * cellH);
        renderer.render(map, cam, fb);
        shader.apply(fb, cellW, cellH, grid);
    }
};

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
    return c;
}

int runSnapshot(const Map& map, const Options& o) {
    game::Game g(map);
    render::Camera cam = g.camera();
    if (o.snapAt) {
        cam.pos = {o.atX, o.atY};
        cam.angle = o.atAngle * 3.14159265f / 180.0f;
        cam.sector = map.sectorAt(cam.pos);
        if (cam.sector < 0) {
            std::fprintf(stderr, "--at position is outside the map\n");
            return 1;
        }
        cam.z = map.sectors[static_cast<size_t>(cam.sector)].floor + 1.6f;
    }
    View view;
    view.draw(map, cam, o.snapCols, o.snapRows, o.cellW, 2.0);
    for (int y = 0; y < view.grid.rows; ++y) {
        std::string line;
        for (int x = 0; x < view.grid.cols; ++x) gfx::appendUtf8(line, view.grid.at(x, y).glyph);
        std::puts(line.c_str());
    }
    return 0;
}

int runInteractive(const Map& map, const Options& o) {
    term::Terminal tty;
    if (!tty.enter()) {
        std::fprintf(stderr, "textgme needs an interactive terminal (try --snapshot)\n");
        return 1;
    }
    term::KeyState keys(tty.hasKeyReleaseEvents());
    gfx::Presenter presenter;
    View view;
    game::Game g(map);
    std::string inbuf;

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    double last = 0;
    const double frameTime = 1.0 / 30.0;
    for (;;) {
        double now = std::chrono::duration<double>(clock::now() - t0).count();
        double dt = std::min(0.1, now - last);
        last = now;

        keys.beginFrame(now);
        tty.readAvailable(inbuf);
        bool quit = false;
        for (const auto& e : term::decode(inbuf)) {
            if (e.key == term::Key::Escape || e.key == term::keyChar('q') || (e.ctrl && e.key == term::keyChar('c')))
                quit = true;
            keys.apply(e, now);
        }
        if (quit) break;

        g.update(readControls(keys), static_cast<float>(dt));

        if (tty.consumeResize()) presenter.invalidate();
        term::Size sz = tty.size();
        view.draw(g.map(), g.camera(), sz.cols, sz.rows, o.cellW, sz.cellAspect());
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
