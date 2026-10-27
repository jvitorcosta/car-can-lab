#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/util/monplan.h"
#include "../../src/util/obd.h"

using monplan::Plan;
using monplan::State;

void setUp() {}
void tearDown() {}

static uint32_t now = 0;
static std::vector<std::string> sent;

// Every command the plan ever returns goes through here: it must pass the read-only gate.
static std::string cmd(Plan& p) {
    const char* c = p.nextCommand(now);
    if (!c) return "";
    TEST_ASSERT_TRUE_MESSAGE(obd::allowed(c), c);
    sent.push_back(c);
    return c;
}

// The adapter answers the last command: its lines, then the prompt.
static void reply(Plan& p, std::initializer_list<const char*> lines) {
    for (const char* l : lines) p.onLine(l, now);
    p.onLine(">", now);
}

static bool everSent(const char* c) {
    for (const std::string& s : sent)
        if (s == c) return true;
    return false;
}

static void expect(Plan& p, const char* want) {
    const std::string got = cmd(p);
    TEST_ASSERT_EQUAL_STRING(want, got.c_str());
}

// Link up, the probe answered as the Vgate would: ends in Monitoring with ATMA sent for window 0.
static void probe(Plan& p, const char* cf = "ATCF000\r", const char* cm = "ATCM780\r") {
    now = 1000;
    sent.clear();
    p.onLinkBack(now);
    TEST_ASSERT_EQUAL(State::Probing, p.state());
    expect(p, "ATZ\r");
    reply(p, {"ATZ", "", "ELM327 v2.2"});
    expect(p, "ATE0\r");
    reply(p, {"ATE0", "OK"});
    expect(p, "ATL0\r");
    reply(p, {"OK"});
    expect(p, "ATS0\r");
    reply(p, {"OK"});
    expect(p, "ATH1\r");
    reply(p, {"OK"});
    expect(p, "ATSP0\r");
    reply(p, {"OK"});
    expect(p, "0100\r");
    reply(p, {"SEARCHING...", "7E8064100BE3EB811"});
    expect(p, "ATDPN\r");
    reply(p, {"A6"});
    expect(p, "ATCSM1\r");
    reply(p, {"OK"});
    expect(p, "ATI\r");
    reply(p, {"ELM327 v2.2"});
    TEST_ASSERT_EQUAL(State::Monitoring, p.state());
    TEST_ASSERT_EQUAL_STRING("ELM327 v2.2", p.adapter());
    TEST_ASSERT_EQUAL_STRING("A6", p.protocol());
    expect(p, cf);
    reply(p, {"OK"});
    expect(p, cm);
    reply(p, {"OK"});
    expect(p, "ATMA\r");
}

static void drainNotes(Plan& p, std::vector<std::string>* out) {
    monplan::Note n;
    while (p.takeNote(&n)) out->push_back(n.text);
}

void test_happy_probe_reaches_monitoring() {
    Plan p;
    TEST_ASSERT_EQUAL(State::Searching, p.state());
    TEST_ASSERT_EQUAL_STRING("", cmd(p).c_str());  // no link: nothing to send
    probe(p);
}

void test_dwell_then_stop_then_next_window() {
    Plan p;
    probe(p);
    p.onLine("1A60004", now);
    now += 1000;
    TEST_ASSERT_EQUAL_STRING("", cmd(p).c_str());  // still dwelling
    now += Plan::DWELL_MS;
    expect(p, "\r");
    TEST_ASSERT_EQUAL_STRING("", cmd(p).c_str());  // waits for the prompt
    reply(p, {"STOPPED"});
    expect(p, "ATCF080\r");
}

void test_not_can_never_monitors() {
    Plan p;
    now = 0;
    sent.clear();
    p.onLinkBack(now);
    const char* answers[][2] = {{"ELM327 v1.5", ""}, {"OK", ""}, {"OK", ""}, {"OK", ""}, {"OK", ""}, {"OK", ""},
                                {"410098180013", ""}, {"3", ""}};
    for (auto& a : answers) {
        cmd(p);
        reply(p, {a[0]});
    }
    TEST_ASSERT_EQUAL(State::NotCan, p.state());
    for (int i = 0; i < 20; i++, now += 1000) cmd(p);
    TEST_ASSERT_FALSE(everSent("ATMA\r"));
    TEST_ASSERT_FALSE(everSent("ATCSM1\r"));
}

