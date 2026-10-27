// Copied from spotify-pokemon-deck src/util/termpal.cpp @ 9ced38d.
#include "termpal.h"
#include <cmath>

namespace termpal {

namespace {
uint32_t scale(uint32_t c, uint32_t pct) {
    const uint32_t r = ((c >> 16) & 0xFF) * pct / 100, g = ((c >> 8) & 0xFF) * pct / 100, b = (c & 0xFF) * pct / 100;
    return (r << 16) | (g << 8) | b;
}

// One sRGB channel (0..255) to linear light, as WCAG 2.x defines it.
double linear(uint32_t c8) {
    const double c = static_cast<double>(c8) / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
}  // namespace

uint32_t rgb(Role r, bool night) {
    static const uint32_t DAY[] = {0x000000, 0xAAFF8C, 0x5ABE28, 0x308A24, 0xFF3C28};
    static const uint32_t NIGHT[] = {0x000000, 0xFF6600, 0xC84E00, 0xA03A00, 0xFF2A1A};
    return night ? NIGHT[r] : DAY[r];
}

// Per theme (ledlook's swatch order), day then night, BG TEXT MID DIM ALERT; the figures in termpal.h.
static const uint32_t THEMES[8][2][5] = {
    {{0, 0xFF8C78, 0xFF4A3A, 0xF03424, 0xFFFFFF}, {0, 0xFF5A46, 0xE03C2A, 0xC02818, 0xC8C8C8}},  // red
    {{0, 0xFFAA64, 0xF57814, 0xDC6400, 0xFFFFFF}, {0, 0xFF6600, 0xC84E00, 0xA03A00, 0xC8C8C8}},  // orange
    {{0, 0xFF8CCD, 0xFF50B4, 0xF0379E, 0xFFFFFF}, {0, 0xFF4BAA, 0xE0328C, 0xC22678, 0xC8C8C8}},  // pink
    {{0, 0xD2B4FF, 0xB48CFF, 0x9A6EFA, 0xFF3C28}, {0, 0xAA87FF, 0x9068F5, 0x7C50EB, 0xFF2A1A}},  // purple
    {{0, 0xA0C8FF, 0x78A5FF, 0x5A87F5, 0xFF3C28}, {0, 0x7896FF, 0x5A7DF0, 0x4664E6, 0xFF2A1A}},  // blue
    {{0, 0xAAFF8C, 0x5ABE28, 0x308A24, 0xFF3C28}, {0, 0x64D246, 0x40A028, 0x2C7A18, 0xFF2A1A}},  // green
    {{0, 0xF0F0E6, 0xB4B4AA, 0x8C8C84, 0xFF3C28}, {0, 0xFF6600, 0xC84E00, 0xA03A00, 0xFF2A1A}},  // white
    {{0, 0xFFDC5A, 0xFF9900, 0xD27E00, 0xFF3C28}, {0, 0xFF9900, 0xC87800, 0xA06000, 0xFF2A1A}},  // yellow
};

uint32_t rgb(Role r, bool night, int theme) {
    if (theme < 0 || theme >= 8 || r > ALERT) return rgb(r, night);
    return THEMES[theme][night ? 1 : 0][r];
}

uint32_t scan(bool night) { return night ? 0x050300 : 0x030503; }

uint32_t tile(uint32_t screenRgb, bool night) { return night ? scale(screenRgb, 70) : screenRgb; }

double contrastOnBlack(uint32_t c) {
    const double l = 0.2126 * linear((c >> 16) & 0xFF) + 0.7152 * linear((c >> 8) & 0xFF) + 0.0722 * linear(c & 0xFF);
    return (l + 0.05) / 0.05;
}

}  // namespace termpal
