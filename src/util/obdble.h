// Copied from spotify-pokemon-deck src/util/obdble.h @ df11e4a (branch feat/obd).
#pragma once
#include <cstddef>
#include <cstdint>
// The OBD link over Bluetooth Low Energy (the owner's "OBDBLE" adapter): which characteristics
// carry the ELM327's serial stream, and the inbox its notifications fill. Clones expose the stream
// as one GATT service with a notify (or indicate) characteristic for replies and a write one for
// commands: FFF0 (FFF1 / FFF2), FFE0 (FFE1 both ways) or 18F0 (2AF0 / 2AF1). PURE, host-tested.
namespace obdble {
// One characteristic as discovered: its service's and its own 16-bit UUID (0 when 128-bit), and
// what it can do.
struct Char { uint16_t service, uuid; bool write, writeNoResp, notify, indicate; };
struct Pick { int write = -1, notify = -1; };   // indices into the list; -1: none usable
// FFF0, then FFE0, then 18F0, then any other service, the first with both a notify/indicate and a
// write characteristic (the same one may do both).
Pick pick(const Char* chars, int n);
// A 16-bit service UUID the clones use for the serial stream (FFF0, FFE0, 18F0): an advertised one
// marks an adapter even when it sends no name.
bool serialService(uint16_t uuid);

// Bytes from the adapter's notifications, in order, read one by one up to the '>' prompt. A push
// that doesn't fit is cut (a runaway reply, never a normal one). Not thread-safe: the link guards it.
class Inbox {
public:
    static constexpr size_t CAP = 512;
    void push(const uint8_t* data, size_t len);
    bool pop(char* c);
    void clear() { head_ = count_ = 0; }
private:
    char buf_[CAP];
    size_t head_ = 0, count_ = 0;
};
}
