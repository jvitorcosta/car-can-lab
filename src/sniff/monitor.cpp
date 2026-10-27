#include "monitor.h"
#include <Arduino.h>
#include <freertos/semphr.h>
#include <time.h>
#include "../util/elmmon.h"
#include "../util/reclog.h"
#include "../util/version.h"
#include "link.h"
#include "store.h"

namespace sniff {
namespace monitor {
static SemaphoreHandle_t mux = nullptr;
static monplan::Plan plan;
static idtable::IdTable table;
static reclog::Rec rec;
static bool recording = false, filterOn = false, logFull = false;
static int recNumber = 0, marks = 0;
static uint32_t recStart = 0, lastTry = 0, lastFrameAt = 0, monitoringSince = 0;
static uint32_t secStart = 0, secFrames = 0;
static int msgPerSec = 0;
static monplan::State shown = monplan::State::Searching;

namespace {
struct Lock {
    Lock() { xSemaphoreTake(mux, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mux); }
};
}

static void writer(const char* line, void*) { store::write(line); }

static uint32_t recMs(uint32_t nowMs) { return nowMs - recStart; }

static void stopRec(uint32_t nowMs) {
    if (!recording) return;
    rec.end(recMs(nowMs));
    store::close();
    recording = false;
    plan.setRecording(false, nullptr, 0, nowMs);
    Serial.printf("[mon] recording %d stopped\n", recNumber);
}

// Under the lock, every pass, adapter or not: the plan's notes into the recording at the time they
// happened, the held values and the flush, and the end of a recording the flash can't take more of.
static void keepRecording(uint32_t nowMs) {
    monplan::Note n;
    while (plan.takeNote(&n)) {
        if (!recording) continue;
        const uint32_t at = static_cast<int32_t>(n.ms - recStart) < 0 ? 0 : n.ms - recStart;  // never before REC
        if (n.kind == monplan::Note::Kind::Obd) rec.obd(n.text, n.value, at);
        else rec.state(n.text, at);
    }
    if (plan.state() != shown) {
        shown = plan.state();
        if (shown == monplan::State::Monitoring) monitoringSince = nowMs;
        Serial.printf("[mon] state %d\n", static_cast<int>(shown));
    }
    if (!recording) return;
    rec.tick(recMs(nowMs));
    store::tick(nowMs);
    if (store::full()) {  // the reserve takes these last lines, then it closes
        rec.state("log full", recMs(nowMs));
        stopRec(nowMs);
        logFull = true;
        Serial.println("[mon] LOG FULL");
    } else if (!store::isOpen()) {  // the file went away under it
        recording = false;
        plan.setRecording(false, nullptr, 0, nowMs);
    }
}

void begin() {
    if (!mux) mux = xSemaphoreCreateMutex();
}

void tick(uint32_t nowMs, bool scanFree) {
    if (!link::connected()) {
        {
            Lock l;
            const monplan::State s = plan.state();
            if (s != monplan::State::Searching && s != monplan::State::Lost) plan.onLinkLost(nowMs);
        }
        if (scanFree && (lastTry == 0 || nowMs - lastTry >= RETRY_MS)) {
            lastTry = nowMs | 1;
            if (link::tryConnect()) {
                Lock l;
                plan.onLinkBack(millis());
            }
        }
        if (!link::connected()) {
            {
                Lock l;
                keepRecording(millis());  // "adapter lost" at its time, and the flushes go on
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            return;
        }
    }
    char line[elmmon::Lines::CAP];
    Lock l;
    // a burst of lines at most per pass, so the screen's calls never wait long
    for (int k = 0; k < 32 && link::readLine(line, sizeof line, k == 0 ? 5 : 0); k++) {
        const uint32_t now = millis();
        elmmon::Frame f;
        if (elmmon::parse(line, &f) == elmmon::Kind::Frame && plan.state() == monplan::State::Monitoring) {
            table.update(f, now);
            if (recording) rec.frame(f, recMs(now));
            lastFrameAt = now;
            secFrames++;
        }
        plan.onLine(line, now);
    }
    const uint32_t now = millis();
    if (const char* c = plan.nextCommand(now)) link::send(c);
    keepRecording(now);
    if (now - secStart >= 1000) {
        msgPerSec = static_cast<int>(secFrames * 1000 / (now - secStart));
        secStart = now;
        secFrames = 0;
    }
}

View view(uint32_t nowMs) {
    Lock l;
    View v{};
    v.state = plan.state();
    v.inStateMs = nowMs - plan.stateSinceMs();
    v.cycleMs = plan.cycleMs();
    v.ids = table.size();
    v.dropped = table.dropped();
    v.msgPerSec = msgPerSec;
    v.recording = recording;
    v.filterOn = filterOn;
    v.logFull = logFull;
    v.recNumber = recNumber;
    v.logPct = store::usedPct();
    v.marks = marks;
    const uint32_t heard = lastFrameAt > monitoringSince ? lastFrameAt : monitoringSince;
    v.nothingHeard = v.state == monplan::State::Monitoring && nowMs - heard >= NOTHING_HEARD_MS;
    return v;
}

void copyTable(idtable::IdTable* out) {
    Lock l;
    *out = table;
}

void togglePick(uint16_t id) {
    Lock l;
    table.togglePick(id);
}

bool toggleFilter(uint32_t nowMs) {
    (void)nowMs;
    Lock l;
    if (filterOn) {
        filterOn = false;
        plan.setFilter(nullptr, 0);
        return true;
    }
    uint16_t ids[idtable::IdTable::MAX_IDS];
    const int n = table.picked(ids, idtable::IdTable::MAX_IDS);
    if (n == 0) return false;
    filterOn = true;
    plan.setFilter(ids, n);
    return true;
}

int mark(uint32_t nowMs) {
    Lock l;
    if (!recording) return 0;
    rec.mark(++marks, recMs(nowMs));
    return marks;
}

bool toggleRec(uint32_t nowMs) {
    Lock l;
    if (recording) {
        stopRec(nowMs);
        return true;
    }
    int n = 0;
    if (!store::open(&n)) {
        logFull = store::full();
        return false;
    }
    char when[24] = "unsynced";
    const time_t t = time(nullptr);
    if (t > 1700000000) {
        struct tm local;
        localtime_r(&t, &local);
        strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", &local);
    }
    recording = true;
    logFull = false;
    recNumber = n;
    marks = 0;
    recStart = nowMs;
    rec.begin(writer, nullptr, reclog::Header{n, when, plan.adapter()[0] ? plan.adapter() : "-",
                                             plan.protocol()[0] ? plan.protocol() : "-", FW_VERSION});
    uint16_t ids[idtable::IdTable::MAX_IDS];
    const int k = table.order(ids, idtable::IdTable::MAX_IDS);
    char s[40];
    if (filterOn) {
        uint16_t picks[idtable::IdTable::MAX_IDS];
        snprintf(s, sizeof s, "rec watching %d picked ids", table.picked(picks, idtable::IdTable::MAX_IDS));
    } else {
        snprintf(s, sizeof s, "rec watching %d ids", k);
    }
    rec.state(s, 0);
    plan.setRecording(true, ids, k, nowMs);
    Serial.printf("[mon] recording %d started, %d ids\n", n, k);
    return true;
}
}
}
