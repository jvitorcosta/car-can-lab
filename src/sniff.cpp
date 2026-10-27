// car-can-lab's sniff firmware for the yellow deck (ESP32 CYD): watches the car's CAN bus through
// an ELM327 BLE adapter in silent monitor mode (docs/superpowers/specs/2026-10-27-sniff-firmware-design.md).
// It replaces the deck repo's LED panel on the deck while a session runs; both update each other
// over WiFi from a PC on the iPhone hotspot:
//   pio run -e sniff_ota -t upload          (here)
//   pio run -e ledpanel_ota -t upload       (in spotify-pokemon-deck: the lights come back)
// It reads the panel's touch calibration (NVS "touch"/"hom") and never writes it: with none
// stored, the screen says so and the Xbox controller still drives everything.
// Test hooks: -D STORE_TEST writes a recording of 1,000 made-up lines at boot (the download
// check); -D NIGHT_TEST forces night mode.
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
#include "sniff/screen.h"
#include "sniff/store.h"
#include "sniff/web.h"
#include "ui/backlight.h"
#include "util/daynight.h"
#include "util/otapage.h"
#include "util/version.h"

static TFT_eSPI tft;
static constexpr const char* MDNS_NAME = "cardeck-sniff";

static bool loadCalibration() {
    Preferences p;
    if (!p.begin("touch", true)) return false;  // read-only: the namespace is the LED panel's
    touchcal::Homography k;
    const bool ok = p.getBytesLength("hom") == sizeof(k) && p.getBytes("hom", &k, sizeof(k)) == sizeof(k);
    p.end();
    if (ok) touch::setCalibration(k);
    return ok;
}

// TFT_eSPI's ILI9341_2_DRIVER init leaves a flat gamma that washes this panel out; the standard
// ILI9341 tables, as the LED panel sets them.
static void vividGamma() {
    static const uint8_t POS[15] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                                    0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
    static const uint8_t NEG[15] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                                    0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
    tft.writecommand(0x26);
    tft.writedata(0x01);
    tft.writecommand(0xE0);
    for (uint8_t v : POS) tft.writedata(v);
    tft.writecommand(0xE1);
    for (uint8_t v : NEG) tft.writedata(v);
}

static void netTask(void*) {
    net::connectAny();
    while (!net::isOnline()) {
        net::loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    netclock::begin();  // SNTP: night mode and the recordings' date
    ota::begin(MDNS_NAME);
    sniff::web::begin();
    Serial.printf("[net] on: http://%s/recs?k=<REMOTE_TOKEN>\n", net::deviceIp().c_str());
    for (;;) {
        if (!ota::active()) sniff::web::loop();
        net::loop();  // rejoins after a drop
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

// This pass's inputs: a new touch press, the controller's button presses (edges) and the D-pad
// (held: repeats after 400 ms, every 150 ms; the sticks are ignored, as on the LED panel).
static sniff::screen::Input readInput() {
    sniff::screen::Input in{};
    static bool wasDown = false;
    static uint32_t loopStart = 0;
    if (loopStart == 0) loopStart = millis() | 1;
    int x = 0, y = 0;
    // the panel reports a phantom press at power-up: the first second is ignored
    const bool down = touch::read(&x, &y) && millis() - loopStart > 1000;
    if (down && !wasDown) {
        in.tap = true;
        in.x = x;
        in.y = y;
    }
    wasDown = down;

    in.pad = xbox.isConnected() && !xbox.isWaitingForFirstNotification();
    static bool padWas = false;
    if (in.pad != padWas) Serial.printf("[pad] %s\n", in.pad ? "connected" : "gone");
    padWas = in.pad;
    if (!in.pad) return in;
    const XboxControllerNotificationParser& n = xbox.xboxNotif;
    static bool pA = false, pB = false, pX = false, pY = false, pLB = false, pRB = false;
    auto edge = [](bool now, bool& prev) {
        const bool d = now && !prev;
        prev = now;
        return d;
    };
    in.a = edge(n.btnA, pA);
    in.b = edge(n.btnB, pB);
    in.bx = edge(n.btnX, pX);
    in.by = edge(n.btnY, pY);
    in.lb = edge(n.btnLB, pLB);
    in.rb = edge(n.btnRB, pRB);
    const int dx = (n.btnDirRight ? 1 : 0) - (n.btnDirLeft ? 1 : 0);
    const int dy = dx ? 0 : (n.btnDirDown ? 1 : 0) - (n.btnDirUp ? 1 : 0);
    static int lastDx = 0, lastDy = 0;
    static uint32_t repeatAt = 0;
    bool fire = false;
    if (dx || dy) {
        if (dx != lastDx || dy != lastDy) {
            fire = true;
            repeatAt = millis() + 400;
        } else if (static_cast<int32_t>(millis() - repeatAt) >= 0) {
            fire = true;
            repeatAt = millis() + 150;
        }
    }
    lastDx = dx;
    lastDy = dy;
    if (fire) {
        in.left = dx < 0;
        in.right = dx > 0;
        in.up = dy < 0;
        in.down = dy > 0;
    }
    return in;
}

// Night mode (18:00-06:00 local, util/daynight), checked once a second; the backlight drops
// before the repaint.
static void updateDayNight() {
    static uint32_t checkedAt = 0;
    static bool night = false;
    if (millis() - checkedAt < 1000) return;
    checkedAt = millis();
    int m = 0, y = 0, month = 0, day = 0;
    bool want = netclock::localNow(&m, &y, &month, &day) && daynight::isNight(m);
#ifdef NIGHT_TEST
    want = true;
#endif
    if (want == night) return;
    night = want;
    backlight::set(night);
    Serial.printf("[ui] night mode %s\n", night ? "on" : "off");
    sniff::screen::setNight(night);
}

// The update page holds the screen while an update arrives; true meanwhile.
static bool updateTick() {
    static otapage::Shown shown;
    const ota::View v = ota::view();
    switch (otapage::step(shown, v.phase, v.pct)) {
        case otapage::Draw::Page:
        case otapage::Draw::Bar: sniff::screen::updatePage(v.phase, v.pct); break;
        case otapage::Draw::Back: sniff::screen::redraw(); break;
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
    tft.setAttribute(UTF8_SWITCH, 0);  // the texts are Latin-1 bytes, as on the LED panel
    vividGamma();
    touch::begin();
    const bool calibrated = loadCalibration();
    sniff::screen::begin(&tft, calibrated);
    sniff::screen::boot();
    sniff::store::begin();
#ifdef STORE_TEST
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
    sniff::screen::redraw();
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
    // Up bootguard::HEALTHY_MS, online and the update listener started: this firmware has proven
    // itself (net/ota) and can take the next update (the LED panel, after the session).
    static uint32_t provenAt = 0;
    if (millis() - provenAt >= 1000) {
        provenAt = millis();
        if (net::isOnline() && (ota::listening() || !ota::enabled())) ota::onGoodPoll();
    }
    if (updateTick()) {
        delay(10);
        return;
    }
    updateDayNight();
    sniff::screen::loop(millis(), readInput());
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
