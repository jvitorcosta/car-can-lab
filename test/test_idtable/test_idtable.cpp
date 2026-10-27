#include <unity.h>
#include <cstring>
#include "../../src/util/idtable.h"

using elmmon::Frame;
using idtable::IdTable;

void setUp() {}
void tearDown() {}

static Frame frame(uint16_t id, uint8_t b0, uint8_t b1, uint8_t b2 = 0, uint8_t len = 3) {
    Frame f{};
    f.id = id;
    f.len = len;
    f.data[0] = b0;
    f.data[1] = b1;
    f.data[2] = b2;
    return f;
}

static IdTable t;  // ~5 KB: off the test's stack

void test_first_sighting_is_new_without_changes() {
    t = IdTable();
    const idtable::Update u = t.update(frame(0x1A6, 0, 4), 100);
    TEST_ASSERT_TRUE(u.isNew);
    TEST_ASSERT_EQUAL_HEX8(0, u.changed);
    TEST_ASSERT_EQUAL(1, t.size());
    TEST_ASSERT_NOT_NULL(t.find(0x1A6));
    TEST_ASSERT_NULL(t.find(0x1A7));
}

void test_same_data_no_change_byte_change_marked() {
    t = IdTable();
    t.update(frame(0x1A6, 0, 4, 9), 100);
    idtable::Update u = t.update(frame(0x1A6, 0, 4, 9), 200);
    TEST_ASSERT_FALSE(u.isNew);
    TEST_ASSERT_EQUAL_HEX8(0, u.changed);
    u = t.update(frame(0x1A6, 0, 4, 7), 300);
    TEST_ASSERT_EQUAL_HEX8(0x04, u.changed);
    const idtable::Row* r = t.find(0x1A6);
    TEST_ASSERT_EQUAL_UINT32(300, r->changedMs);
    TEST_ASSERT_EQUAL_UINT32(300, r->byteMs[2]);
    TEST_ASSERT_EQUAL_UINT32(300, r->seenMs);
    TEST_ASSERT_EQUAL_HEX8(7, r->data[2]);
}

void test_length_change_marks_every_byte() {
    t = IdTable();
    t.update(frame(0x1A6, 0, 4, 9, 3), 100);
    const idtable::Update u = t.update(frame(0x1A6, 0, 4, 9, 2), 200);
    TEST_ASSERT_EQUAL_HEX8(0xFF, u.changed);
    TEST_ASSERT_EQUAL(2, t.find(0x1A6)->len);
}

void test_order_last_changed_first() {
    t = IdTable();
    t.update(frame(0x100, 0, 0), 100);
    t.update(frame(0x200, 0, 0), 200);
    t.update(frame(0x300, 0, 0), 300);
    t.update(frame(0x100, 1, 0), 400);  // 100 changed last
    t.update(frame(0x200, 0, 0), 500);  // seen, unchanged
    uint16_t ids[8];
    const int n = t.order(ids, 8);
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL_HEX16(0x100, ids[0]);
    TEST_ASSERT_EQUAL_HEX16(0x300, ids[1]);  // first seen at 300: changedMs 300 > 200
    TEST_ASSERT_EQUAL_HEX16(0x200, ids[2]);
    TEST_ASSERT_EQUAL(1, t.order(ids, 1));
}

void test_fade_and_idle() {
    t = IdTable();
    t.update(frame(0x1A6, 0, 4), 1000);
    t.update(frame(0x1A6, 0, 5), 2000);
    const idtable::Row& r = *t.find(0x1A6);
    TEST_ASSERT_EQUAL_UINT8(255, IdTable::fade(r, 1, 2000));
    TEST_ASSERT_UINT8_WITHIN(2, 128, IdTable::fade(r, 1, 2500));
    TEST_ASSERT_EQUAL_UINT8(0, IdTable::fade(r, 1, 3000));
    TEST_ASSERT_EQUAL_UINT8(0, IdTable::fade(r, 1, 9000));
    TEST_ASSERT_EQUAL_UINT8(0, IdTable::fade(r, 0, 2000));  // byte 0 never changed
    TEST_ASSERT_FALSE(IdTable::idle(r, 6999));
    TEST_ASSERT_TRUE(IdTable::idle(r, 7000));
}

// Review focus 5: more IDs than the table holds.
void test_full_table_counts_dropped_ids() {
    t = IdTable();
    for (int i = 0; i < IdTable::MAX_IDS; i++) t.update(frame(static_cast<uint16_t>(i), 0, 0), 10);
    TEST_ASSERT_EQUAL(IdTable::MAX_IDS, t.size());
    const idtable::Update u = t.update(frame(0x700, 0, 0), 20);
    TEST_ASSERT_FALSE(u.isNew);
    TEST_ASSERT_NULL(t.find(0x700));
    TEST_ASSERT_EQUAL_UINT32(1, t.dropped());
    t.update(frame(0x700, 1, 0), 30);
    TEST_ASSERT_EQUAL_UINT32(1, t.dropped());
    t.update(frame(0x701, 1, 0), 30);
    TEST_ASSERT_EQUAL_UINT32(2, t.dropped());
    t.update(frame(5, 1, 0), 40);  // a known one still updates
    TEST_ASSERT_EQUAL_HEX8(1, t.find(5)->data[0]);
}

void test_picks() {
    t = IdTable();
    t.update(frame(0x100, 0, 0), 1);
    t.update(frame(0x200, 0, 0), 2);
    t.togglePick(0x200);
    t.togglePick(0x999);  // unknown: nothing
    uint16_t ids[4];
    TEST_ASSERT_EQUAL(1, t.picked(ids, 4));
    TEST_ASSERT_EQUAL_HEX16(0x200, ids[0]);
    TEST_ASSERT_TRUE(t.find(0x200)->picked);
    t.togglePick(0x200);
    TEST_ASSERT_EQUAL(0, t.picked(ids, 4));
    t.togglePick(0x100);
    t.togglePick(0x200);
    t.clearPicks();
    TEST_ASSERT_EQUAL(0, t.picked(ids, 4));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_sighting_is_new_without_changes);
    RUN_TEST(test_same_data_no_change_byte_change_marked);
    RUN_TEST(test_length_change_marks_every_byte);
    RUN_TEST(test_order_last_changed_first);
    RUN_TEST(test_fade_and_idle);
    RUN_TEST(test_full_table_counts_dropped_ids);
    RUN_TEST(test_picks);
    return UNITY_END();
}