void test_not_silent_never_monitors() {
    Plan p;
    now = 0;
    sent.clear();
    p.onLinkBack(now);
    const char* answers[] = {"ELM327 v1.5", "OK", "OK", "OK", "OK", "OK", "4100BE3EB811", "6", "?"};
    for (const char* a : answers) {
        cmd(p);
        reply(p, {a});
    }
    TEST_ASSERT_EQUAL(State::NotSilent, p.state());
    for (int i = 0; i < 20; i++, now += 1000) cmd(p);
    TEST_ASSERT_FALSE(everSent("ATMA\r"));
}

void test_engine_off_retries() {
    for (const char* no : {"NO DATA", "UNABLE TO CONNECT"}) {
        Plan p;
        now = 0;
        sent.clear();
        p.onLinkBack(now);
        for (const char* a : {"ELM327 v2.2", "OK", "OK", "OK", "OK", "OK"}) {
            cmd(p);
            reply(p, {a});
        }
        expect(p, "0100\r");
        reply(p, {"SEARCHING...", no});
        TEST_ASSERT_EQUAL(State::EngineOff, p.state());
        TEST_ASSERT_EQUAL_STRING("", cmd(p).c_str());
        now += Plan::ENGINE_RETRY_MS;
        expect(p, "0100\r");
        reply(p, {"4100BE3EB811"});
        expect(p, "ATDPN\r");
        TEST_ASSERT_EQUAL(State::Probing, p.state());
    }
}

void test_buffer_full_splits_and_repeats_the_window() {
    Plan p;
    probe(p);
    TEST_ASSERT_EQUAL(16, p.windows().windows());
    p.onLine("1A60004", now);
    p.onLine("BUFFER FULL", now);
    TEST_ASSERT_EQUAL_STRING("", cmd(p).c_str());  // the ELM stops by itself: wait for its prompt
    p.onLine(">", now);
    TEST_ASSERT_EQUAL(17, p.windows().windows());
    expect(p, "ATCF000\r");
    reply(p, {"OK"});
    expect(p, "ATCM7C0\r");
    std::vector<std::string> notes;
    drainNotes(p, &notes);
    bool full = false;
    for (auto& n : notes) full = full || n == "buffer full";
    TEST_ASSERT_TRUE(full);
}

void test_buffer_full_without_a_prompt_is_stopped() {
    Plan p;
    probe(p);
    p.onLine("BUFFER FULL", now);
    now += Plan::REPLY_MS;
    expect(p, "\r");
}

void test_recording_reads_rpm_and_coolant() {
    Plan p;
    probe(p);
    const uint16_t ids[] = {0x1A6, 0x305};
    p.setRecording(true, ids, 2, now);
    now += Plan::OBD_EVERY_MS;
    expect(p, "\r");
    reply(p, {});
    expect(p, "ATCF7E8\r");
    reply(p, {"OK"});
    expect(p, "ATCM7F8\r");
    reply(p, {"OK"});
    expect(p, "010C\r");
    TEST_ASSERT_EQUAL(State::Querying, p.state());
    reply(p, {"7E804410C0CB0"});
    expect(p, "0167\r");
    reply(p, {"7E8054167037E00"});
    TEST_ASSERT_EQUAL(State::Monitoring, p.state());
    expect(p, "ATCF1A6\r");  // the frozen set's first window
    std::vector<std::string> notes;
    monplan::Note n;
    bool rpm = false, coolant = false;
    while (p.takeNote(&n)) {
        if (n.kind == monplan::Note::Kind::Obd && strcmp(n.text, "rpm") == 0) rpm = n.value > 811 && n.value < 813;
        if (n.kind == monplan::Note::Kind::Obd && strcmp(n.text, "coolant") == 0) coolant = n.value > 85 && n.value < 87;
    }
    TEST_ASSERT_TRUE(rpm);
    TEST_ASSERT_TRUE(coolant);
}

