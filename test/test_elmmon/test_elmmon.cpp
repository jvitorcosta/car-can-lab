#include <unity.h>
#include <cstring>
#include <string>
#include "../../src/util/elmmon.h"

using elmmon::Frame;
using elmmon::Kind;

void setUp() {}
void tearDown() {}

void test_frame_with_spaces() {
    Frame f{};
    TEST_ASSERT_EQUAL(Kind::Frame, elmmon::parse("1A6 00 04 00 00 3C 00 00 11", &f));
    TEST_ASSERT_EQUAL_HEX16(0x1A6, f.id);
    TEST_ASSERT_EQUAL(8, f.len);
    TEST_ASSERT_EQUAL_HEX8(0x04, f.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11, f.data[7]);
}

void test_frame_without_spaces() {
    Frame f{};
    TEST_ASSERT_EQUAL(Kind::Frame, elmmon::parse("1A60004", &f));
    TEST_ASSERT_EQUAL_HEX16(0x1A6, f.id);
    TEST_ASSERT_EQUAL(2, f.len);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x04, f.data[1]);
}

void test_short_frame() {
    Frame f{};
    TEST_ASSERT_EQUAL(Kind::Frame, elmmon::parse("7E8 03 41 0C", &f));
    TEST_ASSERT_EQUAL_HEX16(0x7E8, f.id);
    TEST_ASSERT_EQUAL(3, f.len);
}

void test_status_lines() {
    Frame f{};
    TEST_ASSERT_EQUAL(Kind::BufferFull, elmmon::parse("BUFFER FULL", &f));
    TEST_ASSERT_EQUAL(Kind::Stopped, elmmon::parse("STOPPED", &f));
    TEST_ASSERT_EQUAL(Kind::Error, elmmon::parse("CAN ERROR", &f));
    TEST_ASSERT_EQUAL(Kind::Error, elmmon::parse("<RX ERROR", &f));
    TEST_ASSERT_EQUAL(Kind::Error, elmmon::parse("?", &f));
    TEST_ASSERT_EQUAL(Kind::Prompt, elmmon::parse(">", &f));
}

void test_junk_is_other() {
    Frame f{};
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse("1A6 00 04 0", &f));                     // odd digit
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse("1A6 00 01 02 03 04 05 06 07 08", &f));  // 9 bytes
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse("GG6 00", &f));
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse("OK", &f));
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse("", &f));
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse(nullptr, &f));
    TEST_ASSERT_EQUAL(Kind::Other, elmmon::parse("8A6 00", &f));  // past 11 bits
}

void test_frame_with_no_data() {
    Frame f{};
    TEST_ASSERT_EQUAL(Kind::Frame, elmmon::parse("1A6", &f));
    TEST_ASSERT_EQUAL(0, f.len);
}

// Review focus 1: a line split over two BLE notifications, two lines in one.
void test_lines_split_and_joined() {
    elmmon::Lines l;
    char out[elmmon::Lines::CAP];
    l.push("1A6 00 0", 8);
    TEST_ASSERT_FALSE(l.next(out, sizeof out));
    l.push("4\r191 0B\r", 9);
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("1A6 00 04", out);
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("191 0B", out);
    TEST_ASSERT_FALSE(l.next(out, sizeof out));
}

void test_lines_prompt_alone() {
    elmmon::Lines l;
    char out[elmmon::Lines::CAP];
    l.push("\r\r>", 3);
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING(">", out);
    TEST_ASSERT_FALSE(l.next(out, sizeof out));
}

void test_lines_prompt_after_text_and_newlines() {
    elmmon::Lines l;
    char out[elmmon::Lines::CAP];
    l.push("OK\r\n\r\n>", 7);
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("OK", out);
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING(">", out);
}

void test_lines_overlong_cut() {
    elmmon::Lines l;
    char out[elmmon::Lines::CAP];
    std::string s(100, 'A');
    s += "\r1A6 00\r";
    l.push(s.data(), s.size());
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL(95, (int)strlen(out));
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("1A6 00", out);
    TEST_ASSERT_FALSE(l.next(out, sizeof out));
}

void test_lines_small_out_buffer() {
    elmmon::Lines l;
    char out[5];
    l.push("1A6 00 04\r", 10);
    TEST_ASSERT_TRUE(l.next(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("1A6 ", out);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_frame_with_spaces);
    RUN_TEST(test_frame_without_spaces);
    RUN_TEST(test_short_frame);
    RUN_TEST(test_status_lines);
    RUN_TEST(test_junk_is_other);
    RUN_TEST(test_frame_with_no_data);
    RUN_TEST(test_lines_split_and_joined);
    RUN_TEST(test_lines_prompt_alone);
    RUN_TEST(test_lines_prompt_after_text_and_newlines);
    RUN_TEST(test_lines_overlong_cut);
    RUN_TEST(test_lines_small_out_buffer);
    return UNITY_END();
}
