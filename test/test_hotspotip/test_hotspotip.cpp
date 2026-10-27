// Copied from spotify-pokemon-deck test/test_hotspotip/test_hotspotip.cpp @ 9ced38d.
#include <unity.h>
#include <cstdint>
#include "../../src/util/hotspotip.h"

void setUp() {}
void tearDown() {}

static constexpr uint32_t ip(int a, int b, int c, int d) {
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d;
}
static const uint32_t GW = ip(172, 20, 10, 1), MASK = ip(255, 255, 255, 240);

void test_on_the_iphone_hotspot_asks_for_its_fixed_address() {
    TEST_ASSERT_EQUAL_HEX32(ip(172, 20, 10, 10), hotspotip::wanted(ip(172, 20, 10, 3), GW, MASK, 10));
    TEST_ASSERT_EQUAL_HEX32(ip(172, 20, 10, 11), hotspotip::wanted(ip(172, 20, 10, 3), GW, MASK, 11));
}

void test_already_there_means_no_change() {
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(ip(172, 20, 10, 10), GW, MASK, 10));
}

void test_other_networks_keep_dhcp() {
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(ip(192, 168, 100, 13), ip(192, 168, 100, 1),
                                                 ip(255, 255, 255, 0), 10));
    // Same subnet numbers but another mask: not an iPhone hotspot.
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(ip(172, 20, 10, 3), GW, ip(255, 255, 0, 0), 10));
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(0, 0, 0, 10));   // not connected
}

void test_addresses_outside_the_hotspot_range_are_refused() {
    // /28: .1 is the iPhone, .15 broadcast; only .2-.14 are hosts.
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(ip(172, 20, 10, 3), GW, MASK, 1));
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(ip(172, 20, 10, 3), GW, MASK, 15));
    TEST_ASSERT_EQUAL_HEX32(0, hotspotip::wanted(ip(172, 20, 10, 3), GW, MASK, 0));
}

void test_on_hotspot() {
    TEST_ASSERT_TRUE(hotspotip::onHotspot(ip(172, 20, 10, 11), GW, MASK));
    TEST_ASSERT_FALSE(hotspotip::onHotspot(ip(192, 168, 100, 10), ip(192, 168, 100, 1), ip(255, 255, 255, 0)));
    TEST_ASSERT_FALSE(hotspotip::onHotspot(ip(172, 20, 10, 3), GW, ip(255, 255, 0, 0)));
    TEST_ASSERT_FALSE(hotspotip::onHotspot(0, 0, 0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_on_the_iphone_hotspot_asks_for_its_fixed_address);
    RUN_TEST(test_already_there_means_no_change);
    RUN_TEST(test_other_networks_keep_dhcp);
    RUN_TEST(test_addresses_outside_the_hotspot_range_are_refused);
    RUN_TEST(test_on_hotspot);
    return UNITY_END();
}
