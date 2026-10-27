# Sniff firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A separate firmware for the yellow deck (CYD) that watches the car's CAN bus through a Vgate iCar Pro BLE (ELM327) in silent monitor mode, shows it live as screen A, records changes with MARKs and known OBD values to flash, serves recordings over the hotspot, plus a PC script that turns a recording into suspects.

**Architecture:** A new PlatformIO repo (`car-can-lab`) that copies the shared pieces from the deck repo once. Pure, host-tested modules in `src/util/` (read-only gate, monitor-line parser, sweep planner, ID table, recording formatter, recording names) and thin device code around them (`src/sniff/` BLE link and monitor state machine, recording store, web routes, screen). Python tools in `tools/` with pytest.

**Tech Stack:** PlatformIO `espressif32@6.9.0`, Arduino, NimBLE-Arduino 2.5.1, TFT_eSPI 2.5.43, XPT2046_Touchscreen, the vendored `lib/xboxpad`, LittleFS, Unity host tests via `tools/run_tests.ps1` (g++), Python 3 + pytest.

**Spec:** `docs/superpowers/specs/2026-10-27-sniff-firmware-design.md`

**Source repo for copies:** `D:\dev\projects\spotify-pokemon-deck` (main) and its branch `feat/obd` (`git show feat/obd:<path>`). Never modify the deck repo.

## Global Constraints

