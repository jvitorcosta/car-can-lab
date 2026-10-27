#pragma once
// Copy to src/config.h and fill in. src/config.h is git-ignored. Same keys as the deck repo's
// config.h (spotify-pokemon-deck include/config.example.h): on the yellow deck, use the same values
// as the LED panel, so either firmware can update the other over WiFi.

// Up to 3 WiFi networks tried in order. The phone hotspot goes FIRST: the deck pins
// 172.20.10.11 on it (util/hotspotip). Enable "Maximize Compatibility" on the iPhone (2.4 GHz).
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_NETWORKS[] = {
    {"YOUR_IPHONE_HOTSPOT", "YOUR_HOTSPOT_PASSWORD"},
};
static const int WIFI_NETWORK_COUNT = 1;

// Your time zone as a POSIX TZ string (night mode runs 18:00-06:00 local time), e.g. "<-03>3".
#define TIMEZONE              "UTC0"

// The key for downloading recordings (GET /recs?k=..., /rec?n=..&k=...). Any long random string;
// empty turns WiFi and the download server off.
#define REMOTE_TOKEN          ""

// Firmware updates over WiFi: a different random string, never REMOTE_TOKEN; tools/ota_auth.py
// reads it for `pio run -e sniff_ota -t upload`. Must match the LED panel's to swap back and forth.
#define OTA_PASSWORD          ""
