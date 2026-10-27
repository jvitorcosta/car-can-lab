// Copied from spotify-pokemon-deck src/util/remotegate.h @ 9ced38d.
#pragma once
#include <cstdint>
// Who may play a clip from the phone remote (net/remote), and when. Pure.
//   wrong or missing key -> BadKey (403; checked first, so a stranger learns nothing else)
//   a clip is playing    -> Busy (409; queued clips would each hold a task stack on the CYD)
//   < MIN_GAP_MS since the last accepted play -> TooSoon (429)
// No key configured (REMOTE_TOKEN empty) turns the remote off: every request is BadKey.
namespace remotegate {
constexpr uint32_t MIN_GAP_MS = 3000;
enum class Result { Play, BadKey, Busy, TooSoon };

class Gate {
public:
    explicit Gate(const char* key) : key_(key) {}
    bool enabled() const { return key_ && key_[0]; }
    // Key check only (GET /state): never counts as a play. Same constant-time compare.
    bool keyOk(const char* key) const;
    Result request(const char* key, bool clipPlaying, uint32_t nowMs);
private:
    const char* key_;
    bool played_ = false;
    uint32_t lastPlayMs_ = 0;
};
}
