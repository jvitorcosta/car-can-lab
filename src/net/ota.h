// Copied from spotify-pokemon-deck src/net/ota.h @ 9ced38d.
#pragma once
#include "../util/otaview.h"
// Firmware updates over WiFi, pushed from the PC, on both decks (board-neutral; each app hands in
// its own hooks):
//   pio run -e s3_ota -t upload          the black deck (main.cpp)
//   pio run -e ledpanel_ota -t upload    the yellow deck's LED panel (ledpanel.cpp)
// ArduinoOTA on port 3232, password OTA_PASSWORD (its own, not the remote's REMOTE_TOKEN); off when
// OTA_PASSWORD is empty. Never pulled from the internet: the firmware carries the secrets (config.h).
// The update writes the other app slot and switches only once its SHA-256 checks out; the
// boot-loop guard (util/bootguard) goes back to the previous slot when new firmware keeps crashing.
namespace ota {
// Early in setup(): counts a crash reset and, after bootguard::LIMIT of them on firmware not yet
// proven, boots the other slot ("[ota] rolled back").
void checkBoot();
// Starts the listener task; it listens once `hold` is false (the S3's boot scene; nullptr: never
// held) and WiFi is up. `name`: ArduinoOTA's hostname (its mDNS stays off: each app already answers
// to its own .local name). `onStart` runs on the listener task as an upload begins (nullptr:
// nothing; neither deck needs one, their clips end on their own); everything else pauses by
// polling active().
void begin(const char* name, bool (*hold)() = nullptr, void (*onStart)() = nullptr);
// The update screen's state (util/otaview): Receiving with its percent, Done (the deck reboots
// otaview::DONE_MS later, after the screen's flash), Failed (held otaview::FAILED_HOLD_MS), Idle.
struct View { otaview::Phase phase; int pct; };   // pct -1 when Idle
View view();
bool active();     // Receiving or Done: network work pauses (Failed resumes it at once)
// Updates are on: OTA_PASSWORD is set. Off, begin() starts no listener and listening() stays false.
bool enabled();
// The listener is up (ArduinoOTA.begin() has run): the deck can take the next update. With updates
// on, the LED panel counts its firmware as proven only then, so a good firmware is always one that
// listens; with them off, being online is enough (or no build could ever prove itself).
bool listening();
// The listener task's least stack to spare so far (bytes), for a [mem] line; 0 before begin().
unsigned stackLeft();
int progress();    // 0-100 while active, else -1
// After each sign the app works (the S3: a good Spotify poll; the LED panel: a check while online
// and listening()): once the deck has run bootguard::HEALTHY_MS the running firmware counts as
// good and the crash count clears.
void onGoodPoll();
}
