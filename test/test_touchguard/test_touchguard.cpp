// Copied from spotify-pokemon-deck test/test_touchguard/test_touchguard.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/touchguard.h"

void setUp() {}
void tearDown() {}

void test_a_normal_press_passes() {
    touchguard::Guard g;
    TEST_ASSERT_TRUE(g.update(true, 1000, 2000, 0));
    TEST_ASSERT_TRUE(g.update(true, 1010, 1990, 4000));
    TEST_ASSERT_FALSE(g.update(false, 0, 0, 4100));
}
void test_a_stuck_point_is_ignored_after_five_seconds() {
    touchguard::Guard g;
    TEST_ASSERT_TRUE(g.update(true, 0, 3977, 0));
    TEST_ASSERT_TRUE(g.update(true, 0, 3980, 4999));
    TEST_ASSERT_FALSE(g.update(true, 0, 3975, 5000));
    TEST_ASSERT_FALSE(g.update(true, 0, 3977, 60000));          // stays ignored
}
void test_moving_finger_is_not_stuck() {
    touchguard::Guard g;
    g.update(true, 1000, 1000, 0);
    g.update(true, 1500, 1000, 3000);                            // moved: the clock restarts
    TEST_ASSERT_TRUE(g.update(true, 1500, 1000, 7000));
}
void test_release_clears_the_stuck_state() {
    touchguard::Guard g;
    g.update(true, 0, 3977, 0);
    g.update(true, 0, 3977, 6000);
    TEST_ASSERT_FALSE(g.update(false, 0, 0, 6100));
    TEST_ASSERT_TRUE(g.update(true, 2000, 2000, 6200));          // a real touch again
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_normal_press_passes);
    RUN_TEST(test_a_stuck_point_is_ignored_after_five_seconds);
    RUN_TEST(test_moving_finger_is_not_stuck);
    RUN_TEST(test_release_clears_the_stuck_state);
    return UNITY_END();
}
