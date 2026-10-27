// Copied from spotify-pokemon-deck test/test_netdiag/test_netdiag.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/netdiag.h"

void setUp() {}
void tearDown() {}

// What a failed Spotify poll means, from the facts the deck can check (owner, 2026-10-14: "can
// you log why no internet appears?").
void test_wifi_down() {
    TEST_ASSERT_EQUAL_STRING("WiFi is down", netdiag::verdict(false, false, false, -1));
}
void test_phone_does_not_answer() {
    TEST_ASSERT_EQUAL_STRING("WiFi link broken: the phone/router doesn't answer",
                             netdiag::verdict(true, false, false, -1));
}
void test_phone_answers_but_no_dns() {
    TEST_ASSERT_EQUAL_STRING("the phone/router has no internet (DNS fails)",
                             netdiag::verdict(true, true, false, -1));
}
void test_dns_ok_but_request_failed() {
    TEST_ASSERT_EQUAL_STRING("internet slow or Spotify unreachable (DNS OK, request failed)",
                             netdiag::verdict(true, true, true, -1));
}
void test_an_http_answer_is_spotify_side() {
    TEST_ASSERT_EQUAL_STRING("Spotify answered with an error", netdiag::verdict(true, true, true, 500));
    TEST_ASSERT_EQUAL_STRING("Spotify answered with an error", netdiag::verdict(true, false, false, 401));
}
void test_router_ignoring_ping_but_dns_ok() {
    // Some routers don't answer ping; working DNS shows the link is fine.
    TEST_ASSERT_EQUAL_STRING("internet slow or Spotify unreachable (DNS OK, request failed)",
                             netdiag::verdict(true, false, true, -1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_down);
    RUN_TEST(test_phone_does_not_answer);
    RUN_TEST(test_phone_answers_but_no_dns);
    RUN_TEST(test_dns_ok_but_request_failed);
    RUN_TEST(test_an_http_answer_is_spotify_side);
    RUN_TEST(test_router_ignoring_ping_but_dns_ok);
    return UNITY_END();
}
