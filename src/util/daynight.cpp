// Copied from spotify-pokemon-deck src/util/daynight.cpp @ 9ced38d.
#include "daynight.h"

namespace daynight {

bool isNight(int m) { return m >= NIGHT_START || m < NIGHT_END; }

bool synced(int64_t utcSeconds) { return utcSeconds >= SYNCED_AFTER; }

}
