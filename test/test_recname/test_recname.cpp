#include <unity.h>
#include "../../src/util/recname.h"

void setUp() {}
void tearDown() {}

void test_next_number() {
    const int some[] = {1, 2, 7};
    TEST_ASSERT_EQUAL(8, recname::next(some, 3));
    TEST_ASSERT_EQUAL(1, recname::next(nullptr, 0));
    const int unordered[] = {12, 3};
    TEST_ASSERT_EQUAL(13, recname::next(unordered, 2));
}

void test_path() {
    char p[24];
    recname::path(3, p, sizeof p);
    TEST_ASSERT_EQUAL_STRING("/rec/003.log", p);
    recname::path(1234, p, sizeof p);
    TEST_ASSERT_EQUAL_STRING("/rec/1234.log", p);
}

void test_parse() {
    TEST_ASSERT_EQUAL(3, recname::parse("003.log"));
    TEST_ASSERT_EQUAL(120, recname::parse("120.log"));
    TEST_ASSERT_EQUAL(-1, recname::parse("x.log"));
    TEST_ASSERT_EQUAL(-1, recname::parse("003.txt"));
    TEST_ASSERT_EQUAL(-1, recname::parse(".log"));
    TEST_ASSERT_EQUAL(-1, recname::parse("/rec/003.log"));
    TEST_ASSERT_EQUAL(-1, recname::parse(nullptr));
    TEST_ASSERT_EQUAL(-1, recname::parse("99999999999.log"));
}

void test_plan_space_enough_free() {
    const int ex[] = {1, 2, 3};
    const recname::Space s = recname::planSpace(20000, 16384, ex, 3, 3);
    TEST_ASSERT_EQUAL(0, s.dropOldest);
    TEST_ASSERT_FALSE(s.full);
}

void test_plan_space_drops_the_oldest_finished() {
    const int ex[] = {1, 2, 3};
    const recname::Space s = recname::planSpace(10000, 16384, ex, 3, 3);
    TEST_ASSERT_TRUE(s.dropOldest > 0);
    TEST_ASSERT_FALSE(s.full);
    TEST_ASSERT_EQUAL(1, recname::oldest(ex, 3, 3));
}

// Review focus 3: the flash fills while recording and only the current one is left.
void test_plan_space_full_when_only_the_current_is_left() {
    const int ex[] = {3};
    const recname::Space s = recname::planSpace(1000, 16384, ex, 1, 3);
    TEST_ASSERT_TRUE(s.full);
    TEST_ASSERT_EQUAL(0, s.dropOldest);
    TEST_ASSERT_EQUAL(-1, recname::oldest(ex, 1, 3));
}

void test_oldest_skips_the_current() {
    const int ex[] = {5, 2, 9};
    TEST_ASSERT_EQUAL(5, recname::oldest(ex, 3, 2));
    TEST_ASSERT_EQUAL(2, recname::oldest(ex, 3, -1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_next_number);
    RUN_TEST(test_path);
    RUN_TEST(test_parse);
    RUN_TEST(test_plan_space_enough_free);
    RUN_TEST(test_plan_space_drops_the_oldest_finished);
    RUN_TEST(test_plan_space_full_when_only_the_current_is_left);
    RUN_TEST(test_oldest_skips_the_current);
    return UNITY_END();
}
