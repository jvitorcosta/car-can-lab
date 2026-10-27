#include <unity.h>
#include <string>
#include <vector>
#include "../../src/util/reclog.h"

using elmmon::Frame;

void setUp() {}
void tearDown() {}

static std::vector<std::string> lines;
static void capture(const char* line, void*) { lines.push_back(line); }

static Frame two(uint16_t id, uint8_t a, uint8_t b) {
    Frame f{};
    f.id = id;
    f.len = 2;
    f.data[0] = a;
    f.data[1] = b;
    return f;
}

static reclog::Rec rec;

static void start() {
    lines.clear();
    rec = reclog::Rec();
    rec.begin(capture, nullptr, reclog::Header{3, "2026-10-27T14:03:00", "ELM327 v2.2", "6", "0.1.0"});
}

void test_header() {
    start();
    TEST_ASSERT_EQUAL(1, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("# car-can-lab rec 3 2026-10-27T14:03:00 adapter=ELM327 v2.2 proto=6 fw=0.1.0", lines[0].c_str());
}

void test_first_sighting_and_rate_cap() {
    start();
    rec.frame(two(0x1A6, 0x00, 0x04), 100);
    TEST_ASSERT_EQUAL_STRING("F 100 1A6 00 04", lines.back().c_str());
    rec.frame(two(0x1A6, 0x00, 0x05), 200);
    rec.frame(two(0x1A6, 0x00, 0x06), 300);
    rec.tick(349);
    TEST_ASSERT_EQUAL(2, (int)lines.size());
    rec.tick(350);
    TEST_ASSERT_EQUAL(3, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("F 350 1A6 00 06", lines.back().c_str());
    rec.frame(two(0x1A6, 0x00, 0x06), 400);  // same data
    rec.tick(1000);
    TEST_ASSERT_EQUAL(3, (int)lines.size());
}

void test_change_after_the_gap_is_written_at_once() {
    start();
    rec.frame(two(0x1A6, 0x00, 0x04), 100);
    rec.frame(two(0x1A6, 0x00, 0x05), 360);
    TEST_ASSERT_EQUAL_STRING("F 360 1A6 00 05", lines.back().c_str());
}

void test_held_value_back_to_the_written_one_is_dropped() {
    start();
    rec.frame(two(0x1A6, 0x00, 0x04), 100);
    rec.frame(two(0x1A6, 0x00, 0x05), 150);
    rec.frame(two(0x1A6, 0x00, 0x04), 200);
    rec.tick(500);
    TEST_ASSERT_EQUAL(2, (int)lines.size());
}

void test_ids_are_capped_separately() {
    start();
    rec.frame(two(0x1A6, 0, 1), 100);
    rec.frame(two(0x191, 0, 1), 110);
    TEST_ASSERT_EQUAL_STRING("F 110 191 00 01", lines.back().c_str());
}

void test_mark_obd_state() {
    start();
    rec.mark(3, 500);
    TEST_ASSERT_EQUAL_STRING("M 500 3", lines.back().c_str());
    rec.obd("rpm", 812.4f, 600);
    TEST_ASSERT_EQUAL_STRING("O 600 rpm 812", lines.back().c_str());
    rec.state("window 100/780", 0);
    TEST_ASSERT_EQUAL_STRING("S 0 window 100/780", lines.back().c_str());
}

void test_end_flushes_held_values() {
    start();
    rec.frame(two(0x1A6, 0x00, 0x04), 800);
    rec.frame(two(0x1A6, 0x00, 0x07), 850);
    rec.end(900);
    TEST_ASSERT_EQUAL(4, (int)lines.size());
    TEST_ASSERT_EQUAL_STRING("F 900 1A6 00 07", lines[2].c_str());
    TEST_ASSERT_EQUAL_STRING("S 900 end", lines[3].c_str());
}

void test_eight_bytes_and_empty_frame() {
    start();
    Frame f{};
    f.id = 0x05;
    f.len = 8;
    for (int i = 0; i < 8; i++) f.data[i] = static_cast<uint8_t>(0xA0 + i);
    rec.frame(f, 1);
    TEST_ASSERT_EQUAL_STRING("F 1 005 A0 A1 A2 A3 A4 A5 A6 A7", lines.back().c_str());
    Frame e{};
    e.id = 0x7FF;
    rec.frame(e, 2);
    TEST_ASSERT_EQUAL_STRING("F 2 7FF", lines.back().c_str());
}

void test_nothing_before_begin_or_after_end() {
    lines.clear();
    rec = reclog::Rec();
    rec.frame(two(1, 2, 3), 1);
    rec.mark(1, 1);
    TEST_ASSERT_EQUAL(0, (int)lines.size());
    start();
    rec.end(5);
    const size_t n = lines.size();
    rec.mark(2, 6);
    TEST_ASSERT_EQUAL(n, lines.size());
}

void test_more_ids_than_tracked_are_counted() {
    start();
    for (int i = 0; i < reclog::Rec::TRACK + 3; i++) rec.frame(two(static_cast<uint16_t>(i), 0, 0), 10);
    TEST_ASSERT_EQUAL(1 + reclog::Rec::TRACK, (int)lines.size());
    TEST_ASSERT_EQUAL_UINT32(3, rec.untracked());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_header);
    RUN_TEST(test_first_sighting_and_rate_cap);
    RUN_TEST(test_change_after_the_gap_is_written_at_once);
    RUN_TEST(test_held_value_back_to_the_written_one_is_dropped);
    RUN_TEST(test_ids_are_capped_separately);
    RUN_TEST(test_mark_obd_state);
    RUN_TEST(test_end_flushes_held_values);
    RUN_TEST(test_eight_bytes_and_empty_frame);
    RUN_TEST(test_nothing_before_begin_or_after_end);
    RUN_TEST(test_more_ids_than_tracked_are_counted);
    return UNITY_END();
}
