// Copied from spotify-pokemon-deck src/util/remotegate.cpp @ 9ced38d.
#include "remotegate.h"
#include <cstring>

namespace remotegate {

// Compares every byte of the shorter-or-equal input regardless of where they differ.
static bool sameKey(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    unsigned char diff = la != lb;
    for (size_t i = 0; i < la && i < lb; ++i) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

bool Gate::keyOk(const char* key) const { return enabled() && key && sameKey(key, key_); }

Result Gate::request(const char* key, bool clipPlaying, uint32_t nowMs) {
    if (!keyOk(key)) return Result::BadKey;
    if (clipPlaying) return Result::Busy;
    if (played_ && nowMs - lastPlayMs_ < MIN_GAP_MS) return Result::TooSoon;   // unsigned: wrap-safe
    played_ = true;
    lastPlayMs_ = nowMs;
    return Result::Play;
}
}
