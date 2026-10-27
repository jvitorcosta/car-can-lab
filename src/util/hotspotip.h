// Copied from spotify-pokemon-deck src/util/hotspotip.h @ 9ced38d.
#pragma once
#include <cstdint>
// A fixed address on the iPhone Personal Hotspot, so a Home Screen bookmark never breaks:
// the hotspot is always 172.20.10.0/28 with the iPhone at .1, and hands out .2-.14 by DHCP.
// Addresses are host order (a << 24 | b << 16 | c << 8 | d). PURE, host-tested.
namespace hotspotip {
// True on an iPhone hotspot (172.20.10.0/28, gateway .1).
bool onHotspot(uint32_t ip, uint32_t gateway, uint32_t mask);
// The address to switch to (172.20.10.<lastOctet>), or 0 to keep the DHCP one: not on an
// iPhone hotspot, already there, or lastOctet outside .2-.14.
uint32_t wanted(uint32_t ip, uint32_t gateway, uint32_t mask, uint8_t lastOctet);
}
