#pragma once
// Decodes terminal input bytes (legacy VT sequences and the kitty keyboard
// protocol) into key events, and tracks which keys are held.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tg::term {

enum class Key : uint16_t {
    None = 0,
    // 1..127 map directly to ASCII (letters are always lowercase).
    Escape = 27,
    Space = 32,
    Enter = 13,
    Tab = 9,
    Backspace = 127,
    Up = 256,
    Down,
    Left,
    Right,
    Shift,
    Ctrl,
    Alt,
    FocusIn,   // terminal window gained / lost focus (mode 1004)
    FocusOut,
    Count
};

constexpr Key keyChar(char c) { return static_cast<Key>(static_cast<unsigned char>(c)); }

enum class KeyAction : uint8_t { Press, Repeat, Release };

struct KeyEvent {
    Key key = Key::None;
    KeyAction action = KeyAction::Press;
    bool shift = false;
    bool ctrl = false;
};

// SGR mouse report (modes 1006 / 1016). Coordinates are 1-based cells, or
// pixels when the terminal is in SGR-pixel mode.
struct MouseEvent {
    int x = 0, y = 0;
    int button = -1;  // 0 left, 1 middle, 2 right, -1 none (pure motion)
    bool press = false, release = false, motion = false;
    int wheel = 0;    // +1 up, -1 down
};

// Parses as much of `buf` as forms complete events and erases the consumed
// bytes. A trailing lone ESC is treated as the Escape key. Mouse reports go
// to `mouse` when given, and are dropped otherwise.
std::vector<KeyEvent> decode(std::string& buf, std::vector<MouseEvent>* mouse = nullptr);

class MouseState {
public:
    void beginFrame();
    // `width`/`height`: the coordinate space of the reports (cells or pixels).
    void apply(const MouseEvent& e, double now, int width, int height);
    void releaseAll();

    bool left() const { return left_; }
    bool right() const { return right_; }
    bool rightPressed() const { return rightPressed_; }
    int wheel() const { return wheel_; }
    // Horizontal motion since beginFrame, as a fraction of the window width.
    float dx() const { return dx_; }
    // Last pointer position as 0..1 fractions; valid when hasPosition().
    bool hasPosition() const { return hasPos_; }
    float x() const { return x_; }
    float y() const { return y_; }
    double lastEventTime() const { return last_; }

private:
    bool left_ = false, right_ = false, rightPressed_ = false, hasPos_ = false;
    int wheel_ = 0;
    float dx_ = 0, x_ = 0.5f, y_ = 0.5f;
    double last_ = -1e9;
};

class KeyState {
public:
    // `releaseEvents`: the terminal reports key-up. Otherwise keys count as
    // held until key-repeat stops arriving.
    explicit KeyState(bool releaseEvents = false) : releaseEvents_(releaseEvents) {}
    void setReleaseEvents(bool v) { releaseEvents_ = v; }

    void beginFrame(double nowSeconds);
    void apply(const KeyEvent& e, double nowSeconds);

    bool held(Key k) const { return slot(k).held; }
    // Pressed since the previous beginFrame().
    bool pressed(Key k) const { return slot(k).pressedThisFrame; }
    // Lets go of everything (e.g. on focus loss, when releases can be missed).
    void releaseAll();

private:
    struct Slot {
        bool held = false;
        bool pressedThisFrame = false;
        bool repeating = false;
        double lastSeen = 0;
    };
    const Slot& slot(Key k) const { return slots_[static_cast<size_t>(k) % slots_.size()]; }
    Slot& slot(Key k) { return slots_[static_cast<size_t>(k) % slots_.size()]; }

    std::array<Slot, static_cast<size_t>(Key::Count)> slots_{};
    bool releaseEvents_;
};

}  // namespace tg::term
