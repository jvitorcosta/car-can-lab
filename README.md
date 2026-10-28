# car-can-lab

A listen-only toolkit for reverse engineering a car's CAN bus with an ESP32 and a Bluetooth
ELM327 adapter. Standard OBD-II PIDs leave out a lot of what a car knows; much of the rest travels
on the CAN bus as broadcast messages that nobody has documented for your model. This repo helps
you find them: record the bus while you do something in the car (shift gear, fill the tank), mark
the moment, and let the PC tools point at the bytes that changed with it.

It runs on an ESP32-2432S028R "Cheap Yellow Display" (the yellow deck of
[car-cyberdeck](https://github.com/jvitorcosta/car-cyberdeck)) and never sends anything on the car's bus
(see [Listen only](#listen-only)). It was built on a **Honda City**, hunting the two things its
OBD-II PIDs leave out: the **gear** (P R N D S and the paddles) and the **fuel level** (no PID 2F).
Nothing else in it is specific to that car; see [Using it on another car](#using-it-on-another-car).

- `src/`: the **sniff firmware**. It watches the bus through the adapter in silent monitor mode,
  shows it live (one row per CAN ID, changed bytes lit and fading), and records what changed, with
  your MARKs and a few known OBD values, into the deck's flash.
- `tools/sniff/`: the **PC tools**. They read a recording, convert an ICSim `candump` log, and list
  the suspects per MARK and the bytes that track RPM.
- `dbc/`: the signals found, as a DBC file grown session by session.
- `docs/`: the design (`docs/superpowers/specs/`), the build plan, the session guide
  (`docs/sessions.md`) and what's waiting on a check on the deck (`docs/BACKLOG.md`).

## Using it on another car

- **Background OBD values.** Between monitoring windows the firmware reads RPM (`010C`) and
  coolant. The City has no standard coolant PID (`05`), so it reads the extended one (`0167`); on
  most other cars `0105` is the one to use. The choice is in `src/util/monplan.h`; both PIDs are
  already in the firmware's table, and the listen-only gate allows any single mode 01 read.
- **Signals.** `dbc/` is where confirmed signals go, one DBC file per car. It is still empty:
  none of the City's are confirmed yet.
- **Adapter.** Your ELM327 must answer `ATCSM1` (silent monitoring) with `OK`; the firmware
  refuses to monitor otherwise.

## Hardware

- The yellow deck: ESP32-2432S028R (ESP32, 4 MB flash, 2.8" ILI9341 320 x 240, XPT2046 touch).
- An ELM327 BLE adapter. The tool is built for and accepted against the **Vgate iCar Pro BLE 4.0**.
  A cheap clone's BLE side works for a smoke test of the link only.
- Touch or an Xbox Series X|S / One (1708+) controller over BLE: either one drives every screen
  on its own, and both work at once. Tap a row to pick it and a button to press it. Hold BOOT for
  2 s to calibrate the touch.

## Listen only

The firmware never sends anything on the car's bus:

- Every byte sent to the adapter goes through one gate, `obd::allowed()` (`src/util/obd.cpp`),
  which accepts exactly these: mode 01 reads, the adapter's own setup (`ATZ`, `ATE0`, `ATS0`,
  `ATH1`, `ATSP0`, `ATI`, `ATDPN`, ...), silent monitoring (`ATCSM1`, `ATMA`), the receive filters
  (`ATCF`, `ATCM`, `ATCRA`), and a bare `\r` to stop monitoring. It refuses headers (`ATSH`), raw
  frames, `ATCSM0`, every other mode and anything malformed. Host tests pin both lists.
- **Silent monitoring is required.** If the adapter doesn't answer `ATCSM1` with `OK`, the screen
  shows `ADAPTER CAN'T MONITOR (NOT SILENT)` and `ATMA` is never sent. In silent mode the adapter
  doesn't even acknowledge frames.

## How it covers the bus

A busy 500 kbit/s bus carries about a thousand frames a second, more than `ATMA` can push over
BLE. So the firmware **sweeps** the 11-bit ID space in windows, using the ELM's receive filter
(`ATCF`/`ATCM`) and dwelling 1.5 s in each window. A window that overflows (`BUFFER FULL`) is
split in two. **REC** freezes the set of IDs heard so far and cycles only through those, in small
windows. The header shows how long one cycle takes (`CYCLE 4 s`), and the recording logs which
window was open at every moment (an `S ... window` line), so the analyzer can tell "didn't change"
apart from "wasn't listening". **FILTER** narrows everything to the rows you tapped. While
recording, the firmware leaves monitor mode every 3 s to read RPM (`010C`) and coolant (`0167`) the
normal way: that is the ground truth the analyzer checks the method against.

## Build and flash

PlatformIO, then:

    cp src/config.example.h src/config.h       # WiFi, TIMEZONE, REMOTE_TOKEN, OTA_PASSWORD
    pio run -e sniff                           # build
    pio run -e sniff -t upload                 # over USB (first time / recovery)
    pio run -e sniff_ota -t upload             # over WiFi: PC on the phone's hotspot, deck at 172.20.10.11

To put the LED panel back after a session, run `pio run -e ledpanel_ota -t upload` from the deck
repo. The PC must be on the hotspot for both commands.

### The deck compatibility rule

`sniff_ota` and the deck repo's `ledpanel_ota` must be able to replace each other over WiFi
without USB, and the panel must come back exactly as it was. So, on the yellow deck:

- car-can-lab keeps the same **`min_spiffs.csv`** partition table (two 1.9 MB app slots and a
  ~128 KB data partition, where the recordings live);
- it **never writes the LED panel's NVS** (its looks, trip and touch calibration). It reads the
  panel's touch calibration (`touch`/`hom`) when it has none of its own, and keeps its own in
  `sniff`/`hom`. The update code's own boot guard and the Xbox
  controller's bond are shared as the copied code already shares them;
- `src/config.h` uses the same `OTA_PASSWORD` as the panel.

An update over WiFi only rewrites an app slot, so recordings survive a swap to the panel and back.

## Downloading recordings

With the deck on the phone's hotspot (the home router isolates it):

    http://172.20.10.11/recs?k=<REMOTE_TOKEN>          the list: "003.log 12345"
    http://172.20.10.11/rec?n=3&k=<REMOTE_TOKEN>       recording 3

From the phone (Safari, then share) or from the PC on the hotspot. A wrong or missing key gets
403. The deck's LOGS page lists the recordings and the address. Keep downloads in `recordings/`,
which git ignores: raw recordings are never committed.

## Analyzing

    pip install -r requirements-dev.txt         # pytest, for the tests only; the tools need nothing
    python -m tools.sniff.analyze recordings/003.log
    python -m tools.sniff.candump2rec candump-icsim.log > recordings/icsim.log
    python -m pytest                             # the tools' tests

The output is plain text: per MARK, the bytes that changed right after it and were quiet before
(counters and checksums are left out as noise), then the bytes and 16-bit pairs that track each
OBD value best.

## Tests

    powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1     # firmware logic, on the PC (g++ + Unity)
    python -m pytest                                                  # PC tools

The firmware's logic is split into pure modules under `src/util/`, each host-tested: the gate,
the monitor-line parser, the sweep, the ID table, the recording format, the monitor state machine
and the screen's texts. The device code around them (`src/sniff/`) is thin.

## Where things came from

The touch, WiFi, clock, update-over-WiFi, backlight, VT323 font, palette, night mode and the
vendored Xbox library are copied once from the deck repo. Each file's first line says from where
and at which commit. The OBD gate and the BLE helpers come from the deck repo's OBD branch, and
the BLE link is ported from NimBLE 1.4 to 2.5.1. Nothing is shared by submodule.
