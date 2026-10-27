#pragma once
#include <cstddef>
#include <cstdint>
// What an ELM327 says while it monitors the CAN bus (ATMA with headers on, ATH1): one frame per
// line, "1A6 00 04 00 00 3C 00 00 11" with spaces (ATS1) or "1A60004000003C000011" without (ATS0),
// an 11-bit ID then 0-8 data bytes; or a status ("BUFFER FULL", "STOPPED", "CAN ERROR"), or the
// '>' prompt once it has stopped. Lines carries the BLE notifications' bytes, which cut lines
// anywhere, into whole lines. PURE, host-tested.
namespace elmmon {
struct Frame {
    uint16_t id;
    uint8_t len;
    uint8_t data[8];
};
// Error: CAN ERROR, <RX ERROR, ? (the adapter refused the last command). Other: anything else (OK,
// a banner, a cut-off or malformed frame).
enum class Kind : uint8_t { Frame, BufferFull, Stopped, Error, Prompt, Other };
// *out is written only for Frame.
Kind parse(const char* line, Frame* out);

// Bytes in, whole lines out: splits on '\r' and '\n', drops empty lines, yields '>' as a line of
// its own (the ELM sends it with no newline after). A line longer than CAP - 1 is cut there and
// the rest of it, up to its newline, discarded. CUT (obdble::Inbox's mark for lost bytes) drops
// the line it falls in, both sides of it.
class Lines {
public:
    static constexpr size_t CAP = 96;
    static constexpr char CUT = '\x18';
    void push(const char* bytes, size_t n);
    // The oldest complete line into out (cut to outCap - 1); false when none is complete.
    bool next(char* out, size_t outCap);
    void clear() { qn_ = cur_ = 0; skipping_ = false; }
private:
    static constexpr int QUEUE = 16;  // complete lines waiting; the oldest goes when it's full
    void finish();
    char q_[QUEUE][CAP] = {};
    int qhead_ = 0, qn_ = 0;
    char line_[CAP] = {};
    size_t cur_ = 0;
    bool skipping_ = false;
};
}
