// Copied from spotify-pokemon-deck test/test_otapage/test_otapage.cpp @ 9ced38d.
#include <unity.h>
#include <cstring>
#include "../../src/util/otapage.h"

using otapage::Draw;
using otaview::Phase;
void setUp() {}
void tearDown() {}

// One cell per 10 %, like the Lights level bar; clamped.
void test_cells() {
    TEST_ASSERT_EQUAL_INT(0, otapage::cells(-1));
    TEST_ASSERT_EQUAL_INT(0, otapage::cells(0));
    TEST_ASSERT_EQUAL_INT(0, otapage::cells(9));
    TEST_ASSERT_EQUAL_INT(1, otapage::cells(10));
    TEST_ASSERT_EQUAL_INT(9, otapage::cells(99));
    TEST_ASSERT_EQUAL_INT(otapage::CELLS, otapage::cells(100));
    TEST_ASSERT_EQUAL_INT(otapage::CELLS, otapage::cells(250));
}

void test_titles() {
    TEST_ASSERT_EQUAL_STRING(">UPDATING", otapage::title(Phase::Receiving));
    TEST_ASSERT_EQUAL_STRING(">REBOOTING", otapage::title(Phase::Done));
    TEST_ASSERT_EQUAL_STRING(">UPDATE FAILED", otapage::title(Phase::Failed));
    TEST_ASSERT_EQUAL_STRING("", otapage::title(Phase::Idle));
    TEST_ASSERT_TRUE(otapage::alert(Phase::Failed));
    TEST_ASSERT_FALSE(otapage::alert(Phase::Receiving));
    TEST_ASSERT_FALSE(otapage::alert(Phase::Done));
}

// Nothing while no update ever came: the deck's own screens draw as before.
void test_idle_draws_nothing() {
    otapage::Shown s;
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Idle, -1));
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Idle, -1));
    TEST_ASSERT_FALSE(otapage::holds(Phase::Idle));
}

// The page in full when it appears, then only the bar as the percent moves, and nothing when it
// doesn't (a pass every 10 ms must not repaint 100 times a second).
void test_receiving_page_then_bar() {
    otapage::Shown s;
    TEST_ASSERT_EQUAL_INT((int)Draw::Page, (int)otapage::step(s, Phase::Receiving, 0));
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Receiving, 0));
    TEST_ASSERT_EQUAL_INT((int)Draw::Bar, (int)otapage::step(s, Phase::Receiving, 1));
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Receiving, 1));
    TEST_ASSERT_EQUAL_INT((int)Draw::Bar, (int)otapage::step(s, Phase::Receiving, 57));
    TEST_ASSERT_TRUE(otapage::holds(Phase::Receiving));
}

// The end: the page again with >REBOOTING; the deck restarts from there, so it holds.
void test_done_is_a_new_page() {
    otapage::Shown s;
    otapage::step(s, Phase::Receiving, 99);
    TEST_ASSERT_EQUAL_INT((int)Draw::Page, (int)otapage::step(s, Phase::Done, 100));
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Done, 100));
    TEST_ASSERT_TRUE(otapage::holds(Phase::Done));
}

// A failure: its page (held by otaview for FAILED_HOLD_MS), then the screen it covered comes back
// once, in full; after that nothing until the next update.
void test_failed_then_back() {
    otapage::Shown s;
    otapage::step(s, Phase::Receiving, 30);
    TEST_ASSERT_EQUAL_INT((int)Draw::Page, (int)otapage::step(s, Phase::Failed, 30));
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Failed, 30));
    TEST_ASSERT_TRUE(otapage::holds(Phase::Failed));
    TEST_ASSERT_EQUAL_INT((int)Draw::Back, (int)otapage::step(s, Phase::Idle, -1));
    TEST_ASSERT_EQUAL_INT((int)Draw::None, (int)otapage::step(s, Phase::Idle, -1));
}

// A new upload during the failed hold takes the page over at once.
void test_retry_during_failed_hold() {
    otapage::Shown s;
    otapage::step(s, Phase::Receiving, 30);
    otapage::step(s, Phase::Failed, 30);
    TEST_ASSERT_EQUAL_INT((int)Draw::Page, (int)otapage::step(s, Phase::Receiving, 0));
    TEST_ASSERT_EQUAL_INT((int)Draw::Bar, (int)otapage::step(s, Phase::Receiving, 4));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cells);
    RUN_TEST(test_titles);
    RUN_TEST(test_idle_draws_nothing);
    RUN_TEST(test_receiving_page_then_bar);
    RUN_TEST(test_done_is_a_new_page);
    RUN_TEST(test_failed_then_back);
    RUN_TEST(test_retry_during_failed_hold);
    return UNITY_END();
}
