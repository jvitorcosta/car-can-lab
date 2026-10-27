// Copied from spotify-pokemon-deck src/util/netdiag.h @ 9ced38d.
#pragma once
// What a failed Spotify poll means, from what the deck can check on the spot: is WiFi up, does
// the phone/router answer a ping, does api.spotify.com resolve, and did Spotify answer at all
// (code > 0 is an HTTP status). Logged with the facts as "[diag] ..." so a "No internet" screen
// can be traced to the link that broke. PURE, host-tested.
namespace netdiag {
inline const char* verdict(bool wifiUp, bool gatewayReplies, bool dnsOk, int code) {
    if (code > 0) return "Spotify answered with an error";
    if (!wifiUp) return "WiFi is down";
    if (dnsOk) return "internet slow or Spotify unreachable (DNS OK, request failed)";
    if (!gatewayReplies) return "WiFi link broken: the phone/router doesn't answer";
    return "the phone/router has no internet (DNS fails)";
}
}
