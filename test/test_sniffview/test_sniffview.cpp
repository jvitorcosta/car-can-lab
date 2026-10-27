#include <unity.h>
#include <cstring>
#include "../../src/ui/termfont.h"
#include "../../src/util/sniffview.h"

using monplan::State;

void setUp() {}
void tearDown() {}

void test_status_texts() {
    TEST_ASSERT_EQUAL_STRING("SEARCHING...", sniffview::statusText(State::Searching, 0));
    TEST_ASSERT_EQUAL_STRING("SEARCHING...", sniffview::statusText(State::Searching, 19999));
    TEST_ASSERT_EQUAL_STRING("ADAPTER NOT FOUND", sniffview::statusText(State::Searching, 20000));
    TEST_ASSERT_EQUAL_STRING("CONNECTING...", sniffview::statusText(State::Probing, 0));
    TEST_ASSERT_EQUAL_STRING("NOT A CAN CAR?", sniffview::statusText(State::NotCan, 0));
    TEST_ASSERT_EQUAL_STRING("ADAPTER CAN'T MONITOR (NOT SILENT)", sniffview::statusText(State::NotSilent, 0));
    TEST_ASSERT_EQUAL_STRING("ADAPTER CAN'T MONITOR", sniffview::statusText(State::CantMonitor, 0));
    TEST_ASSERT_EQUAL_STRING("ENGINE OFF?", sniffview::statusText(State::EngineOff, 0));
    TEST_ASSERT_EQUAL_STRING("ADAPTER LOST", sniffview::statusText(State::Lost, 0));
    TEST_ASSERT_NULL(sniffview::statusText(State::Monitoring, 0));
    TEST_ASSERT_NULL(sniffview::statusText(State::Querying, 0));
}

void test_alerts_are_the_problems() {
    TEST_ASSERT_FALSE(sniffview::isAlert(State::Searching, 0));
    TEST_ASSERT_TRUE(sniffview::isAlert(State::Searching, 20000));
    TEST_ASSERT_FALSE(sniffview::isAlert(State::Probing, 0));
    TEST_ASSERT_TRUE(sniffview::isAlert(State::NotSilent, 0));
    TEST_ASSERT_TRUE(sniffview::isAlert(State::EngineOff, 0));
    TEST_ASSERT_TRUE(sniffview::isAlert(State::Lost, 0));
}

void test_header_live() {
    char h[64];
    sniffview::header(h, sizeof h, 23, 0, 41, false, 0, 0);
    TEST_ASSERT_EQUAL_STRING("SNIFF // LIVE  23 IDs  41 msg/s", h);
    sniffview::header(h, sizeof h, 96, 2, 41, false, 0, 0);
    TEST_ASSERT_EQUAL_STRING("SNIFF // LIVE  96 IDs +2  41 msg/s", h);
}

void test_header_recording() {
    char h[64];
    sniffview::header(h, sizeof h, 23, 0, 41, true, 34, 3200);
    TEST_ASSERT_EQUAL_STRING("REC 34%  CYCLE 4 s  23 IDs  41/s", h);
    sniffview::header(h, sizeof h, 96, 2, 41, true, 100, 1000);
    TEST_ASSERT_EQUAL_STRING("REC 100%  CYCLE 1 s  96 IDs +2  41/s", h);
}

void test_rows() {
    char r[40];
    idtable::Row row{};
    row.id = 0x1A6;
    row.len = 2;
    row.data[0] = 0x00;
    row.data[1] = 0x04;
    sniffview::row(r, sizeof r, row);
    TEST_ASSERT_EQUAL_STRING("1A6  00 04", r);
    const uint8_t b[8] = {0x00, 0x04, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x11};
    row.len = 8;
    memcpy(row.data, b, 8);
    sniffview::row(r, sizeof r, row);
    TEST_ASSERT_EQUAL_STRING("1A6  00 04 00 00 3C 00 00 11", r);
    row.picked = true;
    sniffview::row(r, sizeof r, row);
    TEST_ASSERT_EQUAL_STRING("1A6* 00 04 00 00 3C 00 00 11", r);
    TEST_ASSERT_EQUAL(5 + 3 * 4, sniffview::byteColumn(4));
}

void test_the_widest_texts_fit_the_screen() {
    char h[64];
    sniffview::header(h, sizeof h, 96, 9999, 9999, true, 100, 99000);
    TEST_ASSERT_LESS_OR_EQUAL(312, termfont::width(termfont::Face::SMALL, h));
    TEST_ASSERT_LESS_OR_EQUAL(312, termfont::width(termfont::Face::SMALL, "ADAPTER CAN'T MONITOR (NOT SILENT)"));
    TEST_ASSERT_LESS_OR_EQUAL(312, termfont::width(termfont::Face::BODY, "7FF* FF FF FF FF FF FF FF FF"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_status_texts);
    RUN_TEST(test_alerts_are_the_problems);
    RUN_TEST(test_header_live);
    RUN_TEST(test_header_recording);
    RUN_TEST(test_rows);
    RUN_TEST(test_the_widest_texts_fit_the_screen);
    return UNITY_END();
}
