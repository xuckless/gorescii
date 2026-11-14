#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <string>
#include <unistd.h>

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

render::Camera startCamera(const Map& map) {
    const Thing* p = map.playerStart();
    render::Camera cam;
    cam.pos = p->pos;
    cam.angle = p->angle;
    cam.sector = map.sectorAt(cam.pos);
    cam.z = map.sectors[static_cast<size_t>(cam.sector)].floor + 1.6f;
    return cam;
}

int runSnapshot(const Map& map, const Options& o) {
    render::Camera cam = startCamera(map);
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
    render::Camera cam = startCamera(map);
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

        // Free-fly viewer: no collision yet.
        float speed = 4.0f * static_cast<float>(dt), turn = 2.2f * static_cast<float>(dt);
        using term::Key, term::keyChar;
        if (keys.held(Key::Left)) cam.angle += turn;
        if (keys.held(Key::Right)) cam.angle -= turn;
        Vec2 fwd{std::cos(cam.angle), std::sin(cam.angle)}, right{fwd.y, -fwd.x};
        Vec2 move{};
        if (keys.held(keyChar('w')) || keys.held(Key::Up)) move += fwd;
        if (keys.held(keyChar('s')) || keys.held(Key::Down)) move -= fwd;
        if (keys.held(keyChar('d'))) move += right;
        if (keys.held(keyChar('a'))) move -= right;
        Vec2 next = cam.pos + normalize(move) * speed;
        int ns = map.sectorAt(next, cam.sector);
        if (ns >= 0) {
            cam.pos = next;
            cam.sector = ns;
            cam.z = map.sectors[static_cast<size_t>(ns)].floor + 1.6f;
        }

        if (tty.consumeResize()) presenter.invalidate();
        term::Size sz = tty.size();
        view.draw(map, cam, sz.cols, sz.rows, o.cellW, sz.cellAspect());
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
