#pragma once
#include <cstdint>
// Which IDs the adapter passes on while it monitors. A busy 500 kbit/s bus carries ~1000 frames a
// second, more than ATMA can push over BLE, so the ELM's receive filter (ATCF filter, ATCM mask)
// opens one window of the 11-bit ID space at a time and the plan cycles through them. A window that
// overflows (BUFFER FULL) is split in two. PURE, host-tested.
namespace sweep {
// Passes the IDs where (id & mask) == filter.
struct Window {
    uint16_t filter, mask;
};

class Plan {
public:
    static constexpr uint16_t START_SIZE = 0x80, MIN_SIZE = 1;
    static constexpr int MAX_WINDOWS = 128;
    // The whole ID space, 0x000-0x7FF, in windows of START_SIZE.
    void scan();
    // Only these IDs (REC's frozen set, FILTER's picks). IDs in the same aligned block of 2, 4 or
    // 8 share a window when at least half that block is wanted, so few unwanted IDs come along;
    // the rest get a window each. None given: scan().
    void only(const uint16_t* ids, int n);
    Window current() const { return w_[cur_]; }
    void next();
    // The current window becomes its two halves, both kept in the cycle, current on the first.
    // Nothing at MIN_SIZE, or with MAX_WINDOWS already planned.
    bool split();  // false when it could not split (MIN_SIZE, or MAX_WINDOWS planned)
    int windows() const { return n_; }
    // True from the next() that went back to the first window until the next next().
    bool wrapped() const { return wrapped_; }
private:
    void add(uint16_t filter, uint16_t mask);
    Window w_[MAX_WINDOWS] = {};
    int n_ = 0, cur_ = 0;
    bool wrapped_ = false;
};

// "ATCF%03X\r" and "ATCM%03X\r" for w; each buffer at least 9 bytes.
void commands(const Window& w, char* cf, char* cm);
}
