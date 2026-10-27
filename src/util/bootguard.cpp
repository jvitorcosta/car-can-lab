// Copied from spotify-pokemon-deck src/util/bootguard.cpp @ 9ced38d.
#include "bootguard.h"

namespace bootguard {

uint32_t afterBoot(uint32_t magic, uint32_t crashes, bool crashed) {
    if (!crashed) return 0;
    if (magic != MAGIC || crashes > LIMIT) crashes = 0;   // no count yet (or noise)
    return crashes + 1;
}

bool rollBack(uint32_t crashes, bool good, bool otherValid) {
    return crashes >= LIMIT && !good && otherValid;
}

bool healthy(uint32_t upMs, bool goodPoll) { return goodPoll && upMs >= HEALTHY_MS; }

}
