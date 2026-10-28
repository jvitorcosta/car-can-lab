// The five-cross routine is the LED panel's (spotify-pokemon-deck src/ledpanel.cpp @ 9ced38d,
// "Touch calibration"); it saves into sniff's own namespace instead of the panel's.
#include "calib.h"
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "../hw/touch.h"
#include "../util/touchcal.h"

namespace sniff {
namespace calib {
static constexpr int N_TARGET = 5;
static const int CAL_SX[N_TARGET] = {24, 296, 296, 24, 160};
static const int CAL_SY[N_TARGET] = {24, 24, 216, 216, 120};

static bool loadFrom(const char* ns) {
    Preferences p;
    if (!p.begin(ns, true)) return false;  // read-only
    touchcal::Homography k;
    const bool ok = p.getBytesLength("hom") == sizeof(k) && p.getBytes("hom", &k, sizeof(k)) == sizeof(k);
    p.end();
    if (ok) touch::setCalibration(k);
    return ok;
}

bool load() { return loadFrom("sniff") || loadFrom("touch"); }

static void drawTarget(TFT_eSPI& tft, int x, int y, uint16_t c) {
    tft.drawFastHLine(x - 14, y, 29, c);
    tft.drawFastVLine(x, y - 14, 29, c);
    tft.drawCircle(x, y, 8, c);
}

// One pen tap: waits for the panel to read released, then averages the press's readings.
static void waitTap(int* rx, int* ry) {
    int x, y;
    for (uint32_t freeSince = millis(); millis() - freeSince < 150;) {
        if (touch::readRaw(&x, &y)) freeSince = millis();
        delay(5);
    }
    for (;;) {
        while (!touch::readRaw(&x, &y)) delay(5);
        delay(60);  // let the pen settle
        long sx = 0, sy = 0;
        int n = 0;
        for (uint32_t t0 = millis(); millis() - t0 < 250 && touch::readRaw(&x, &y); n++) {
            sx += x;
            sy += y;
            delay(10);
        }
        if (n >= 5) {
            *rx = static_cast<int>(sx / n);
            *ry = static_cast<int>(sy / n);
            return;
        }
    }
}

static void say(TFT_eSPI& tft, const char* a, const char* b, uint16_t c) {
    tft.fillScreen(TFT_BLACK);
    tft.setFreeFont(nullptr);
    tft.setTextFont(4);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(c, TFT_BLACK);
    tft.drawString(a, 160, 90);
    if (b) {
        tft.setTextFont(2);
        tft.drawString(b, 160, 150);
    }
}

void run(TFT_eSPI& tft) {
    for (;;) {
        say(tft, "Touch calibration", "Tap each cross with the pen", TFT_WHITE);
        int rx[N_TARGET], ry[N_TARGET];
        for (int i = 0; i < N_TARGET; i++) {
            drawTarget(tft, CAL_SX[i], CAL_SY[i], TFT_RED);
            waitTap(&rx[i], &ry[i]);
            drawTarget(tft, CAL_SX[i], CAL_SY[i], TFT_GREEN);
            Serial.printf("[cal] target %d,%d <- raw %d,%d\n", CAL_SX[i], CAL_SY[i], rx[i], ry[i]);
        }
        // The four corners fit the map exactly; the centre tap checks it. Pen taps on this panel
        // scatter ~10 px, so 25 px still means "hit the cross".
        touchcal::Homography k;
        const bool ok = touchcal::fit(rx, ry, CAL_SX, CAL_SY, &k);
        const int err = ok ? touchcal::error(k, rx[4], ry[4], CAL_SX[4], CAL_SY[4]) : 999;
        Serial.printf("[cal] fit %s, centre off by %d px\n", ok ? "ok" : "failed", err);
        if (ok && err <= 25) {
            Preferences p;
            p.begin("sniff", false);
            p.putBytes("hom", &k, sizeof(k));
            p.end();
            touch::setCalibration(k);
            say(tft, "Calibrated", nullptr, TFT_GREEN);
            delay(800);
            return;
        }
        say(tft, "Missed a cross - again", nullptr, TFT_ORANGE);
        delay(1500);
    }
}
}
}
