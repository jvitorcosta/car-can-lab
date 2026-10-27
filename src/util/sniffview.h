#pragma once
#include <cstddef>
#include <cstdint>
#include "idtable.h"
#include "monplan.h"
// Screen A's words (spec 2026-10-27 "The screen"): the status in words when the list can't show
// frames, the header and one row's text. ASCII only (the fonts are Latin-1, no ellipsis glyph).
// PURE, host-tested.
namespace sniffview {
constexpr uint32_t NOT_FOUND_MS = 20000;  // searching this long: ADAPTER NOT FOUND

// nullptr while Monitoring / Querying (the list shows).
const char* statusText(monplan::State s, uint32_t inStateMs);
// The status is a problem (drawn in the alert colour).
bool isAlert(monplan::State s, uint32_t inStateMs);
// "SNIFF // LIVE  23 IDs +2  41 msg/s"; recording: "REC 34%  CYCLE 4 s  23 IDs +2  41/s"
// (+N only when IDs were dropped, CYCLE in whole seconds rounded up).
void header(char* out, size_t cap, int ids, uint32_t dropped, int msgPerSec, bool rec, int logPct, uint32_t cycleMs);
// "1A6  00 04 00 00 3C 00 00 11"; a picked ID "1A6* ...".
void row(char* out, size_t cap, const idtable::Row& r);
// The character column where byte i starts in row()'s text.
constexpr int byteColumn(int i) { return 5 + 3 * i; }
}
