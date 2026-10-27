// Copied from spotify-pokemon-deck src/util/obd.h @ df11e4a (branch feat/obd).
#pragma once
#include <cstddef>
#include <cstdint>
// ELM327 requests and replies for the old deck's live gauges (companion/obdlink): mode 01 PIDs
// and the adapter's own battery reading. Read-only by design: nothing here can send mode 03/04.
// Tolerates what cheap clones wrap around a reply (echo, SEARCHING..., CAN headers, several
// ECUs, spaces or none). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obd {
// PedalD/E: accelerator pedal (49/4A); Fuel: tank level (2F); Map: manifold kPa (0B); Maf: air g/s
// (10); Ethanol: % (52); FuelSys: fuel system 1 status (03; 4 = open loop on deceleration: fuel cut);
// CoolantX / IntakeX: the extended coolant / intake readings (67 / 68, sensor 1), all the City has.
enum class Pid : uint8_t { Rpm, Speed, Coolant, Load, Intake, Throttle, Battery, PedalD, PedalE, Fuel,
                           Map, Maf, Ethanol, FuelSys, CoolantX, IntakeX, COUNT };
// Ok: *value set. NoData: the car doesn't support this PID. NoEngine: the adapter is up but the
// car doesn't answer (ignition off). Error: anything else (timeout, junk, cut-off reply).
enum class Reply : uint8_t { Ok, NoData, NoEngine, Error };
const char* request(Pid p);          // the command, ending in '\r'
// raw: everything the adapter sent before its '>' prompt. *value is touched only on Ok.
Reply parse(Pid p, const char* raw, float* value);
// The supported-PID scan: block 0..3 asks 0100/0120/0140/0160 (nullptr past 3); parseSupported
// reads the 4 bytes after "41 <block*20h>" (bit 31 = PID block*20h + 1), ORed over every ECU's
// line (engine and transmission both answer); isSupported looks a PID
// (01..80h) up in the four blocks read.
const char* supportRequest(int block);
bool parseSupported(int block, const char* raw, uint32_t* bits);
bool isSupported(const uint32_t bits[4], uint8_t pid);
// The only gate to the car (spec 2026-10-19 "Read-only guarantee"): true for exactly a mode 01
// request ("01" + two upper-case hex digits) or one of the adapter's own setup commands (ATZ, ATE0,
// ATL0, ATS0, ATH0, ATH1, ATSP0, ATRV, ATI, ATDPN), each ending in exactly one '\r'.
// car-can-lab adds silent CAN monitoring and nothing that transmits: ATCSM1, ATSP6, ATMA, ATCRA,
// ATCRA + 3 of hex or X, ATCF / ATCM + 3 hex, and a bare '\r' (stops a monitor). Anything else,
// any other mode, a header (ATSH), ATCSM0, ATMR / ATMT, lower case, spacing or a second command, is false.
bool allowed(const char* cmd);
// The adapter's answer to one of its setup commands is a real one: ATZ's banner ("ELM...", or
// "OK" on some clones), "OK" for the rest. A cut-off banner or a "?" is not.
bool setupOk(const char* cmd, const char* raw);
// A Bluetooth name that is probably an ELM327 (OBD, ELM, V-LINK / VLINK, any case).
bool looksLikeAdapter(const char* btName);
}
