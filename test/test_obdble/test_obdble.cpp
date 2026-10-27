// Copied from spotify-pokemon-deck test/test_obdble/test_obdble.cpp @ df11e4a (branch feat/obd).
#include <unity.h>
#include <cstring>
#include "../../src/util/obdble.h"

using obdble::Char;

void setUp() {}
void tearDown() {}

// The common clone layout: service FFF0, FFF1 notify, FFF2 write.
void test_fff0_clone() {
    const Char c[] = {{0x180A, 0x2A29, false, false, false, false},     // device info: ignored
                      {0xFFF0, 0xFFF1, false, false, true, false},
                      {0xFFF0, 0xFFF2, true, true, false, false}};
    const obdble::Pick p = obdble::pick(c, 3);
    TEST_ASSERT_EQUAL_INT(1, p.notify);
    TEST_ASSERT_EQUAL_INT(2, p.write);
}

// FFE0 / FFE1: one characteristic both notifies and takes writes.
void test_ffe0_single_characteristic() {
    const Char c[] = {{0xFFE0, 0xFFE1, false, true, true, false}};
    const obdble::Pick p = obdble::pick(c, 1);
    TEST_ASSERT_EQUAL_INT(0, p.notify);
    TEST_ASSERT_EQUAL_INT(0, p.write);
}

// vLinker-style 18F0 (2AF0 notify, 2AF1 write); FFF0 wins when both exist.
void test_known_services_in_order() {
    const Char c[] = {{0x18F0, 0x2AF0, false, false, true, false},
                      {0x18F0, 0x2AF1, true, false, false, false},
                      {0xFFF0, 0xFFF1, false, false, true, false},
                      {0xFFF0, 0xFFF2, true, false, false, false}};
    obdble::Pick p = obdble::pick(c, 4);
    TEST_ASSERT_EQUAL_INT(2, p.notify);
    TEST_ASSERT_EQUAL_INT(3, p.write);
    p = obdble::pick(c, 2);
    TEST_ASSERT_EQUAL_INT(0, p.notify);
    TEST_ASSERT_EQUAL_INT(1, p.write);
}

// An unknown layout: any one service that has both a notify/indicate and a write characteristic.
void test_unknown_service_with_both() {
    const Char c[] = {{0x1800, 0x2A00, false, false, false, false},
                      {0xABCD, 0x0001, false, false, false, true},     // indicate counts
                      {0xABCD, 0x0002, false, true, false, false}};
    const obdble::Pick p = obdble::pick(c, 3);
    TEST_ASSERT_EQUAL_INT(1, p.notify);
    TEST_ASSERT_EQUAL_INT(2, p.write);
}

void test_nothing_usable() {
    const Char c[] = {{0xFFF0, 0xFFF1, false, false, true, false},     // notify only, no write
                      {0x1234, 0x0001, true, false, false, false}};    // write in another service
    const obdble::Pick p = obdble::pick(c, 2);
    TEST_ASSERT_EQUAL_INT(-1, p.notify);
    TEST_ASSERT_EQUAL_INT(-1, p.write);
    TEST_ASSERT_EQUAL_INT(-1, obdble::pick(nullptr, 0).write);
}

// Replies arrive in notification chunks; the link reads them byte by byte up to the '>' prompt.
void test_inbox_keeps_order_across_chunks() {
    obdble::Inbox in;
    in.push((const uint8_t*)"41 0C", 5);
    in.push((const uint8_t*)" 0B B8\r\r>", 9);
    char out[32];
    size_t n = 0;
    char c;
    while (in.pop(&c)) out[n++] = c;
    out[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("41 0C 0B B8\r\r>", out);
    TEST_ASSERT_FALSE(in.pop(&c));
}

void test_inbox_drops_what_does_not_fit_and_clears() {
    obdble::Inbox in;
    uint8_t big[obdble::Inbox::CAP + 10];
    memset(big, 'A', sizeof big);
    in.push(big, sizeof big);
    size_t n = 0;
    char c;
    while (in.pop(&c)) ++n;
    TEST_ASSERT_EQUAL_UINT((unsigned)obdble::Inbox::CAP, (unsigned)n);
    in.push((const uint8_t*)"xy", 2);
    in.clear();
    TEST_ASSERT_FALSE(in.pop(&c));
    in.push((const uint8_t*)"z", 1);                    // still works after wrapping and clearing
    TEST_ASSERT_TRUE(in.pop(&c));
    TEST_ASSERT_EQUAL_CHAR('z', c);
}

// Some adapters advertise no name, only their serial service: recognise them by it.
void test_serial_services_are_recognised() {
    TEST_ASSERT_TRUE(obdble::serialService(0xFFF0));
    TEST_ASSERT_TRUE(obdble::serialService(0xFFE0));
    TEST_ASSERT_TRUE(obdble::serialService(0x18F0));
    TEST_ASSERT_FALSE(obdble::serialService(0x180A));    // device information
    TEST_ASSERT_FALSE(obdble::serialService(0xFE95));    // a phone/dashcam's vendor service
    TEST_ASSERT_FALSE(obdble::serialService(0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fff0_clone);
    RUN_TEST(test_ffe0_single_characteristic);
    RUN_TEST(test_known_services_in_order);
    RUN_TEST(test_unknown_service_with_both);
    RUN_TEST(test_nothing_usable);
    RUN_TEST(test_inbox_keeps_order_across_chunks);
    RUN_TEST(test_inbox_drops_what_does_not_fit_and_clears);
    RUN_TEST(test_serial_services_are_recognised);
    return UNITY_END();
}
