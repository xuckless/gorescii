#include "check.h"
#include "gfx/ascii.h"
#include "gfx/present.h"

using namespace tg::gfx;

TEST(direction_buckets) {
    CHECK(AsciiShader::directionBucket(1, 0) == 0);    // left-right change -> |
    CHECK(AsciiShader::directionBucket(-1, 0) == 0);
    CHECK(AsciiShader::directionBucket(0, 1) == 1);    // up-down change -> _
    CHECK(AsciiShader::directionBucket(1, 1) == 2);    // brighter down-right -> /
    CHECK(AsciiShader::directionBucket(-1, 1) == 3);   // brighter down-left -> backslash
    CHECK(AsciiShader::directionBucket(-1, -1) == 2);
    CHECK(AsciiShader::directionBucket(0, 0) == -1);
}

TEST(flat_image_uses_fill_ramp) {
    Framebuffer fb;
    fb.resize(16, 16);
    fb.clear({255, 255, 255});
    CellGrid g;
    g.resize(4, 2);
    AsciiShader sh;
    sh.apply(fb, 4, 8, g);
    for (auto& c : g.cells) CHECK(c.glyph == sh.settings.ramp.back());
    fb.clear({0, 0, 0});
    sh.apply(fb, 4, 8, g);
    for (auto& c : g.cells) CHECK(c.glyph == U' ');
}

TEST(vertical_boundary_gives_pipe) {
    Framebuffer fb;
    fb.resize(32, 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) fb.at(x, y) = x < 14 ? Rgb{40, 40, 40} : Rgb{230, 230, 230};
    CellGrid g;
    g.resize(8, 4);
    AsciiShader sh;
    sh.settings.depthEdges = false;
    sh.apply(fb, 4, 8, g);
    int pipes = 0;
    for (int y = 0; y < 4; ++y) pipes += g.at(3, y).glyph == U'|' || g.at(4, y).glyph == U'|';
    CHECK(pipes >= 3);
}

TEST(presenter_only_sends_changes) {
    CellGrid g;
    g.resize(10, 3);
    Presenter p;
    std::string first = p.frame(g);
    std::string second = p.frame(g);
    CHECK(first.size() > second.size());
    CHECK(second.find('H') == std::string::npos);  // no cursor moves: nothing changed
    g.at(5, 1).glyph = U'@';
    std::string third = p.frame(g);
    CHECK(third.find("\x1b[2;6H") != std::string::npos);
    CHECK(third.find('@') != std::string::npos);
}

TEST(monochrome_tint) {
    Framebuffer fb;
    fb.resize(4, 8);
    fb.clear({200, 40, 40});
    CellGrid g;
    g.resize(1, 1);
    AsciiShader sh;
    sh.settings.colorBlend = 0;
    sh.settings.tint = {60, 255, 90};
    sh.apply(fb, 4, 8, g);
    Rgb c = g.at(0, 0).fg;
    CHECK(c.g > c.r && c.g > c.b);
}
