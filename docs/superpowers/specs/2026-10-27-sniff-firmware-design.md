# car-can-lab: a CAN sniff firmware and toolkit (first target: the yellow deck): design

Date: 2026-10-27. Status: approved by the owner. Designed in the spotify-pokemon-deck repo (the CarDeck); references to `src/ledpanel.cpp`, `feat/obd` and other specs point there.

## Why

The owner is learning car hacking (CAN bus reverse engineering) on his own Honda City, as a hobby
and as a portfolio piece next to his Python/ML work. The OBD gauge cluster (`feat/obd`) already
reads the standard OBD-II PIDs, but the City leaves out things he wants: **gear** (P R N D S and
the paddles) and **fuel level** (no PID 2F). Those exist on the car's CAN bus as broadcast
messages nobody has documented for this car. Sniff mode is the tool for finding them: it shows
the bus live on the yellow deck while he does things in the car, and records what it saw so the
PC can work out which ID and byte is which signal. Signals found later become gauges.

## Goals

- A **separate sniff firmware** for the yellow deck (its own build next to `ledpanel`) that
  watches CAN traffic live through the OBD adapter and shows it as screen A (below). It is flashed
  over WiFi before a hunting session and the LED panel is flashed back afterwards.
- A **recording** of what changed, with the owner's **MARKs** and a few known OBD values, stored
  on the deck and fetched over the hotspot afterwards.
- A **PC script** that turns a recording into a short list of suspects per MARK.
- Built for the sessions in "How it gets used" below: parked, engine running.

## Non-goals (for this version)

- Sessions while driving (wheel speeds, vehicle speed, outside temperature hunting). Later.
- Screens B (hunt: before/after) and C (watch one ID) from the mockups. The PC script does B's
  job; C may come later as a confirmation screen.
- Honda diagnostic queries (UDS mode 22) to read fuel or gear directly. A separate idea if the
  bus turns out quiet (see Risks).
- A wired CAN transceiver (SN65HVD230 on CN1: 3.3 V, GND, GPIO 22/27, the ESP32's TWAI in
  listen-only mode). Only if the adapter turns out too slow; it gets its own design.
- Switching between the two firmwares from the deck itself (e.g. one in each app slot with a
  boot switch). Swapping is a WiFi flash from the PC for now.
- Decoding signals on the deck, or new gauges. Sniff mode finds; gauges come after.

## Anti-goals (a failure even if it works)

- **Anything that sends on the car's bus.** Sniff mode listens only. The adapter must stay in
  silent monitoring (no ACKs), and no command that transmits a frame (headers, raw data, modes
  other than the existing mode 01 reads) may ever leave the deck. The existing read-only gate in
  `util/obd` (`obd::allowed`) is extended, not bypassed.
- A list that silently stays empty. If the adapter can't monitor, or hears nothing, the screen
  says so in words.
- Touching the LED panel. The `ledpanel` firmware, its menu and its behaviour don't change; the
  sniff firmware is a separate build. Swapping back must leave the panel exactly as it was (touch
  calibration, looks, trip, Xbox bond in NVS).

## How it gets used (the owner's plan)

Session 0, at home: practise the method on ICSim (a simulated instrument cluster on a virtual CAN
bus) on the PC, and run the PC script on that capture.

Session 1, in the car, parked, engine running, foot on the brake:

