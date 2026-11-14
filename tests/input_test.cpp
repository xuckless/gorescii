#include "check.h"
#include "term/input.h"

#include <cmath>

using namespace tg::term;

TEST(decode_legacy_chars_and_arrows) {
    std::string b = "wA\x1b[D\x03";
    auto ev = decode(b);
    CHECK(ev.size() == 4);
    CHECK(ev[0].key == keyChar('w'));
    CHECK(ev[1].key == keyChar('a') && ev[1].shift);
    CHECK(ev[2].key == Key::Left);
    CHECK(ev[3].key == keyChar('c') && ev[3].ctrl);
    CHECK(b.empty());
}

TEST(decode_kitty_press_repeat_release) {
    std::string b = "\x1b[119u\x1b[119;1:2u\x1b[119;1:3u\x1b[1;1:3A\x1b[57441;2u";
    auto ev = decode(b);
    CHECK(ev.size() == 5);
    CHECK(ev[0].key == keyChar('w') && ev[0].action == KeyAction::Press);
    CHECK(ev[1].action == KeyAction::Repeat);
    CHECK(ev[2].action == KeyAction::Release);
    CHECK(ev[3].key == Key::Up && ev[3].action == KeyAction::Release);
    CHECK(ev[4].key == Key::Shift);
}

TEST(decode_keeps_incomplete_sequence) {
    std::string b = "\x1b[11";
    auto ev = decode(b);
    CHECK(ev.empty());
    CHECK(b == "\x1b[11");
    b += "9u";
    ev = decode(b);
    CHECK(ev.size() == 1 && ev[0].key == keyChar('w'));
}

TEST(decode_ignores_terminal_replies) {
    std::string b = "\x1b[?11u\x1b[?62;22c";
    CHECK(decode(b).empty());
}

TEST(keystate_legacy_hold_expires) {
    KeyState ks(false);
    ks.beginFrame(0.0);
    ks.apply({keyChar('w')}, 0.0);
    CHECK(ks.pressed(keyChar('w')) && ks.held(keyChar('w')));
    ks.beginFrame(0.3);
    CHECK(ks.held(keyChar('w')) && !ks.pressed(keyChar('w')));
    ks.beginFrame(1.0);
    CHECK(!ks.held(keyChar('w')));
}

TEST(keystate_release_events) {
    KeyState ks(true);
    ks.beginFrame(0.0);
    ks.apply({keyChar('w')}, 0.0);
    ks.beginFrame(5.0);
    CHECK(ks.held(keyChar('w')));
    ks.apply({keyChar('w'), KeyAction::Release}, 5.0);
    CHECK(!ks.held(keyChar('w')));
}

TEST(decode_sgr_mouse) {
    std::string b = "\x1b[<35;10;5M\x1b[<0;12;5M\x1b[<0;12;5m\x1b[<65;1;1M\x1b[<2;3;4Mw";
    std::vector<MouseEvent> mouse;
    auto keys = decode(b, &mouse);
    CHECK(keys.size() == 1 && keys[0].key == keyChar('w'));
    CHECK(mouse.size() == 5);
    if (mouse.size() != 5) return;
    CHECK(mouse[0].motion && mouse[0].button == -1 && mouse[0].x == 10 && mouse[0].y == 5);
    CHECK(mouse[1].press && mouse[1].button == 0);
    CHECK(mouse[2].release && mouse[2].button == 0);
    CHECK(mouse[3].wheel == -1);
    CHECK(mouse[4].press && mouse[4].button == 2);
}

TEST(decode_focus_and_decrpm_reply) {
    std::string b = "\x1b[O\x1b[?1016;2$y\x1b[I";
    auto ev = decode(b);
    CHECK(ev.size() == 2);
    if (ev.size() == 2) CHECK(ev[0].key == Key::FocusOut && ev[1].key == Key::FocusIn);
}

TEST(mouse_state_tracks_motion_and_buttons) {
    MouseState m;
    m.beginFrame();
    m.apply({51, 10, -1, false, false, true, 0}, 0, 101, 20);
    CHECK(m.dx() == 0);  // first report only sets the position
    m.beginFrame();
    m.apply({61, 10, -1, false, false, true, 0}, 0.1, 101, 20);
    m.apply({0 + 71, 10, 0, true, false, false, 0}, 0.1, 101, 20);
    CHECK(std::abs(m.dx() - 0.2f) < 1e-4f);
    CHECK(m.left());
    m.apply({71, 10, 0, false, true, false, 0}, 0.2, 101, 20);
    CHECK(!m.left());
    m.apply({71, 10, 2, true, false, false, 0}, 0.2, 101, 20);
    CHECK(m.rightPressed() && m.right());
}
