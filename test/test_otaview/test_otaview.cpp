// Copied from spotify-pokemon-deck test/test_otaview/test_otaview.cpp @ 9ced38d.
#include <unity.h>
#include "../../src/util/otaview.h"

using otaview::Phase;
using otaview::Tracker;
void setUp() {}
void tearDown() {}

static bool is(const Tracker& t, Phase p, uint32_t now) { return t.phase(now) == p; }

void test_idle_at_first() {
    Tracker t;
    TEST_ASSERT_TRUE(is(t, Phase::Idle, 0));
    TEST_ASSERT_FALSE(t.rebootDue(100000));
}

// Receiving with its percent; Done on the end; the reboot DONE_MS later (time for the flash).
void test_receiving_then_done_then_reboot() {
    Tracker t;
    t.onStart();
    t.onProgress(40);
    TEST_ASSERT_TRUE(is(t, Phase::Receiving, 500));
    TEST_ASSERT_EQUAL_INT(40, t.pct());
    t.onEnd(1000);
    TEST_ASSERT_TRUE(is(t, Phase::Done, 1000));
    TEST_ASSERT_EQUAL_INT(100, t.pct());
    TEST_ASSERT_FALSE(t.rebootDue(1000 + otaview::DONE_MS - 1));
    TEST_ASSERT_TRUE(t.rebootDue(1000 + otaview::DONE_MS));
}

// ArduinoOTA's handle() returns after a good end too: that is not a failure.
void test_end_then_return_is_done() {
    Tracker t;
    t.onStart();
    t.onEnd(1000);
    t.onHandleReturned(1001);
    TEST_ASSERT_TRUE(is(t, Phase::Done, 1001));
}

// handle() returning mid-upload (cut off on a path that skips onError): Failed, held, then Idle.
void test_cut_off_is_failed_then_idle() {
    Tracker t;
    t.onStart();
    t.onProgress(30);
    t.onHandleReturned(5000);
    TEST_ASSERT_TRUE(is(t, Phase::Failed, 5000));
    TEST_ASSERT_TRUE(is(t, Phase::Failed, 5000 + otaview::FAILED_HOLD_MS - 1));
    TEST_ASSERT_TRUE(is(t, Phase::Idle, 5000 + otaview::FAILED_HOLD_MS));
    TEST_ASSERT_FALSE(t.rebootDue(9000));
}

void test_error_is_failed() {
    Tracker t;
    t.onStart();
    t.onError(10);
    TEST_ASSERT_TRUE(is(t, Phase::Failed, 10));
    t.onHandleReturned(11);                       // the return after the error changes nothing
    TEST_ASSERT_TRUE(is(t, Phase::Failed, 11));
}

// handle() returning with nothing started: still Idle.
void test_return_while_idle_stays_idle() {
    Tracker t;
    t.onHandleReturned(50);
    TEST_ASSERT_TRUE(is(t, Phase::Idle, 50));
}

// A new upload during the failed hold takes the screen at once.
void test_new_start_during_failed_hold() {
    Tracker t;
    t.onStart();
    t.onError(5000);
    t.onStart();
    t.onProgress(3);
    TEST_ASSERT_TRUE(is(t, Phase::Receiving, 5100));
    TEST_ASSERT_EQUAL_INT(3, t.pct());
}

// A second upload invited during the Done window can't cancel the good update's reboot.
void test_start_during_done_is_ignored() {
    Tracker t;
    t.onStart();
    t.onEnd(1000);
    t.onStart();
    t.onProgress(5);
    t.onError(1100);
    TEST_ASSERT_TRUE(is(t, Phase::Done, 1100));
    TEST_ASSERT_TRUE(t.rebootDue(1000 + otaview::DONE_MS));
}

// Shadow hovers HOVER px over the rail: his box's bottom edge sits that far above its top.
void test_rider_top_hovers() {
    TEST_ASSERT_EQUAL_INT(otaview::RAIL_Y - otaview::HOVER - 36, otaview::riderTop(36));
    TEST_ASSERT_EQUAL_INT(6, otaview::HOVER);
    TEST_ASSERT_TRUE(otaview::riderTop(64) >= 20);   // the tallest frame stays under the top strip
}

