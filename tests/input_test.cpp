#include "check.h"
#include "term/input.h"

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
