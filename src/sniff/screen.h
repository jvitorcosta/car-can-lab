#pragma once
#include <cstdint>
#include "../util/otaview.h"
class TFT_eSPI;
// Screen A (the live list), the home page and the logs page on the yellow deck's 320 x 240, in the
// CYBERDECK's VT323 and Seegson palette (util/termpal), orange at night. The loop task (core 1)
// calls loop() with the inputs of the pass; the data comes from sniff/monitor.
//   LIVE  header (IDs, msg/s, REC and CYCLE), up to 9 rows, a changed byte lit and fading over
//         1 s, idle rows dim, picked rows *, then [PAUSE] [FILTER] [MARK] [REC] [BACK]
//   HOME  [LIVE] [LOGS], LIGHTS: FLASH LEDPANEL BACK (BACK on LIVE comes here; the deck boots in LIVE)
//   LOGS  each recording and its size, the address to download them from
// Touch taps; the Xbox controller: D-pad up / down the row cursor (left / right on HOME), A pick
// (press on HOME), X MARK, Y REC, LB FILTER, RB PAUSE, B back.
namespace sniff {
namespace screen {
struct Input {
    bool tap;  // a new press, at x, y (screen pixels)
    int x, y;
    bool pad;  // a controller is connected (shows the cursor)
    bool up, down, left, right, a, b, bx, by, lb, rb;  // presses (edges), this pass
};
void begin(TFT_eSPI* tft, bool calibrated);
void boot();  // the boot lines (~1 s), then LIVE
void loop(uint32_t nowMs, const Input& in);
void setNight(bool night);
void redraw();
// The update page (net/ota): the phase's title and a 10-cell bar.
void updatePage(otaview::Phase p, int pct);
}
}