// The rail's repaint starts at the rail itself when Shadow is drawn (his rows reach down to it,
// composed with him: repainting them bare first made his feet blink on every percent step), and
// at the band's top above the rail for the jet glows, whose rows the rail repaint cleans.
void test_rail_repaint_top() {
    TEST_ASSERT_EQUAL_INT(otaview::RAIL_Y, otaview::railRepaintTop(true));
    TEST_ASSERT_EQUAL_INT(otaview::RAIL_Y - 14, otaview::railRepaintTop(false));
}

// What a frame repaints: everything on a change of phase or percent; on a bob step only the
// jets (never the rail, or the fill would flicker 8 times a second), and no jets once failed.
void test_repaint_plan() {
    using otaview::repaint;
    otaview::Repaint r = repaint(true, false, false, Phase::Receiving);
    TEST_ASSERT_TRUE(r.strip && r.rail && r.jets);
    r = repaint(false, true, false, Phase::Receiving);
    TEST_ASSERT_TRUE(r.strip && r.rail && r.jets);
    r = repaint(false, false, true, Phase::Receiving);
    TEST_ASSERT_TRUE(!r.strip && !r.rail && r.jets);
    r = repaint(false, false, true, Phase::Failed);
    TEST_ASSERT_TRUE(!r.strip && !r.rail && !r.jets);
    r = repaint(false, true, false, Phase::Failed);
    TEST_ASSERT_TRUE(r.strip && r.rail && !r.jets);
    r = repaint(false, false, false, Phase::Receiving);
    TEST_ASSERT_TRUE(!r.strip && !r.rail && !r.jets);
}

void test_hold_survives_millis_wrap() {
    Tracker t;
    t.onStart();
    t.onError(0xFFFFF000u);
    TEST_ASSERT_TRUE(is(t, Phase::Failed, 0xFFFFF000u + otaview::FAILED_HOLD_MS - 1));
    TEST_ASSERT_TRUE(is(t, Phase::Idle, 0xFFFFF000u + otaview::FAILED_HOLD_MS));
    Tracker d;
    d.onStart();
    d.onEnd(0xFFFFFF00u);
    TEST_ASSERT_TRUE(d.rebootDue(0xFFFFFF00u + otaview::DONE_MS));
}

// The fill is the rail's inner width (RAIL_W - 2 * BORDER = 284 px) times the percent, clamped;
// the rider's centre sits on the fill's tip, clamped onto the rail.
void test_fill_and_rider_clamp() {
    TEST_ASSERT_EQUAL_INT(0, otaview::fillWidth(0));
    TEST_ASSERT_EQUAL_INT(142, otaview::fillWidth(50));
    TEST_ASSERT_EQUAL_INT(284, otaview::fillWidth(100));
    TEST_ASSERT_EQUAL_INT(284, otaview::fillWidth(101));
    TEST_ASSERT_EQUAL_INT(0, otaview::fillWidth(-1));
    TEST_ASSERT_EQUAL_INT(16, otaview::riderX(0, 20));
    TEST_ASSERT_EQUAL_INT(284, otaview::riderX(100, 20));
    TEST_ASSERT_EQUAL_INT(18 + 142 - 10, otaview::riderX(50, 20));
    TEST_ASSERT_EQUAL_INT(16, otaview::riderX(-5, 20));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_idle_at_first);
    RUN_TEST(test_receiving_then_done_then_reboot);
    RUN_TEST(test_end_then_return_is_done);
    RUN_TEST(test_cut_off_is_failed_then_idle);
    RUN_TEST(test_error_is_failed);
    RUN_TEST(test_return_while_idle_stays_idle);
    RUN_TEST(test_new_start_during_failed_hold);
    RUN_TEST(test_start_during_done_is_ignored);
    RUN_TEST(test_repaint_plan);
    RUN_TEST(test_rider_top_hovers);
    RUN_TEST(test_rail_repaint_top);
    RUN_TEST(test_hold_survives_millis_wrap);
    RUN_TEST(test_fill_and_rider_clamp);
    return UNITY_END();
}
