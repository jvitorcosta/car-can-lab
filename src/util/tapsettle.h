#pragma once
#include <cstdint>
// One tap per press, where the finger settled: a resistive panel's first readings jump around, so a
// press is averaged over its first SETTLE_MS (at least MIN_READS readings) and acts once; a tap
// shorter than that acts on release with what it had. PURE, host-tested.
namespace tapsettle {
constexpr uint32_t SETTLE_MS = 40;
constexpr int MIN_READS = 3;
class Settle {
public:
    // The panel's reading this pass (x, y in screen pixels while down); true once per press, with
    // the settled point in *outX, *outY.
    bool update(bool down, int x, int y, uint32_t nowMs, int* outX, int* outY) {
        if (down) {
            if (!down_) {
                down_ = true;
                acted_ = false;
                t0_ = nowMs;
                sx_ = sy_ = 0;
                n_ = 0;
            }
            if (acted_) return false;
            sx_ += x;
            sy_ += y;
            n_++;
            if (nowMs - t0_ < SETTLE_MS || n_ < MIN_READS) return false;
            return act(outX, outY);
        }
        const bool quick = down_ && !acted_ && n_ > 0;
        down_ = false;
        return quick && act(outX, outY);
    }
private:
    bool act(int* outX, int* outY) {
        acted_ = true;
        *outX = static_cast<int>(sx_ / n_);
        *outY = static_cast<int>(sy_ / n_);
        return true;
    }
    bool down_ = false, acted_ = false;
    uint32_t t0_ = 0;
    long sx_ = 0, sy_ = 0;
    int n_ = 0;
};
}
