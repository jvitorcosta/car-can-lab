// car-can-lab's sniff firmware for the yellow deck (ESP32 CYD): watches the car's CAN bus through
// an ELM327 BLE adapter in silent monitor mode (docs/superpowers/specs/2026-10-27-sniff-firmware-design.md).
// It replaces the deck repo's LED panel on the deck while a session runs; both update each other
// over WiFi from a PC on the iPhone hotspot:
//   pio run -e sniff_ota -t upload          (here)
//   pio run -e ledpanel_ota -t upload       (in spotify-pokemon-deck: the lights come back)
// It reads the panel's touch calibration (NVS "touch"/"hom") and never writes it: with none
// stored, the screen says so and the Xbox controller still drives everything.
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "hw/touch.h"
#include "net/clock.h"
#include "net/ota.h"
#include "net/remote_config.h"
#include "net/wifi.h"
#include "ui/backlight.h"
#include "ui/termfont.h"
#include "util/otapage.h"
#include "util/termpal.h"
#include "util/version.h"

static TFT_eSPI tft;
static constexpr const char* MDNS_NAME = "cardeck-sniff";
static bool night = false;

using termfont::Face;

static uint16_t c565(uint32_t rgb) { return tft.color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF); }
static uint16_t pal(termpal::Role r) { return c565(termpal::rgb(r, night)); }

// VT323 text; `datum` as TFT_eSPI's nine, its vertical part by the capitals (as the LED panel's).
static int text(const char* s, int x, int y, Face f, uint8_t datum) {
    static int shown = -1;
    if (static_cast<int>(f) != shown) {
        shown = static_cast<int>(f);
        tft.setFreeFont(reinterpret_cast<const GFXfont*>(&termfont::font(f)));
    }
    const int cap = termfont::capHeight(f), v = datum / 3;
    const int base = v == 0 ? y + cap : v == 1 ? y + cap / 2 : y;
    tft.setTextDatum(L_BASELINE + datum % 3);
    return tft.drawString(s, x, base);
}

static void clearBg(int x, int y, int w, int h) {
    tft.fillRect(x, y, w, h, pal(termpal::BG));
    const uint16_t line = c565(termpal::scan(night));
    for (int row = y + (3 - y % 3) % 3; row < y + h; row += 3) tft.drawFastHLine(x, row, w, line);
}

static bool calibrated = false;
static bool loadCalibration() {
    Preferences p;
    if (!p.begin("touch", true)) return false;  // read-only: the namespace is the LED panel's
    touchcal::Homography k;
    const bool ok = p.getBytesLength("hom") == sizeof(k) && p.getBytes("hom", &k, sizeof(k)) == sizeof(k);
    p.end();
    if (ok) touch::setCalibration(k);
    return ok;
}

static void drawBoot() {
    clearBg(0, 0, tft.width(), tft.height());
    tft.setTextColor(pal(termpal::TEXT));
    text("CAR-CAN-LAB // SNIFF", 8, 8, Face::TITLE, TL_DATUM);
    tft.setTextColor(pal(termpal::MID));
    text("LIGHTS: FLASH LEDPANEL BACK", 8, 40, Face::BODY, TL_DATUM);
    if (!calibrated) {
        tft.setTextColor(pal(termpal::ALERT));
        text("TOUCH NOT CALIBRATED: USE THE XBOX PAD", 8, 64, Face::SMALL, TL_DATUM);
    }
    tft.setTextColor(pal(termpal::DIM));
    text(FW_LABEL, 8, 220, Face::SMALL, TL_DATUM);
}

static void netTask(void*) {
    net::connectAny();
    while (!net::isOnline()) {
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    netclock::begin();
    ota::begin(MDNS_NAME);
    Serial.printf("[net] on: %s\n", net::deviceIp().c_str());
    for (;;) {
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// The update page: the title and a 10-cell bar, as the LED panel's.
static bool updateTick() {
    static otapage::Shown shown;
    const ota::View v = ota::view();
    switch (otapage::step(shown, v.phase, v.pct)) {
        case otapage::Draw::Page:
        case otapage::Draw::Bar: {
            clearBg(0, 0, tft.width(), tft.height());
            tft.setTextColor(pal(otapage::alert(v.phase) ? termpal::ALERT : termpal::TEXT));
            text(otapage::title(v.phase), tft.width() / 2, 80, Face::BIG, TC_DATUM);
            const int full = otapage::cells(v.pct);
            for (int k = 0; k < otapage::CELLS; k++) {
                const int x = 40 + k * 24;
                if (k < full) tft.fillRect(x, 150, 20, 16, pal(termpal::TEXT));
                else tft.drawRect(x, 150, 20, 16, pal(termpal::MID));
            }
            break;
        }
        case otapage::Draw::Back: drawBoot(); break;
        case otapage::Draw::None: break;
    }
    return otapage::holds(v.phase);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("[boot] sniff v%s\n", FW_VERSION);
    ota::checkBoot();  // may boot the other app slot (net/ota), before anything else starts
    tft.init();
    backlight::begin();
    tft.invertDisplay(true);  // CYD panel: colours are inverted without this
    tft.setRotation(1);
    tft.setAttribute(UTF8_SWITCH, 0);
    touch::begin();
    calibrated = loadCalibration();
    drawBoot();
    if (REMOTE_TOKEN[0]) xTaskCreatePinnedToCore(netTask, "net", 6144, nullptr, 0, nullptr, 1);
    else Serial.println("[net] off (REMOTE_TOKEN empty)");
}

void loop() {
    static uint32_t provenAt = 0;
    if (millis() - provenAt >= 1000) {
        provenAt = millis();
        if (net::isOnline() && (ota::listening() || !ota::enabled())) ota::onGoodPoll();
    }
    if (updateTick()) {
        delay(10);
        return;
    }
    static uint32_t memAt = 0;
    if (millis() - memAt > 60000) {
        memAt = millis();
        Serial.printf("[mem] heap %u (min %u)\n", static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getMinFreeHeap()));
    }
    delay(10);
}
