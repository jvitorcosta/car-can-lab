#pragma once
#include <cstdint>
#include "sweep.h"
// What to say to the ELM327 next, and what its answers mean (spec 2026-10-27 "Covering the bus").
// The probe on every (re)connect, in this order: ATZ, ATE0, ATL0, ATS0, ATH1, ATSP0, 0100 (no
// answer: ENGINE OFF?, again every ENGINE_RETRY_MS), ATDPN (must be 11-bit CAN: 6 or 8, A6 or A8; 7 and 9 are 29-bit, Bus29),
// ATCSM1 (silent monitoring: must be OK, else NOT SILENT and never ATMA), ATI (the adapter's name
// for the recording's header). Then, window by window (util/sweep): ATCF, ATCM, ATMA, dwell, '\r'
// to stop, wait for the prompt. BUFFER FULL splits the window and runs it again. While recording,
// every OBD_EVERY_MS: stop, open the filter to the engine's replies (ATCF7E8 / ATCM7F8), read rpm
// (010C) and coolant (0105 when the car's 0100 reply lists it, else 0167), then the window again.
// Every command it returns passes obd::allowed (the device's link refuses anything else anyway).
// Frames are the caller's: it parses each line (util/elmmon) for the table and the recording and
// hands every line here too. PURE, host-tested.
namespace monplan {
enum class State : uint8_t { Searching, Probing, NotCan, Bus29, NotSilent, CantMonitor, EngineOff, Monitoring, Querying, Lost };

// For the recording: a state change ("window 1A0/7F0", "buffer full", "adapter lost", ...) or an
// OBD value (text "rpm" / "coolant").
struct Note {
    enum class Kind : uint8_t { State, Obd };
    Kind kind;
    char text[32];
    float value;
    uint32_t ms;  // when it happened
};

class Plan {
public:
    static constexpr uint32_t DWELL_MS = 1500, FROZEN_DWELL_MS = 300, OBD_EVERY_MS = 3000;
    static constexpr uint32_t ENGINE_RETRY_MS = 5000;
    static constexpr int FILTER_TRIES = 3;  // an ATCF / ATCM without OK this often in a row: CantMonitor
    static constexpr uint32_t REPLY_MS = 2000, SLOW_REPLY_MS = 8000;  // SLOW: ATZ, 0100 (SEARCHING...)
    Plan();
    // The command to send now, or nullptr (wait: for a reply, the dwell, a retry, the link).
    const char* nextCommand(uint32_t nowMs);
    // Each line from the adapter, the '>' prompt included (util/elmmon::Lines).
    void onLine(const char* line, uint32_t nowMs);
    // The last command's answer didn't come (nextCommand calls it itself past REPLY_MS / SLOW_REPLY_MS).
    void onTimeout(uint32_t nowMs);
    void onLinkLost(uint32_t nowMs);
    void onLinkBack(uint32_t nowMs);  // also the first connect: probes
    // REC: on freezes the windows to `ids` (the table's IDs; FILTER's picks win when set), off
    // goes back to the sweep (or FILTER).
    void setRecording(bool on, const uint16_t* ids, int n, uint32_t nowMs);
    // FILTER: only these IDs; none: the sweep (or REC's frozen set).
    void setFilter(const uint16_t* ids, int n);
    State state() const { return state_; }
    uint32_t stateSinceMs() const { return since_; }
    // One full cycle of the windows: measured once a cycle has run, estimated before.
    uint32_t cycleMs() const;
    bool takeNote(Note* out);
    const char* adapter() const { return adapter_; }
    const char* protocol() const { return proto_; }
    const sweep::Plan& windows() const { return sweep_; }
    bool recording() const { return recording_; }
private:
    enum class Tag : uint8_t { None, Probe, WinCF, WinCM, WinMA, Stop, ObdCF, ObdCM, Rpm, Coolant };
    enum class Step : uint8_t { Idle, Wait, Dwell };
    struct Cmd {
        Tag tag;
        char text[10];
    };
    void to(State s, uint32_t nowMs);
    void push(Tag t, const char* text);
    void queueProbe(int from);
    void queueWindow(uint32_t nowMs);
    void queueObd();
    void replan(uint32_t nowMs);
    void answered(uint32_t nowMs);
    void note(Note::Kind k, const char* text, float v);
    bool obdDue(uint32_t nowMs) const;

    State state_ = State::Searching;
    uint32_t since_ = 0;
    sweep::Plan sweep_;
    Cmd queue_[12] = {};
    int qn_ = 0;
    Cmd pending_ = {};
    Step step_ = Step::Idle;
    uint32_t sentAt_ = 0, dwellEnd_ = 0, retryAt_ = 0, lastObd_ = 0;
    int probeIdx_ = 0, filterFails_ = 0;
    char reply_[160] = {};
    bool bufferFull_ = false, replanned_ = false, recording_ = false, lost_ = false;
    uint16_t filter_[96] = {}, frozen_[96] = {};
    bool coolant05_ = false;
    int nFilter_ = 0, nFrozen_ = 0;
    uint32_t cycleStart_ = 0, cycleMs_ = 0, now_ = 0;
    char adapter_[32] = {}, proto_[8] = {};
    Note notes_[16] = {};
    int nhead_ = 0, nn_ = 0;
};
}
