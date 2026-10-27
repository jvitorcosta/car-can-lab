# Backlog

## Waiting on the owner (on the deck)

Nothing here has been flashed to the deck yet: the owner flashes and checks each item.

- [ ] Task 1: from the running LED panel, `pio run -e sniff_ota -t upload` (PC on the hotspot).
  Expect the boot line `CAR-CAN-LAB // SNIFF`, no calibration message, `[ota] this firmware is
  good` on Serial. Then the deck repo's `pio run -e ledpanel_ota -t upload` puts the panel back
  with its settings (touch, looks, trip, Xbox bond).
