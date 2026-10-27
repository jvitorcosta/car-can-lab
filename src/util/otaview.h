// Copied from spotify-pokemon-deck src/util/otaview.h @ 9ced38d.
#pragma once
#include <cstdint>
// The firmware-update screen's state over time and the rail's geometry (spec
// docs/superpowers/specs/2026-10-22-shadow-update-screen-design.md). net/ota feeds the Tracker
// from ArduinoOTA's callbacks; the UI reads phase() and pct(). PURE, host-tested.
//   Receiving: an upload is coming in (pct 0..100)
//   Done:      it ended well; the deck reboots DONE_MS later (the screen's red flash)
//   Failed:    an error, or handle() returned mid-upload; shown FAILED_HOLD_MS, then Idle
namespace otaview {
enum class Phase : uint8_t { Idle, Receiving, Done, Failed };
constexpr uint32_t DONE_MS = 600, FAILED_HOLD_MS = 3000;

class Tracker {
public:
    // Ignored once Done: a second upload invited in the reboot window can't cancel the good one.
    void onStart() { if (phase_ != Phase::Done) { phase_ = Phase::Receiving; pct_ = 0; } }
    void onProgress(int pct) { if (phase_ == Phase::Receiving) pct_ = pct; }
    void onEnd(uint32_t nowMs) { phase_ = Phase::Done; pct_ = 100; at_ = nowMs; }
    void onError(uint32_t nowMs) { if (phase_ == Phase::Receiving) fail(nowMs); }
    // After each ArduinoOTA.handle(): an upload still Receiving was cut off (no onError).
    void onHandleReturned(uint32_t nowMs) { if (phase_ == Phase::Receiving) fail(nowMs); }
    Phase phase(uint32_t nowMs) const {   // wrap-safe
        return phase_ == Phase::Failed && nowMs - at_ >= FAILED_HOLD_MS ? Phase::Idle : phase_;
    }
    int pct() const { return pct_; }
    bool rebootDue(uint32_t nowMs) const { return phase_ == Phase::Done && nowMs - at_ >= DONE_MS; }
private:
    void fail(uint32_t nowMs) { phase_ = Phase::Failed; at_ = nowMs; }
    Phase phase_ = Phase::Idle;
    int pct_ = 0;
    uint32_t at_ = 0;
};

// The rail (the progress bar): a BORDER-px frame around the track, the fill inside it.
constexpr int RAIL_X = 16, RAIL_Y = 200, RAIL_W = 288, RAIL_H = 10, BORDER = 2;
constexpr int INNER_X = RAIL_X + BORDER, INNER_W = RAIL_W - 2 * BORDER;   // 18, 284

// The fill's width for a percent, clamped to 0..INNER_W.
inline int fillWidth(int pct) {
    if (pct <= 0) return 0;
    if (pct >= 100) return INNER_W;
    return INNER_W * pct / 100;
}

// What one frame repaints: the strip and the rail on a change of phase or percent (or full); the
// jets also on a bob step, and never once the update failed (he stops). A bob alone leaves the
// rail alone, or the fill would flicker 8 times a second.
struct Repaint { bool strip, rail, jets; };
inline Repaint repaint(bool full, bool changed, bool bobStep, Phase p) {
    const bool all = full || changed;
    return {all, all, (all || bobStep) && p == Phase::Receiving};
}

// Where a rail repaint starts: at the rail when Shadow is drawn (his own rows reach down to it and
// are composed with him; repainting them bare first made his feet blink), else 14 rows above it,
// where the jet glows sit.
inline int railRepaintTop(bool riderDrawn) { return riderDrawn ? RAIL_Y : RAIL_Y - 14; }

// Shadow hovers on his jet shoes: his box's top for a frame h px high, HOVER px over the rail.
constexpr int HOVER = 6;
inline int riderTop(int h) { return RAIL_Y - HOVER - h; }

// The rider's left edge: his centre on the fill's tip, kept on the rail.
inline int riderX(int pct, int riderW) {
    const int x = INNER_X + fillWidth(pct) - riderW / 2;
    const int lo = RAIL_X, hi = RAIL_X + RAIL_W - riderW;
    return x < lo ? lo : x > hi ? hi : x;
}
}
