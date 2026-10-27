// Copied from spotify-pokemon-deck src/util/obdble.cpp @ df11e4a (branch feat/obd).
#include "obdble.h"
#include <initializer_list>

namespace obdble {
namespace {
// The first notify and the first write characteristic of service s; true when it has both.
bool inService(const Char* c, int n, uint16_t s, Pick* out) {
    Pick p;
    for (int i = 0; i < n; ++i) {
        if (c[i].service != s) continue;
        if (p.notify < 0 && (c[i].notify || c[i].indicate)) p.notify = i;
        if (p.write < 0 && (c[i].write || c[i].writeNoResp)) p.write = i;
    }
    if (p.notify < 0 || p.write < 0) return false;
    *out = p;
    return true;
}
}

Pick pick(const Char* chars, int n) {
    Pick p;
    if (!chars || n <= 0) return p;
    for (uint16_t s : {(uint16_t)0xFFF0, (uint16_t)0xFFE0, (uint16_t)0x18F0})
        if (inService(chars, n, s, &p)) return p;
    for (int i = 0; i < n; ++i)
        if (inService(chars, n, chars[i].service, &p)) return p;
    return Pick{};
}

bool serialService(uint16_t uuid) { return uuid == 0xFFF0 || uuid == 0xFFE0 || uuid == 0x18F0; }

void Inbox::push(const uint8_t* data, size_t len) {
    if (len == 0) return;
    if (cut_ && count_ < CAP) {  // the first bytes after a cut: marked
        buf_[(head_ + count_) % CAP] = CUT;
        ++count_;
        cut_ = false;
    }
    size_t i = 0;
    for (; i < len && count_ < CAP; ++i, ++count_) buf_[(head_ + count_) % CAP] = (char)data[i];
    if (i < len) cut_ = true;
}

bool Inbox::pop(char* c) {
    if (count_ == 0) return false;
    *c = buf_[head_];
    head_ = (head_ + 1) % CAP;
    --count_;
    return true;
}
}
