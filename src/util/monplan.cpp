#include "monplan.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "obd.h"

namespace monplan {
static const char* const PROBE[] = {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATH1\r", "ATSP0\r",
                                    "0100\r", "ATDPN\r", "ATCSM1\r", "ATI\r"};
static constexpr int N_PROBE = sizeof(PROBE) / sizeof(PROBE[0]);
static constexpr int PROBE_0100 = 6;

static bool has(const char* s, const char* w) { return strstr(s, w) != nullptr; }

// The first non-empty line of a reply, without surrounding spaces.
static void firstLine(const char* reply, char* out, size_t cap) {
    const char* p = reply;
    while (*p == '\n' || *p == ' ') p++;
    size_t n = 0;
    while (p[n] && p[n] != '\n') n++;
    while (n > 0 && p[n - 1] == ' ') n--;
    if (n > cap - 1) n = cap - 1;
    memcpy(out, p, n);
    out[n] = '\0';
}

Plan::Plan() { sweep_.scan(); }

void Plan::to(State s, uint32_t nowMs) {
    if (s == state_) return;
    state_ = s;
    since_ = nowMs;
}

void Plan::push(Tag t, const char* text) {
    if (qn_ == static_cast<int>(sizeof queue_ / sizeof queue_[0])) return;
    Cmd& c = queue_[qn_++];
    c.tag = t;
    snprintf(c.text, sizeof c.text, "%s", text);
}

void Plan::note(Note::Kind k, const char* text, float v) {
    const int cap = static_cast<int>(sizeof notes_ / sizeof notes_[0]);
    if (nn_ == cap) {  // nobody is reading: the oldest goes
        nhead_ = (nhead_ + 1) % cap;
        nn_--;
    }
    Note& n = notes_[(nhead_ + nn_) % cap];
    n.kind = k;
    snprintf(n.text, sizeof n.text, "%s", text);
    n.value = v;
    n.ms = now_;
    nn_++;
}

bool Plan::takeNote(Note* out) {
    if (nn_ == 0) return false;
    const int cap = static_cast<int>(sizeof notes_ / sizeof notes_[0]);
    *out = notes_[nhead_];
    nhead_ = (nhead_ + 1) % cap;
    nn_--;
    return true;
}

void Plan::queueProbe(int from) {
    qn_ = 0;
    for (int i = from; i < N_PROBE; i++) push(Tag::Probe, PROBE[i]);
}

void Plan::queueWindow(uint32_t nowMs) {
    (void)nowMs;
    const sweep::Window w = sweep_.current();
    char cf[9], cm[9];
    sweep::commands(w, cf, cm);
    push(Tag::WinCF, cf);
    push(Tag::WinCM, cm);
    push(Tag::WinMA, "ATMA\r");
    char t[32];
    snprintf(t, sizeof t, "window %03X/%03X", static_cast<unsigned>(w.filter), static_cast<unsigned>(w.mask));
    note(Note::Kind::State, t, 0);
}

void Plan::queueObd() {
    // The window's filter would hide the engine's replies (7E8-7EF): open it to them first.
    push(Tag::ObdCF, "ATCF7E8\r");
    push(Tag::ObdCM, "ATCM7F8\r");
    push(Tag::Rpm, "010C\r");
    push(Tag::Coolant, coolant05_ ? "0105\r" : "0167\r");
}

bool Plan::obdDue(uint32_t nowMs) const {
    return recording_ && state_ == State::Monitoring && nowMs - lastObd_ >= OBD_EVERY_MS;
}

void Plan::replan(uint32_t nowMs) {
    if (nFilter_ > 0) sweep_.only(filter_, nFilter_);
    else if (recording_ && nFrozen_ > 0) sweep_.only(frozen_, nFrozen_);
    else sweep_.scan();
    replanned_ = true;
    cycleMs_ = 0;
    cycleStart_ = nowMs;
}

uint32_t Plan::cycleMs() const {
    if (cycleMs_ > 0) return cycleMs_;
    const uint32_t dwell = recording_ ? FROZEN_DWELL_MS : DWELL_MS;
    return static_cast<uint32_t>(sweep_.windows()) * (dwell + 300);  // ~3 round trips per window
}

const char* Plan::nextCommand(uint32_t nowMs) {
    now_ = nowMs;
    switch (state_) {
        case State::Searching:
        case State::Lost:
        case State::NotCan:
        case State::Bus29:
        case State::NotSilent:
        case State::CantMonitor: return nullptr;
        case State::EngineOff:
            if (step_ != Step::Idle || static_cast<int32_t>(nowMs - retryAt_) < 0) return nullptr;
            to(State::Probing, nowMs);
            queueProbe(PROBE_0100);
            break;
        default: break;
    }
    if (step_ == Step::Wait) {
        const bool slow = strcmp(pending_.text, "ATZ\r") == 0 || strcmp(pending_.text, "0100\r") == 0;
        if (nowMs - sentAt_ < (slow ? SLOW_REPLY_MS : REPLY_MS)) return nullptr;
        onTimeout(nowMs);
        if (state_ != State::Probing && state_ != State::Monitoring && state_ != State::Querying) return nullptr;
    }
    if (step_ == Step::Dwell) {
        if (static_cast<int32_t>(nowMs - dwellEnd_) < 0 && !obdDue(nowMs) && !replanned_) return nullptr;
        pending_.tag = Tag::Stop;
        snprintf(pending_.text, sizeof pending_.text, "\r");
        step_ = Step::Wait;
        sentAt_ = nowMs;
        reply_[0] = '\0';
        return pending_.text;
    }
    if (step_ != Step::Idle || qn_ == 0) return nullptr;
    pending_ = queue_[0];
    for (int i = 1; i < qn_; i++) queue_[i - 1] = queue_[i];
    qn_--;
    reply_[0] = '\0';
    sentAt_ = nowMs;
    if (pending_.tag == Tag::WinMA) {
        step_ = Step::Dwell;
        dwellEnd_ = nowMs + (recording_ ? FROZEN_DWELL_MS : DWELL_MS);
    } else {
        step_ = Step::Wait;
        if (pending_.tag == Tag::Rpm) to(State::Querying, nowMs);
    }
    return pending_.text;
}

void Plan::onLine(const char* line, uint32_t nowMs) {
    now_ = nowMs;
    if (!line) return;
    // The banner outside the probe: the adapter reset itself (a brown-out) and lost its settings,
    // silent monitoring among them. Probe again before anything else.
    if (strncmp(line, "ELM327", 6) == 0 && !(step_ == Step::Wait && pending_.tag == Tag::Probe)) {
        note(Note::Kind::State, "adapter reset", 0);
        step_ = Step::Idle;
        bufferFull_ = false;
        replanned_ = false;
        to(State::Probing, nowMs);
        queueProbe(0);
        return;
    }
    if (strcmp(line, ">") == 0) {
        if (step_ == Step::Idle) return;
        step_ = Step::Idle;
        answered(nowMs);
        return;
    }
    if (step_ == Step::Dwell) {
        if (has(line, "BUFFER FULL")) {
            bufferFull_ = true;
            note(Note::Kind::State, "buffer full", 0);
            pending_.tag = Tag::Stop;  // the ELM stops by itself: its prompt comes next
            pending_.text[0] = '\0';
            step_ = Step::Wait;
            sentAt_ = nowMs;
        } else if (strcmp(line, "?") == 0) {
            snprintf(reply_, sizeof reply_, "?");
        }
        return;
    }
    if (step_ == Step::Wait && pending_.tag == Tag::Stop && has(line, "BUFFER FULL")) {
        // the overflow raced the dwell-end '\r': still an overflow
        if (!bufferFull_) note(Note::Kind::State, "buffer full", 0);
        bufferFull_ = true;
        return;
    }
    if (step_ == Step::Wait) {
        const size_t n = strlen(reply_);
        snprintf(reply_ + n, sizeof reply_ - n, "%s%s", n ? "\n" : "", line);
    }
}

void Plan::answered(uint32_t nowMs) {
    switch (pending_.tag) {
        case Tag::Probe: {
            const char* c = pending_.text;
            if (strcmp(c, "0100\r") == 0) {
                uint32_t bits = 0;
                if (!obd::parseSupported(0, reply_, &bits)) {
                    qn_ = 0;
                    retryAt_ = nowMs + ENGINE_RETRY_MS;
                    to(State::EngineOff, nowMs);
                } else {
                    const uint32_t supported[4] = {bits, 0, 0, 0};
                    coolant05_ = obd::isSupported(supported, 0x05);
                }
            } else if (strcmp(c, "ATDPN\r") == 0) {
                char p[sizeof proto_];
                firstLine(reply_, p, sizeof p);
                const char* d = p[0] == 'A' ? p + 1 : p;
                if (!(d[0] >= '6' && d[0] <= '9' && d[1] == '\0')) {
                    qn_ = 0;
                    to(State::NotCan, nowMs);
                } else if (d[0] == '7' || d[0] == '9') {  // the filters and the log are 11-bit
                    qn_ = 0;
                    to(State::Bus29, nowMs);
                } else {
                    snprintf(proto_, sizeof proto_, "%s", p);
                }
            } else if (strcmp(c, "ATCSM1\r") == 0) {
                if (!has(reply_, "OK")) {
                    qn_ = 0;
                    to(State::NotSilent, nowMs);
                }
            } else if (strcmp(c, "ATI\r") == 0) {
                firstLine(reply_, adapter_, sizeof adapter_);
                to(State::Monitoring, nowMs);
                if (lost_ && recording_) note(Note::Kind::State, "adapter back", 0);
                lost_ = false;
                replanned_ = false;
                cycleStart_ = nowMs;
                queueWindow(nowMs);
            } else if (!obd::setupOk(c, reply_)) {
                qn_ = 0;
                to(State::CantMonitor, nowMs);
            }
            break;
        }
        case Tag::WinCF:
        case Tag::WinCM:
            if (has(reply_, "OK")) {
                if (pending_.tag == Tag::WinCM) filterFails_ = 0;
                break;
            }
            // No OK: refused, or it landed in a monitor still running (a '\r' that met an idle ELM
            // repeats ATMA; its frames, then a prompt, come back). The window again, a few times.
            qn_ = 0;
            if (++filterFails_ >= FILTER_TRIES) {
                to(State::CantMonitor, nowMs);
                break;
            }
            queueWindow(nowMs);
            break;
        case Tag::WinMA:
            if (has(reply_, "?")) {
                qn_ = 0;
                to(State::CantMonitor, nowMs);
                break;
            }
            [[fallthrough]];  // stopped early (the ELM's own stop): as a stop
        case Tag::Stop:
            if (bufferFull_) {
                bufferFull_ = false;
                if (!sweep_.split()) {  // one ID, or no room for more windows: on to the next
                    note(Note::Kind::State, "window too busy", 0);
                    sweep_.next();
                    if (sweep_.wrapped()) {
                        cycleMs_ = nowMs - cycleStart_;
                        cycleStart_ = nowMs;
                    }
                }
                queueWindow(nowMs);
            } else if (obdDue(nowMs)) {
                lastObd_ = nowMs;
                replanned_ = false;  // the window after the readings is the new plan's first
                queueObd();
            } else if (replanned_) {
                replanned_ = false;
                queueWindow(nowMs);
            } else {
                sweep_.next();
                if (sweep_.wrapped()) {
                    cycleMs_ = nowMs - cycleStart_;
                    cycleStart_ = nowMs;
                }
                queueWindow(nowMs);
            }
            break;
        case Tag::ObdCF:
        case Tag::ObdCM: break;
        case Tag::Rpm: {
            float v = 0;
            if (obd::parse(obd::Pid::Rpm, reply_, &v) == obd::Reply::Ok) note(Note::Kind::Obd, "rpm", v);
            break;
        }
        case Tag::Coolant: {
            float v = 0;
            if (obd::parse(coolant05_ ? obd::Pid::Coolant : obd::Pid::CoolantX, reply_, &v) == obd::Reply::Ok &&
                !std::isnan(v))
                note(Note::Kind::Obd, "coolant", v);
            to(State::Monitoring, nowMs);
            queueWindow(nowMs);
            break;
        }
        case Tag::None: break;
    }
}

void Plan::onTimeout(uint32_t nowMs) {
    now_ = nowMs;
    step_ = Step::Idle;
    switch (pending_.tag) {
        case Tag::Probe:
            if (strcmp(pending_.text, "0100\r") == 0) {  // no answer from the car
                qn_ = 0;
                retryAt_ = nowMs + ENGINE_RETRY_MS;
                to(State::EngineOff, nowMs);
            } else {
                queueProbe(0);  // the adapter lost the thread: start over
            }
            break;
        case Tag::Stop:  // no prompt after BUFFER FULL or the '\r': stop it (again)
            qn_ = 0;
            push(Tag::Stop, "\r");
            break;
        case Tag::Rpm:  // no reading this time: carry on
            break;
        case Tag::Coolant:
            to(State::Monitoring, nowMs);
            queueWindow(nowMs);
            break;
        default:  // a filter command unanswered: the window from the start
            qn_ = 0;
            if (state_ == State::Querying) to(State::Monitoring, nowMs);
            queueWindow(nowMs);
            break;
    }
}

void Plan::onLinkLost(uint32_t nowMs) {
    now_ = nowMs;
    qn_ = 0;
    step_ = Step::Idle;
    bufferFull_ = false;
    if (recording_ && !lost_) note(Note::Kind::State, "adapter lost", 0);
    lost_ = true;
    to(State::Lost, nowMs);
}

void Plan::onLinkBack(uint32_t nowMs) {
    now_ = nowMs;
    step_ = Step::Idle;
    bufferFull_ = false;
    replanned_ = false;
    filterFails_ = 0;
    to(State::Probing, nowMs);
    queueProbe(0);
}

void Plan::setRecording(bool on, const uint16_t* ids, int n, uint32_t nowMs) {
    now_ = nowMs;
    recording_ = on;
    nFrozen_ = 0;
    if (on) {
        for (int i = 0; ids && i < n && nFrozen_ < static_cast<int>(sizeof frozen_ / sizeof frozen_[0]); i++)
            frozen_[nFrozen_++] = ids[i];
        lastObd_ = nowMs;
    }
    replan(nowMs);
}

void Plan::setFilter(const uint16_t* ids, int n) {
    nFilter_ = 0;
    for (int i = 0; ids && i < n && nFilter_ < static_cast<int>(sizeof filter_ / sizeof filter_[0]); i++)
        filter_[nFilter_++] = ids[i];
    char t[32];
    if (nFilter_) snprintf(t, sizeof t, "filter on %d", nFilter_);
    else snprintf(t, sizeof t, "filter off");
    note(Note::Kind::State, t, 0);
    replan(now_);
}
}
