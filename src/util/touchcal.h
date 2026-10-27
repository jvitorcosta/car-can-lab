// Copied from spotify-pokemon-deck src/util/touchcal.h @ 9ced38d.
#pragma once
// Touch calibration as a perspective map (homography) from raw XPT2046 readings to screen pixels,
// fitted exactly through taps on four corner targets; a fifth tap (the centre) checks it. The old
// deck's panel reads rotated ~30 degrees and squeezed into an irregular quadrilateral, which a
// per-axis min/max (touchmap) or even an affine fit can't follow (20-45 px off); this lands within
// ~10 px. It also absorbs swapped or flipped axes. PURE, host-tested.
namespace touchcal {
// sx = (h0*rx + h1*ry + h2) / w,  sy = (h3*rx + h4*ry + h5) / w,  w = h6*rx + h7*ry + 1
struct Homography { double h[8]; };

// Solves the 8x8 system (Gaussian elimination, partial pivoting). False when singular.
inline bool solve8(double m[8][9], double out[8]) {
    for (int c = 0; c < 8; c++) {
        int p = c;
        for (int r = c + 1; r < 8; r++) {
            if ((m[r][c] < 0 ? -m[r][c] : m[r][c]) > (m[p][c] < 0 ? -m[p][c] : m[p][c])) p = r;
        }
        if (m[p][c] > -1e-9 && m[p][c] < 1e-9) return false;
        for (int k = 0; k < 9; k++) {
            const double t = m[c][k];
            m[c][k] = m[p][k];
            m[p][k] = t;
        }
        for (int r = 0; r < 8; r++) {
            if (r == c) continue;
            const double f = m[r][c] / m[c][c];
            for (int k = c; k < 9; k++) m[r][k] -= f * m[c][k];
        }
    }
    for (int i = 0; i < 8; i++) out[i] = m[i][8] / m[i][i];
    return true;
}

// Exact fit through four (raw, screen) pairs, the screen points being a rectangle's corners. False
// when the raw points are degenerate (three in a line, repeated), i.e. the taps didn't register.
inline bool fit(const int rx[4], const int ry[4], const int sx[4], const int sy[4], Homography* out) {
    double m[8][9];
    for (int i = 0; i < 4; i++) {
        const double x = rx[i], y = ry[i], u = sx[i], v = sy[i];
        const double a[9] = {x, y, 1, 0, 0, 0, -u * x, -u * y, u};
        const double b[9] = {0, 0, 0, x, y, 1, -v * x, -v * y, v};
        for (int k = 0; k < 9; k++) {
            m[2 * i][k] = a[k];
            m[2 * i + 1][k] = b[k];
        }
    }
    return solve8(m, out->h);
}

inline int clampi(double v, int hi) { return v < 0 ? 0 : v > hi ? hi : static_cast<int>(v + 0.5); }

// Raw reading -> screen (320x240), clamped to the screen.
inline void apply(const Homography& k, int rx, int ry, int* x, int* y) {
    const double* h = k.h;
    const double w = h[6] * rx + h[7] * ry + 1;
    if (w > -1e-9 && w < 1e-9) {
        *x = *y = 0;
        return;
    }
    *x = clampi((h[0] * rx + h[1] * ry + h[2]) / w, 319);
    *y = clampi((h[3] * rx + h[4] * ry + h[5]) / w, 239);
}

// The larger axis distance (px) between a target and where its tap maps: the fit's check.
inline int error(const Homography& k, int rx, int ry, int sx, int sy) {
    int x, y;
    apply(k, rx, ry, &x, &y);
    const int dx = x > sx ? x - sx : sx - x, dy = y > sy ? y - sy : sy - y;
    return dx > dy ? dx : dy;
}
}
