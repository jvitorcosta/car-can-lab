// Copied from spotify-pokemon-deck src/net/ipaddr.h @ 9ced38d.
#pragma once
#include <IPAddress.h>
#include <cstdint>
// An IPAddress as a host-order number, for the pure address logic in util/hotspotip.
inline uint32_t hostOrder(const IPAddress& a) {
    return (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3];
}
