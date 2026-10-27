// Copied from spotify-pokemon-deck test/test_termpal/test_termpal.cpp @ 9ced38d.
#include <unity.h>
#include <initializer_list>
#include "../../src/util/termpal.h"

using namespace termpal;

void setUp() {}
void tearDown() {}

static uint32_t luma(uint32_t c) { return (77 * ((c >> 16) & 0xFF) + 151 * ((c >> 8) & 0xFF) + 28 * (c & 0xFF)) >> 8; }
static uint32_t red(uint32_t c) { return (c >> 16) & 0xFF; }
static uint32_t green(uint32_t c) { return (c >> 8) & 0xFF; }
static uint32_t blue(uint32_t c) { return c & 0xFF; }
static bool near(double want, double got, double tol) { return got > want - tol && got < want + tol; }

void test_values() {
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, false));
    TEST_ASSERT_EQUAL_HEX32(0xAAFF8C, rgb(TEXT, false));
    TEST_ASSERT_EQUAL_HEX32(0x5ABE28, rgb(MID, false));
    TEST_ASSERT_EQUAL_HEX32(0x308A24, rgb(DIM, false));
    TEST_ASSERT_EQUAL_HEX32(0xFF3C28, rgb(ALERT, false));
    TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, true));
    TEST_ASSERT_EQUAL_HEX32(0xFF6600, rgb(TEXT, true));
    TEST_ASSERT_EQUAL_HEX32(0xC84E00, rgb(MID, true));
    TEST_ASSERT_EQUAL_HEX32(0xA03A00, rgb(DIM, true));
    TEST_ASSERT_EQUAL_HEX32(0xFF2A1A, rgb(ALERT, true));
    TEST_ASSERT_EQUAL_HEX32(0x030503, scan(false));
    TEST_ASSERT_EQUAL_HEX32(0x050300, scan(true));
    TEST_ASSERT_EQUAL_HEX32(0xFF0090, tile(0xFF0090, false));
    TEST_ASSERT_EQUAL_HEX32(0xB20064, tile(0xFF0090, true));
}

// WCAG 2.x: black on black is 1:1, white 21:1, and known greys and greens give their known figures.
void test_contrast_on_black() {
    TEST_ASSERT_TRUE(near(1.0, contrastOnBlack(0x000000), 1e-9));
    TEST_ASSERT_TRUE(near(21.0, contrastOnBlack(0xFFFFFF), 1e-9));
    TEST_ASSERT_TRUE(near(4.69, contrastOnBlack(0x777777), 0.01));
    TEST_ASSERT_TRUE(near(15.30, contrastOnBlack(0x00FF00), 0.01));
    TEST_ASSERT_TRUE(contrastOnBlack(0x0000FF) < contrastOnBlack(0xFF0000));  // blue weighs least
}

// Read at a glance in a moving car: by day every role at least 4.5:1 on black (DIM included), at
// night (a dimmer backlight on top) at least 3:1; and the three brightnesses keep their order.
void test_contrast_thresholds() {
    for (int night = 0; night < 2; night++) {
        const double least = night ? 3.0 : 4.5;
        for (int r = TEXT; r <= ALERT; r++) {
            TEST_ASSERT_TRUE_MESSAGE(contrastOnBlack(rgb(static_cast<Role>(r), night)) >= least, night ? "night role" : "day role");
        }
        TEST_ASSERT_TRUE(contrastOnBlack(rgb(TEXT, night)) > contrastOnBlack(rgb(MID, night)));
        TEST_ASSERT_TRUE(contrastOnBlack(rgb(MID, night)) > contrastOnBlack(rgb(DIM, night)));
    }
}

// The hues: night is amber (no role with more blue than red) and dimmer than day; the day's darker
// greens carry little blue (the panel turns a blue-green to teal); ALERT is red in both modes and a
// hue apart from TEXT.
void test_hues() {
    for (int r = BG; r <= ALERT; r++) {
        const uint32_t n = rgb(static_cast<Role>(r), true);
        TEST_ASSERT_TRUE(blue(n) <= red(n));
        if (r != BG) TEST_ASSERT_TRUE(luma(n) < luma(rgb(static_cast<Role>(r), false)));
    }
    for (Role r : {MID, DIM}) TEST_ASSERT_TRUE(blue(rgb(r, false)) < green(rgb(r, false)) / 2);
    TEST_ASSERT_TRUE(green(rgb(TEXT, false)) > red(rgb(TEXT, false)));
    for (int night = 0; night < 2; night++) {
        const uint32_t a = rgb(ALERT, night);
        TEST_ASSERT_TRUE(a != rgb(TEXT, night));
        TEST_ASSERT_TRUE(red(a) > green(a));
    }
    TEST_ASSERT_TRUE(luma(rgb(ALERT, true)) < luma(rgb(ALERT, false)));
}

