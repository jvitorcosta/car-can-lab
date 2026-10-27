#pragma once
#include <cstddef>
#include <cstdint>
// The BLE link to the ELM327 adapter (the Vgate iCar Pro BLE; the clone's "OBDBLE" side for a
// smoke test): scan, connect, pick the serial characteristics (util/obdble), subscribe. Ported from
// spotify-pokemon-deck feat/obd src/companion/obdlink.cpp (NimBLE 1.4) to NimBLE 2.5.1, next to
// the Xbox controller's link (lib/xboxpad), which inits NimBLE. Not thread-safe: one task uses it
// (sniff/monitor).
namespace sniff {
namespace link {
// Scans SCAN_MS for an adapter (an adapter-like name or a clone's serial service), connects and
// subscribes. Blocks for the scan and the connect.
bool tryConnect();
bool connected();
// The only way to the adapter: refuses (false, "[link] refused") anything obd::allowed rejects.
bool send(const char* cmd);
// The next whole line from the adapter (util/elmmon::Lines: '>' as a line of its own), waiting
// up to waitMs for one.
bool readLine(char* out, size_t cap, uint32_t waitMs);
void close();
}
}
