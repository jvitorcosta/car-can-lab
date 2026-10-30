# car-can-lab for any car, with or without the ESP32: design

Date: 2026-10-30. Status: approved by the owner. Extends the 2026-10-27 design; where the two
differ, this one wins.

## Why

car-can-lab should be a good open source project, not one person's tool: anyone with a car and an
OBD-II adapter should be able to reverse engineer their own car with it. Two things stand in the
way today: it needs one specific ESP32 board, and parts of it assume the Honda City and the
owner's deck.

## Goals

- **A PC sniffer.** The same method as the firmware from a laptop, over USB, a Bluetooth Classic
  COM port or BLE. It writes the same recording format, so the analyzer works unchanged.
- **An adapter check.** One command that says whether an adapter can be used, before anyone goes
  out to the car. The answer comes as a line ready to paste into the adapter list.
- **A try-it-now demo.** A simulated car and adapter, so a stranger can run the whole method with
  no hardware in a minute.
- **No car or owner assumptions in the code paths**: coolant PID chosen from what the car reports,
  download address taken from the network, no deck-specific messages unless a build asks for them.
- **What an open source project needs**: licence, safety notice, contribution guide, CI, issue
  templates, an adapter compatibility list, and a place for other people's DBC files.

## Non-goals

- 29-bit CAN IDs. The filters, the recording format and the analyzer are 11-bit. A car whose bus
  is 29-bit is refused **by name**, not left on an empty list.
- A package on PyPI, or a GUI. `python -m tools.sniff.<tool>` from a clone is enough for now.
- Other ESP32 boards. The firmware stays on the ESP32-2432S028R.
- STN-specific fast monitoring (`STMA`). A later speed-up for genuine adapters.

## Anti-goals

- **Anything that sends on the car's bus**, from the PC tools too. The gate is the same list as
  the firmware's, and a shared vector file makes the two fail together if they drift.
- A PC tool that looks more tested than it is: nothing here has met a real adapter yet. The docs
  say so until someone reports otherwise.

## Decisions

- **Python port, not a shared core.** The PC side is a small blocking Python version of the
  monitor (`tools/sniff/`), not the C++ state machine bound through a compiler. Blocking code is
  much shorter than the firmware's non-blocking plan, and Python contributors can read it.
- **Same rules as the firmware:** the same probe order; `ATCSM1` must answer `OK` or nothing
  monitors; windows sweep, split on `BUFFER FULL`; REC freezes the IDs heard; `O` readings every 3 s
  with the filter opened to `7E8/7F8`; an `ELM327` banner in the stream means the adapter reset and
  lost silent mode, so it probes again before anything else.
- **Shared gate vectors:** `test/test_obd/gate.inc` lists commands the gate must accept and refuse.
  The C++ test `#include`s it, the Python test parses it.
- **The PC gate sits in one place:** `elm.Adapter.send`. Links only move bytes, and nothing else
  writes to a link. The baud search goes through the same gate.
- **PC recordings are not rate-capped.** The firmware's cap exists to protect 128 KB of flash.
- **Coolant:** `0105` when the `0100` reply lists PID 05, else `0167`. Both may give nothing; the
  recording then has no coolant, which the analyzer already handles.
- **29-bit:** protocols 7 and 9 from `ATDPN` become a new refused state, `29-BIT CAN: NOT SUPPORTED`.
- **Deck-specific text:** the home page's reminder to flash the LED panel back moves to an optional
  `SWAP_HINT` in `src/config.h`. The LOGS page's address fallback no longer names a hotspot address.
- **Deck compatibility rule stays**, documented as what it is: only for people sharing the board
  with car-cyberdeck.
- **Licence: MIT**, the same as the deck repo the code was copied from.
- **Adapter list:** `docs/adapters.md` reports what was tested, by whom, with what result. It
  starts almost empty on purpose: unverified claims are worse than a short list.
- **Demo:** `python -m tools.sniff.sniffer --simulate` runs a 40 s scripted session (rev, shift,
  MARK) against the simulated adapter. The test suite runs the same session on a virtual clock and
  checks the analyzer finds the simulated gear byte, so the demo can't quietly rot.

## Risks

- `ATCSM1` can say `OK` on a clone that ignores it. No AT command can prove otherwise; the docs say
  so, and a listen-only CAN interface is the fully safe option.
- USB adapters at low baud rates overflow quickly. Window splitting handles it, at the cost of
  longer cycles; the cycle time is shown, as on the deck.
- BLE and serial links are tested against fakes only until real adapters report in.