- Listen only: nothing that transmits a CAN frame ever leaves the deck. Every byte sent to the adapter goes through `obd::allowed()` (one choke point, `sniff::link::send`). No `ATSH`, no raw frames, no modes other than 01.
- Silent monitoring required: if `ATCSM1` is not answered `OK`, show `ADAPTER CAN'T MONITOR (NOT SILENT)` and never send `ATMA`.
- Board: CYD (`esp32dev`), partition table `min_spiffs.csv` (same as the deck's `ledpanel`), hotspot address `172.20.10.11`, OTA password `REMOTE_TOKEN`.
- NVS: read the panel's touch calibration; never write or erase any other namespace the LED panel uses (`leds`, `hom`, `touch`, OTA guard keys are shared only as the copied OTA code already does).
- Bluetooth: NimBLE 2.5.1, two links only (adapter + Xbox). The Xbox controller always stays usable.
- Screen: 320 × 240, VT323 (`ui/termfont`), Seegson palette (`util/termpal`), orange night mode 18:00-06:00 (`util/daynight`), problems in red.
- Recording line formats (exact): header line starting `# car-can-lab`; `F <ms> <id> <b0> … <bn>`; `M <ms> <n>`; `O <ms> <name> <value>`; `S <ms> <text>`. IDs and bytes upper-case hex, ID 3 digits, bytes 2 digits, single spaces.
- Secrets in git-ignored `src/config.h` (template `src/config.example.h` committed). Raw recordings never committed (`recordings/`, `*.log` ignored).
- No SD card.
- Commits: conventional commits `type(scope): subject`, ending with the `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` line.

## Review Focus

1. A monitor line split across two BLE notifications, or two lines in one: the line assembler must yield whole lines only (test in Task 3).
2. The adapter drops mid-recording: the recording gets `S <ms> adapter lost`, stays open and readable, and resumes with `S <ms> adapter back` after reconnect (test of the state machine's transitions in Task 8).
3. Flash fills mid-recording: `LOG FULL`, the file is closed and still downloadable (test of `recname::planSpace` in Task 7).
4. Ignition off / power cut mid-recording: everything up to the last flush (every 2 s) is readable; a truncated last line is ignored by the analyzer (test in Task 10).
5. More IDs than the table holds (`IdTable::MAX_IDS` = 96): extra IDs are counted (`dropped()`), shown as `+N`, nothing crashes (test in Task 5).

---

### Task 1: Repo scaffold that boots on the deck

**Files:**
- Create: `platformio.ini` (envs `sniff`, `sniff_ota`, `native`), `src/sniff.cpp`, `src/config.example.h`, `README.md` (stub: one paragraph + build commands)
- Copy from the deck repo, unchanged except include paths: `include/pins.h` (CYD part), `src/net/{wifi,clock,ota}.{h,cpp}`, `src/net/{ipaddr,remote_config,http_config}.*`, `src/ui/{backlight,termfont}.*`, `src/ui/termlayout.h`, `src/fonts/vt323_*.h`, `src/companion/touch.*` → `src/hw/touch.*`, `lib/xboxpad/`, the `src/util/` headers/sources these pull in (`hotspotip, netdiag, netup, roam, bootguard, otaview, otapage, daynight, touchcal, touchguard, touchmap, termpal, remotegate, deckaddr, version.h`), `tools/{run_tests.ps1,ntest.ps1,version.py,ota_auth.py}`, and the host test suites under `test/` for every copied pure module.
- Modify: `.gitignore` (replace `include/secrets.h` with `src/config.h`; add `include/fw_version.h`)

**Interfaces:**
- Produces: `[env:sniff]` (extends a `[cyd]` section copied from the deck: same TFT flags, `-D BOARD_CYD=1 -D LOAD_GFXFF=1`, `board_build.partitions = min_spiffs.csv`, lib_deps NimBLE-Arduino@2.5.1, the two asukiaaa Xbox deps, TFT_eSPI@2.5.43, XPT2046 at `#f956c5d`), `[env:sniff_ota]` (`upload_protocol = espota`, `upload_port = 172.20.10.11`, `post:tools/ota_auth.py`), `[env:native]` as in the deck.
- Every copied file gets a first-line comment `// Copied from spotify-pokemon-deck <path> @ <short sha>.`

- [ ] **Step 1:** Copy the files above, following each `#include "…"` until the `sniff` build links; copy only what the closure needs (drop `http_config`'s CA bundle if nothing uses TLS).
- [ ] **Step 2:** Write `src/sniff.cpp` setup/loop: backlight, TFT rotation as the panel, touch with the stored calibration, WiFi + clock + OTA (with the boot-loop guard), Serial heap line every 60 s (`[mem] heap %u (min %u)`), and a VT323 boot line `CAR-CAN-LAB // SNIFF` plus `LIGHTS: FLASH LEDPANEL BACK`.
- [ ] **Step 3:** Run `pio run -e sniff`. Expected: `SUCCESS`, firmware under the 1,966,080-byte slot.
- [ ] **Step 4:** Run `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`. Expected: every copied suite `PASS`, `fail=0`.
- [ ] **Step 5 (owner, on the deck):** PC on the hotspot, `pio run -e sniff_ota -t upload` from the running LED panel. Expected: the boot line shows, no calibration screen, `[ota] this firmware is good`. Then `pio run -e ledpanel_ota -t upload` from the deck repo puts the panel back with its settings. Record the result in `docs/BACKLOG.md`.
- [ ] **Step 6:** Commit `chore(repo): scaffold the sniff firmware from the deck's shared pieces`.

### Task 2: The read-only gate for monitoring

**Files:**
- Copy from `feat/obd`: `src/util/obd.{h,cpp}`, `src/util/obdble.{h,cpp}`, `test/test_obd/`, `test/test_obdble/`
- Modify: `src/util/obd.h`, `src/util/obd.cpp` (`allowed`)
- Test: `test/test_obd/test_obd.cpp`

**Interfaces:**
- Produces: `bool obd::allowed(const char* cmd)` now also true for exactly: `ATMA\r`, `ATCSM1\r`, `ATSP6\r`, `ATCRA\r`, `ATCRA` + 3 chars each `0-9A-F` or `X` + `\r`, `ATCF` + 3 hex + `\r`, `ATCM` + 3 hex + `\r`, and the bare `\r` (stops monitoring; repeating the last command can only repeat an allowed one). Still false for everything else.

- [ ] **Step 1: Write the failing tests**

```cpp
void test_monitor_commands_allowed() {
    for (const char* c : {"ATMA\r", "ATCSM1\r", "ATSP6\r", "ATCRA\r", "ATCRA1XX\r",
                          "ATCRA7E8\r", "ATCF1A0\r", "ATCM7F0\r", "\r"})
        TEST_ASSERT_TRUE_MESSAGE(obd::allowed(c), c);
}
void test_transmitting_or_malformed_refused() {
    for (const char* c : {"ATSH7DF\r", "ATCSM0\r", "ATMA", "atma\r", "ATCF 1A0\r", "ATCF1A00\r",
                          "ATCFZZZ\r", "ATCM1X0\r", "ATCRA1XXX\r", "\r\r", "ATMR00\r", "ATMT00\r",
                          "0322F190\r", "1001\r", "ATBD\r", "ATMA\rATSH7DF\r", ""})
        TEST_ASSERT_FALSE_MESSAGE(obd::allowed(c), c);
}
```

- [ ] **Step 2:** Run the suite. Expected: FAIL on the first test.
- [ ] **Step 3:** Extend `allowed` with exact matching (no regex), keeping the existing list.
- [ ] **Step 4:** Run all suites. Expected: PASS.
- [ ] **Step 5:** Commit `feat(obd): allow silent CAN monitor and filter commands, nothing that transmits`.

### Task 3: Monitor lines and line assembly

**Files:**
- Create: `src/util/elmmon.h`, `src/util/elmmon.cpp`
- Test: `test/test_elmmon/test_elmmon.cpp`

**Interfaces:**
- Produces:
  - `struct elmmon::Frame { uint16_t id; uint8_t len; uint8_t data[8]; };`
  - `enum class elmmon::Kind : uint8_t { Frame, BufferFull, Stopped, Error, Prompt, Other };`
  - `elmmon::Kind elmmon::parse(const char* line, elmmon::Frame* out);` (11-bit IDs; spaces optional; `BUFFER FULL`, `STOPPED`, `CAN ERROR`/`<RX ERROR`/`?` → Error, `>` → Prompt)
  - `class elmmon::Lines { public: static constexpr size_t CAP = 96; void push(const char* bytes, size_t n); bool next(char* out, size_t outCap); };` splits on `\r`/`\n`, drops empty lines, yields `>` as its own line, cuts an over-long line at CAP and discards its rest.

- [ ] **Step 1: Write the failing tests**

```cpp
// parse
"1A6 00 04 00 00 3C 00 00 11" -> Frame, id 0x1A6, len 8, data[1]==0x04, data[7]==0x11
"1A60004" -> Frame, id 0x1A6, len 2, data {0x00,0x04}
"7E8 03 41 0C" -> Frame, id 0x7E8, len 3
"BUFFER FULL" -> BufferFull;  "STOPPED" -> Stopped;  "CAN ERROR" -> Error;  "?" -> Error
"1A6 00 04 0" (odd digit) -> Other;  "1A6 00 01 02 03 04 05 06 07 08" (9 bytes) -> Other;  "GG6 00" -> Other
// Lines: review focus 1
push("1A6 00 0", 8); push("4\r191 0B\r", 9) -> next() yields "1A6 00 04", then "191 0B", then false
push("\r\r>", 3) -> yields ">" only
push(100 x 'A' + "\r1A6 00\r") -> yields the cut line (95 chars), then "1A6 00"
```

- [ ] **Step 2:** Run. Expected: FAIL (no module).
- [ ] **Step 3:** Implement in `src/util/elmmon.cpp`. Pure: no Arduino headers.
- [ ] **Step 4:** Run. Expected: PASS.
- [ ] **Step 5:** Commit `feat(sniff): parse ELM monitor lines and assemble them from BLE pushes`.

### Task 4: Sweep planner

**Files:**
- Create: `src/util/sweep.h`, `src/util/sweep.cpp`
- Test: `test/test_sweep/test_sweep.cpp`

**Interfaces:**
- Produces:
  - `struct sweep::Window { uint16_t filter, mask; };` (passes IDs where `(id & mask) == filter`)
  - `class sweep::Plan` with `static constexpr uint16_t START_SIZE = 0x80, MIN_SIZE = 1;`
    - `void scan();` cover 0x000-0x7FF in windows of START_SIZE
    - `void only(const uint16_t* ids, int n);` cycle the given IDs (REC freeze, FILTER), grouping neighbours that share a power-of-two block of at most 8 IDs into one window
    - `Window current() const;` `void next();` (wraps) `void split();` (current window halves into two, both kept in the cycle; no-op at MIN_SIZE) `int windows() const;` `bool wrapped() const;` (true once after each full cycle)
  - `void sweep::commands(const Window& w, char* cf, char* cm);` writes `"ATCF%03X\r"` and `"ATCM%03X\r"` (each buffer ≥ 9 bytes)

- [ ] **Step 1: Write the failing tests**

```cpp
scan(): windows()==16; first current()=={0x000,0x780}; after 15 next(): {0x780,0x780}; next() wraps to {0x000,0x780} and wrapped() is true once
split() on {0x100,0x780}: windows()==17, current()=={0x100,0x7C0}, next()=={0x140,0x7C0}
only({0x1A6,0x1A7,0x305},3): windows()==2; windows cover 0x1A6 and 0x1A7 together, 0x305 alone with mask 0x7FF
commands({0x1A0,0x7F0}) -> "ATCF1A0\r", "ATCM7F0\r", and obd::allowed() true for both
```

- [ ] **Step 2-4:** Run (FAIL), implement, run (PASS).
- [ ] **Step 5:** Commit `feat(sniff): plan filter windows over the ID space`.

### Task 5: Live ID table

**Files:**
- Create: `src/util/idtable.h`, `src/util/idtable.cpp`
- Test: `test/test_idtable/test_idtable.cpp`

**Interfaces:**
- Consumes: `elmmon::Frame`
- Produces:
  - `struct idtable::Row { uint16_t id; uint8_t len; uint8_t data[8]; uint32_t seenMs, changedMs; uint32_t byteMs[8]; bool picked; };`
  - `struct idtable::Update { bool isNew; uint8_t changed; };` (`changed` bit i = byte i differs; a length change marks all bytes)
  - `class idtable::IdTable` with `static constexpr int MAX_IDS = 96; static constexpr uint32_t IDLE_MS = 5000, FADE_MS = 1000;`
    - `Update update(const elmmon::Frame& f, uint32_t nowMs);`
    - `int order(uint16_t* ids, int max) const;` most recent `changedMs` first, then `seenMs`
    - `const Row* find(uint16_t id) const;` `int size() const;` `uint32_t dropped() const;` (distinct IDs refused when full)
    - `void togglePick(uint16_t id);` `int picked(uint16_t* ids, int max) const;` `void clearPicks();`
    - `static bool idle(const Row& r, uint32_t nowMs);` `static uint8_t fade(const Row& r, int byte, uint32_t nowMs);` 255 at the change, 0 at FADE_MS and after

- [ ] **Step 1: Write the failing tests** — first sighting `isNew` with `changed==0`; same data → `changed==0`; byte 2 differs → `changed==0x04`; `order` puts the last changed first; `fade` 255 at 0 ms, ~128 at 500 ms, 0 at 1000 ms; `idle` after 5000 ms without change; 97th distinct ID → not stored, `dropped()==1`, the same ID again → still `1` (review focus 5).
- [ ] **Step 2-4:** Run (FAIL), implement, run (PASS).
- [ ] **Step 5:** Commit `feat(sniff): live ID table with change marks, fade and picks`.

### Task 6: Recording format and per-ID cap

**Files:**
- Create: `src/util/reclog.h`, `src/util/reclog.cpp`
- Test: `test/test_reclog/test_reclog.cpp`

**Interfaces:**
- Consumes: `elmmon::Frame`
- Produces:
  - `using reclog::Writer = void (*)(const char* line, void* ctx);` (line without newline)
  - `struct reclog::Header { int number; const char* when; const char* adapter; const char* proto; const char* fw; };`
  - `class reclog::Rec` with `static constexpr uint32_t MIN_GAP_MS = 250;` `static constexpr int TRACK = 128;`
    - `void begin(Writer w, void* ctx, const Header& h);` writes `# car-can-lab rec <n> <when> adapter=<..> proto=<..> fw=<..>`
    - `void frame(const elmmon::Frame& f, uint32_t ms);` first sighting and changes; a change within MIN_GAP_MS of that ID's last line is held, latest value wins, written by `tick` once the gap has passed
    - `void mark(int n, uint32_t ms);` `void obd(const char* name, float value, uint32_t ms);` (value printed with `%.0f`) `void state(const char* text, uint32_t ms);` `void tick(uint32_t ms);` `void end(uint32_t ms);` (flushes held values, then `S <ms> end`)
  - `ms` is milliseconds since `begin`.

- [ ] **Step 1: Write the failing tests** (capture lines into a vector):

```text
begin(#3,"2026-10-27T14:03:00","ELM327 v2.2","6","0.1.0") -> "# car-can-lab rec 3 2026-10-27T14:03:00 adapter=ELM327 v2.2 proto=6 fw=0.1.0"
frame(1A6 {00 04},100) -> "F 100 1A6 00 04"
frame(1A6 {00 05},200) -> nothing; frame(1A6 {00 06},300) -> nothing; tick(349) -> nothing; tick(350) -> "F 350 1A6 00 06"
frame(1A6 {00 06},400) (same data) -> nothing
mark(3,500) -> "M 500 3";  obd("rpm",812.4f,600) -> "O 600 rpm 812";  state("window 100/780",0) -> "S 0 window 100/780"
end(900) with a held value -> its "F 900 …" line, then "S 900 end"
```

- [ ] **Step 2-4:** Run (FAIL), implement, run (PASS).
- [ ] **Step 5:** Commit `feat(sniff): recording lines with a per-ID rate cap`.

### Task 7: Recording store and download over the hotspot

**Files:**
- Create: `src/util/recname.h`, `src/util/recname.cpp` (pure), `src/sniff/store.h`, `src/sniff/store.cpp` (LittleFS), `src/sniff/web.h`, `src/sniff/web.cpp`
- Test: `test/test_recname/test_recname.cpp`

**Interfaces:**
- Produces:
  - `int recname::next(const int* existing, int n);` highest + 1, 1 when none
  - `void recname::path(int number, char* out, size_t cap);` → `/rec/003.log`
  - `int recname::parse(const char* name);` `"003.log"` → 3, anything else → -1
  - `struct recname::Space { int dropOldest; bool full; };` `recname::Space recname::planSpace(size_t freeBytes, size_t reserveBytes, const int* existing, int n, int current);` how many oldest finished recordings to delete so free ≥ reserve; `full` when that would need deleting `current`. `reserveBytes` = 16 KB.
  - `sniff::store`: `bool begin();` (LittleFS on the data partition, format only if it can't mount) `bool open(int* number);` `void write(const char* line);` (buffered, `flush()` every 2 s from `tick`) `void tick(uint32_t nowMs);` `void close();` `bool full() const;` `int list(int* numbers, size_t* sizes, int max);`
  - `sniff::web`: `void begin();` on port 80: `GET /recs?k=<REMOTE_TOKEN>` plain text `NNN.log <bytes>` per line; `GET /rec?n=<N>&k=<REMOTE_TOKEN>` the file as `text/plain` attachment; wrong/missing token → 403 via `util/remotegate`. `void loop();`

- [ ] **Step 1: Write the failing tests** for `recname`: `next({1,2,7},3)==8`, `next(nullptr,0)==1`, `path(3)=="/rec/003.log"`, `parse("003.log")==3`, `parse("x.log")==-1`; `planSpace(10000,16384,{1,2,3},3,3)` → `dropOldest>0, full=false`; `planSpace(1000,16384,{3},1,3)` → `full=true` (review focus 3).
- [ ] **Step 2-4:** Run (FAIL), implement `recname`, run (PASS).
- [ ] **Step 5:** Implement `store` and `web`; `pio run -e sniff` builds.
- [ ] **Step 6 (deck):** a debug REC that writes 1,000 fake `F` lines; from the phone on the hotspot `http://172.20.10.11/recs?k=…` lists it and `/rec?n=1&k=…` downloads it intact; without `k` → 403. Flash `ledpanel_ota` and back: the file is still listed.
- [ ] **Step 7:** Commit `feat(sniff): store recordings in flash and serve them over the hotspot`.

### Task 8: BLE link to the adapter and the monitor state machine

**Files:**
- Create: `src/sniff/link.h`, `src/sniff/link.cpp` (port of `feat/obd:src/companion/obdlink.cpp`'s BLE path from NimBLE 1.4 to 2.5.1), `src/util/monplan.h`, `src/util/monplan.cpp` (pure state machine), `src/sniff/monitor.h`, `src/sniff/monitor.cpp` (device glue, own FreeRTOS task on core 0)
- Test: `test/test_monplan/test_monplan.cpp`

**Interfaces:**
- Consumes: `obd::allowed`, `obdble::pick`, `obdble::Inbox`, `elmmon::Lines/parse`, `sweep::Plan/commands`, `idtable::IdTable`, `reclog::Rec`, `sniff::store`
- Produces:
  - `sniff::link`: `void start();` scans for an adapter (`obd::looksLikeAdapter` name or `obdble::serialService` UUID), connects, picks characteristics; `bool connected();` `bool send(const char* cmd);` refuses (returns false, logs `[link] refused`) anything `obd::allowed` rejects — the only path to the adapter; `bool readLine(char* out, size_t cap, uint32_t waitMs);`
  - `enum class monplan::State : uint8_t { Searching, Probing, NotCan, NotSilent, CantMonitor, EngineOff, Monitoring, Querying, Lost };`
  - `class monplan::Plan`: `const char* nextCommand();` (nullptr = wait) `void onLine(const char* line, uint32_t nowMs);` `void onTimeout(uint32_t nowMs);` `void onLinkLost(uint32_t nowMs);` `void onLinkBack(uint32_t nowMs);` `void setRecording(bool on, uint32_t nowMs);` `void setFilter(const uint16_t* ids, int n);` `State state() const;` `uint32_t cycleMs() const;`
  - Probe order (exact): `ATZ`, `ATE0`, `ATL0`, `ATS0`, `ATH1`, `ATSP0`, `0100` (no answer → EngineOff, retry every 5 s), `ATDPN` (must be `6`-`9` or `A6`-`A9`, else NotCan), `ATCSM1` (must be `OK`, else NotSilent), `ATI` (kept for the header). Then per window: `ATCF…`, `ATCM…`, `ATMA`, dwell `DWELL_MS = 1500` (`FROZEN_DWELL_MS = 300` per window while recording), `\r`, wait for `>`.
  - `BUFFER FULL` → `sweep.split()` and re-run the window. While recording, every `OBD_EVERY_MS = 3000`: leave monitoring, `010C` (rpm) and `0167` (coolant, the City has no 05), log as `O`, resume.
  - Recording on → `only()` with every ID the table holds (or the picked ones when FILTER is on); each window change logs `S <ms> window <filter>/<mask>`.
  - Lost link while recording → `S <ms> adapter lost`; back → re-probe, `S <ms> adapter back`, resume the same windows (review focus 2).
  - Spaces stay off (`ATS0`, already allowed): monitor lines arrive as `1A60004…`, which `elmmon::parse` handles.

- [ ] **Step 1: Write the failing tests** for `monplan`, feeding replies: happy probe reaches Monitoring and the commands in the exact order; `ATDPN` → `3` gives NotCan and no `ATMA` ever; `ATCSM1` → `?` gives NotSilent and no `ATMA` ever; `0100` → `NO DATA`/`UNABLE TO CONNECT` gives EngineOff; `BUFFER FULL` splits and repeats the window; recording + 3000 ms gives `\r`, `010C`, `0167`, then the window again; `onLinkLost` then `onLinkBack` while recording re-probes and resumes; every command it ever returns passes `obd::allowed` (assert inside a helper used by all tests).
- [ ] **Step 2-4:** Run (FAIL), implement `monplan`, run (PASS).
- [ ] **Step 5:** Port `link`, write `monitor` (task owns `link`, `Plan`, the table under a mutex, `Rec` + `store`); `pio run -e sniff` builds.
- [ ] **Step 6 (deck, smoke test):** with the current clone's BLE side ("OBDBLE") at home or in the car, Serial shows the probe and either `Monitoring` or a named refusal state. With the Vgate when it arrives: in the car, engine on, Serial logs frames. Note results in `docs/BACKLOG.md`.
- [ ] **Step 7:** Commit `feat(sniff): BLE link and the silent monitor state machine`.

### Task 9: Screen A, home page and controls

**Files:**
- Create: `src/sniff/screen.h`, `src/sniff/screen.cpp`, `src/util/sniffview.h`, `src/util/sniffview.cpp` (pure: what text goes where)
- Modify: `src/sniff.cpp` (wire screen, touch, Xbox)
- Test: `test/test_sniffview/test_sniffview.cpp`

**Interfaces:**
- Consumes: `idtable::IdTable` (`order`, `find`, `fade`, `idle`, `dropped`), `monplan::State`, `monplan::Plan::cycleMs`, `sniff::store::list`
- Produces:
  - `const char* sniffview::statusText(monplan::State s, uint32_t inStateMs);` exact: Searching `SEARCHING…` under 20 s, `ADAPTER NOT FOUND` from 20 s, NotCan `NOT A CAN CAR?`, NotSilent `ADAPTER CAN'T MONITOR (NOT SILENT)`, CantMonitor `ADAPTER CAN'T MONITOR`, EngineOff `ENGINE OFF?`, Lost `ADAPTER LOST`, Monitoring/Querying nullptr
  - `void sniffview::header(char* out, size_t cap, int ids, uint32_t dropped, int msgPerSec, bool rec, int logPct, uint32_t cycleMs);` → `SNIFF // LIVE   23 IDs +2  41 msg/s` / with REC `REC 34%  CYCLE 4 s`
  - `void sniffview::row(char* out, size_t cap, const idtable::Row& r);` → `1A6  00 04 00 00 3C 00 00 11`
  - Screen: header, rows sorted by `order` (as many as fit at VT323 17), changed bytes drawn in the bright palette colour blended to the normal one by `fade`, idle rows dim, picked rows marked `*`. Bottom buttons `[PAUSE] [FILTER] [MARK] [REC] [BACK]`, 44 px tall like the panel. `NOTHING HEARD` when Monitoring and no frame for 10 s. Status texts in the red palette colour.
  - Touch: tap a row toggles its pick; FILTER with picks → `setFilter(picks)`, FILTER again clears. MARK numbers from 1 per recording and flashes `MARK n` for 1 s. REC toggles; `LOG FULL` when the store says full.
  - Home page (BACK): `[LIVE]`, `[LOGS]` (each recording `NNN.log  12 KB`, then `http://172.20.10.11/recs?k=…` without the token value: `k=<token>`), and `LIGHTS: FLASH LEDPANEL BACK`. Boots into LIVE.
  - Xbox: D-pad up/down moves a row cursor, A picks, X = MARK, Y = REC, LB = FILTER, RB = PAUSE, B = Back.
- [ ] **Step 1: Write the failing tests** for `sniffview`: each `statusText`; `header` with and without REC and with `dropped>0` (`+2`); `row` for 2-byte and 8-byte frames.
- [ ] **Step 2-4:** Run (FAIL), implement `sniffview`, run (PASS).
- [ ] **Step 5:** Implement `screen` and wiring; `pio run -e sniff` builds.
- [ ] **Step 6 (deck):** with the adapter, rows appear, a changing byte lights and fades, PAUSE freezes, FILTER narrows, MARK and REC show; the Xbox drives everything; night mode turns it orange after 18:00.
- [ ] **Step 7:** Commit `feat(sniff): live list screen, home page, touch and Xbox controls`.

### Task 10: PC tools: ICSim converter and the analyzer

**Files:**
- Create: `tools/sniff/recording.py` (read a recording), `tools/sniff/candump2rec.py`, `tools/sniff/analyze.py` (CLI `python -m tools.sniff.analyze <file>`), `tools/sniff/tests/test_*.py`, `tools/sniff/tests/data/*.log`, `requirements-dev.txt` (`pytest`)

**Interfaces:**
- Produces:
  - `recording.read(path) -> Recording` with `frames: list[tuple[int ms, int id, bytes]]`, `marks: list[tuple[int ms, int n]]`, `obd: dict[str, list[tuple[int, float]]]`, `windows: list[tuple[int ms, int filter, int mask]]`; lines that don't parse (a truncated last line) are skipped and counted in `skipped`.
  - `candump2rec.convert(lines) -> list[str]`: `(1698400000.123456) vcan0 244#0000000123` → `F <ms> 244 00 00 00 01 23`, ms from the first frame, `# car-can-lab rec 0 … adapter=candump proto=vcan fw=-` header.
  - `analyze.noisy(rec, quiet_ms=2000) -> set[tuple[int id, int byte]]` bytes that change with no mark within quiet_ms
  - `analyze.suspects(rec, mark_n, after_ms=1500, before_ms=2000) -> list[Suspect(id, byte, old, new, score)]` changed within after_ms after the mark, unchanged in before_ms before it, not noisy; sorted by score
  - `analyze.track(rec, name) -> list[Track(id, start_byte, width, order, corr)]` single bytes and 16-bit pairs in both byte orders, Pearson correlation against the `O` values (each matched to the frame value at that time), best first
  - `analyze.watched(rec, id, ms) -> bool` from the `S window` lines; suspects and baseline only count IDs being watched.
  - CLI prints, per mark, the top 5 suspects, then the top 5 tracks for each `O` name, as plain text.
- [ ] **Step 1: Write the failing tests** with small hand-written recordings: a byte that flips right after `M 2` is the top suspect for mark 2; a counter byte changing every 100 ms is in `noisy` and never a suspect; a 16-bit big-endian value equal to rpm × 4 tracks `rpm` with corr > 0.99 above every single byte; a truncated last line is skipped (review focus 4); a byte that changed while its ID wasn't watched isn't counted; `candump2rec` output reads back with `recording.read`.
- [ ] **Step 2:** Run `python -m pytest tools/sniff/tests -q`. Expected: FAIL.
- [ ] **Step 3:** Implement the three modules, standard library only.
- [ ] **Step 4:** Run. Expected: all pass.
- [ ] **Step 5:** Commit `feat(tools): ICSim converter and the recording analyzer`.

### Task 11: README and session guide

**Files:**
- Modify: `README.md`
- Create: `docs/sessions.md`, `docs/BACKLOG.md` (if not created earlier), `dbc/.gitkeep`

- [ ] **Step 1:** README: what it is, hardware (yellow deck, Vgate iCar Pro BLE), how it listens only (silent mode, the gate), build/flash (`sniff_ota` ↔ the deck repo's `ledpanel_ota`, PC on the hotspot), the **deck compatibility rule** (same `min_spiffs.csv`, panel NVS untouched), downloading recordings, running the analyzer, and where findings go (`dbc/`). No secrets, no car-identifying data.
- [ ] **Step 2:** `docs/sessions.md`: Session 0 (ICSim + `candump2rec` + analyzer) and Session 1 (the spec's parked steps, in order, with "hold each step at least one CYCLE").
- [ ] **Step 3:** Run `tools\run_tests.ps1` and `python -m pytest tools/sniff/tests -q`. Expected: all pass. `pio run -e sniff` SUCCESS.
- [ ] **Step 4:** Commit `docs(readme): how car-can-lab works and how to run a session`.
