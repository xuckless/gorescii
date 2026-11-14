#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

#include "gfx/framebuffer.h"

namespace tg::gfx {

struct Cell {
    char32_t glyph = U' ';
    Rgb fg{200, 200, 200};
    Rgb bg{0, 0, 0};
    friend bool operator==(const Cell&, const Cell&) = default;
};

// One character per terminal cell; what the presenter sends to the tty.
struct CellGrid {
    int cols = 0, rows = 0;
    std::vector<Cell> cells;

    void resize(int c, int r) {
        cols = c;
        rows = r;
        cells.assign(static_cast<size_t>(c) * r, Cell{});
    }
    Cell& at(int x, int y) { return cells[static_cast<size_t>(y) * cols + x]; }
    const Cell& at(int x, int y) const { return cells[static_cast<size_t>(y) * cols + x]; }

    // Writes ASCII text, clipped to the grid. A negative bg alpha keeps the existing bg.
    void text(int x, int y, std::string_view s, Rgb fg, const Rgb* bg = nullptr) {
        if (y < 0 || y >= rows) return;
        for (char ch : s) {
            if (x >= 0 && x < cols) {
                Cell& c = at(x, y);
                c.glyph = static_cast<unsigned char>(ch);
                c.fg = fg;
                if (bg) c.bg = *bg;
            }
            ++x;
        }
    }
};

}  // namespace tg::gfx
