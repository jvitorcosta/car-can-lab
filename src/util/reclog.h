#pragma once
#include <cstdint>
#include "elmmon.h"
// A recording's lines (spec 2026-10-27 "The recording"), plain text, one event per line, ms since
// begin:
//   # car-can-lab rec <n> <when> adapter=<ATI> proto=<ATDPN> fw=<version>
//   F <ms> <id> <b0> ... <bn>     an ID's first sighting and each change of its data
//   M <ms> <n>                    the owner's MARK n
//   O <ms> <name> <value>         a value read the normal way (rpm, coolant)
//   S <ms> <text>                 a state change (window, filter, buffer full, adapter lost, end)
// IDs upper-case hex, 3 digits; bytes 2 digits; single spaces. A fast counter would fill the flash,
// so an ID writes at most one F line per MIN_GAP_MS: a change inside the gap is held, the latest
// value wins, and tick() writes it once the gap has passed. PURE, host-tested.
namespace reclog {
using Writer = void (*)(const char* line, void* ctx);  // a line without its newline

struct Header {
    int number;
    const char* when;
    const char* adapter;
    const char* proto;
    const char* fw;
};

class Rec {
public:
    static constexpr uint32_t MIN_GAP_MS = 250;
    static constexpr int TRACK = 128;  // IDs it can cap; more are counted (untracked) and not logged
    void begin(Writer w, void* ctx, const Header& h);
    void frame(const elmmon::Frame& f, uint32_t ms);
    void mark(int n, uint32_t ms);
    void obd(const char* name, float value, uint32_t ms);
    void state(const char* text, uint32_t ms);
    void tick(uint32_t ms);
    // Writes the held values, then "S <ms> end"; nothing is written after it.
    void end(uint32_t ms);
    bool open() const { return w_ != nullptr; }
    uint32_t untracked() const { return untracked_; }
private:
    struct Slot {
        uint16_t id;
        uint8_t len, heldLen;
        bool held;
        uint8_t data[8], heldData[8];
        uint32_t lineMs;
    };
    void writeFrame(uint16_t id, const uint8_t* data, uint8_t len, uint32_t ms);
    Writer w_ = nullptr;
    void* ctx_ = nullptr;
    Slot slots_[TRACK] = {};
    int n_ = 0;
    uint32_t untracked_ = 0;
};
}
