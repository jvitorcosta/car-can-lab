// Copied from spotify-pokemon-deck src/util/touchguard.h @ 9ced38d.
#pragma once
#include <cstdint>
// Ignores a resistive touch panel that reports one press that never moves (a damaged layer or a
// frame pressing an edge): the same raw point (within SLACK) for STUCK_MS reads as released
// until the panel really reads released. PURE, host-tested.
namespace touchguard {
constexpr uint32_t STUCK_MS = 5000;
constexpr int SLACK = 60;
class Guard {
public:
    // The panel's reading; returns whether to treat it as pressed.
    bool update(bool down, int rawX, int rawY, uint32_t nowMs) {
        if (!down) {
            stuck_ = false;
            on_ = false;
            return false;
        }
        if (stuck_) return false;
        const bool same = on_ && rawX - x_ <= SLACK && x_ - rawX <= SLACK && rawY - y_ <= SLACK &&
                          y_ - rawY <= SLACK;
        if (!same) {
            on_ = true;
            x_ = rawX;
            y_ = rawY;
            t0_ = nowMs;
            return true;
        }
        if (nowMs - t0_ >= STUCK_MS) {
            stuck_ = true;
            return false;
        }
        return true;
    }

private:
    bool on_ = false, stuck_ = false;
    int x_ = 0, y_ = 0;
    uint32_t t0_ = 0;
};
}
