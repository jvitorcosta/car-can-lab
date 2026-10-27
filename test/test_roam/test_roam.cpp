// Copied from spotify-pokemon-deck test/test_roam/test_roam.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/roam.h"

void setUp() {}
void tearDown() {}

// The decks stay on the network they joined (owner, 2026-10-14: home WiFi is for development;
// in the car there is only the hotspot). What remains: a short hotspot blip gets two tries.
void test_tries_per_network_after_a_drop() {
    TEST_ASSERT_EQUAL_INT(2, roam::tries(0, 0));    // dropped off the hotspot: give it two tries
    TEST_ASSERT_EQUAL_INT(1, roam::tries(0, 1));    // was on the fallback: one
    TEST_ASSERT_EQUAL_INT(1, roam::tries(1, 0));
    TEST_ASSERT_EQUAL_INT(1, roam::tries(0, -1));   // boot
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tries_per_network_after_a_drop);
    return UNITY_END();
}
