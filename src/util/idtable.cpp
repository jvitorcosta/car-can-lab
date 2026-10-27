#include "idtable.h"
#include <cstring>

namespace idtable {
Row* IdTable::findMut(uint16_t id) {
    for (int i = 0; i < n_; i++)
        if (rows_[i].id == id) return &rows_[i];
    return nullptr;
}

const Row* IdTable::find(uint16_t id) const {
    for (int i = 0; i < n_; i++)
        if (rows_[i].id == id) return &rows_[i];
    return nullptr;
}

Update IdTable::update(const elmmon::Frame& f, uint32_t nowMs) {
    const uint8_t len = f.len > 8 ? 8 : f.len;
    Row* r = findMut(f.id);
    if (!r) {
        if (n_ == MAX_IDS) {
            const uint16_t id = f.id & 0x7FF;
            uint8_t& bits = droppedSeen_[id / 8];
            if (!(bits & (1u << (id % 8)))) {
                bits = static_cast<uint8_t>(bits | 1u << (id % 8));
                dropped_++;
            }
            return Update{false, 0};
        }
        r = &rows_[n_++];
        *r = Row{};
        r->id = f.id;
        r->len = len;
        memcpy(r->data, f.data, len);
        r->seenMs = r->changedMs = nowMs;
        return Update{true, 0};
    }
    uint8_t changed = 0;
    if (len != r->len) {
        changed = 0xFF;
    } else {
        for (int i = 0; i < len; i++)
            if (r->data[i] != f.data[i]) changed = static_cast<uint8_t>(changed | 1u << i);
    }
    r->seenMs = nowMs;
    if (changed) {
        r->changedMs = nowMs;
        for (int i = 0; i < 8; i++)
            if (changed & (1u << i)) r->byteMs[i] = nowMs;
        r->len = len;
        memset(r->data, 0, sizeof r->data);
        memcpy(r->data, f.data, len);
    }
    return Update{false, changed};
}

int IdTable::order(uint16_t* ids, int max) const {
    // insertion sort of indices: 96 rows at most
    int idx[MAX_IDS];
    for (int i = 0; i < n_; i++) {
        int j = i;
        const Row& a = rows_[i];
        while (j > 0) {
            const Row& b = rows_[idx[j - 1]];
            const bool before = a.changedMs > b.changedMs || (a.changedMs == b.changedMs && a.seenMs > b.seenMs);
            if (!before) break;
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = i;
    }
    const int k = n_ < max ? n_ : max;
    for (int i = 0; i < k; i++) ids[i] = rows_[idx[i]].id;
    return k < 0 ? 0 : k;
}

void IdTable::togglePick(uint16_t id) {
    if (Row* r = findMut(id)) r->picked = !r->picked;
}

int IdTable::picked(uint16_t* ids, int max) const {
    int k = 0;
    for (int i = 0; i < n_ && k < max; i++)
        if (rows_[i].picked) ids[k++] = rows_[i].id;
    return k;
}

void IdTable::clearPicks() {
    for (int i = 0; i < n_; i++) rows_[i].picked = false;
}

bool IdTable::idle(const Row& r, uint32_t nowMs) { return nowMs - r.changedMs >= IDLE_MS; }

uint8_t IdTable::fade(const Row& r, int byte, uint32_t nowMs) {
    if (byte < 0 || byte > 7 || r.byteMs[byte] == 0) return 0;
    const uint32_t age = nowMs - r.byteMs[byte];
    if (age >= FADE_MS) return 0;
    return static_cast<uint8_t>(255u * (FADE_MS - age) / FADE_MS);
}
}
