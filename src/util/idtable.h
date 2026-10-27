#pragma once
#include <cstdint>
#include "elmmon.h"
// Screen A's live list: every CAN ID heard, its latest bytes, when each byte last changed (for
// the fading highlight) and whether the owner picked it for FILTER. Up to MAX_IDS; more distinct
// IDs are counted, not stored (the header shows "+N"). PURE, host-tested.
namespace idtable {
struct Row {
    uint16_t id;
    uint8_t len;
    uint8_t data[8];
    uint32_t seenMs, changedMs;  // changedMs: the first sighting counts as a change (new rows go on top)
    uint32_t byteMs[8];          // when each byte last changed; 0: never since the first sighting
    bool picked;
};
// changed: bit i = byte i differs from the last frame (a length change marks all eight).
struct Update {
    bool isNew;
    uint8_t changed;
};

class IdTable {
public:
    static constexpr int MAX_IDS = 96;
    static constexpr uint32_t IDLE_MS = 5000, FADE_MS = 1000;
    Update update(const elmmon::Frame& f, uint32_t nowMs);
    // Up to max IDs, the most recent changedMs first, then the most recent seenMs.
    int order(uint16_t* ids, int max) const;
    const Row* find(uint16_t id) const;
    int size() const { return n_; }
    // Distinct IDs refused because the table was full.
    uint32_t dropped() const { return dropped_; }
    void togglePick(uint16_t id);
    int picked(uint16_t* ids, int max) const;
    void clearPicks();
    // No change for IDLE_MS.
    static bool idle(const Row& r, uint32_t nowMs);
    // A changed byte's highlight: 255 at the change, down to 0 at FADE_MS and after.
    static uint8_t fade(const Row& r, int byte, uint32_t nowMs);
private:
    Row* findMut(uint16_t id);
    Row rows_[MAX_IDS] = {};
    int n_ = 0;
    uint32_t dropped_ = 0;
    uint8_t droppedSeen_[0x800 / 8] = {};
};
}
