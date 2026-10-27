// Copied from spotify-pokemon-deck src/util/termpal.h @ 9ced38d.
// The terminal look's palette ("Seegson", Alien: Isolation): pale green by day, amber at night, on black;
// one hue at three brightnesses, plus RED (ALERT) reserved for problems. Read at a glance in a moving
// car, so every role clears a WCAG contrast on black: 4.5:1 by day (DIM included), 3:1 at night (the
// backlight is dimmer then anyway). The day's darker greens carry little blue: the panel turns a
// blue-green to teal. PURE, host-tested.
#pragma once
#include <stdint.h>

namespace termpal {

enum Role : uint8_t { BG, TEXT, MID, DIM, ALERT };

// Day: BG 0x000000, TEXT 0xAAFF8C (17.4:1), MID 0x5ABE28 (8.8:1), DIM 0x308A24 (4.8:1),
// ALERT 0xFF3C28 (5.9:1).
// Night (amber, picked on the panel): TEXT 0xFF6600 (7.2:1), MID 0xC84E00 (4.5:1), DIM 0xA03A00
// (3.1:1), ALERT 0xFF2A1A (5.6:1, a redder red), BG 0. The night greens sit well under red (at most
// 0.6x) so the amber reads warm, not olive, on the panel.
uint32_t rgb(Role r, bool night);
// The same roles in a theme: the colour of the car's INSIDE light, `theme` a ledlook swatch index
// (0..7: red, orange, pink, purple, blue, green, white, yellow); -1 (or anything out of range) is the
// default above. Each theme is its hue at three brightnesses with the same rules as the default (day
// TEXT >= 7:1, MID and DIM >= 4.5:1; night every role >= 3:1). Blue and purple are lightened so they
// read; green by day is the default green; white is a neutral grey-white by day and the default amber
// at night (no white glare); the night ambers (orange, yellow, white) keep green <= 0.6 x red.
// ALERT is white (night 0xC8C8C8) on red, orange and pink, where a red would not stand out, else
// the default red; it is always a channel 0x60 or more from TEXT (or, two warm colours, a green/red
// under half TEXT's and 0.1 or more under it: white at night, the default night). Contrast on
// black, TEXT / MID / DIM / ALERT:
//   red     day FF8C78 9.3  FF4A3A 6.3  F03424 5.2  FFFFFF 21    night FF5A46 6.8  E03C2A 4.9  C02818 3.6  C8C8C8 12.6
//   orange  day FFAA64 11.2 F57814 7.6  DC6400 5.9  FFFFFF 21    night FF6600 7.2  C84E00 4.5  A03A00 3.1  C8C8C8 12.6
//   pink    day FF8CCD 9.9  FF50B4 7.1  F0379E 5.8  FFFFFF 21    night FF4BAA 6.8  E0328C 5.0  C22678 3.8  C8C8C8 12.6
//   purple  day D2B4FF 11.7 B48CFF 8.1  9A6EFA 6.0  FF3C28 5.9   night AA87FF 7.6  9068F5 5.5  7C50EB 4.2  FF2A1A 5.6
//   blue    day A0C8FF 12.2 78A5FF 8.6  5A87F5 6.2  FF3C28 5.9   night 7896FF 7.6  5A7DF0 5.6  4664E6 4.2  FF2A1A 5.6
//   green   day AAFF8C 17.4 5ABE28 8.8  308A24 4.8  FF3C28 5.9   night 64D246 10.9 40A028 6.3  2C7A18 3.9  FF2A1A 5.6
//   white   day F0F0E6 18.3 B4B4AA 10.1 8C8C84 6.2  FF3C28 5.9   night FF6600 7.2  C84E00 4.5  A03A00 3.1  FF2A1A 5.6
//   yellow  day FFDC5A 15.6 FF9900 9.8  D27E00 6.7  FF3C28 5.9   night FF9900 9.8  C87800 6.1  A06000 4.2  FF2A1A 5.6
uint32_t rgb(Role r, bool night, int theme);
// The scanline colour drawn over the screen: day 0x030503, night 0x050300.
uint32_t scan(bool night);
// A colour tile on the screen (e.g. a swatch): day unchanged; night each channel x70/100.
uint32_t tile(uint32_t screenRgb, bool night);
// WCAG 2.x contrast of 0xRRGGBB on black: (L + 0.05) / 0.05, L its relative luminance; 1 .. 21.
double contrastOnBlack(uint32_t rgb);

}  // namespace termpal
