#include <unity.h>
#include "../../src/util/tapsettle.h"

using tapsettle::Settle;

void setUp() {}
void tearDown() {}

// A press acts once, at the average of its first SETTLE_MS (a fingertip's first readings jump).
void test_press_acts_once_at_the_settled_average() {
    Settle s;
    int x = 0, y = 0;
    TEST_ASSERT_FALSE(s.update(true, 100, 50, 0, &x, &y));
    TEST_ASSERT_FALSE(s.update(true, 110, 54, 20, &x, &y));
    TEST_ASSERT_TRUE(s.update(true, 120, 58, tapsettle::SETTLE_MS, &x, &y));
    TEST_ASSERT_EQUAL(110, x);
    TEST_ASSERT_EQUAL(54, y);
    TEST_ASSERT_FALSE(s.update(true, 300, 200, 100, &x, &y));  // held: nothing more
    TEST_ASSERT_FALSE(s.update(false, 0, 0, 120, &x, &y));      // lifted: nothing
}

// Settled in time but too few readings: wait for the third.
void test_needs_three_readings() {
    Settle s;
    int x = 0, y = 0;
    TEST_ASSERT_FALSE(s.update(true, 10, 10, 0, &x, &y));
    TEST_ASSERT_FALSE(s.update(true, 20, 20, 60, &x, &y));
    TEST_ASSERT_TRUE(s.update(true, 30, 30, 70, &x, &y));
    TEST_ASSERT_EQUAL(20, x);
}

// A tap shorter than SETTLE_MS acts on release with what it had.
void test_quick_tap_acts_on_release() {
    Settle s;
    int x = 0, y = 0;
    TEST_ASSERT_FALSE(s.update(true, 200, 100, 0, &x, &y));
    TEST_ASSERT_FALSE(s.update(true, 202, 102, 10, &x, &y));
    TEST_ASSERT_TRUE(s.update(false, 0, 0, 20, &x, &y));
    TEST_ASSERT_EQUAL(201, x);
    TEST_ASSERT_EQUAL(101, y);
}

// Each new press acts again.
void test_next_press_acts_again() {
    Settle s;
    int x = 0, y = 0;
    s.update(true, 5, 5, 0, &x, &y);
    s.update(false, 0, 0, 10, &x, &y);
    TEST_ASSERT_FALSE(s.update(true, 50, 60, 100, &x, &y));
    TEST_ASSERT_TRUE(s.update(false, 0, 0, 110, &x, &y));
    TEST_ASSERT_EQUAL(50, x);
    TEST_ASSERT_EQUAL(60, y);
}

void test_nothing_without_a_press() {
    Settle s;
    int x = 7, y = 7;
    TEST_ASSERT_FALSE(s.update(false, 0, 0, 0, &x, &y));
    TEST_ASSERT_FALSE(s.update(false, 0, 0, 1000, &x, &y));
    TEST_ASSERT_EQUAL(7, x);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_press_acts_once_at_the_settled_average);
    RUN_TEST(test_needs_three_readings);
    RUN_TEST(test_quick_tap_acts_on_release);
    RUN_TEST(test_next_press_acts_again);
    RUN_TEST(test_nothing_without_a_press);
    return UNITY_END();
}