// Night stays amber, not olive: every role but BG and ALERT has green at most 0.6 x red.
void test_night_warm() {
    for (Role r : {TEXT, MID, DIM}) {
        const uint32_t n = rgb(r, true);
        TEST_ASSERT_TRUE(green(n) * 10 <= red(n) * 6);
    }
}

// A night problem must read redder than the amber: ALERT's green/red is under half of MID's.
void test_night_alert_redder() {
    const double alert = static_cast<double>(green(rgb(ALERT, true))) / red(rgb(ALERT, true));
    const double mid = static_cast<double>(green(rgb(MID, true))) / red(rgb(MID, true));
    TEST_ASSERT_TRUE(alert < 0.5 * mid);
}

// ---- Themes: the terminal in the INSIDE light's colour (a ledlook swatch index, -1 the default) ----
static constexpr int N_THEME = 8;  // red, orange, pink, purple, blue, green, white, yellow (ledlook's order)
static constexpr int RED_T = 0, ORANGE_T = 1, PINK_T = 2, PURPLE_T = 3, BLUE_T = 4, GREEN_T = 5, WHITE_T = 6, YELLOW_T = 7;
static bool warmAlertTheme(int t) { return t == RED_T || t == ORANGE_T || t == PINK_T; }
static uint32_t chanDiff(uint32_t a, uint32_t b) {
    uint32_t d = 0;
    for (int s = 0; s <= 16; s += 8) {
        const uint32_t x = (a >> s) & 0xFF, y = (b >> s) & 0xFF, e = x > y ? x - y : y - x;
        if (e > d) d = e;
    }
    return d;
}

// The default theme is today's palette, whatever asks for it (-1, or an index out of range).
void test_theme_default() {
    for (int night = 0; night < 2; night++) {
        for (int r = BG; r <= ALERT; r++) {
            const Role role = static_cast<Role>(r);
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, -1));
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, N_THEME));
            TEST_ASSERT_EQUAL_HEX32(rgb(role, night), rgb(role, night, -2));
        }
    }
}

// The owner's fixed points: green by day is the Seegson green, white at night is today's amber night
// (no white glare), and the warm themes' problems are white (dimmer at night).
void test_theme_fixed() {
    for (int r = TEXT; r <= ALERT; r++) {
        const Role role = static_cast<Role>(r);
        TEST_ASSERT_EQUAL_HEX32(rgb(role, false), rgb(role, false, GREEN_T));
        TEST_ASSERT_EQUAL_HEX32(rgb(role, true), rgb(role, true, WHITE_T));
    }
    for (int t : {RED_T, ORANGE_T, PINK_T}) {
        TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, rgb(ALERT, false, t));
        TEST_ASSERT_TRUE(rgb(ALERT, true, t) < 0xFFFFFF);
    }
}

// Every theme, day and night: BG black; by day TEXT >= 7:1, MID and DIM >= 4.5:1; at night every role
// >= 3:1; TEXT > MID > DIM; ALERT as readable as DIM; and every role dimmer at night than by day.
void test_theme_contrast() {
    for (int t = -1; t < N_THEME; t++) {
        for (int night = 0; night < 2; night++) {
            TEST_ASSERT_EQUAL_HEX32(0, rgb(BG, night, t));
            const double text = contrastOnBlack(rgb(TEXT, night, t)), mid = contrastOnBlack(rgb(MID, night, t)),
                         dim = contrastOnBlack(rgb(DIM, night, t)), alert = contrastOnBlack(rgb(ALERT, night, t));
            TEST_ASSERT_TRUE_MESSAGE(text >= (night ? 3.0 : 7.0), "TEXT contrast");
            TEST_ASSERT_TRUE_MESSAGE(mid >= (night ? 3.0 : 4.5), "MID contrast");
            TEST_ASSERT_TRUE_MESSAGE(dim >= (night ? 3.0 : 4.5), "DIM contrast");
            TEST_ASSERT_TRUE_MESSAGE(alert >= (night ? 3.0 : 4.5), "ALERT contrast");
            TEST_ASSERT_TRUE_MESSAGE(text > mid && mid > dim, "TEXT > MID > DIM");
        }
        for (int r = TEXT; r <= ALERT; r++) {
            const Role role = static_cast<Role>(r);
            TEST_ASSERT_TRUE_MESSAGE(luma(rgb(role, true, t)) < luma(rgb(role, false, t)), "night dimmer");
        }
    }
}

