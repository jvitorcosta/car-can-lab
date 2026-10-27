// Copied from spotify-pokemon-deck src/util/roam.h @ 9ced38d.
#pragma once
// Which network to try how often (config.h order: the iPhone hotspot first). A deck stays on the
// network it joined (owner, 2026-10-14: the home WiFi is for development; in the car there is
// only the hotspot), so all that's left is: a short hotspot blip gets two tries. PURE, host-tested.
namespace roam {
// Connect tries for network `index` after a drop from `lastIndex` (-1 = boot): two for the
// preferred network the deck was just on, else one.
inline int tries(int index, int lastIndex) { return (index == 0 && lastIndex == 0) ? 2 : 1; }
}
