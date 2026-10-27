// Copied from spotify-pokemon-deck src/net/http_config.h @ 9ced38d.
#pragma once
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../util/netup.h"
// Shared HTTP(S) settings for every request the deck makes (Spotify, LRCLIB, Apple, art,
// walk sheets). Inline where possible: no extra RAM or stack at the call sites.
namespace netcfg {
// TLS handshake limit. The 120 s default froze polls on a half-dead hotspot.
constexpr unsigned long TLS_HANDSHAKE_S = 8;
// HTTPClient connect/read timeout (ms).
constexpr uint16_t HTTP_TIMEOUT_MS = 8000;
// Our own body-reading loops give up when no byte has arrived for this long (ms).
constexpr uint32_t STALL_MS = 8000;
constexpr const char* USER_AGENT = "CarDeck/1.0 (ESP32)";
// Image bodies (art, walk sheets, sprites): whole-body limit. The stall clock restarts on every
// byte, so without it a trickling server could hold the network task indefinitely.
constexpr uint32_t IMAGE_DEADLINE_MS = 20000;

// WiFi is up. Every request checks it first: with the link gone, a connect would still sit
// out its timeouts (connect + handshake + read), several times per walker, and the network
// task (which also reconnects WiFi) froze for 2 minutes on 2026-10-13.
// "Up" also needs an address: the driver can say connected with 0.0.0.0 (util/netup).
inline bool linkUp() { return netup::usable(WiFi.status() == WL_CONNECTED, (uint32_t)WiFi.localIP()); }

// No certificate validation, bounded handshake: for public data only (lyrics, genres,
// sprites), where a full chain check isn't worth the RAM. See SECURITY.md.
inline void secure(WiFiClientSecure& c) {
    c.setInsecure();
    c.setHandshakeTimeout(TLS_HANDSHAKE_S);
}
// Spotify requests carry the client secret and tokens: the server must chain to one of the
// bundled roots (src/net/ca_bundle.inc, tools/gen_ca_bundle.py). Bounded handshake.
void spotify(WiFiClientSecure& c);
// True, and logged once per call, when c's last connect failed because the server's
// certificate didn't chain to a bundled root (Spotify changed CA: regenerate the bundle).
bool certRejected(WiFiClientSecure& c, const char* host);
// Plain (non-chunked, HTTP/1.0) body read straight from the stream, bounded waits.
inline void streamed(HTTPClient& h) {
    h.useHTTP10(true);
    h.setTimeout(HTTP_TIMEOUT_MS);
}
}
