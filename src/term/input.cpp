#include "term/input.h"

#include <cctype>
#include <cstdio>

namespace tg::term {
namespace {

// Kitty functional key codes for modifiers.
constexpr int kKittyLeftShift = 57441, kKittyLeftCtrl = 57442, kKittyLeftAlt = 57443;
constexpr int kKittyRightShift = 57447, kKittyRightCtrl = 57448, kKittyRightAlt = 57449;

Key fromCodepoint(int cp) {
    switch (cp) {
        case 27: return Key::Escape;
        case 13: return Key::Enter;
        case 9: return Key::Tab;
        case 8:
        case 127: return Key::Backspace;
        case kKittyLeftShift:
        case kKittyRightShift: return Key::Shift;
        case kKittyLeftCtrl:
        case kKittyRightCtrl: return Key::Ctrl;
        case kKittyLeftAlt:
        case kKittyRightAlt: return Key::Alt;
        default: break;
    }
    if (cp >= 'A' && cp <= 'Z') return static_cast<Key>(cp - 'A' + 'a');
    if (cp > 32 && cp < 127) return static_cast<Key>(cp);
    if (cp == 32) return Key::Space;
    return Key::None;
}

Key fromCsiFinal(char f) {
    switch (f) {
        case 'A': return Key::Up;
        case 'B': return Key::Down;
        case 'C': return Key::Right;
        case 'D': return Key::Left;
        default: return Key::None;
    }
}

void applyMods(KeyEvent& e, int mods) {
    if (mods > 0) {
        int bits = mods - 1;
        e.shift = bits & 1;
        e.ctrl = bits & 4;
    }
}

// Parses "a:b;c:d;e" style CSI parameters. Returns field values, -1 if absent.
struct CsiParams {
    int key = -1, mods = -1, event = -1;
};

CsiParams parseParams(const std::string& s) {
    CsiParams p;
    int field = 0, sub = 0, val = -1;
    auto commit = [&] {
        if (field == 0 && sub == 0) p.key = val;
        if (field == 1 && sub == 0) p.mods = val;
        if (field == 1 && sub == 1) p.event = val;
        val = -1;
    };
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            val = (val < 0 ? 0 : val * 10) + (c - '0');
        } else if (c == ':') {
            commit();
            ++sub;
        } else if (c == ';') {
            commit();
            ++field;
            sub = 0;
        }
    }
    commit();
    return p;
}

KeyAction actionFor(int event) {
    if (event == 2) return KeyAction::Repeat;
    if (event == 3) return KeyAction::Release;
    return KeyAction::Press;
}

}  // namespace

