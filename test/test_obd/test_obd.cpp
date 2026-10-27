// Copied from spotify-pokemon-deck test/test_obd/test_obd.cpp @ df11e4a (branch feat/obd).
#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "../../src/util/obd.h"

using obd::Pid;
using obd::Reply;

void setUp() {}
void tearDown() {}

static Reply run(Pid p, const char* raw, float* v) { *v = -999.0f; return obd::parse(p, raw, v); }

void test_requests_end_with_cr_and_are_read_only() {
    TEST_ASSERT_EQUAL_STRING("010C\r", obd::request(Pid::Rpm));
    TEST_ASSERT_EQUAL_STRING("ATRV\r", obd::request(Pid::Battery));
    for (int i = 0; i < (int)Pid::COUNT; ++i) {          // mode 01 or AT only: never 03 / 04
        const char* r = obd::request((Pid)i);
        TEST_ASSERT_TRUE(strncmp(r, "01", 2) == 0 || strncmp(r, "AT", 2) == 0);
    }
}

void test_formulas_at_their_ends() {
    struct { Pid p; const char* raw; float want; } rows[] = {
        {Pid::Rpm, "41 0C 00 00", 0.0f},          {Pid::Rpm, "41 0C FF FF", 16383.75f},
        {Pid::Rpm, "41 0C 0B B8", 750.0f},        {Pid::Speed, "41 0D FF", 255.0f},
        {Pid::Coolant, "41 05 00", -40.0f},       {Pid::Coolant, "41 05 FF", 215.0f},
        {Pid::Load, "41 04 FF", 100.0f},          {Pid::Intake, "41 0F 3C", 20.0f},
        {Pid::Throttle, "41 11 00", 0.0f},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)Reply::Ok, (int)run(r.p, r.raw, &v), r.raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, r.want, v, r.raw);
    }
}

void test_what_clones_wrap_around_a_reply() {
    const char* forms[] = {
        "410C0BB8",                                  // ATS0: no spaces
        "010C\n41 0C 0B B8",                         // echo still on
        "SEARCHING...\n41 0C 0B B8",                 // first request after ATSP0
        "BUS INIT: ...OK\n41 0C 0B B8\n",            // K-line init
        "7E8 04 41 0C 0B B8",                        // CAN headers on
        "41 0C 0B B8\n41 0C 0B B9",                  // two ECUs: the first wins
        "\n\n41 0c 0b b8  \n",                       // blank lines, lower case
    };
    for (const char* raw : forms) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)Reply::Ok, (int)run(Pid::Rpm, raw, &v), raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 750.0f, v, raw);
    }
}

void test_errors_never_give_a_number() {
    struct { const char* raw; Reply want; } rows[] = {
        {"NO DATA", Reply::NoData},
        {"SEARCHING...\nUNABLE TO CONNECT", Reply::NoEngine},
        {"CAN ERROR", Reply::NoEngine},
        {"BUS INIT: ...ERROR", Reply::NoEngine},
        {"STOPPED", Reply::Error},
        {"?", Reply::Error},
        {"", Reply::Error},
        {"41 0C 1A", Reply::Error},                  // cut off before its second byte
        {"41 0D 32", Reply::Error},                  // a reply for another PID
        {"ELM327 v1.5", Reply::Error},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)r.want, (int)run(Pid::Rpm, r.raw, &v), r.raw);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-999.0f, v, r.raw);   // untouched
    }
}

void test_battery_voltage_forms() {
    struct { const char* raw; Reply want; float v; } rows[] = {
        {"12.6V", Reply::Ok, 12.6f}, {"ATRV\n14.1V", Reply::Ok, 14.1f}, {" 9V ", Reply::Ok, 9.0f},
        {"V", Reply::Error, -999.0f}, {"?", Reply::Error, -999.0f}, {"", Reply::Error, -999.0f},
        // a clone's banner after a brown-out reboot is not 327 V
        {"ELM327 v1.5", Reply::Error, -999.0f}, {"\r\rELM327 v2.1\r\r", Reply::Error, -999.0f},
        {"ELM327 v1.5\r12.4V", Reply::Ok, 12.4f}, {"99V", Reply::Error, -999.0f},
    };
    for (auto& r : rows) {
        float v;
        TEST_ASSERT_EQUAL_MESSAGE((int)r.want, (int)run(Pid::Battery, r.raw, &v), r.raw);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, r.v, v, r.raw);
    }
}