Before: at home, the PC on the phone's hotspot flashes the sniff firmware (`pio run -e sniff_ota
-t upload`). After the session the same with `ledpanel_ota` puts the LED panel back.

1. The deck boots into sniff and checks the adapter can monitor.
2. Calibrate: rev in P a few times. The log carries the engine RPM read the normal way (PID 0C);
   finding the CAN ID that matches it proves the method before hunting the unknowns.
3. Baseline: ~30 s of nothing, so the script learns what changes on its own (counters, checksums).
4. Gear: P → R → N → D → S, ~5 s each, MARK at every shift; then the paddles + and − in S.
5. Extras: each door, headlights, indicators, brake pedal, steering wheel full left and right.
6. Fuel: one recording just before refuelling and one just after.

After each session: fetch the log over the hotspot, run the script, and confirm suspects next time.

## Decided by the owner

- Screen A, the live list, watched live on the deck in the car.
- What's seen in the car must reach the PC afterwards; there's no PC in the car.
- Over BLE to the **Vgate iCar Pro BLE 4.0** (ordered, not yet arrived), the adapter already
  planned for the Car screen (menu spec 2026-10-23, Part 4).
- Parked only for now.
- Hunt gear and fuel level first; doors, lights etc. are extras.
- The Xbox controller stays connected no matter what: it controls the deck's interface,
  including SNIFF. Touch works just as well (owner, 2026-10-28): every screen and action is
  reachable by touch alone or by the controller alone, neither is required. Sniff can calibrate
  the touch itself (BOOT held 2 s), saving into its own NVS namespace, never the panel's.
- Sniff is a **separate firmware on the yellow deck**, not part of the LED panel firmware and not
  on the black deck (chosen over a wired CAN transceiver on CN1, which stays a later option).
  The lights are unavailable while the sniff firmware is on the deck.
- It lives in its **own repo, `car-can-lab`**, as a standalone reverse-engineering tool.

## The screen (A, live list)

320 × 240, VT323, the Seegson palette and night mode of the rest of the CYBERDECK
(spec 2026-10-23). Mockup kept in the deck repo's `.superpowers/brainstorm/` (not committed):

```
SNIFF // LIVE                23 IDs  41 msg/s
1A6  00 04 00 00 3C 00 00 11     <- 04 and 11 just changed: lit, fading over ~1 s
191  0B 00 20 00 00 00 00 00
158  00 00 00 00 00 00 21 9A
17C  00 00 03 C8 00 00 00 00     (dim: no change for a while)
...
[PAUSE] [FILTER] [MARK] [REC] [BACK]
```

- One row per CAN ID heard, ID in hex, then its up-to-8 data bytes in hex.
- A byte that changes is drawn highlighted and fades back over about a second.
- Rows that changed most recently sort to the top; rows idle for a while go dim.
- Header: number of IDs heard, frames per second, and the scan state (see "Covering the bus").
- **PAUSE** freezes the screen (recording, if on, continues).
- **FILTER** narrows to the IDs the owner picks (tap rows, then FILTER) so they update as fast as
  the adapter allows; FILTER again clears it.
- **MARK** puts a numbered mark in the recording (`MARK 3`) and flashes the number on screen.
- **REC** starts and stops a recording; the header shows `REC` and how full the log is.
- **BACK** goes to the sniff firmware's home page (my call): `[LIVE]` back to this screen and
  `[LOGS]`, a list of recordings with their size and the address to download them from. The deck
  boots straight into LIVE.
- Status in words when needed: `ADAPTER NOT FOUND`, `ADAPTER CAN'T MONITOR`, `ENGINE OFF?`,
  `NOTHING HEARD`, `LOG FULL`. Problems in red, as elsewhere.

## Covering the bus (my call)

The adapter can't push the whole bus over BLE: a busy 500 kbit/s bus carries on the order of a
thousand frames a second, and `ATMA` (monitor all) fills the ELM's buffer at once. So:

- On opening, sniff mode **sweeps** the 11-bit ID space in windows using the ELM's receive filter
  (`ATCF` / `ATCM`, or `ATCRA` with wildcards), dwelling a couple of seconds in each, so the list
  fills in over roughly the first minute. After the sweep it keeps cycling so values stay fresh.
- A window that overflows (`BUFFER FULL`) is split into smaller windows.
- FILTER replaces the sweep with just the chosen IDs.
- **While recording** (my call): REC freezes the set of IDs heard so far and cycles only through
  that set, in windows as small as the adapter keeps up with, so every known ID is watched often
  enough for MARKs ~5 s apart. With FILTER on, REC watches only the filtered IDs. The log records
  which IDs are being watched at every moment (an `S` line on every window change), so the script
  can tell "didn't change" from "wasn't listening". If coverage still turns out thin on the real
  adapter, the session procedure repeats each action a few times; the owner gets told on screen
  how long one full cycle takes (`CYCLE 4 s`) so he can hold each step at least that long.
- Window size and dwell time are the builder's call, tuned on the real adapter.
- First thing on opening: ask the adapter who it is (`ATI`), set CAN protocol, turn on silent
  monitoring (`ATCSM1`) and headers, and check the monitor commands are accepted; if not, say
  `ADAPTER CAN'T MONITOR`.
- **Strict about silent mode** (my call, following the listen-only anti-goal): an adapter that
  rejects `ATCSM1` gets `ADAPTER CAN'T MONITOR (NOT SILENT)` and no monitoring, even if that leaves
  the current clone unusable. Built and accepted against the **Vgate iCar Pro BLE**; before it
  arrives the clone's BLE side ("OBDBLE") may be used as a smoke test of the link only, with no
  extra hardening for its flakiness.

## Known values in the log (ground truth)

While recording, sniff mode briefly leaves monitor mode every few seconds to read **RPM (0C)**
and **coolant (05 or 67)** the normal way and writes them into the log, then resumes. The adapter
can't monitor and query at once; the gap is accepted. (Speed 0D is for the later driving
version; reading it parked is harmless and the builder may include it.)

## The recording

- Stored in the deck's flash data partition (the `min_spiffs.csv` table already used by the
  ledpanel build leaves a small one, about 128 KB; the builder confirms the size and filesystem).
  No SD card (keeping the deck card-free is a standing decision). An update over WiFi only
  rewrites an app slot, so recordings survive swapping back to the LED panel and are still there
  the next time sniff is flashed.
- Recording is **explicit** (REC on/off), not always on, so the budget goes to the experiments.
  A new REC starts a new numbered recording; oldest recordings are dropped when space runs out,
  never the one in progress (that one stops with `LOG FULL`).
- Plain text, one line per event, timestamps in ms since REC:
  - a header line: date/time, adapter `ATI`, protocol, firmware version;
  - `F <ms> <id> <bytes>` the first time an ID is seen and whenever its data changes;
  - `M <ms> <n>` for each MARK;
  - `O <ms> rpm 812` / `O <ms> coolant 88` for the known values;
  - `S <ms> <text>` for state changes (sweep window, filter on/off, buffer full, pause).
