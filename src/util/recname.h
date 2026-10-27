#pragma once
#include <cstddef>
// Recordings in the deck's flash: /rec/001.log, /rec/002.log, ... and when to drop the oldest.
// PURE, host-tested.
namespace recname {
// Free flash a recording keeps in hand (spec: oldest recordings go when space runs out).
constexpr size_t RESERVE_BYTES = 16 * 1024;

// The number after the highest existing one; 1 when none.
int next(const int* existing, int n);
// "/rec/003.log" (at least 3 digits).
void path(int number, char* out, size_t cap);
// "003.log" -> 3; anything else (another name, a path, too many digits) -> -1.
int parse(const char* name);

// What to do for the space: drop the oldest finished recording (one at a time, so the caller
// deletes it, measures the free space again and asks again), or full when only `current` is left
// to drop (it is never dropped: the recording stops with LOG FULL). current -1: none in progress.
struct Space {
    int dropOldest;
    bool full;
};
Space planSpace(size_t freeBytes, size_t reserveBytes, const int* existing, int n, int current);
// The lowest number that isn't `current`; -1 when there is none.
int oldest(const int* existing, int n, int current);
}
