// car-can-lab's sniff firmware for the yellow deck (ESP32 CYD): watches the car's CAN bus through
// an ELM327 BLE adapter in silent monitor mode (docs/superpowers/specs/2026-10-27-sniff-firmware-design.md).
// It replaces the deck repo's LED panel on the deck while a session runs; both update each other
// over WiFi from a PC on the iPhone hotspot:
//   pio run -e sniff_ota -t upload          (here)
//   pio run -e ledpanel_ota -t upload       (in spotify-pokemon-deck: the lights come back)
// It reads the panel's touch calibration (NVS "touch"/"hom") and never writes it: with none
// stored, the screen says so and the Xbox controller still drives everything.
#include <Arduino.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"            // NimBLE 2's and the Xbox library's headers;
#pragma GCC diagnostic ignored "-Wdouble-promotion"  // src/ builds with -Werror
#include <NimBLEDevice.h>
#include <XboxPad.hpp>  // lib/xboxpad: asukiaaa's Xbox library, vendored with shorter timeouts
#pragma GCC diagnostic pop
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "hw/touch.h"
#include "net/clock.h"
#include "net/ota.h"
#include "net/remote_config.h"
#include "net/wifi.h"
#include "sniff/monitor.h"
#include "sniff/store.h"
#include "sniff/web.h"
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
    sniff::web::begin();
    Serial.printf("[net] on: %s\n", net::deviceIp().c_str());
    for (;;) {
        if (!ota::active()) sniff::web::loop();
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// The Xbox controller (Series X|S / One 1708+, over BLE): the first one found in pairing mode, or
// the bonded one (the LED panel's bond, NimBLE's own NVS) when it's switched on.
static XboxSeriesXControllerESP32_asukiaaa::Core xbox;
static TaskHandle_t bleHandle = nullptr;

// All Bluetooth on core 0, so the screen (core 1) never waits on it: the controller (as the LED
// panel runs it: every tick while connected or mid-pairing, else one 4 s scan every 15 s) and the
// adapter (sniff/monitor: its scans only while the controller's scanner is free). An update over
// WiFi (net/ota) has the radio: no scans until it ends.
static constexpr uint32_t PAD_SCAN_EVERY_MS = 15000;
static void bleTask(void*) {
    uint32_t lastPadScan = 0;
    for (;;) {
        if (ota::active()) {
            NimBLEScan* scan = NimBLEDevice::getScan();
            if (scan->isScanning()) scan->stop();
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const uint32_t now = millis();
        const bool padBusy = xbox.isConnected() || XboxSeriesXControllerESP32_asukiaaa::advDevice != nullptr ||
                             NimBLEDevice::getScan()->isScanning();
        if (padBusy) {
            xbox.onLoop();
        } else if (now - lastPadScan > PAD_SCAN_EVERY_MS) {
            lastPadScan = now;
            xbox.onLoop();  // starts a scan
        }
        const bool scanFree = XboxSeriesXControllerESP32_asukiaaa::advDevice == nullptr &&
                              !NimBLEDevice::getScan()->isScanning();
        sniff::monitor::tick(millis(), scanFree);
        vTaskDelay(pdMS_TO_TICKS(2));
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
    sniff::store::begin();
#ifdef STORE_TEST
    // Test hook (-D STORE_TEST): a recording of 1,000 made-up F lines, to check the download.
    int n = 0;
    if (sniff::store::open(&n)) {
        char line[40];
        for (int i = 0; i < 1000; i++) {
            snprintf(line, sizeof line, "F %d %03X 00 %02X", i * 10, 0x100 + i % 32, i & 0xFF);
            sniff::store::write(line);
        }
        sniff::store::close();
    }
#endif
    // NimBLE 2 (the Xbox library inits it): bonding on for the controller, +9 dBm. Its bonds live in
    // NVS "nimble_bond", shared with the LED panel (the same controller, the same bond).
    xbox.begin();
    sniff::monitor::begin();
    // 8 KB: the Xbox library's and the adapter's connect and discovery (std::function, std::vector)
    xTaskCreatePinnedToCore(bleTask, "ble", 8192, nullptr, 1, &bleHandle, 0);
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
        Serial.printf("[mem] heap %u (min %u)  stack left: ble %u, ota %u, loop %u\n",
                      static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()),
                      bleHandle ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(bleHandle)) : 0u, ota::stackLeft(),
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    delay(10);
}
