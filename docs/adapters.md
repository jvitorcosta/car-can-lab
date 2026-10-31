# Adapters

An adapter works with car-can-lab if it answers `ATCSM1` with `OK` and really stays silent. Many
cheap clones don't, and the name printed on the box tells you little: two adapters that look the
same can carry different chips.

## Check yours

With the adapter in the car's OBD port and the ignition on:

```bash
python -m tools.sniff.adaptercheck --port COM5
```

Use `--ble` for a BLE adapter. On Linux the port looks like `/dev/ttyUSB0`, or `/dev/rfcomm0` for
Bluetooth Classic. With the engine running it also tries a short silent monitor. It ends with a
line for the table below: please send it as an
[adapter report](https://github.com/jvitorcosta/car-can-lab/issues/new?template=adapter-report.yml).

| Verdict | Meaning |
|---|---|
| `USABLE` | It monitored silently and heard frames. |
| `PROMISING` | It accepts `ATCSM1`, but no car answered. Try again with the engine running. |
| `NOTHING HEARD` | It monitors but no frames came: a gateway in the car, or a clone that ignores `ATMA`. |
| `NOT USABLE` | It refused `ATCSM1`, so car-can-lab won't monitor with it. |

`OK` to `ATCSM1` is a claim, not proof: a clone can say `OK` and still acknowledge frames, and no AT
command can tell. If that matters for your car, use a CAN interface with a real listen-only mode.

## Which link

| Link | Works with | Notes |
|---|---|---|
| BLE | The ESP32 firmware, the PC tools | The slowest. The firmware needs this one. |
| Bluetooth Classic | The PC tools | Pair it first; it appears as a COM port. The firmware can't use it. |
| USB | The PC tools | Usually the fastest and steadiest. The ESP32 board has no USB host. |

All three speak the same ELM327 commands. A faster link means shorter cycles, and it doesn't change
whether the chip can monitor silently.

## Reported

Only results someone actually measured go here.

| Product | ATI | Link | Silent | Frames heard | Car |
|---|---|---|---|---|---|
| Nothing reported yet | | | | | |
