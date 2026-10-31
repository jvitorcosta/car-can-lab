#include "sniffview.h"
#include <cstdio>

namespace sniffview {
const char* statusText(monplan::State s, uint32_t inStateMs) {
    using monplan::State;
    switch (s) {
        case State::Searching: return inStateMs < NOT_FOUND_MS ? "SEARCHING..." : "ADAPTER NOT FOUND";
        case State::Probing: return "CONNECTING...";
        case State::NotCan: return "NOT A CAN CAR?";
        case State::Bus29: return "29-BIT CAN: NOT SUPPORTED";
        case State::NotSilent: return "ADAPTER CAN'T MONITOR (NOT SILENT)";
        case State::CantMonitor: return "ADAPTER CAN'T MONITOR";
        case State::EngineOff: return "ENGINE OFF?";
        case State::Lost: return "ADAPTER LOST";
        case State::Monitoring:
        case State::Querying: return nullptr;
    }
    return nullptr;
}

bool isAlert(monplan::State s, uint32_t inStateMs) {
    using monplan::State;
    if (s == State::Searching) return inStateMs >= NOT_FOUND_MS;
    return s != State::Probing && s != State::Monitoring && s != State::Querying;
}

void header(char* out, size_t cap, int ids, uint32_t dropped, int msgPerSec, bool rec, int logPct, uint32_t cycleMs) {
    char plus[16] = "";
    if (dropped) snprintf(plus, sizeof plus, " +%lu", static_cast<unsigned long>(dropped));
    if (rec) {
        const unsigned long s = (cycleMs + 999) / 1000;
        snprintf(out, cap, "REC %d%%  CYCLE %lu s  %d IDs%s  %d/s", logPct, s, ids, plus, msgPerSec);
    } else {
        snprintf(out, cap, "SNIFF // LIVE  %d IDs%s  %d msg/s", ids, plus, msgPerSec);
    }
}

void row(char* out, size_t cap, const idtable::Row& r) {
    int k = snprintf(out, cap, "%03X%s", static_cast<unsigned>(r.id & 0x7FF), r.picked ? "* " : "  ");
    for (int i = 0; i < r.len && i < 8 && k > 0 && static_cast<size_t>(k) < cap; i++)
        k += snprintf(out + k, cap - k, i ? " %02X" : "%02X", r.data[i]);
}
}