// The setup checks the adapter's answer, not just that it said something.
void test_setup_replies() {
    TEST_ASSERT_TRUE(obd::setupOk("ATZ\r", "\n\nELM327 v1.5\n"));
    TEST_ASSERT_TRUE(obd::setupOk("ATZ\r", "ATZ\nELM327 v2.1"));
    TEST_ASSERT_TRUE(obd::setupOk("ATE0\r", "ATE0\nOK"));
    TEST_ASSERT_TRUE(obd::setupOk("ATSP0\r", "OK"));
    TEST_ASSERT_FALSE(obd::setupOk("ATSP0\r", "?"));
    TEST_ASSERT_FALSE(obd::setupOk("ATZ\r", "v1."));
    TEST_ASSERT_FALSE(obd::setupOk("ATL0\r", ""));
    TEST_ASSERT_FALSE(obd::setupOk(nullptr, "OK"));
}

void test_adapter_names() {
    const char* yes[] = {"OBDII", "OBD2", "obd-ii", "V-LINK", "ELM327", "Android-Vlink", "IOS-Vlink"};
    const char* no[] = {"JBL Flip 5", "iPhone", "", nullptr};
    for (const char* n : yes) TEST_ASSERT_TRUE_MESSAGE(obd::looksLikeAdapter(n), n);
    for (const char* n : no) TEST_ASSERT_FALSE(obd::looksLikeAdapter(n));
}

// The read-only gate (spec 2026-10-19 "Read-only guarantee"): only mode 01 and the adapter's own setup.
void test_every_command_the_code_sends_is_allowed() {
    for (int i = 0; i < (int)Pid::COUNT; ++i) TEST_ASSERT_TRUE(obd::allowed(obd::request((Pid)i)));
    const char* setup[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATSP0\r", "ATRV\r", "ATI\r", "ATDPN\r",
                           "ATH0\r", "ATH1\r", "0100\r", "0120\r", "0140\r", "0160\r"};
    for (const char* c : setup) TEST_ASSERT_TRUE_MESSAGE(obd::allowed(c), c);
}

void test_writes_and_other_modes_are_refused() {
    const char* bad[] = {"04\r", "0400\r", "03\r", "0300\r", "0200\r", "0800\r", "0902\r", "2F0101\r",
                         "3B01\r", "2201\r", "2101\r", "ATSH7E0\r", "ATCSM0\r",
                         "ATBD\r", "ATPP0CSV01\r", "ATWS\r", "", nullptr};
    for (const char* c : bad) TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c ? c : "null");
}

void test_near_misses_are_refused() {
    const char* bad[] = {"010c\r", "01 0C\r", "010C\r04\r", "010C\r\r", "01AB0\r", "01G0\r", "ATRV1\r",
                         "atz\r", "ATZ ",
                         "010C", "ATZ", "ATI"};   // no '\r': the ELM would join it to the next command
    for (const char* c : bad) TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c);
}

// car-can-lab's sniff firmware (spec 2026-10-27): silent CAN monitoring and the receive filters
// join the gate; nothing that transmits does. A bare '\r' stops a monitor (it would also repeat the
// last command, which the gate already let through).
void test_monitor_commands_allowed() {
    for (const char* c : {"ATMA\r", "ATCSM1\r", "ATSP6\r", "ATCRA\r", "ATCRA1XX\r",
                          "ATCRA7E8\r", "ATCF1A0\r", "ATCM7F0\r", "\r"})
        TEST_ASSERT_TRUE_MESSAGE(obd::allowed(c), c);
}

void test_transmitting_or_malformed_refused() {
    for (const char* c : {"ATSH7DF\r", "ATCSM0\r", "ATMA", "atma\r", "ATCF 1A0\r", "ATCF1A00\r",
                          "ATCFZZZ\r", "ATCM1X0\r", "ATCRA1XXX\r", "\r\r", "ATMR00\r", "ATMT00\r",
                          "0322F190\r", "1001\r", "ATBD\r", "ATMA\rATSH7DF\r", ""})
        TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c);
}

