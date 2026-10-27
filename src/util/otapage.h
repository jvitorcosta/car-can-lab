// Copied from spotify-pokemon-deck src/util/otapage.h @ 9ced38d.
#pragma once
#include <cstdint>
#include "otaview.h"
// The yellow deck's update page (ledpanel.cpp): while an update arrives over WiFi (net/ota) a
// full-screen terminal page in the current theme holds the screen: the phase's title, a 10-cell
// block bar and the %. net/ota's otaview::Tracker says the phase; this says what a loop pass draws
// for it, so the page appears once, then only the bar moves, and the screen it covered comes back
// in full after a failure (a good update reboots from the page). PURE, host-tested.
namespace otapage {
constexpr int CELLS = 10;

// Filled cells for a percent: one per 10 %, as the Lights level bar; clamped to 0..CELLS.
inline int cells(int pct) { return pct <= 0 ? 0 : pct >= 100 ? CELLS : pct / 10; }

// The page's title: ">UPDATING", ">REBOOTING" (Done: the restart is due), ">UPDATE FAILED"; ""
// when Idle (no page).
inline const char* title(otaview::Phase p) {
    switch (p) {
        case otaview::Phase::Receiving: return ">UPDATING";
        case otaview::Phase::Done: return ">REBOOTING";
        case otaview::Phase::Failed: return ">UPDATE FAILED";
        default: return "";
    }
}
// The title in ALERT (a problem), else TEXT.
inline bool alert(otaview::Phase p) { return p == otaview::Phase::Failed; }

// The page holds the screen: nothing else draws or acts on it (Failed too, for its hold).
inline bool holds(otaview::Phase p) { return p != otaview::Phase::Idle; }

// What a loop pass draws: the whole page (it appeared, or its phase changed), only the bar and the
// % (the percent moved), the covered screen in full (the page went away), or nothing.
enum class Draw : uint8_t { None, Page, Bar, Back };
struct Shown {   // what is on screen; step() keeps it
    otaview::Phase phase = otaview::Phase::Idle;
    int pct = -1;
};
inline Draw step(Shown& s, otaview::Phase p, int pct) {
    if (p == s.phase) {
        if (p != otaview::Phase::Receiving || pct == s.pct) return Draw::None;
        s.pct = pct;
        return Draw::Bar;
    }
    s.phase = p;
    s.pct = pct;
    return p == otaview::Phase::Idle ? Draw::Back : Draw::Page;
}
}
