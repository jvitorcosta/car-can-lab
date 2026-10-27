// Copied from spotify-pokemon-deck src/net/wifi.h @ 9ced38d.
#pragma once
#include <Arduino.h>
namespace net {
bool connectAny();             // config.h order (the hotspot first; two tries after a drop from it)
// Move to the next saved network (falls back to connectAny if it isn't there). The companion
// uses it to follow the new deck onto its network.
bool switchNetwork();
bool isOnline();
void loop();
String deviceIp();
// Logs why a Spotify poll failed ("[diag] ..."): WiFi, signal, address, whether the phone/router
// answers a ping and api.spotify.com resolves, and what that means (util/netdiag). At most every
// 15 s; takes up to a few seconds, so only the network task calls it, after a failed poll.
void diagnose(int code, uint32_t sinceOkMs);
String ssid();                            // connected network, "" when offline
void savedNetworks(char* out, size_t n);  // "Home, iPhone" from config.h
}