// Each theme reads as its light's colour (TEXT, MID, DIM by the order of their channels): white by
// day a neutral grey-white, at night the amber.
void test_theme_hues() {
    for (int night = 0; night < 2; night++) {
        for (Role role : {TEXT, MID, DIM}) {
            uint32_t c = rgb(role, night, RED_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && red(c) > blue(c) && red(c) >= 0xC0, "red");
            c = rgb(role, night, ORANGE_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && green(c) > blue(c), "orange");
            c = rgb(role, night, PINK_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) > blue(c) && blue(c) > green(c), "pink");
            c = rgb(role, night, PURPLE_T);
            TEST_ASSERT_TRUE_MESSAGE(blue(c) > red(c) && red(c) > green(c), "purple");
            c = rgb(role, night, BLUE_T);
            TEST_ASSERT_TRUE_MESSAGE(blue(c) > green(c) && green(c) > red(c), "blue");
            c = rgb(role, night, GREEN_T);
            TEST_ASSERT_TRUE_MESSAGE(green(c) > red(c) && red(c) > blue(c), "green");
            c = rgb(role, night, YELLOW_T);
            TEST_ASSERT_TRUE_MESSAGE(red(c) >= green(c) && green(c) > 2 * blue(c), "yellow");
            c = rgb(role, night, WHITE_T);
            if (night) TEST_ASSERT_TRUE_MESSAGE(red(c) > green(c) && green(c) > blue(c), "white night: amber");
            else TEST_ASSERT_TRUE_MESSAGE(chanDiff(c, (red(c) << 16) | (red(c) << 8) | red(c)) <= 0x10, "white day: grey");
        }
    }
}

// The panel's lessons: dark greens carry little blue (it turns them teal), and the night ambers keep
// green at most 0.6 x red (it turns them olive), on the default theme and on orange, yellow, white.
void test_theme_panel_guards() {
    for (int t : {-1, GREEN_T}) {
        for (int night = 0; night < 2; night++) {
            for (Role role : {MID, DIM}) TEST_ASSERT_TRUE_MESSAGE(blue(rgb(role, night, t)) < green(rgb(role, night, t)) / 2, "teal");
        }
    }
    for (int t : {-1, ORANGE_T, YELLOW_T, WHITE_T}) {
        for (Role role : {TEXT, MID, DIM}) {
            const uint32_t n = rgb(role, true, t);
            TEST_ASSERT_TRUE_MESSAGE(green(n) * 10 <= red(n) * 6, "olive");
        }
    }
}

// ALERT: white on the red / orange / pink themes, red on the rest; and always clearly apart from the
// theme's TEXT: a channel at least 0x60 apart, or (two warm colours) a green/red under half TEXT's
// and at least 0.1 under it.
void test_theme_alert() {
    for (int t = -1; t < N_THEME; t++) {
        for (int night = 0; night < 2; night++) {
            const uint32_t a = rgb(ALERT, night, t), tx = rgb(TEXT, night, t);
            if (t >= 0 && warmAlertTheme(t)) {
                TEST_ASSERT_TRUE_MESSAGE(red(a) == green(a) && green(a) == blue(a) && red(a) >= 0xB0, "white alert");
            } else {
                TEST_ASSERT_TRUE_MESSAGE(red(a) >= 0xF0 && green(a) * 3 < red(a) && blue(a) * 3 < red(a), "red alert");
            }
            // The warm-colour branch is for white's night (and the default night): today's amber TEXT
            // FF6600 with today's red ALERT FF2A1A, only 0x3C apart; every other theme clears 0x60.
            const double ga = red(a) ? static_cast<double>(green(a)) / red(a) : 1.0,
                         gt = red(tx) ? static_cast<double>(green(tx)) / red(tx) : 0.0;
            const bool apart = chanDiff(a, tx) >= 0x60 || (2.0 * ga < gt && gt - ga >= 0.1);
            TEST_ASSERT_TRUE_MESSAGE(apart, "ALERT apart from TEXT");
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_values);
    RUN_TEST(test_contrast_on_black);
    RUN_TEST(test_contrast_thresholds);
    RUN_TEST(test_hues);
    RUN_TEST(test_night_warm);
    RUN_TEST(test_night_alert_redder);
    RUN_TEST(test_theme_default);
    RUN_TEST(test_theme_fixed);
    RUN_TEST(test_theme_contrast);
    RUN_TEST(test_theme_hues);
    RUN_TEST(test_theme_panel_guards);
    RUN_TEST(test_theme_alert);
    return UNITY_END();
}
