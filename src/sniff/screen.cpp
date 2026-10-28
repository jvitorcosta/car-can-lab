#include "screen.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstring>
#include "../net/wifi.h"
#include "../ui/termfont.h"
#include "../util/idtable.h"
#include "../util/otapage.h"
#include "../util/sniffview.h"
#include "../util/tapguard.h"
#include "../util/tapsettle.h"
#include "../util/termpal.h"
#include "../util/version.h"
#include "monitor.h"
#include "store.h"

namespace sniff {
namespace screen {
using termfont::Face;

static TFT_eSPI* tft = nullptr;
static bool night = false, calibrated = true;
enum class Page : uint8_t { Live, Home, Logs };
static Page page = Page::Live;

// ---- layout (320 x 240) ----
static constexpr int W = 320, H = 240;
static constexpr int HEADER_Y = 3, RULE_Y = 21;
static constexpr int ROW_Y0 = 24, ROW_H = 19, N_ROWS = 9, ROW_X = 6;
static constexpr int BTN_Y = 196, BTN_H = 44, N_BTN = 5, BTN_W = W / N_BTN;
static const char* const BTN[N_BTN] = {"PAUSE", "FILTER", "MARK", "REC", "BACK"};
enum Btn { B_PAUSE, B_FILTER, B_MARK, B_REC, B_BACK };
static constexpr uint32_t REFRESH_MS = 100, MESSAGE_MS = 1200;
struct Rect {
    int x, y, w, h;
    bool has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
static const Rect HOME_LIVE = {20, 64, 130, 64}, HOME_LOGS = {170, 64, 130, 64};
static const Rect LOGS_BACK = {W - 100, BTN_Y, 100, BTN_H};

// ---- drawing helpers (as the LED panel's) ----
static uint16_t c565(uint32_t rgb) { return tft->color565(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF); }
static uint32_t rgbOf(termpal::Role r) { return termpal::rgb(r, night); }
static uint16_t pal(termpal::Role r) { return c565(rgbOf(r)); }
static uint16_t mix(termpal::Role a, termpal::Role b, uint8_t t) {  // a at 0, b at 255
    const uint32_t x = rgbOf(a), y = rgbOf(b);
    auto ch = [&](int s) {
        const int ca = (x >> s) & 0xFF, cb = (y >> s) & 0xFF;
        return static_cast<uint32_t>(ca + (cb - ca) * t / 255) << s;
    };
    return c565(ch(16) | ch(8) | ch(0));
}

static int text(const char* s, int x, int y, Face f, uint8_t datum) {
    static int shown = -1;
    if (static_cast<int>(f) != shown) {
        shown = static_cast<int>(f);
        tft->setFreeFont(reinterpret_cast<const GFXfont*>(&termfont::font(f)));
    }
    const int cap = termfont::capHeight(f), v = datum / 3;
    const int base = v == 0 ? y + cap : v == 1 ? y + cap / 2 : y;
    tft->setTextDatum(L_BASELINE + datum % 3);
    return tft->drawString(s, x, base);
}

static void clearBg(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    tft->fillRect(x, y, w, h, pal(termpal::BG));
    const uint16_t line = c565(termpal::scan(night));
    for (int row = y + (3 - y % 3) % 3; row < y + h; row += 3) tft->drawFastHLine(x, row, w, line);
}

static void box(const Rect& r, const char* label, bool lit, bool focus, Face f = Face::SMALL) {
    clearBg(r.x, r.y, r.w, r.h);
    if (lit) tft->fillRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, pal(termpal::TEXT));
    else tft->drawRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, pal(termpal::MID));
    if (focus) tft->drawRect(r.x, r.y, r.w, r.h, pal(termpal::TEXT));
    tft->setTextColor(pal(lit ? termpal::BG : termpal::TEXT));
    text(label, r.x + r.w / 2, r.y + r.h / 2, f, MC_DATUM);
}

// ---- state of the pages ----
static idtable::IdTable shownTable;  // the copy being drawn (~6 KB, off the stack)
static uint16_t shownIds[N_ROWS];
static int nShown = 0;
struct RowCache {
    char text[40];
    bool lit, dim, cursor;
};
static RowCache cache[N_ROWS];
static bool rowsValid = false;
static char headerShown[64] = "";
static const char* statusShown = nullptr;
static bool paused = false;
static int cursor = -1;  // the Xbox cursor's row; -1: none
static char message[40] = "";
static bool messageAlert = false;
static uint32_t messageAt = 0;
static bool btnLit[N_BTN] = {};
static int homeFocus = 0;

static void setMessage(const char* m, bool alert, uint32_t nowMs) {
    snprintf(message, sizeof message, "%s", m);
    messageAlert = alert;
    messageAt = nowMs | 1;
    headerShown[0] = '\0';  // redraw
}

static void drawButtons(const monitor::View& v) {
    const bool lit[N_BTN] = {paused, v.filterOn, false, v.recording, false};
    for (int i = 0; i < N_BTN; i++) {
        btnLit[i] = lit[i];
        box(Rect{i * BTN_W, BTN_Y, BTN_W, BTN_H}, BTN[i], lit[i], false);
    }
}

static void drawHeader(const char* s, bool alert) {
    clearBg(0, 0, W, RULE_Y);
    tft->setTextColor(pal(alert ? termpal::ALERT : termpal::TEXT));
    text(s, ROW_X, HEADER_Y, Face::SMALL, TL_DATUM);
    tft->drawFastHLine(0, RULE_Y, W, pal(termpal::MID));
}

static void drawRow(int i, const idtable::Row& r, uint32_t nowMs, bool isCursor) {
    const int y = ROW_Y0 + i * ROW_H;
    clearBg(0, y, W, ROW_H);
    char t[40];
    sniffview::row(t, sizeof t, r);
    const bool dim = idtable::IdTable::idle(r, nowMs);
    const int cell = termfont::cell(Face::BODY);
    const int ty = y + (ROW_H - termfont::capHeight(Face::BODY)) / 2;
    char idPart[6];
    memcpy(idPart, t, 5);
    idPart[5] = '\0';
    tft->setTextColor(pal(dim ? termpal::DIM : termpal::TEXT));
    text(idPart, ROW_X, ty, Face::BODY, TL_DATUM);
    bool lit = false;
    for (int b = 0; b < r.len && b < 8; b++) {
        const int x = ROW_X + sniffview::byteColumn(b) * cell;
        const uint8_t f = idtable::IdTable::fade(r, b, nowMs);
        if (f) {
            lit = true;
            tft->fillRect(x - 1, y + 1, 2 * cell + 2, ROW_H - 2, mix(termpal::BG, termpal::TEXT, f));
            tft->setTextColor(f > 128 ? pal(termpal::BG) : pal(termpal::TEXT));
        } else {
            tft->setTextColor(pal(dim ? termpal::DIM : termpal::MID));
        }
        char hex[3] = {t[sniffview::byteColumn(b)], t[sniffview::byteColumn(b) + 1], '\0'};
        text(hex, x, ty, Face::BODY, TL_DATUM);
    }
    if (isCursor) tft->drawRect(0, y, W, ROW_H, pal(termpal::TEXT));
    RowCache& c = cache[i];
    snprintf(c.text, sizeof c.text, "%s", t);
    c.lit = lit;
    c.dim = dim;
    c.cursor = isCursor;
}

static void drawStatus(const char* s, bool alert, const char* hint) {
    clearBg(0, ROW_Y0, W, BTN_Y - ROW_Y0);
    tft->setTextColor(pal(alert ? termpal::ALERT : termpal::TEXT));
    text(s, W / 2, 90, Face::BODY, MC_DATUM);
    if (hint) {
        tft->setTextColor(pal(termpal::MID));
        text(hint, W / 2, 124, Face::SMALL, MC_DATUM);
    }
    rowsValid = false;
    nShown = 0;
}

static const char* hintFor(monplan::State s) {
    switch (s) {
        case monplan::State::Searching: return "ADAPTER IN? PHONE BLUETOOTH OFF?";
        case monplan::State::EngineOff: return "START THE ENGINE";
        case monplan::State::NotSilent: return "IT WOULD ACK ON THE BUS: NOT USED";
        case monplan::State::Lost: return "RECONNECTING...";
        default: return nullptr;
    }
}

// ---- LIVE ----
static void drawLive(uint32_t nowMs, bool full) {
    const monitor::View v = monitor::view(nowMs);
    if (full) {
        clearBg(0, 0, W, H);
        drawButtons(v);
        headerShown[0] = '\0';
        statusShown = nullptr;
        rowsValid = false;
    } else {
        const bool lit[N_BTN] = {paused, v.filterOn, false, v.recording, false};
        if (memcmp(lit, btnLit, sizeof lit) != 0) drawButtons(v);
    }
    // the header, or a message for MESSAGE_MS
    char h[64];
    bool alert = false;
    if (message[0] && nowMs - messageAt < MESSAGE_MS) {
        snprintf(h, sizeof h, "%s", message);
        alert = messageAlert;
    } else {
        message[0] = '\0';
        sniffview::header(h, sizeof h, v.ids, v.dropped, v.msgPerSec, v.recording, v.logPct, v.cycleMs);
        if (paused) snprintf(h, sizeof h, "PAUSED  %d IDs", v.ids);
        if (v.logFull && !v.recording) {  // sticky until the next REC: the recording stopped itself
            snprintf(h, sizeof h, "LOG FULL: DOWNLOAD REC %d", v.recNumber);
            alert = true;
        }
    }
    if (strcmp(h, headerShown) != 0) {
        drawHeader(h, alert);
        snprintf(headerShown, sizeof headerShown, "%s", h);
    }
    if (paused && rowsValid) return;
    // the status in words, or the rows
    const char* st = sniffview::statusText(v.state, v.inStateMs);
    bool stAlert = sniffview::isAlert(v.state, v.inStateMs);
    if (!st && v.nothingHeard && v.ids == 0) {
        st = "NOTHING HEARD";
        stAlert = true;
    }
    if (st) {
        if (st != statusShown) drawStatus(st, stAlert, v.state == monplan::State::Monitoring ? "QUIET BUS? A GATEWAY?" : hintFor(v.state));
        statusShown = st;
        return;
    }
    if (statusShown) {
        statusShown = nullptr;
        clearBg(0, ROW_Y0, W, BTN_Y - ROW_Y0);
        rowsValid = false;
    }
    monitor::copyTable(&shownTable);
    uint16_t ids[N_ROWS];
    const int n = shownTable.order(ids, N_ROWS);
    for (int i = 0; i < N_ROWS; i++) {
        if (i >= n) {
            if (!rowsValid || i < nShown) clearBg(0, ROW_Y0 + i * ROW_H, W, ROW_H);
            continue;
        }
        const idtable::Row* r = shownTable.find(ids[i]);
        char t[40];
        sniffview::row(t, sizeof t, *r);
        const bool lit = [&] {
            for (int b = 0; b < r->len && b < 8; b++)
                if (idtable::IdTable::fade(*r, b, nowMs)) return true;
            return false;
        }();
        const bool dim = idtable::IdTable::idle(*r, nowMs);
        const RowCache& c = cache[i];
        if (!rowsValid || i >= nShown || lit || c.lit || dim != c.dim || (cursor == i) != c.cursor || strcmp(t, c.text) != 0)
            drawRow(i, *r, nowMs, cursor == i);
    }
    memcpy(shownIds, ids, sizeof ids);
    nShown = n;
    if (cursor >= n) cursor = n - 1;
    rowsValid = true;
}

static void pressLive(int b, uint32_t nowMs) {
    switch (b) {
        case B_PAUSE:
            paused = !paused;
            headerShown[0] = '\0';
            break;
        case B_FILTER:
            if (!monitor::toggleFilter(nowMs)) setMessage("PICK ROWS FIRST (TAP THEM)", true, nowMs);
            break;
        case B_MARK: {
            const int n = monitor::mark(nowMs);
            char m[24];
            snprintf(m, sizeof m, "MARK %d", n);
            if (n > 0) setMessage(m, false, nowMs);
            else setMessage("REC FIRST, THEN MARK", true, nowMs);
            break;
        }
        case B_REC:
            if (!monitor::toggleRec(nowMs)) setMessage(store::ready() ? "LOG FULL" : "NO FLASH: CAN'T REC", true, nowMs);
            else if (!monitor::view(nowMs).recording) setMessage("REC STOPPED", false, nowMs);
            break;
        case B_BACK:
            page = Page::Home;
            redraw();
            break;
        default: break;
    }
}

static void pickRow(int i) {
    if (i < 0 || i >= nShown) return;
    monitor::togglePick(shownIds[i]);
    rowsValid = false;  // the * shows on the next refresh
}

static void inputLive(const Input& in, uint32_t nowMs) {
    if (in.tap) {
        if (in.y >= BTN_Y) pressLive(in.x / BTN_W, nowMs);
        else if (in.y >= ROW_Y0) pickRow((in.y - ROW_Y0) / ROW_H);
    }
    if (page != Page::Live) return;
    if (!in.pad) {
        if (cursor >= 0) rowsValid = false;
        cursor = -1;
    } else {
        if (in.up || in.down) {
            const int c = cursor < 0 ? 0 : cursor + (in.down ? 1 : -1);
            cursor = c < 0 ? 0 : c >= nShown ? (nShown > 0 ? nShown - 1 : 0) : c;
            rowsValid = false;
        }
        if (in.a) pickRow(cursor);
        if (in.bx) pressLive(B_MARK, nowMs);
        if (in.by) pressLive(B_REC, nowMs);
        if (in.lb) pressLive(B_FILTER, nowMs);
        if (in.rb) pressLive(B_PAUSE, nowMs);
        if (in.b) pressLive(B_BACK, nowMs);
    }
}

// ---- HOME ----
static void drawHome(uint32_t nowMs) {
    clearBg(0, 0, W, H);
    tft->setTextColor(pal(termpal::TEXT));
    text("CAR-CAN-LAB // SNIFF", ROW_X, 10, Face::TITLE, TL_DATUM);
    tft->drawFastHLine(0, 44, W, pal(termpal::MID));
    box(HOME_LIVE, "[LIVE]", false, homeFocus == 0, Face::BODY);
    box(HOME_LOGS, "[LOGS]", false, homeFocus == 1, Face::BODY);
    const monitor::View v = monitor::view(nowMs);
    const char* st = sniffview::statusText(v.state, v.inStateMs);
    char line[48];
    if (st) snprintf(line, sizeof line, "ADAPTER: %s", st);
    else snprintf(line, sizeof line, "ADAPTER: MONITORING, %d IDs", v.ids);
    tft->setTextColor(pal(st && sniffview::isAlert(v.state, v.inStateMs) ? termpal::ALERT : termpal::TEXT));
    text(line, ROW_X, 146, Face::SMALL, TL_DATUM);
    tft->setTextColor(pal(termpal::ALERT));
    text("LIGHTS: FLASH LEDPANEL BACK", ROW_X, 170, Face::SMALL, TL_DATUM);
    if (!calibrated) {
        tft->setTextColor(pal(termpal::ALERT));
        text("TOUCH UNCALIBRATED: HOLD BOOT 2 S", ROW_X, 194, Face::SMALL, TL_DATUM);
    }
    tft->setTextColor(pal(termpal::DIM));
    text(FW_LABEL, ROW_X, 218, Face::SMALL, TL_DATUM);
}

static void drawLogs() {
    clearBg(0, 0, W, H);
    tft->setTextColor(pal(termpal::TEXT));
    text("LOGS", ROW_X, 6, Face::TITLE, TL_DATUM);
    char used[24];
    snprintf(used, sizeof used, "FLASH %d%% USED", store::usedPct());
    tft->setTextColor(pal(termpal::MID));
    text(used, W - ROW_X, 10, Face::SMALL, TR_DATUM);
    tft->drawFastHLine(0, 36, W, pal(termpal::MID));
    int nums[64];
    size_t sizes[64];
    const int n = store::list(nums, sizes, 64);
    const int first = n > 6 ? n - 6 : 0;  // the newest six
    tft->setTextColor(pal(termpal::TEXT));
    if (n == 0) text(store::ready() ? "NO RECORDINGS YET" : "FLASH NOT MOUNTED", ROW_X, 50, Face::BODY, TL_DATUM);
    for (int i = first; i < n; i++) {
        char l[32];
        snprintf(l, sizeof l, "%03d.log  %u KB", nums[i], static_cast<unsigned>((sizes[i] + 1023) / 1024));
        text(l, ROW_X, 44 + (i - first) * 21, Face::BODY, TL_DATUM);
    }
    String ip = net::isOnline() ? net::deviceIp() : String("172.20.10.11");
    char url[64];
    snprintf(url, sizeof url, "http://%s/recs?k=<token>", ip.c_str());
    tft->setTextColor(pal(termpal::MID));
    text(url, ROW_X, 176, Face::SMALL, TL_DATUM);
    box(LOGS_BACK, "BACK", false, false);
}

void begin(TFT_eSPI* t, bool cal) {
    tft = t;
    calibrated = cal;
}

void boot() {
    clearBg(0, 0, W, H);
    static const char* const LINES[] = {"CAR-CAN-LAB // SNIFF", "LISTEN ONLY: SILENT MONITOR", "LIGHTS: FLASH LEDPANEL BACK"};
    for (int i = 0; i < 3; i++) {
        tft->setTextColor(pal(i == 2 ? termpal::ALERT : termpal::TEXT));
        text(LINES[i], ROW_X, 20 + i * 24, Face::BODY, TL_DATUM);
        delay(250);
    }
    if (!calibrated) {
        tft->setTextColor(pal(termpal::ALERT));
        text("TOUCH UNCALIBRATED: HOLD BOOT 2 S", ROW_X, 100, Face::SMALL, TL_DATUM);
    }
    tft->setTextColor(pal(termpal::DIM));
    text(FW_LABEL, ROW_X, 218, Face::SMALL, TL_DATUM);
    delay(600);
    page = Page::Live;
}

// Touch: one tap per press where the finger settled (util/tapsettle); a page drawn anew waits for
// the finger to lift first (util/tapguard), so the tap that opened it can't press what lies under it.
static tapguard::Guard tapGuard;
static tapsettle::Settle tapSettle;

void redraw() {
    tapGuard.arm();
    const uint32_t now = millis();
    if (page == Page::Live) drawLive(now, true);
    else if (page == Page::Home) drawHome(now);
    else drawLogs();
}

void setCalibrated(bool cal) { calibrated = cal; }

void setNight(bool n) {
    if (n == night) return;
    night = n;
    redraw();
}

void loop(uint32_t nowMs, const Input& raw) {
    static uint32_t refreshAt = 0, homeAt = 0;
    Input in = raw;
    in.tap = tapSettle.update(tapGuard.update(raw.touchDown, nowMs), raw.x, raw.y, nowMs, &in.x, &in.y);
    switch (page) {
        case Page::Live:
            inputLive(in, nowMs);
            if (page == Page::Live && nowMs - refreshAt >= REFRESH_MS) {
                refreshAt = nowMs;
                drawLive(nowMs, false);
            }
            break;
        case Page::Home: {
            int go = -1;
            if (in.tap) go = HOME_LIVE.has(in.x, in.y) ? 0 : HOME_LOGS.has(in.x, in.y) ? 1 : -1;
            if (in.pad && (in.left || in.right)) {
                homeFocus = in.right ? 1 : 0;
                drawHome(nowMs);
            }
            if (in.pad && in.a) go = homeFocus;
            if (go == 0 || (in.pad && in.b)) {
                page = Page::Live;
                redraw();
            } else if (go == 1) {
                page = Page::Logs;
                redraw();
            } else if (nowMs - homeAt >= 2000) {  // the adapter line stays current
                homeAt = nowMs;
                drawHome(nowMs);
            }
            break;
        }
        case Page::Logs:
            if ((in.tap && LOGS_BACK.has(in.x, in.y)) || (in.pad && in.b)) {
                page = Page::Home;
                redraw();
            }
            break;
    }
}

void updatePage(otaview::Phase p, int pct) {
    clearBg(0, 0, W, H);
    tft->setTextColor(pal(otapage::alert(p) ? termpal::ALERT : termpal::TEXT));
    text(otapage::title(p), W / 2, 80, Face::BIG, TC_DATUM);
    const int full = otapage::cells(pct);
    for (int k = 0; k < otapage::CELLS; k++) {
        const int x = 40 + k * 24;
        if (k < full) tft->fillRect(x, 150, 20, 16, pal(termpal::TEXT));
        else tft->drawRect(x, 150, 20, 16, pal(termpal::MID));
    }
}
}
}
