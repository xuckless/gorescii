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

// Parses as much of `buf` as forms complete events and erases the consumed
// bytes. A trailing lone ESC is treated as the Escape key.
std::vector<KeyEvent> decode(std::string& buf);

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
