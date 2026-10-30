# car-can-lab

[![CI](https://github.com/jvitorcosta/car-can-lab/actions/workflows/ci.yml/badge.svg)](https://github.com/jvitorcosta/car-can-lab/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Find out what your car knows and won't tell you. Standard OBD-II leaves out a lot, like the gear
or often the fuel level, but much of it travels on the CAN bus as broadcast messages nobody has
documented for your model. car-can-lab is a **listen-only** toolkit for finding them with a cheap
ELM327 adapter: record the bus while you do something in the car, mark the moment, and let the
tools point at the bytes that changed.

```bash
git clone https://github.com/jvitorcosta/car-can-lab
cd car-can-lab
python -m tools.sniff.sniffer --simulate
python -m tools.sniff.analyze recordings/001.log
```

That runs the whole method on a simulated car in about a minute, with nothing to install. The
analyzer points at the simulated gear byte and engine speed, something like:

```text
MARK 1 at 10.0 s:
  1A0 byte 2  01 -> 02  score 1.257
rpm (12 readings) tracks best:
  0C8 bytes 0-1 be  corr +0.948
```

> **Status:** early. The tools are tested against simulated adapters; nothing has run on a real car
> yet. If you try it, an [adapter report](docs/adapters.md) helps a lot.

## What you need

A car with **11-bit CAN** on its OBD port, which covers most cars from about 2008 on, and one of
these:

| Route | Hardware | Good for |
|---|---|---|
| **Laptop** | Any ELM327 over USB, Bluetooth Classic or BLE, and a laptop | The easiest start. |
| **Handheld** | An ESP32-2432S028R "Cheap Yellow Display" and a BLE ELM327 | No laptop in the car: a live list on screen, recordings saved on the board. |

Either way, **the adapter must support silent monitoring** (`ATCSM1`), and many cheap clones don't.
Check yours before you go out to the car: see [docs/adapters.md](docs/adapters.md).

Before a session, read [SECURITY.md](SECURITY.md): parked, outdoors, on a car you may work on.

## Using it from a laptop

```bash
pip install -r requirements.txt
python -m tools.sniff.adaptercheck --port COM5     # can this adapter do it?
python -m tools.sniff.sniffer --port COM5          # or --ble, or /dev/ttyUSB0
```

It sweeps the bus once to learn which IDs the car sends, then records them. Press Enter for a MARK
and `q` then Enter to stop. [docs/sessions.md](docs/sessions.md) walks through a session.

## What's here

- `tools/sniff/`: the **PC tools**. The sniffer, the adapter check, the analyzer, and a converter
  for `candump` logs, so recordings from a SocketCAN interface or ICSim work too.
- `src/`: the **ESP32 firmware**. It shows the bus live, one row per CAN ID with changed bytes lit
  and fading, and records into the board's flash.
- `dbc/`: confirmed signals, one file per car. [Yours are welcome](dbc/README.md).
- `docs/`: [running a session](docs/sessions.md), [adapters](docs/adapters.md), and the design.

## Using it on another car

Nothing in it is specific to one car. It was built on a Honda City, hunting the gear and the fuel
level, and these adapt by themselves:

- **Protocol and speed:** the adapter finds them. 500 and 250 kbit/s, 11-bit, both work.
- **Coolant reading:** PID 05 when your car lists it, otherwise PID 67.
- **Adapter:** anything that answers `ATCSM1` with `OK`.

**Not covered:** 29-bit CAN is refused with a message, and cars with a gateway at the OBD port pass
only diagnostic replies, so you'd see `NOTHING HEARD`. Many newer cars do this; older ones are the
better bet.

## The ESP32 firmware

- ESP32-2432S028R (ESP32, 4 MB flash, 2.8" ILI9341 320 x 240, XPT2046 touch).
- A BLE ELM327. Built for the **Vgate iCar Pro BLE 4.0**, not yet tested on one.
- Touch or an Xbox Series X|S / One (1708+) controller over BLE: either one drives every screen
  on its own, and both work at once. Tap a row to pick it and a button to press it. Hold BOOT for
  2 s to calibrate the touch.

## Listen only

Nothing here sends a frame of its own on the car's bus. The only things that reach the bus are
standard mode 01 OBD readings: one when connecting, and a few between monitoring windows while
recording.

- Every byte sent to the adapter goes through one gate, `obd::allowed()` on the ESP32 and
  `elm.allowed()` on the PC, held to the same list by `test/test_obd/gate.inc`. It accepts exactly
  these: mode 01 reads, the adapter's own setup (`ATZ`, `ATE0`, `ATS0`,
  `ATH1`, `ATSP0`, `ATI`, `ATDPN`, ...), silent monitoring (`ATCSM1`, `ATMA`), the receive filters
  (`ATCF`, `ATCM`, `ATCRA`), and a bare `\r` to stop monitoring. It refuses headers (`ATSH`), raw
  frames, `ATCSM0`, every other mode and anything malformed.
- **Silent monitoring is required.** If the adapter doesn't answer `ATCSM1` with `OK`, it says
  so and `ATMA` is never sent. In silent mode the adapter
  doesn't even acknowledge frames.

## How it covers the bus

A busy 500 kbit/s bus carries about a thousand frames a second, more than `ATMA` can push over
BLE or a slow serial link. So the tools **sweep** the 11-bit ID space in windows, using the ELM's receive filter
(`ATCF`/`ATCM`) and dwelling 1.5 s in each window. A window that overflows (`BUFFER FULL`) is
split in two. **REC** freezes the set of IDs heard so far and cycles only through those, in small
windows. The header shows how long one cycle takes (`CYCLE 4 s`), and the recording logs which
window was open at every moment (an `S ... window` line), so the analyzer can tell "didn't change"
apart from "wasn't listening". On the ESP32, **FILTER** narrows everything to the rows you tapped.
While recording, the tools leave monitor mode every 3 s to read RPM and coolant the normal way: that
is the ground truth the analyzer checks the method against.

## Build and flash the firmware

PlatformIO, then:

    cp src/config.example.h src/config.h       # WiFi, TIMEZONE, REMOTE_TOKEN, OTA_PASSWORD
    pio run -e sniff                           # build
    pio run -e sniff -t upload                 # over USB (first time / recovery)
    pio run -e sniff_ota -t upload --upload-port <the board's address>     # over WiFi

The LOGS page shows the address. On an iPhone hotspot the board always takes 172.20.10.11.

### Sharing the board with car-cyberdeck

Only if your board also runs [car-cyberdeck](https://github.com/jvitorcosta/car-cyberdeck)'s LED
panel. `sniff_ota` and its `ledpanel_ota` must be able to replace each other over WiFi without
USB, and the panel must come back exactly as it was. So:

- car-can-lab keeps the same **`min_spiffs.csv`** partition table (two 1.9 MB app slots and a
  ~128 KB data partition, where the recordings live);
- it **never writes the LED panel's NVS** (its looks, trip and touch calibration). It reads the
  panel's touch calibration (`touch`/`hom`) when it has none of its own, and keeps its own in
  `sniff`/`hom`. The update code's own boot guard and the Xbox
  controller's bond are shared as the copied code already shares them;
- `src/config.h` uses the same `OTA_PASSWORD` as the panel, and `SWAP_HINT` can put a reminder on
  the home page.

An update over WiFi only rewrites an app slot, so recordings survive a swap to the panel and back.

## Downloading recordings from the board

With the board and your phone or PC on the same network:

    http://<address>/recs?k=<REMOTE_TOKEN>          the list: "003.log 12345"
    http://<address>/rec?n=3&k=<REMOTE_TOKEN>       recording 3

A wrong or missing key gets 403. The LOGS page lists the recordings and the address. Keep downloads in `recordings/`,
which git ignores: raw recordings are never committed.

## Analyzing

    python -m tools.sniff.analyze recordings/003.log
    python -m tools.sniff.candump2rec candump-icsim.log > recordings/icsim.log

The output is plain text: per MARK, the bytes that changed right after it and were quiet before
(counters and checksums are left out as noise), then the bytes and 16-bit pairs that track each
OBD value best.

## Tests

    pip install -r requirements-dev.txt
    python -m pytest                                                  # PC tools
    powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1     # firmware logic, on the PC (g++ + Unity)

The firmware's logic is split into pure modules under `src/util/`, each host-tested: the gate,
the monitor-line parser, the sweep, the ID table, the recording format, the monitor state machine
and the screen's texts. The device code around them (`src/sniff/`) is thin. The PC tools are tested
against `tools/sniff/fakeelm.py`, a simulated adapter and car, including a full session whose
result the analyzer must get right.

## Contributing

Adapter reports and car signals help most, and neither needs code. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## Where things came from

The touch, WiFi, clock, update-over-WiFi, backlight, VT323 font, palette, night mode and the
vendored Xbox library are copied once from the deck repo. Each file's first line says from where
and at which commit. The OBD gate and the BLE helpers come from the deck repo's OBD branch, and
the BLE link is ported from NimBLE 1.4 to 2.5.1. Nothing is shared by submodule.

MIT licensed. [THIRD_PARTY.md](THIRD_PARTY.md) lists what comes from elsewhere.