std::vector<KeyEvent> decode(std::string& buf, std::vector<MouseEvent>* mouse) {
    std::vector<KeyEvent> out;
    size_t i = 0;
    while (i < buf.size()) {
        unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c != 0x1b) {
            KeyEvent e;
            if (c >= 1 && c <= 26 && c != 9 && c != 13 && c != 8) {
                e.key = static_cast<Key>('a' + c - 1);
                e.ctrl = true;
            } else {
                if (c >= 'A' && c <= 'Z') e.shift = true;
                e.key = fromCodepoint(c);
            }
            if (e.key != Key::None) out.push_back(e);
            ++i;
            continue;
        }
        // ESC
        if (i + 1 >= buf.size()) {
            out.push_back({Key::Escape});
            ++i;
            continue;
        }
        char next = buf[i + 1];
        if (next == '[' || next == 'O') {
            size_t j = i + 2;
            while (j < buf.size() && !(buf[j] >= 0x40 && buf[j] <= 0x7e)) ++j;
            if (j >= buf.size()) break;  // incomplete sequence, wait for more bytes
            std::string params = buf.substr(i + 2, j - (i + 2));
            char final = buf[j];
            i = j + 1;
            if (!params.empty() && params[0] == '<' && (final == 'M' || final == 'm')) {
                int b = -1, mx = 0, my = 0;
                if (std::sscanf(params.c_str() + 1, "%d;%d;%d", &b, &mx, &my) == 3 && mouse) {
                    MouseEvent me;
                    me.x = mx;
                    me.y = my;
                    if (b & 64) {
                        me.wheel = (b & 1) ? -1 : 1;
                    } else {
                        int btn = b & 3;
                        me.motion = (b & 32) != 0;
                        me.button = btn == 3 ? -1 : btn;
                        if (!me.motion) {
                            me.press = final == 'M';
                            me.release = final == 'm';
                        }
                    }
                    mouse->push_back(me);
                }
                continue;
            }
            if (!params.empty() && (params[0] == '?' || params[0] == '>' || params[0] == '<')) continue;  // replies
            if (params.empty() && (final == 'I' || final == 'O')) {
                out.push_back({final == 'I' ? Key::FocusIn : Key::FocusOut});
                continue;
            }
            CsiParams p = parseParams(params);
            KeyEvent e;
            if (final == 'u') {
                e.key = fromCodepoint(p.key);
            } else if (final == '~') {
                if (p.key == 3) e.key = Key::Backspace;
            } else {
                e.key = fromCsiFinal(final);
            }
            applyMods(e, p.mods);
            e.action = actionFor(p.event);
            if (e.key != Key::None) out.push_back(e);
            continue;
        }
        // Alt+key in legacy mode: report the key itself.
        KeyEvent e;
        e.key = fromCodepoint(static_cast<unsigned char>(next));
        if (e.key != Key::None) out.push_back(e);
        i += 2;
    }
    buf.erase(0, i);
    return out;
}

void KeyState::beginFrame(double now) {
    for (auto& s : slots_) {
        s.pressedThisFrame = false;
        if (!releaseEvents_ && s.held) {
            // OS key-repeat starts after ~250-500ms, then arrives every ~30-80ms.
            double timeout = s.repeating ? 0.12 : 0.55;
            if (now - s.lastSeen > timeout) {
                s.held = false;
                s.repeating = false;
            }
        }
    }
}

void KeyState::apply(const KeyEvent& e, double now) {
    Slot& s = slot(e.key);
    switch (e.action) {
        case KeyAction::Press:
            if (!releaseEvents_ && s.held) {
                s.repeating = true;  // legacy repeat looks like a fresh press
            } else {
                s.pressedThisFrame = true;
            }
            s.held = true;
            s.lastSeen = now;
            break;
        case KeyAction::Repeat:
            s.held = true;
            s.repeating = true;
            s.lastSeen = now;
            break;
        case KeyAction::Release:
            s.held = false;
            s.repeating = false;
            break;
    }
}

void KeyState::releaseAll() {
    for (auto& s : slots_) s = Slot{};
}

void MouseState::beginFrame() {
    dx_ = 0;
    wheel_ = 0;
    rightPressed_ = false;
}

void MouseState::apply(const MouseEvent& e, double now, int width, int height) {
    last_ = now;
    if (e.wheel) {
        wheel_ += e.wheel;
        return;
    }
    float nx = width > 1 ? static_cast<float>(e.x - 1) / static_cast<float>(width - 1) : 0.5f;
    float ny = height > 1 ? static_cast<float>(e.y - 1) / static_cast<float>(height - 1) : 0.5f;
    if (hasPos_) dx_ += nx - x_;
    x_ = nx;
    y_ = ny;
    hasPos_ = true;
    if (e.press || e.release) {
        bool down = e.press;
        if (e.button == 0) left_ = down;
        if (e.button == 2) {
            if (down && !right_) rightPressed_ = true;
            right_ = down;
        }
    } else if (e.motion && e.button < 0) {
        // Motion with no buttons: any release we missed has happened.
        left_ = right_ = false;
    }
}

void MouseState::releaseAll() {
    left_ = right_ = false;
    hasPos_ = false;
}

}  // namespace tg::term
