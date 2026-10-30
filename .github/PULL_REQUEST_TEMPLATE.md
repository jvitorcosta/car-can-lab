## What and why

## Checks

- [ ] `python -m pytest` passes
- [ ] Firmware changes: `pwsh tools/run_tests.ps1` and `pio run -e sniff` pass
- [ ] Nothing here can transmit on the bus, and `test/test_obd/gate.inc` is unchanged, or changed along with both gates
- [ ] No recordings, secrets or network names
