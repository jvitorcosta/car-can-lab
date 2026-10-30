# Changelog

## Unreleased

Nothing here has run on a real adapter or car yet: everything is tested on the PC.

### Added

- PC sniffer, `tools.sniff.sniffer`, over USB, a Bluetooth Classic COM port or BLE.
- `--simulate`: the whole method on a simulated car, with no hardware.
- Adapter check, `tools.sniff.adaptercheck`, and the adapter list in `docs/adapters.md`.
- A shared gate test list, `test/test_obd/gate.inc`, for the firmware and the PC tools.
- MIT licence, contribution and safety guides, CI and issue templates.

### Changed

- Coolant uses PID 05 when the car lists it, else 67.
- 29-bit CAN buses are refused by name.
- The LOGS page shows the board's real address, and the LED panel reminder is now an optional
  `SWAP_HINT` in `src/config.h`.

### Fixed

- The analyzer dropped a change that shared its millisecond with the next window's log line.
