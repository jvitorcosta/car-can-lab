// Copied from spotify-pokemon-deck test/test_netup/test_netup.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/netup.h"

void setUp() {}
void tearDown() {}

// The WiFi driver can report "connected" before (or after a reconnect, without) an address: the
// S3 sat 40 s at 0.0.0.0 with every DNS lookup failing (2026-10-14).
void test_connected_without_an_address_is_not_usable() {
    TEST_ASSERT_FALSE(netup::usable(true, 0));
}
void test_connected_with_an_address_is_usable() {
    TEST_ASSERT_TRUE(netup::usable(true, 0xAC140A0A));   // 172.20.10.10
}
void test_not_connected_is_not_usable() {
    TEST_ASSERT_FALSE(netup::usable(false, 0xAC140A0A));
}

// Final review M3: one "connected, 0.0.0.0" reading can be a transient; reconnect only when the
// next check (5 s later) still says so. A real disconnect reconnects at once.
void test_reconnect_on_a_real_drop_at_once() {
    netup::Watch w;
    TEST_ASSERT_TRUE(w.reconnect(false, 0));
}
void test_no_address_needs_two_checks_in_a_row() {
    netup::Watch w;
    TEST_ASSERT_FALSE(w.reconnect(true, 0));
    TEST_ASSERT_TRUE(w.reconnect(true, 0));
}
void test_a_good_check_in_between_resets() {
    netup::Watch w;
    w.reconnect(true, 0);
    TEST_ASSERT_FALSE(w.reconnect(true, 0xAC140A0A));
    TEST_ASSERT_FALSE(w.reconnect(true, 0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_connected_without_an_address_is_not_usable);
    RUN_TEST(test_connected_with_an_address_is_usable);
    RUN_TEST(test_not_connected_is_not_usable);
    RUN_TEST(test_reconnect_on_a_real_drop_at_once);
    RUN_TEST(test_no_address_needs_two_checks_in_a_row);
    RUN_TEST(test_a_good_check_in_between_resets);
    return UNITY_END();
}