void test_new_pids_formulas() {
    float v = 0;
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::PedalD, "41 49 80\n\n", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.2f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Fuel, "412FFF", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Map, "41 0B 23", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.0f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Maf, "41 10 04 D2", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.34f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::Ethanol, "7E8 03 41 52 4D", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.2f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::FuelSys, "41 03 04 00", &v));
    TEST_ASSERT_EQUAL_FLOAT(4.0f, v);
    TEST_ASSERT_EQUAL_STRING("0110\r", obd::request(Pid::Maf));
    TEST_ASSERT_EQUAL_STRING("014A\r", obd::request(Pid::PedalE));
}

void test_supported_bitmap() {
    uint32_t bits[4] = {};
    TEST_ASSERT_TRUE(obd::parseSupported(0, "SEARCHING...\n41 00 BE 1F B8 13\n\n", &bits[0]));
    TEST_ASSERT_EQUAL_HEX32(0xBE1FB813u, bits[0]);
    TEST_ASSERT_TRUE(obd::isSupported(bits, 0x01));    // bit 31
    TEST_ASSERT_FALSE(obd::isSupported(bits, 0x02));
    TEST_ASSERT_TRUE(obd::isSupported(bits, 0x0C));
    TEST_ASSERT_TRUE(obd::isSupported(bits, 0x20));    // bit 0: block 1 exists
    TEST_ASSERT_FALSE(obd::isSupported(bits, 0x49));   // block 2 unread: 0
    TEST_ASSERT_FALSE(obd::isSupported(bits, 0x00));
    TEST_ASSERT_FALSE(obd::parseSupported(1, "41 00 BE 1F B8 13", &bits[1]));   // wrong block
    TEST_ASSERT_FALSE(obd::parseSupported(0, "41 00 BE 1F", &bits[0]));         // cut off
    TEST_ASSERT_FALSE(obd::parseSupported(0, "NO DATA", &bits[0]));
    TEST_ASSERT_NULL(obd::supportRequest(4));
    TEST_ASSERT_EQUAL_STRING("0140\r", obd::supportRequest(2));
}

// The car test (2026-10-19): the engine and transmission ECUs both answer 0100, one line each; the
// first line alone missed coolant and intake. The supported set is every ECU's, ORed.
void test_supported_bitmap_from_every_ecu() {
    uint32_t bits = 0;
    TEST_ASSERT_TRUE(obd::parseSupported(0, "41 00 88 18 00 13\n41 00 BE 3E B8 11\n", &bits));
    TEST_ASSERT_EQUAL_HEX32(0x88180013u | 0xBE3EB811u, bits);
    TEST_ASSERT_TRUE(obd::parseSupported(0, "4100BE3EB811\n4100\n", &bits));   // a cut line: skipped
    TEST_ASSERT_EQUAL_HEX32(0xBE3EB811u, bits);
}

// The car test: the City reports coolant and intake only as 67/68 (A: which sensors, B: sensor 1,
// minus 40), not 05/0F.
void test_extended_coolant_and_intake() {
    float v = 0;
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::CoolantX, "41 67 01 82 00", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, v);
    TEST_ASSERT_EQUAL(Reply::Ok, obd::parse(Pid::IntakeX, "416801470000", &v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 31.0f, v);
    TEST_ASSERT_EQUAL(Reply::Error, obd::parse(Pid::CoolantX, "41 67 02 00 82", &v));   // sensor 1 absent
    TEST_ASSERT_EQUAL_STRING("0167\r", obd::request(Pid::CoolantX));
    TEST_ASSERT_EQUAL_STRING("0168\r", obd::request(Pid::IntakeX));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_extended_coolant_and_intake);
    RUN_TEST(test_supported_bitmap_from_every_ecu);
    RUN_TEST(test_new_pids_formulas);
    RUN_TEST(test_supported_bitmap);
    RUN_TEST(test_every_command_the_code_sends_is_allowed);
    RUN_TEST(test_writes_and_other_modes_are_refused);
    RUN_TEST(test_near_misses_are_refused);
    RUN_TEST(test_monitor_commands_allowed);
    RUN_TEST(test_transmitting_or_malformed_refused);
    RUN_TEST(test_requests_end_with_cr_and_are_read_only);
    RUN_TEST(test_formulas_at_their_ends);
    RUN_TEST(test_what_clones_wrap_around_a_reply);
    RUN_TEST(test_errors_never_give_a_number);
    RUN_TEST(test_battery_voltage_forms);
    RUN_TEST(test_setup_replies);
    RUN_TEST(test_adapter_names);
    return UNITY_END();
}
