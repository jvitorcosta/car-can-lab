#include <unity.h>
#include <cstring>
#include "../../src/util/obd.h"
#include "../../src/util/sweep.h"

using sweep::Window;

void setUp() {}
void tearDown() {}

static void assertWindow(uint16_t filter, uint16_t mask, const Window& w) {
    TEST_ASSERT_EQUAL_HEX16(filter, w.filter);
    TEST_ASSERT_EQUAL_HEX16(mask, w.mask);
}

static bool passes(const Window& w, uint16_t id) { return (id & w.mask) == w.filter; }

void test_scan_covers_the_id_space_in_16_windows() {
    sweep::Plan p;
    p.scan();
    TEST_ASSERT_EQUAL(16, p.windows());
    assertWindow(0x000, 0x780, p.current());
    for (int i = 0; i < 15; i++) {
        p.next();
        TEST_ASSERT_FALSE(p.wrapped());
    }
    assertWindow(0x780, 0x780, p.current());
    p.next();
    assertWindow(0x000, 0x780, p.current());
    TEST_ASSERT_TRUE(p.wrapped());
    p.next();
    TEST_ASSERT_FALSE(p.wrapped());
}

void test_every_id_passes_exactly_one_scan_window() {
    sweep::Plan p;
    p.scan();
    for (uint16_t id = 0; id <= 0x7FF; id++) {
        int hits = 0;
        for (int i = 0; i < p.windows(); i++, p.next()) hits += passes(p.current(), id);
        TEST_ASSERT_EQUAL(1, hits);
    }
}

void test_split_halves_the_current_window() {
    sweep::Plan p;
    p.scan();
    p.next();
    p.next();
    assertWindow(0x100, 0x780, p.current());
    p.split();
    TEST_ASSERT_EQUAL(17, p.windows());
    assertWindow(0x100, 0x7C0, p.current());
    p.next();
    assertWindow(0x140, 0x7C0, p.current());
    p.next();
    assertWindow(0x180, 0x780, p.current());
}

void test_split_stops_at_one_id() {
    sweep::Plan p;
    const uint16_t one[] = {0x305};
    p.only(one, 1);
    assertWindow(0x305, 0x7FF, p.current());
    TEST_ASSERT_FALSE(p.split());
    TEST_ASSERT_EQUAL(1, p.windows());
    assertWindow(0x305, 0x7FF, p.current());
    p.scan();
    TEST_ASSERT_TRUE(p.split());
}

void test_only_groups_neighbours() {
    sweep::Plan p;
    const uint16_t ids[] = {0x1A6, 0x1A7, 0x305};
    p.only(ids, 3);
    TEST_ASSERT_EQUAL(2, p.windows());
    bool together = false, alone = false;
    for (int i = 0; i < p.windows(); i++, p.next()) {
        const Window w = p.current();
        if (passes(w, 0x1A6) && passes(w, 0x1A7)) together = true;
        if (passes(w, 0x305) && w.mask == 0x7FF) alone = true;
        TEST_ASSERT_TRUE(w.mask >= 0x7F8);  // never wider than 8 IDs
    }
    TEST_ASSERT_TRUE(together);
    TEST_ASSERT_TRUE(alone);
}

void test_only_covers_every_id_given() {
    sweep::Plan p;
    const uint16_t ids[] = {0x7FF, 0x000, 0x158, 0x159, 0x15A, 0x15B, 0x15C, 0x15D, 0x15E, 0x15F, 0x191, 0x191};
    p.only(ids, 12);
    for (uint16_t id : ids) {
        bool seen = false;
        for (int i = 0; i < p.windows(); i++, p.next()) seen = seen || passes(p.current(), id);
        TEST_ASSERT_TRUE(seen);
    }
}

void test_only_with_nothing_scans() {
    sweep::Plan p;
    p.only(nullptr, 0);
    TEST_ASSERT_EQUAL(16, p.windows());
}

void test_commands_are_allowed() {
    char cf[9], cm[9];
    sweep::commands(Window{0x1A0, 0x7F0}, cf, cm);
    TEST_ASSERT_EQUAL_STRING("ATCF1A0\r", cf);
    TEST_ASSERT_EQUAL_STRING("ATCM7F0\r", cm);
    TEST_ASSERT_TRUE(obd::allowed(cf));
    TEST_ASSERT_TRUE(obd::allowed(cm));
}

void test_every_planned_window_gives_allowed_commands() {
    sweep::Plan p;
    p.scan();
    for (int k = 0; k < 40; k++) p.split(), p.next();
    char cf[9], cm[9];
    for (int i = 0; i < p.windows(); i++, p.next()) {
        sweep::commands(p.current(), cf, cm);
        TEST_ASSERT_TRUE_MESSAGE(obd::allowed(cf), cf);
        TEST_ASSERT_TRUE_MESSAGE(obd::allowed(cm), cm);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_scan_covers_the_id_space_in_16_windows);
    RUN_TEST(test_every_id_passes_exactly_one_scan_window);
    RUN_TEST(test_split_halves_the_current_window);
    RUN_TEST(test_split_stops_at_one_id);
    RUN_TEST(test_only_groups_neighbours);
    RUN_TEST(test_only_covers_every_id_given);
    RUN_TEST(test_only_with_nothing_scans);
    RUN_TEST(test_commands_are_allowed);
    RUN_TEST(test_every_planned_window_gives_allowed_commands);
    return UNITY_END();
}
