#pragma once
// Copy to src/config.h and fill in. src/config.h is git-ignored.

// Up to 3 WiFi networks tried in order, for downloading recordings and updates over WiFi. The LOGS
// page shows the address. On an iPhone hotspot the board pins 172.20.10.11 (util/hotspotip) and
// needs "Maximize Compatibility" on (2.4 GHz). Leave REMOTE_TOKEN empty to skip WiFi altogether.
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_NETWORKS[] = {
    {"YOUR_IPHONE_HOTSPOT", "YOUR_HOTSPOT_PASSWORD"},
};
static const int WIFI_NETWORK_COUNT = 1;

// Your time zone as a POSIX TZ string (night mode runs 18:00-06:00 local time), e.g. "<-03>3".
#define TIMEZONE              "UTC0"

// The key for downloading recordings (GET /recs?k=..., /rec?n=..&k=...). Any long random string;
// empty turns WiFi off, and with it the download server and updates over WiFi: use USB then.
#define REMOTE_TOKEN          ""

// Firmware updates over WiFi: a different random string, never REMOTE_TOKEN; tools/ota_auth.py
// reads it for `pio run -e sniff_ota -t upload`.
#define OTA_PASSWORD          ""

// Optional: a reminder on the home page, for a board that normally runs something else.
// #define SWAP_HINT          "LIGHTS: FLASH LEDPANEL BACK"
