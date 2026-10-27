// Copied from spotify-pokemon-deck src/util/touchmap.h @ 9ced38d.
#pragma once
// XPT2046 raw touch reading -> screen coordinates (320x240, rotation 1). PURE, host-tested.
// A calibration with x0 > x1 (or y0 > y1) flips that axis.
namespace touchmap {
struct Cal { int x0, x1, y0, y1; };   // raw values at screen x 0 / 319 and y 0 / 239

inline int scale(int raw, int r0, int r1, int span) {
    const long v = (long)(raw - r0) * (span - 1) / (r1 - r0);
    return v < 0 ? 0 : v > span - 1 ? span - 1 : (int)v;
}

inline void toScreen(const Cal& c, int rawX, int rawY, int* x, int* y) {
    *x = scale(rawX, c.x0, c.x1, 320);
    *y = scale(rawY, c.y0, c.y1, 240);
}
}
