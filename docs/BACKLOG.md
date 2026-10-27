# Backlog

## Waiting on the owner (on the deck)

Nothing here has been flashed to the deck yet: the owner flashes and checks each item.

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
  frames come in. The Xbox controller must stay connected throughout.
