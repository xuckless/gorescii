#pragma once
// Turns a CellGrid into the minimal ANSI byte stream that updates the screen
// from the previously presented frame.
#include <string>

#include "gfx/cells.h"

namespace tg::gfx {

class Presenter {
public:
    // Colours are rounded to this many bits per channel before comparison and
    // output; fewer bits = fewer changed cells and bytes per frame.
    int colorBits = 6;

    // Forces a full redraw next frame (after resize or a screen clear).
    void invalidate() { prev_.resize(0, 0); }
    // Returns the bytes to write for this frame.
    const std::string& frame(const CellGrid& next);

private:
    Rgb quantize(Rgb c) const;
    CellGrid prev_;
    std::string out_;
};

void appendUtf8(std::string& out, char32_t cp);

}  // namespace tg::gfx
