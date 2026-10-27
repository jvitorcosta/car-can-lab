# Running a session

## Session 0: practise on ICSim (at home, no car)

ICSim is a simulated instrument cluster on a virtual CAN bus (Linux, `vcan`). It's the safe place
to learn the method and check the analyzer.

1. Start ICSim and its controls on `vcan0`, and record the bus: `candump -l vcan0`.
2. While it records, do one thing at a time with a few seconds of nothing between them: a turn
   signal, a door, speed up. Write down the time of each one (or press Enter in a terminal that
   prints the time): these are your MARKs.
3. Convert: `python -m tools.sniff.candump2rec candump-....log > recordings/icsim.log`, then add
   your marks as `M <ms> <n>` lines (ms from the first frame).
4. `python -m tools.sniff.analyze recordings/icsim.log`. The turn signal's byte should come out
   as the top suspect for its mark.

## Session 1: the car, parked, engine running

Before, at home: put the PC on the phone's hotspot and flash the sniff firmware onto the deck
(`pio run -e sniff_ota -t upload`). Turn the phone's own Bluetooth off near the car: the adapter
takes one connection at a time.

In the car, parked, engine running, foot on the brake:

1. **Boot.** The deck opens LIVE and finds the adapter. Wait until the rows fill in (the sweep
   takes about a minute). If it says `ADAPTER CAN'T MONITOR (NOT SILENT)`, stop: this adapter
   can't listen without acknowledging, and the tool won't use it.
2. **REC** (Y on the controller). From now on the header shows `REC n%` and `CYCLE n s`.
   **Hold every step below for at least one CYCLE**, and longer is better: an ID is only heard
   while its window is open.
3. **Calibrate:** rev in P a few times, MARK (X) before each rev. The analyzer should find the
   bytes that track `rpm`. If it does, the method works on this car.
4. **Baseline:** about 30 s of nothing. The analyzer learns what changes on its own (counters,
   checksums).
5. **Gear:** P, R, N, D, S, about 5 s in each, MARK at every shift. Then in S the paddles, + and
   −, a MARK for each.
6. **Extras:** each door, the headlights, the indicators, the brake pedal, the steering wheel full
   left and full right. MARK each one and write down what each mark number was.
7. **REC** again to stop.
8. **Fuel:** one short recording just before refuelling and one just after.

After the session: on the hotspot, download from `http://172.20.10.11/recs?k=<token>` into
`recordings/`, run the analyzer, and note the suspects. Then flash the LED panel back from the
deck repo (`pio run -e ledpanel_ota -t upload`). The deck's home page reminds you: `LIGHTS: FLASH
LEDPANEL BACK`.

Next time, confirm the suspects: FILTER on the suspect rows (tap them, then FILTER) and repeat
the action while watching the byte.

## When something goes wrong

- `ADAPTER NOT FOUND`: is the adapter in the port, and is the phone's Bluetooth off?
- `ENGINE OFF?`: the car doesn't answer OBD. Start the engine. The deck retries every 5 s.
- `NOTHING HEARD`: the adapter monitors but nothing comes through. The OBD port may sit behind a
  gateway that only passes diagnostic replies. The next step would be diagnostic queries, which
  need a separate design.
- `LOG FULL`: the oldest recordings were already dropped to make room. Download, then start a new
  REC.
- The adapter drops mid-recording: the recording logs `adapter lost` and stays open. When the
  adapter comes back it logs `adapter back` and carries on.