void test_recording_freezes_the_windows_and_logs_them() {
    Plan p;
    probe(p);
    const uint16_t ids[] = {0x1A6, 0x1A7, 0x305};
    p.setRecording(true, ids, 3, now);
    TEST_ASSERT_EQUAL(2, p.windows().windows());
    now += Plan::FROZEN_DWELL_MS;
    expect(p, "\r");
    reply(p, {});
    expect(p, "ATCF1A4\r");
    reply(p, {"OK"});
    expect(p, "ATCM7FC\r");
    reply(p, {"OK"});
    expect(p, "ATMA\r");
    std::vector<std::string> notes;
    drainNotes(p, &notes);
    bool logged = false;
    for (auto& s : notes) logged = logged || s == "window 1A4/7FC";
    TEST_ASSERT_TRUE(logged);
}

void test_filter_watches_only_the_picks() {
    Plan p;
    probe(p);
    const uint16_t pick[] = {0x191};
    p.setFilter(pick, 1);
    now += Plan::DWELL_MS;
    expect(p, "\r");
    reply(p, {});
    expect(p, "ATCF191\r");
    reply(p, {"OK"});
    expect(p, "ATCM7FF\r");
    p.setFilter(nullptr, 0);
    TEST_ASSERT_EQUAL(16, p.windows().windows());
}

// Review focus 2: the adapter drops mid-recording and comes back.
void test_link_lost_and_back_while_recording() {
    Plan p;
    probe(p);
    const uint16_t ids[] = {0x1A6, 0x305};
    p.setRecording(true, ids, 2, now);
    std::vector<std::string> notes;
    drainNotes(p, &notes);
    p.onLinkLost(now);
    TEST_ASSERT_EQUAL(State::Lost, p.state());
    TEST_ASSERT_EQUAL_STRING("", cmd(p).c_str());
    probe(p, "ATCF1A6\r", "ATCM7FF\r");  // re-probes from ATZ (probe() asserts the order), resumes the same windows
    TEST_ASSERT_EQUAL(2, p.windows().windows());
    notes.clear();
    drainNotes(p, &notes);
    bool lost = false, back = false;
    for (auto& s : notes) {
        lost = lost || s == "adapter lost";
        back = back || s == "adapter back";
    }
    TEST_ASSERT_TRUE(lost);
    TEST_ASSERT_TRUE(back);
}

void test_monitor_refused_is_cant_monitor() {
    Plan p;
    probe(p);
    p.onLine("?", now);
    p.onLine(">", now);
    TEST_ASSERT_EQUAL(State::CantMonitor, p.state());
    sent.clear();
    for (int i = 0; i < 10; i++, now += 1000) cmd(p);
    TEST_ASSERT_FALSE(everSent("ATMA\r"));
}

void test_probe_timeout_starts_over() {
    Plan p;
    now = 0;
    sent.clear();
    p.onLinkBack(now);
    expect(p, "ATZ\r");
    now += Plan::SLOW_REPLY_MS;
    expect(p, "ATZ\r");
}

void test_cycle_time_is_measured() {
    Plan p;
    probe(p);
    TEST_ASSERT_TRUE(p.cycleMs() > 0);
    for (int w = 0; w < 16; w++) {
        now += Plan::DWELL_MS;
        expect(p, "\r");
        reply(p, {});
        cmd(p);
        reply(p, {"OK"});
        cmd(p);
        reply(p, {"OK"});
        expect(p, "ATMA\r");
    }
    TEST_ASSERT_UINT32_WITHIN(100, 16 * Plan::DWELL_MS, p.cycleMs());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_happy_probe_reaches_monitoring);
    RUN_TEST(test_dwell_then_stop_then_next_window);
    RUN_TEST(test_not_can_never_monitors);
    RUN_TEST(test_not_silent_never_monitors);
    RUN_TEST(test_engine_off_retries);
    RUN_TEST(test_buffer_full_splits_and_repeats_the_window);
    RUN_TEST(test_buffer_full_without_a_prompt_is_stopped);
    RUN_TEST(test_recording_reads_rpm_and_coolant);
    RUN_TEST(test_recording_freezes_the_windows_and_logs_them);
    RUN_TEST(test_filter_watches_only_the_picks);
    RUN_TEST(test_link_lost_and_back_while_recording);
    RUN_TEST(test_monitor_refused_is_cant_monitor);
    RUN_TEST(test_probe_timeout_starts_over);
    RUN_TEST(test_cycle_time_is_measured);
    return UNITY_END();
}
