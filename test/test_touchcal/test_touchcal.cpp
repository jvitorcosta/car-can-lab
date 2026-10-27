// Copied from spotify-pokemon-deck test/test_touchcal/test_touchcal.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/touchcal.h"

void setUp() {}
void tearDown() {}

// The calibration screen's corner targets (top-left, top-right, bottom-right, bottom-left).
static const int SX[4] = {24, 296, 296, 24};
static const int SY[4] = {24, 24, 216, 216};

void test_fits_a_plain_scale_and_offset() {
    // sx = rx/10 - 50, sy = ry/12.5 - 40
    const int rx[4] = {740, 3460, 3460, 740}, ry[4] = {800, 800, 3200, 3200};
    touchcal::Homography k;
    TEST_ASSERT_TRUE(touchcal::fit(rx, ry, SX, SY, &k));
    TEST_ASSERT_LESS_OR_EQUAL(1, touchcal::error(k, 2100, 2000, 160, 120));
}
void test_fits_swapped_and_flipped_axes() {
    // raw x drives screen y, raw y drives screen x backwards
    const int rx[4] = {600, 600, 3000, 3000}, ry[4] = {3400, 600, 600, 3400};
    touchcal::Homography k;
    TEST_ASSERT_TRUE(touchcal::fit(rx, ry, SX, SY, &k));
    TEST_ASSERT_LESS_OR_EQUAL(1, touchcal::error(k, 1800, 2000, 160, 120));
}
// The old deck's panel, pen taps on 2026-10-21 (median of 8 attempts per target): rotated ~30
// degrees and not a parallelogram. An affine fit missed by 20-45 px; this must land near the centre.
void test_fits_the_old_decks_skewed_panel() {
    const int rx[4] = {1428, 3410, 2724, 448}, ry[4] = {1896, 655, 2771, 3556};
    touchcal::Homography k;
    TEST_ASSERT_TRUE(touchcal::fit(rx, ry, SX, SY, &k));
    for (int i = 0; i < 4; i++) TEST_ASSERT_LESS_OR_EQUAL(1, touchcal::error(k, rx[i], ry[i], SX[i], SY[i]));
    TEST_ASSERT_LESS_OR_EQUAL(12, touchcal::error(k, 1946, 2092, 160, 120));
}
void test_rejects_taps_in_one_spot() {
    const int rx[4] = {900, 900, 900, 900}, ry[4] = {1200, 1200, 1200, 1200};
    touchcal::Homography k;
    TEST_ASSERT_FALSE(touchcal::fit(rx, ry, SX, SY, &k));
}
void test_apply_clamps_to_the_screen() {
    const touchcal::Homography k{{1, 0, 0, 0, 1, 0, 0, 0}};
    int x, y;
    touchcal::apply(k, -20, 9000, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    TEST_ASSERT_EQUAL_INT(239, y);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fits_a_plain_scale_and_offset);
    RUN_TEST(test_fits_swapped_and_flipped_axes);
    RUN_TEST(test_fits_the_old_decks_skewed_panel);
    RUN_TEST(test_rejects_taps_in_one_spot);
    RUN_TEST(test_apply_clamps_to_the_screen);
    return UNITY_END();
}
