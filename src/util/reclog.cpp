#include "reclog.h"
#include <cstdio>
#include <cstring>

namespace reclog {
static bool same(const uint8_t* a, uint8_t alen, const uint8_t* b, uint8_t blen) {
    return alen == blen && memcmp(a, b, alen) == 0;
}

void Rec::begin(Writer w, void* ctx, const Header& h) {
    w_ = w;
    ctx_ = ctx;
    n_ = 0;
    untracked_ = 0;
    char line[160];
    snprintf(line, sizeof line, "# car-can-lab rec %d %s adapter=%s proto=%s fw=%s", h.number, h.when ? h.when : "-",
             h.adapter ? h.adapter : "-", h.proto ? h.proto : "-", h.fw ? h.fw : "-");
    w_(line, ctx_);
}

void Rec::writeFrame(uint16_t id, const uint8_t* data, uint8_t len, uint32_t ms) {
    char line[8 + 10 + 4 + 3 * 8 + 1];
    int k = snprintf(line, sizeof line, "F %lu %03X", static_cast<unsigned long>(ms), static_cast<unsigned>(id & 0x7FF));
    for (int i = 0; i < len && i < 8 && k > 0 && static_cast<size_t>(k) < sizeof line; i++)
        k += snprintf(line + k, sizeof line - k, " %02X", data[i]);
    w_(line, ctx_);
}

void Rec::frame(const elmmon::Frame& f, uint32_t ms) {
    if (!w_) return;
    const uint8_t len = f.len > 8 ? 8 : f.len;
    Slot* s = nullptr;
    for (int i = 0; i < n_ && !s; i++)
        if (slots_[i].id == f.id) s = &slots_[i];
    if (!s) {
        if (n_ == TRACK) {
            untracked_++;
            return;
        }
        s = &slots_[n_++];
        *s = Slot{};
        s->id = f.id;
        s->len = len;
        memcpy(s->data, f.data, len);
        s->lineMs = ms;
        writeFrame(f.id, f.data, len, ms);
        return;
    }
    const uint8_t* latest = s->held ? s->heldData : s->data;
    const uint8_t latestLen = s->held ? s->heldLen : s->len;
    if (same(f.data, len, latest, latestLen)) return;
    if (ms - s->lineMs >= MIN_GAP_MS) {
        s->held = false;
        s->len = len;
        memcpy(s->data, f.data, len);
        s->lineMs = ms;
        writeFrame(f.id, f.data, len, ms);
        return;
    }
    s->held = true;
    s->heldLen = len;
    memcpy(s->heldData, f.data, len);
}

void Rec::tick(uint32_t ms) {
    if (!w_) return;
    for (int i = 0; i < n_; i++) {
        Slot& s = slots_[i];
        if (!s.held || ms - s.lineMs < MIN_GAP_MS) continue;
        s.held = false;
        if (same(s.heldData, s.heldLen, s.data, s.len)) continue;  // back where it was: no change
        s.len = s.heldLen;
        memcpy(s.data, s.heldData, s.len);
        s.lineMs = ms;
        writeFrame(s.id, s.data, s.len, ms);
    }
}

void Rec::mark(int n, uint32_t ms) {
    if (!w_) return;
    char line[32];
    snprintf(line, sizeof line, "M %lu %d", static_cast<unsigned long>(ms), n);
    w_(line, ctx_);
}

void Rec::obd(const char* name, float value, uint32_t ms) {
    if (!w_) return;
    char line[64];
    snprintf(line, sizeof line, "O %lu %s %.0f", static_cast<unsigned long>(ms), name, static_cast<double>(value));
    w_(line, ctx_);
}

void Rec::state(const char* text, uint32_t ms) {
    if (!w_) return;
    char line[96];
    snprintf(line, sizeof line, "S %lu %s", static_cast<unsigned long>(ms), text);
    w_(line, ctx_);
}

void Rec::end(uint32_t ms) {
    if (!w_) return;
    for (int i = 0; i < n_; i++) {
        Slot& s = slots_[i];
        if (!s.held) continue;
        s.held = false;
        if (!same(s.heldData, s.heldLen, s.data, s.len)) writeFrame(s.id, s.heldData, s.heldLen, ms);
    }
    state("end", ms);
    w_ = nullptr;
}
}
