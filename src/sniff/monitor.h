#pragma once
#include <cstdint>
#include "../util/idtable.h"
#include "../util/monplan.h"
// The sniff firmware's engine: owns the adapter link (sniff/link), the monitor plan
// (util/monplan), the live table (util/idtable) and the recording (util/reclog + sniff/store).
// tick() runs on the BLE task (core 0), next to the Xbox controller; the screen (core 1) reads a
// View and a copy of the table, and sends the owner's actions. Everything shared sits behind one
// mutex.
namespace sniff {
namespace monitor {
constexpr uint32_t NOTHING_HEARD_MS = 10000;
constexpr uint32_t RETRY_MS = 10000;  // between adapter scans while it's missing

struct View {
    monplan::State state;
    uint32_t inStateMs;
    uint32_t cycleMs;
    int ids;
    uint32_t dropped;
    int msgPerSec;
    bool recording, filterOn, logFull;
    int recNumber, logPct, marks;
    bool nothingHeard;  // Monitoring and no frame for NOTHING_HEARD_MS
};

void begin();
// One pass: connect while the adapter is missing (only when scanFree: the Xbox controller isn't
// scanning or connecting), read the adapter's lines, send the plan's next command, write the
// recording. Returns quickly except while scanning / connecting.
void tick(uint32_t nowMs, bool scanFree);

View view(uint32_t nowMs);
void copyTable(idtable::IdTable* out);
void togglePick(uint16_t id);
// FILTER: the picks, or off again; false when there is nothing picked to filter on.
bool toggleFilter(uint32_t nowMs);
// MARK: the next mark's number, or 0 when not recording.
int mark(uint32_t nowMs);
// REC on / off; false when it couldn't start (the flash full or not mounted).
bool toggleRec(uint32_t nowMs);
}
}
