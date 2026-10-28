// Copied from spotify-pokemon-deck test/test_tapguard/test_tapguard.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/tapguard.h"

void setUp() {}
void tearDown() {}

void test_unarmed_passes_the_panel_through() {
    tapguard::Guard g;
    TEST_ASSERT_FALSE(g.armed());
    TEST_ASSERT_TRUE(g.update(true, 0));
    TEST_ASSERT_FALSE(g.update(false, 10));
    TEST_ASSERT_TRUE(g.update(true, 20));
}
// The tap that changed the screen: its finger is still down, and stays ignored until it lifts.
void test_the_finger_that_changed_the_screen_is_ignored() {
    tapguard::Guard g;
    g.arm();
    TEST_ASSERT_TRUE(g.armed());
    TEST_ASSERT_FALSE(g.update(true, 0));
    TEST_ASSERT_FALSE(g.update(true, 5000));  // held: still nothing, however long
    TEST_ASSERT_TRUE(g.armed());
}
// Lifted, the panel must read released for QUIET_MS before a touch counts again.
void test_cleared_after_the_quiet_time() {
    tapguard::Guard g;
    g.arm();
    g.update(true, 0);
    TEST_ASSERT_FALSE(g.update(false, 100));  // lifted
    TEST_ASSERT_FALSE(g.update(false, 100 + tapguard::QUIET_MS - 1));
    TEST_ASSERT_TRUE(g.armed());
    TEST_ASSERT_FALSE(g.update(false, 100 + tapguard::QUIET_MS));
    TEST_ASSERT_FALSE(g.armed());
    TEST_ASSERT_TRUE(g.update(true, 100 + tapguard::QUIET_MS + 10));  // the next real tap
}
// A double tap or a resistive bounce: the finger back down inside the quiet time is ignored, and the
// quiet time starts over from its lift.
void test_a_quick_second_touch_is_ignored_and_restarts_the_wait() {
    tapguard::Guard g;
    g.arm();
    g.update(true, 0);
    g.update(false, 50);
    TEST_ASSERT_FALSE(g.update(true, 120));  // the second tap of a double tap
    TEST_ASSERT_FALSE(g.update(false, 200));
    TEST_ASSERT_FALSE(g.update(false, 50 + tapguard::QUIET_MS));  // the first lift no longer counts
    TEST_ASSERT_TRUE(g.armed());
    TEST_ASSERT_FALSE(g.update(false, 200 + tapguard::QUIET_MS));
    TEST_ASSERT_FALSE(g.armed());
}
// Armed with no finger on the panel (the Xbox controller changed the screen): QUIET_MS from the
// first released reading.
void test_armed_without_a_finger() {
    tapguard::Guard g;
    g.arm();
    TEST_ASSERT_FALSE(g.update(false, 1000));
    TEST_ASSERT_FALSE(g.update(true, 1100));
    TEST_ASSERT_FALSE(g.update(false, 1200));
    TEST_ASSERT_FALSE(g.update(false, 1200 + tapguard::QUIET_MS));
    TEST_ASSERT_TRUE(g.update(true, 1200 + tapguard::QUIET_MS + 1));
}
// Armed again while waiting (a second screen change): the wait starts over.
void test_rearming_starts_over() {
    tapguard::Guard g;
    g.arm();
    g.update(false, 0);
    g.update(false, 200);
    g.arm();
    TEST_ASSERT_FALSE(g.update(false, tapguard::QUIET_MS));  // 250 after the first, not after this lift
    TEST_ASSERT_TRUE(g.armed());
    TEST_ASSERT_FALSE(g.update(false, 2 * tapguard::QUIET_MS));
    TEST_ASSERT_FALSE(g.armed());
}
// millis() wraps after ~49.7 days: the quiet time still counts across it.
void test_across_the_millis_wrap() {
    tapguard::Guard g;
    g.arm();
    g.update(false, 0xFFFFFF00u);
    TEST_ASSERT_TRUE(g.armed());
    g.update(false, 0xFFFFFF00u + tapguard::QUIET_MS);  // wrapped
    TEST_ASSERT_FALSE(g.armed());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_unarmed_passes_the_panel_through);
    RUN_TEST(test_the_finger_that_changed_the_screen_is_ignored);
    RUN_TEST(test_cleared_after_the_quiet_time);
    RUN_TEST(test_a_quick_second_touch_is_ignored_and_restarts_the_wait);
    RUN_TEST(test_armed_without_a_finger);
    RUN_TEST(test_rearming_starts_over);
    RUN_TEST(test_across_the_millis_wrap);
    return UNITY_END();
}
