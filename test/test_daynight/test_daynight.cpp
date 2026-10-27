// Copied from spotify-pokemon-deck test/test_daynight/test_daynight.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/daynight.h"

void setUp() {}
void tearDown() {}

static int hm(int h, int m) { return h * 60 + m; }

void test_night_starts_at_six_pm() {
    TEST_ASSERT_FALSE(daynight::isNight(hm(17, 59)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(18, 0)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(23, 59)));
}
void test_night_runs_past_midnight_until_six_am() {
    TEST_ASSERT_TRUE(daynight::isNight(hm(0, 0)));
    TEST_ASSERT_TRUE(daynight::isNight(hm(5, 59)));
    TEST_ASSERT_FALSE(daynight::isNight(hm(6, 0)));
    TEST_ASSERT_FALSE(daynight::isNight(hm(12, 0)));
}
void test_clock_counts_as_synced_only_after_2024() {
    TEST_ASSERT_FALSE(daynight::synced(0));            // boot: 1970
    TEST_ASSERT_FALSE(daynight::synced(86400 * 3));
    TEST_ASSERT_TRUE(daynight::synced(1791498600));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_night_starts_at_six_pm);
    RUN_TEST(test_night_runs_past_midnight_until_six_am);
    RUN_TEST(test_clock_counts_as_synced_only_after_2024);
    return UNITY_END();
}
