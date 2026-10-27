// Copied from spotify-pokemon-deck src/util/daynight.h @ 9ced38d.
#pragma once
#include <cstdint>
// Night mode schedule: 18:00-06:00 local time. Pure: net/clock turns the SNTP time into a
// local minute of the day; the result drives the palette and the backlight in main.cpp.
namespace daynight {
constexpr int NIGHT_START = 18 * 60;          // 18:00
constexpr int NIGHT_END   = 6 * 60;           // 06:00
constexpr int64_t SYNCED_AFTER = 1704067200;  // 2024-01-01 00:00 UTC
// True from NIGHT_START to midnight and from midnight to NIGHT_END.
bool isNight(int minuteOfDay);
// False while the clock still counts from 1970 (no SNTP answer yet).
bool synced(int64_t utcSeconds);
}
