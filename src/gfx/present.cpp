#include "gfx/present.h"

#include <charconv>

namespace tg::gfx {
namespace {

void appendInt(std::string& out, int v) {
    char buf[12];
    auto [p, ec] = std::to_chars(buf, buf + sizeof buf, v);
    (void)ec;
    out.append(buf, p);
}

void appendColor(std::string& out, bool fg, Rgb c) {
    out += fg ? "\x1b[38;2;" : "\x1b[48;2;";
    appendInt(out, c.r);
    out += ';';
    appendInt(out, c.g);
    out += ';';
    appendInt(out, c.b);
    out += 'm';
}

}  // namespace

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

Rgb Presenter::quantize(Rgb c) const {
    int drop = 8 - colorBits;
    if (drop <= 0) return c;
    auto q = [&](uint8_t v) {
        int base = (v >> drop) << drop;
        return static_cast<uint8_t>(base | (base >> colorBits));  // keep full range
    };
    return {q(c.r), q(c.g), q(c.b)};
}

const std::string& Presenter::frame(const CellGrid& next) {
    out_.clear();
    bool full = prev_.cols != next.cols || prev_.rows != next.rows;
    if (full) {
        out_ += "\x1b[0m\x1b[2J";
        prev_.resize(next.cols, next.rows);
    }
    out_ += "\x1b[?2026h";  // synchronized update: terminals that support it avoid tearing

    int curX = -1, curY = -1;
    bool haveColor = false;
    Rgb curFg{}, curBg{};
    for (int y = 0; y < next.rows; ++y) {
        for (int x = 0; x < next.cols; ++x) {
            Cell c = next.at(x, y);
            c.fg = quantize(c.fg);
            c.bg = quantize(c.bg);
            Cell& p = prev_.at(x, y);
            if (!full && c == p) continue;
            p = c;
            if (curY != y || curX != x) {
                out_ += "\x1b[";
                appendInt(out_, y + 1);
                out_ += ';';
                appendInt(out_, x + 1);
                out_ += 'H';
            }
            if (!haveColor || c.fg != curFg) appendColor(out_, true, c.fg);
            if (!haveColor || c.bg != curBg) appendColor(out_, false, c.bg);
            haveColor = true;
            curFg = c.fg;
            curBg = c.bg;
            appendUtf8(out_, c.glyph);
            curX = x + 1;
            curY = y;
        }
    }
    out_ += "\x1b[?2026l";
    return out_;
}

}  // namespace tg::gfx
