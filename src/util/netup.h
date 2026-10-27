// Copied from spotify-pokemon-deck src/util/netup.h @ 9ced38d.
#pragma once
#include <cstdint>
// When the WiFi link can carry traffic: connected AND holding an address. The driver can report
// connected with 0.0.0.0 (seen after an auto-reconnect to the hotspot: 40 s of failed DNS), and
// treating that as offline lets net::loop() run a clean reconnect. PURE, host-tested.
namespace netup {
inline bool usable(bool connected, uint32_t ip) { return connected && ip != 0; }

// When the 5-s link check should reconnect: at once on a real drop; for "connected without an
// address" only on the second check in a row (one such reading can be a transient, and a
// reconnect costs seconds).
class Watch {
public:
    bool reconnect(bool connected, uint32_t ip) {
        if (!connected) {
            noAddr_ = false;
            return true;
        }
        if (ip != 0) {
            noAddr_ = false;
            return false;
        }
        if (!noAddr_) {
            noAddr_ = true;
            return false;
        }
        noAddr_ = false;
        return true;
    }

private:
    bool noAddr_ = false;
};
}
