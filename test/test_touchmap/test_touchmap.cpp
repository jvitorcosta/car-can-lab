// Copied from spotify-pokemon-deck test/test_touchmap/test_touchmap.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/touchmap.h"

void setUp() {}
void tearDown() {}

static const touchmap::Cal C{200, 3900, 200, 3900};

void test_corners_and_centre() {
    int x, y;
    touchmap::toScreen(C, 200, 200, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    TEST_ASSERT_EQUAL_INT(0, y);
    touchmap::toScreen(C, 3900, 3900, &x, &y);
    TEST_ASSERT_EQUAL_INT(319, x);
    TEST_ASSERT_EQUAL_INT(239, y);
    touchmap::toScreen(C, 2050, 2050, &x, &y);
    TEST_ASSERT_EQUAL_INT(159, x);
    TEST_ASSERT_EQUAL_INT(119, y);
}
void test_outside_the_calibration_is_clamped() {
    int x, y;
    touchmap::toScreen(C, 50, 4095, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    TEST_ASSERT_EQUAL_INT(239, y);
}
void test_reversed_calibration_flips_the_axis() {
    const touchmap::Cal flip{3900, 200, 200, 3900};      // raw x falls left to right
    int x, y;
    touchmap::toScreen(flip, 3900, 200, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    touchmap::toScreen(flip, 200, 200, &x, &y);
    TEST_ASSERT_EQUAL_INT(319, x);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_corners_and_centre);
    RUN_TEST(test_outside_the_calibration_is_clamped);
    RUN_TEST(test_reversed_calibration_flips_the_axis);
    return UNITY_END();
}
