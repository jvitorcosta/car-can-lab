// Copied from spotify-pokemon-deck src/net/remote_config.h @ 9ced38d.
#pragma once
#include "../config.h"
// Phone-remote and two-deck settings, with the defaults an older config.h leaves out. Shared by
// the new deck (net/remote, net/wifi) and the companion (companion/link, companion/phone).

#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""   // older config.h: remote off
#endif

// The password for firmware updates over WiFi (net/ota), apart from REMOTE_TOKEN: that one travels
// in plain-HTTP URLs, this one never leaves the PC but for espota's challenge (tools/ota_auth.py).
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""   // older config.h: updates over WiFi off
#endif

// The deck's name on the local network (http://cardeck.local/); the companion looks it up off the
// hotspot. Each board's own name: http://<name>.local/ (mDNS / Bonjour), different so both can be
// on at once. REMOTE_NAME renames the deck.
#define DECK_MDNS_NAME "cardeck"
#define COMPANION_MDNS_NAME "cardeck-companion"
#ifndef REMOTE_NAME
#define REMOTE_NAME DECK_MDNS_NAME
#endif

// Fixed last octet on the iPhone hotspot (util/hotspotip): 172.20.10.10 for the S3, .11 for the
// companion, so Home Screen bookmarks keep working. The companion finds the S3 at COMPANION_DECK.
#define DECK_HOTSPOT_OCTET 10
#define COMPANION_HOTSPOT_OCTET 11
#ifndef COMPANION_DECK
#define COMPANION_DECK "172.20.10.10"   // the S3's fixed address on the iPhone hotspot
#endif
