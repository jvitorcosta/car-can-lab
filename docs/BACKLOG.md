# Backlog

## Waiting on the owner (on the deck)

Nothing here has been flashed to the deck yet: the owner flashes and checks each item.

- [ ] PC tools, first real adapter: `adaptercheck` and `sniffer` over the clone's Classic COM port,
  then over BLE, then the Vgate. Only fakes have tested them, so expect small fixes (baud search,
  BLE characteristics, reply formats). Add the results to `docs/adapters.md`.
- [ ] Boot on a build with no `SWAP_HINT`, and on one with it: two boot lines or three, the home
  page's reminder present or not, and the LOGS page's address both on and off WiFi.

- [ ] Task 1: from the running LED panel, `pio run -e sniff_ota -t upload` (PC on the hotspot).
  Expect the boot line `CAR-CAN-LAB // SNIFF`, no calibration message, `[ota] this firmware is
  good` on Serial. Then the deck repo's `pio run -e ledpanel_ota -t upload` puts the panel back
  with its settings (touch, looks, trip, Xbox bond).
- [ ] Task 7: build with `-D STORE_TEST` (e.g. `PLATFORMIO_BUILD_FLAGS="-D STORE_TEST"`), flash it,
  then from the phone on the hotspot `http://172.20.10.11/recs?k=<token>` lists `001.log` and
  `/rec?n=1&k=<token>` downloads 1,000 `F` lines intact; without `k`: 403. Flash `ledpanel_ota` and
  back: the file is still listed.
- [ ] Task 8: with the clone's BLE side ("OBDBLE"), Serial shows `[link] adapter connected`, the
  probe, and either `[mon] state 6` (Monitoring) or a named refusal state (3 NotSilent, 4
  CantMonitor, 2 NotCan, 5 EngineOff). With the Vgate when it arrives: in the car, engine on,
  frames come in. A connected Xbox controller must stay connected throughout.
- [ ] Task 9: with the adapter, rows appear, a changing byte lights and fades, PAUSE freezes,
  FILTER narrows to the tapped rows, MARK and REC show in the header; after 18:00 it turns
  orange. Check the rows' 19 px pitch reads well on the deck. Do it twice:
  - **touch only** (controller off): tap rows to pick, every button, HOME's LIVE / LOGS, LOGS'
    BACK; a tap never lands on the page it just opened; with BOOT held 2 s the five crosses
    calibrate and the taps still hit afterwards;
  - **Xbox only** (no touching): D-pad rows, A pick, X MARK, Y REC, LB FILTER, RB PAUSE, B back,
    D-pad left / right + A on HOME.
