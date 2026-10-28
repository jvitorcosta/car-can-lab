// Copied from spotify-pokemon-deck src/util/yellow/tapguard.h @ 9ced38d.
#pragma once
#include <cstdint>
// Keeps a tap from carrying over a screen change: armed when the screen (or the Lights page)
// changes, it reads the panel as released until the finger has lifted and the panel has read
// released for QUIET_MS, so the second tap of a double tap, or a resistive panel's bounce, can't
// land on the button that sits under it on the new screen. PURE, host-tested.
namespace tapguard {
constexpr uint32_t QUIET_MS = 250;
class Guard {
public:
    void arm() {
        armed_ = true;
        up_ = false;
    }
    bool armed() const { return armed_; }
    // The panel's reading; returns whether to treat it as pressed.
    bool update(bool down, uint32_t nowMs) {
        if (!armed_) return down;
        if (down) {
            up_ = false;
            return false;
        }
        if (!up_) {
            up_ = true;
            upAt_ = nowMs;
        }
        if (nowMs - upAt_ >= QUIET_MS) armed_ = false;
        return false;
    }

private:
    bool armed_ = false, up_ = false;
    uint32_t upAt_ = 0;  // when the panel first read released since it last read pressed (or the arming)
};
}
