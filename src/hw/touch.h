// Copied from spotify-pokemon-deck src/companion/touch.h @ 9ced38d.
#pragma once
// The CYD's resistive touchscreen (XPT2046) for the companion's buttons. It sits on its own pins
// (include/pins.h); the companion drives it on VSPI because the display owns HSPI.
#include "../util/touchcal.h"

namespace touch {
void begin();
// True while pressed hard enough; *x, *y in screen coordinates. Logs the raw reading at each
// touch-down ("[touch] raw x=.. y=.. -> x,y") for calibration.
bool read(int* x, int* y);
// The raw reading while pressed (no stuck-press guard, no mapping), for a calibration screen.
bool readRaw(int* rx, int* ry);
// A fitted calibration (touchcal) to use instead of TOUCH_RAW_* from pins.h.
void setCalibration(const touchcal::Homography& k);
}
