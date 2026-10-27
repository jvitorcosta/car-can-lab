#pragma once
#include <cstddef>
#include <cstdint>
// Recordings in the deck's flash: LittleFS on the min_spiffs.csv data partition (~128 KB), which an
// update over WiFi never touches, so recordings survive swapping to the LED panel and back. One
// recording open at a time; its lines are buffered in RAM and written every FLUSH_MS (a power cut
// loses at most that much). Before each write the oldest finished recordings make room
// (util/recname); when only the open one is left, full() says so (LOG FULL): the lines so far and
// the closing ones still go into the reserve, then the monitor closes it.
// Thread-safe: the monitor task writes, the web server lists and reads.
namespace sniff {
namespace store {
constexpr uint32_t FLUSH_MS = 2000;
bool begin();  // mounts; formats only if it can't
bool ready();
// Starts recording number *number (the next free one); false when the flash can't take one.
bool open(int* number);
void write(const char* line);  // a line without its newline
void tick(uint32_t nowMs);     // flushes every FLUSH_MS
void close();
bool isOpen();
// The flash is full: the open recording must end (cleared by the next open).
bool full();
int list(int* numbers, size_t* sizes, int max);  // oldest first
int usedPct();                                   // of the data partition
}
}