- To keep fast counters from eating the space, an ID logs at most a few changes per second
  (latest value wins); the exact cap is the builder's call.
- Served by a small web server in the sniff firmware (the same `WebServer` and `REMOTE_TOKEN`
  check the LED panel uses for `/say`): a list of recordings and each recording as a download. Reachable on the
  hotspot from the phone (Safari, then share) or from the PC when it's on the hotspot. The home
  router isolates the deck, so not from home Wi-Fi.

## The PC script

`tools/sniff_analyze.py`, plain Python 3, no hardware needed:

- Reads one recording (and, for Session 0, an ICSim `candump` log, converted to the same events).
- **Baseline**: bytes that change with no MARK nearby are tagged noisy (counters, checksums).
- **Per MARK**: lists the ID/byte pairs that changed within a short window after the mark and were
  quiet before it, best first, with the old and new values.
- **Ground truth**: for each `O` value (RPM), lists the ID/byte pairs (single bytes and 16-bit
  pairs, both byte orders) whose value tracks it best.
- Output is plain text the owner can paste back into a conversation; a SavvyCAN-compatible export
  is a nice-to-have.

## The repo (the owner's: a separate repo named `car-can-lab`)

- Its own git repo at `D:\dev\projects\car-can-lab`, separate from `spotify-pokemon-deck`, written as
  a standalone portfolio project: an ESP32 + ELM327-over-BLE CAN reverse-engineering toolkit,
  with the yellow deck (CYD) as its first hardware target and the Honda City as its first car.
- Contents: the firmware (PlatformIO), `tools/` (the PC script, the ICSim converter), host tests,
  `dbc/` for the signals found (a Honda City DBC grown session by session), `docs/` (this design,
  plans, session notes), and a README telling the story: hardware, method, findings.
- **Copied, not shared**: the pieces it needs from the deck repo (touch and its calibration,
  WiFi, clock, OTA with the boot-loop rollback, backlight, VT323 font and the Seegson palette,
  night mode, the vendored Xbox library, `util/obd` and `util/obdble` from `feat/obd` with their
  host tests) are copied in once, each file noting where it came from. No submodules. `feat/obd`
  in the deck repo stays unmerged and untouched.
- **Deck compatibility rule** (written in the README): on the yellow deck, car-can-lab keeps the
  `min_spiffs.csv` partition table and never writes the LED panel's NVS keys except to read the
  shared touch calibration, so `ledpanel_ota` and `sniff_ota` can replace each other over WiFi.
- **Secrets and privacy**: WiFi credentials and `REMOTE_TOKEN` live in a git-ignored secrets
  header (with an example file committed). Raw recordings are git-ignored by default; only DBCs
  and written-up findings are committed.
- Findings flow back to the deck repo later as a small decoder for new gauges (its own design).

## Where it fits in the firmware

- PlatformIO builds `sniff` (CYD board, same `min_spiffs.csv` table as the deck's `ledpanel` so
  the two can replace each other over WiFi without a USB flash) and `sniff_ota`. Its own entry
  file (e.g. `src/sniff.cpp`) using the copied pieces above.
- The sniff firmware must itself accept an update over WiFi, so the panel can be flashed back
  without USB.
- Bluetooth: NimBLE 2.5.1 with two links, the adapter and the Xbox controller. No LED
  controllers, no `/say`, no weather or lyrics polling.
- The adapter link reuses `util/obdble` and `util/obd` from `feat/obd` (the BLE serial stream,
  ELM reply parsing, `obd::allowed`), copied into car-can-lab as above. The BLE connect/discover code in
  `companion/obdlink.cpp` is on NimBLE 1.4 and is ported to the panel's NimBLE 2.5.1, not copied.
- `obd::allowed` grows to accept exactly the monitor/filter/setup commands sniff mode needs and
  nothing that transmits (no `ATSH`, no raw frames, no modes other than 01). Host tests prove it.

## Risks

- **The bus may be quiet at the OBD port.** Some cars put a gateway between the OBD port and the
  internal buses, so only diagnostic replies come through. Then `NOTHING HEARD` is the answer, and
  the next step is UDS queries (a separate design).
- **The adapter may not monitor properly.** Clones often accept `ATMA`/`ATCRA` and do nothing.
  The adapter check on opening catches it; the fallback is the wired transceiver (separate design).
- **Heap** is no longer the squeeze it was inside the panel (two BLE links instead of four, no
  weather/lyrics), but the builder still logs it every minute like the panel does.
- **Forgetting to swap back**: the lights don't work while sniff is on the deck. The sniff home
  page says so (`LIGHTS: FLASH LEDPANEL BACK`), and swapping needs the PC on the hotspot.
- **BLE throughput** may make the sweep slow; FILTER is the workaround.

## Left to the builder

Sweep window sizes and dwell, the per-ID log cap, the filesystem, the exact layout pixels, how many
rows fit, Xbox controls on this screen (B = Back as elsewhere), the script's time windows and
ranking, and anything else not decided above.
