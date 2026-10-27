#include "sweep.h"
#include <cstdio>

namespace sweep {
static constexpr uint16_t ALL = 0x7FF;

void Plan::add(uint16_t filter, uint16_t mask) {
    if (n_ < MAX_WINDOWS) w_[n_++] = Window{static_cast<uint16_t>(filter & mask), mask};
}

void Plan::scan() {
    n_ = cur_ = 0;
    wrapped_ = false;
    for (uint16_t f = 0; f <= ALL; f += START_SIZE) add(f, static_cast<uint16_t>(ALL & ~(START_SIZE - 1)));
}

void Plan::only(const uint16_t* ids, int n) {
    if (!ids || n <= 0) {
        scan();
        return;
    }
    n_ = cur_ = 0;
    wrapped_ = false;
    // which IDs are wanted, and which are already in a window
    static bool want[ALL + 1], done[ALL + 1];
    for (int i = 0; i <= ALL; i++) want[i] = done[i] = false;
    for (int i = 0; i < n; i++) want[ids[i] & ALL] = true;
    for (int id = 0; id <= ALL; id++) {
        if (!want[id] || done[id]) continue;
        uint16_t size = 1;
        for (uint16_t b = 8; b >= 2; b /= 2) {
            const int base = id & ~(b - 1);
            int k = 0;
            for (int j = base; j < base + b; j++) k += want[j] && !done[j];
            if (k >= 2 && 2 * k >= b) {
                size = b;
                break;
            }
        }
        const int base = id & ~(size - 1);
        for (int j = base; j < base + size; j++) done[j] = true;
        add(static_cast<uint16_t>(base), static_cast<uint16_t>(ALL & ~(size - 1)));
    }
}

void Plan::next() {
    if (n_ == 0) return;
    cur_ = (cur_ + 1) % n_;
    wrapped_ = cur_ == 0;
}

void Plan::split() {
    if (n_ == 0 || n_ >= MAX_WINDOWS) return;
    const Window w = w_[cur_];
    if (w.mask == ALL) return;  // one ID: MIN_SIZE
    const uint16_t size = static_cast<uint16_t>((~w.mask & ALL) + 1);
    const uint16_t half = size / 2;
    const uint16_t mask = static_cast<uint16_t>(w.mask | half);
    for (int i = n_; i > cur_ + 1; i--) w_[i] = w_[i - 1];
    w_[cur_] = Window{w.filter, mask};
    w_[cur_ + 1] = Window{static_cast<uint16_t>(w.filter | half), mask};
    n_++;
}

void commands(const Window& w, char* cf, char* cm) {
    snprintf(cf, 9, "ATCF%03X\r", static_cast<unsigned>(w.filter & ALL));
    snprintf(cm, 9, "ATCM%03X\r", static_cast<unsigned>(w.mask & ALL));
}
}
