#include "store.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/semphr.h>
#include "../util/recname.h"

namespace sniff {
namespace store {
static SemaphoreHandle_t mux = nullptr;
static bool mounted = false;
static File file;
static int current = -1;
static bool wasFull = false;
static char buf[4096];
static size_t used = 0;
static uint32_t flushedAt = 0;

namespace {
struct Lock {
    Lock() { xSemaphoreTake(mux, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mux); }
};
}

static int listLocked(int* numbers, size_t* sizes, int max) {
    File dir = LittleFS.open("/rec");
    if (!dir || !dir.isDirectory()) return 0;
    int n = 0;
    for (File f = dir.openNextFile(); f && n < max; f = dir.openNextFile()) {
        const int k = recname::parse(f.name());
        if (k < 0) continue;
        int at = n++;  // insertion: oldest first
        while (at > 0 && numbers[at - 1] > k) {
            numbers[at] = numbers[at - 1];
            if (sizes) sizes[at] = sizes[at - 1];
            at--;
        }
        numbers[at] = k;
        if (sizes) sizes[at] = f.size();
    }
    return n;
}

static size_t freeBytes() { return LittleFS.totalBytes() - LittleFS.usedBytes(); }

// Drops the oldest finished recordings until RESERVE_BYTES are free; false when only the open one
// is left to drop.
static bool makeRoomLocked() {
    for (int guard = 0; guard < 64; guard++) {
        int nums[64];
        const int n = listLocked(nums, nullptr, 64);
        const recname::Space s = recname::planSpace(freeBytes(), recname::RESERVE_BYTES, nums, n, current);
        if (s.full) return false;
        if (s.dropOldest == 0) return true;
        char p[24];
        recname::path(recname::oldest(nums, n, current), p, sizeof p);
        Serial.printf("[store] dropping %s for space\n", p);
        if (!LittleFS.remove(p)) return false;
    }
    return false;
}

static void flushLocked() {
    if (!file || used == 0) {
        used = 0;
        return;
    }
    if (!makeRoomLocked()) {
        Serial.println("[store] LOG FULL");
        file.close();
        current = -1;
        used = 0;
        wasFull = true;
        return;
    }
    file.write(reinterpret_cast<const uint8_t*>(buf), used);
    file.flush();
    used = 0;
}

bool begin() {
    if (!mux) mux = xSemaphoreCreateMutex();
    Lock l;
    mounted = LittleFS.begin(true);  // formats the data partition only when it can't mount
    if (mounted && !LittleFS.exists("/rec")) LittleFS.mkdir("/rec");
    Serial.printf("[store] %s, %u of %u bytes used\n", mounted ? "mounted" : "NOT MOUNTED",
                  mounted ? static_cast<unsigned>(LittleFS.usedBytes()) : 0u,
                  mounted ? static_cast<unsigned>(LittleFS.totalBytes()) : 0u);
    return mounted;
}

bool ready() { return mounted; }

bool open(int* number) {
    if (!mounted) return false;
    Lock l;
    if (file) return false;
    wasFull = false;
    int nums[64];
    const int n = listLocked(nums, nullptr, 64);
    const int k = recname::next(nums, n);
    current = k;  // makeRoom never drops it
    if (!makeRoomLocked()) {
        current = -1;
        wasFull = true;
        return false;
    }
    char p[24];
    recname::path(k, p, sizeof p);
    file = LittleFS.open(p, FILE_WRITE);
    if (!file) {
        current = -1;
        return false;
    }
    used = 0;
    flushedAt = millis();
    *number = k;
    Serial.printf("[store] recording %s\n", p);
    return true;
}

void write(const char* line) {
    if (!mux) return;
    Lock l;
    if (!file) return;
    const size_t n = strlen(line);
    if (used + n + 1 > sizeof buf) flushLocked();
    if (!file || n + 1 > sizeof buf) return;
    memcpy(buf + used, line, n);
    buf[used + n] = '\n';
    used += n + 1;
}

void tick(uint32_t nowMs) {
    if (!mux) return;
    Lock l;
    if (!file || nowMs - flushedAt < FLUSH_MS) return;
    flushedAt = nowMs;
    flushLocked();
}

void close() {
    if (!mux) return;
    Lock l;
    if (!file) return;
    flushLocked();
    if (file) file.close();
    current = -1;
}

bool isOpen() {
    if (!mux) return false;
    Lock l;
    return static_cast<bool>(file);
}

bool full() { return wasFull; }

int list(int* numbers, size_t* sizes, int max) {
    if (!mounted) return 0;
    Lock l;
    return listLocked(numbers, sizes, max);
}

int usedPct() {
    if (!mounted || LittleFS.totalBytes() == 0) return 0;
    return static_cast<int>(100.0 * static_cast<double>(LittleFS.usedBytes()) / static_cast<double>(LittleFS.totalBytes()));
}
}
}
