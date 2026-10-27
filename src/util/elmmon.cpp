#include "elmmon.h"
#include <cstring>

namespace elmmon {
static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

Kind parse(const char* line, Frame* out) {
    if (!line || !line[0]) return Kind::Other;
    if (strcmp(line, ">") == 0) return Kind::Prompt;
    if (strstr(line, "BUFFER FULL")) return Kind::BufferFull;
    if (strstr(line, "STOPPED")) return Kind::Stopped;
    if (strstr(line, "ERROR") || strcmp(line, "?") == 0) return Kind::Error;

    // the hex digits, spaces dropped; anything else makes it not a frame
    char d[3 + 2 * 8 + 1];
    size_t n = 0;
    for (const char* p = line; *p; ++p) {
        if (*p == ' ') continue;
        if (hexVal(*p) < 0 || n + 1 >= sizeof d) return Kind::Other;
        d[n++] = *p;
    }
    if (n < 3 || (n - 3) % 2 != 0) return Kind::Other;
    const int id = hexVal(d[0]) << 8 | hexVal(d[1]) << 4 | hexVal(d[2]);
    if (id > 0x7FF) return Kind::Other;
    Frame f{};
    f.id = static_cast<uint16_t>(id);
    f.len = static_cast<uint8_t>((n - 3) / 2);
    for (int i = 0; i < f.len; ++i) f.data[i] = static_cast<uint8_t>(hexVal(d[3 + 2 * i]) << 4 | hexVal(d[4 + 2 * i]));
    *out = f;
    return Kind::Frame;
}

void Lines::finish() {
    if (cur_ > 0) {
        if (qn_ == QUEUE) {  // nobody is reading: the oldest line goes
            qhead_ = (qhead_ + 1) % QUEUE;
            --qn_;
        }
        char* slot = q_[(qhead_ + qn_) % QUEUE];
        memcpy(slot, line_, cur_);
        slot[cur_] = '\0';
        ++qn_;
    }
    cur_ = 0;
}

void Lines::push(const char* bytes, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const char c = bytes[i];
        if (c == '\r' || c == '\n') {
            if (!skipping_) finish();
            skipping_ = false;
            cur_ = 0;
            continue;
        }
        if (skipping_) continue;
        if (c == '>' && cur_ == 0) {  // the prompt: a line of its own
            line_[0] = '>';
            cur_ = 1;
            finish();
            continue;
        }
        line_[cur_++] = c;
        if (cur_ == CAP - 1) {  // over-long: keep what fits, drop the rest of it
            finish();
            skipping_ = true;
        }
    }
}

bool Lines::next(char* out, size_t outCap) {
    if (qn_ == 0 || outCap == 0) return false;
    const char* s = q_[qhead_];
    qhead_ = (qhead_ + 1) % QUEUE;
    --qn_;
    size_t k = strlen(s);
    if (k > outCap - 1) k = outCap - 1;
    memcpy(out, s, k);
    out[k] = '\0';
    return true;
}
}
