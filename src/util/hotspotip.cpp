// Copied from spotify-pokemon-deck src/util/hotspotip.cpp @ 9ced38d.
#include "hotspotip.h"

namespace hotspotip {

static constexpr uint32_t NET = 0xAC140A00u;      // 172.20.10.0
static constexpr uint32_t GATEWAY = NET | 1;      // the iPhone
static constexpr uint32_t MASK = 0xFFFFFFF0u;     // /28

bool onHotspot(uint32_t ip, uint32_t gateway, uint32_t mask) {
    return gateway == GATEWAY && mask == MASK && (ip & MASK) == NET;
}

uint32_t wanted(uint32_t ip, uint32_t gateway, uint32_t mask, uint8_t lastOctet) {
    if (!onHotspot(ip, gateway, mask)) return 0;
    if (lastOctet < 2 || lastOctet > 14) return 0;
    const uint32_t want = NET | lastOctet;
    return ip == want ? 0 : want;
}

}
