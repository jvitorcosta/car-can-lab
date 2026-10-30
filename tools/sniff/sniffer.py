"""Sniffs the car from a PC, the same way the firmware does, and writes a recording the analyzer reads.

  python -m tools.sniff.sniffer --simulate              no hardware: a scripted session on a fake car
  python -m tools.sniff.sniffer --port COM5             USB, or a Bluetooth Classic COM port
  python -m tools.sniff.sniffer --ble                   a BLE adapter (--address to pick one)

It sweeps the bus once to learn the IDs, then records them. Press Enter for a MARK, q then Enter
to stop. Listen only: every command passes elm.allowed, and it refuses an adapter that can't
monitor silently.
"""
import argparse
import os
import sys
import threading
import time
from datetime import datetime

from tools.sniff import elm, fakeelm, links, sweep

DWELL_S, FROZEN_DWELL_S, OBD_EVERY_S = 1.5, 0.3, 3.0
DEMO_S = 40.0
DEMO_STEPS = ((10.0, "R"), (15.0, "N"), (20.0, "D"), (25.0, "N"), (30.0, "R"), (35.0, "P"))


class RecWriter:
    """The recording format of src/util/reclog.h; an ID writes when its data changes."""

    def __init__(self, path, number, adapter, proto, now):
        self._f = open(path, "w", encoding="utf-8", newline="\n")
        self._now, self._t0 = now, now()
        self._last = {}
        self._lock = threading.Lock()
        self.marks = 0
        when = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
        self._line(f"# car-can-lab rec {number} {when} adapter={adapter or '-'} proto={proto or '-'} fw=pc")

    def _line(self, text):
        with self._lock:
            self._f.write(text + "\n")

    def _ms(self, t=None):
        return max(0, int(round(((self._now() if t is None else t) - self._t0) * 1000)))

    def frame(self, t, cid, data):
        if self._last.get(cid) != data:
            self._last[cid] = data
            self._line(" ".join([f"F {self._ms(t)} {cid:03X}"] + [f"{b:02X}" for b in data]))

    def mark(self):
        self.marks += 1
        self._line(f"M {self._ms()} {self.marks}")
        return self.marks

    def obd(self, name, value):
        self._line(f"O {self._ms()} {name} {value:g}")

    def state(self, text):
        self._line(f"S {self._ms()} {text}")

    def close(self):
        self.state("end")
        self._f.close()


class Session:
    """One adapter, one recording. Lost adapters end the recording; it is never left without its end."""

    def __init__(self, adapter, now, say=print):
        self.adapter, self.now, self.say = adapter, now, say
        self.plan = sweep.Plan()
        self.ids = set()
        self.frames = 0

    def _window(self, dwell, rec=None):
        """One window of the plan, split or moved past as the firmware does. True at a cycle's end."""
        filt, mask = self.plan.current()
        self.adapter.set_window(filt, mask)
        if rec:
            rec.state(f"window {filt:03X}/{mask:03X}")
        gen = self.adapter.monitor(dwell)
        while True:
            try:
                t, cid, data = next(gen)
            except StopIteration as stop:
                overflowed = stop.value
                break
            self.ids.add(cid)
            self.frames += 1
            if rec:
                rec.frame(t, cid, data)
        if overflowed:
            if rec:
                rec.state("buffer full")
            if self.plan.split():
                return False
            if rec:
                rec.state("window too busy")
        return self.plan.next()

    def scan(self):
        """One full cycle of the sweep: learns which IDs the car sends."""
        self.say(f"sweeping {len(self.plan.windows)} windows...")
        t0 = self.now()
        while True:
            try:
                if self._window(DWELL_S):
                    break
            except elm.Reset:
                self.say("the adapter restarted: setting it up again")
                self.adapter.probe()
                continue
            self.say(f"  {len(self.ids)} IDs, window {self.plan.cur + 1}/{len(self.plan.windows)}")
        self.say(f"heard {len(self.ids)} IDs in {self.now() - t0:.0f} s")
        return self.ids

    def record(self, rec, stop):
        """Cycles through the IDs heard until stop(): frames, an O reading every 3 s, windows logged."""
        self.plan.only(self.ids)
        rec.state(f"rec watching {len(self.ids)} ids")
        last_obd = self.now()
        cycle_start = self.now()
        reset = False
        while not stop():
            try:
                if reset:
                    self.adapter.probe()        # silent monitoring first: nothing until it's back
                    reset = False
                if self.now() - last_obd >= OBD_EVERY_S:
                    last_obd = self.now()
                    for name, value in self.adapter.read_obd().items():
                        rec.obd(name, value)
                if self._window(FROZEN_DWELL_S, rec):
                    self.say(f"cycle {self.now() - cycle_start:.1f} s, {self.frames} frames, "
                             f"{rec.marks} marks: hold each step at least one cycle")
                    cycle_start = self.now()
            except elm.Reset:
                rec.state("adapter reset")
                self.say("the adapter restarted: setting it up again")
                reset = True
            except elm.LinkLost as e:
                rec.state("adapter lost")
                self.say(f"adapter lost: {e}")
                return
            except elm.Refused as e:
                rec.state("adapter refused")
                self.say(f"stopped: {e}")
                return


def next_path(folder):
    """recordings/NNN.log, the next free number, and that number."""
    os.makedirs(folder, exist_ok=True)
    n = 1
    while os.path.exists(os.path.join(folder, f"{n:03d}.log")):
        n += 1
    return os.path.join(folder, f"{n:03d}.log"), n


def open_serial(port, baud=None):
    """(adapter, link) at `baud`, or the first baud rate where an ELM answers."""
    for b in ([baud] if baud else links.BAUDS):
        link = links.SerialLink(port, b)
        adapter = elm.Adapter(link)
        if adapter.hello():
            return adapter, link
        link.close()
    raise elm.Refused(f"no ELM327 answered on {port}")


def demo_script(car, rec_holder, start_at, now):
    """The car's side of the demo: revs, then shifts with a MARK at each."""
    steps = list(DEMO_STEPS)

    def on_time(t):
        if rec_holder and rec_holder[0]:
            s = t - start_at()
            car.rpm = 800 + int(150 * abs(s % 16 - 8))      # slow revs, for the rpm track
            while steps and s >= steps[0][0]:
                car.gear = steps.pop(0)[1]
                rec_holder[0].mark()
    return on_time


def run_demo(folder, virtual=False, say=print):
    """A scripted session on the fake car; returns the recording's path."""
    car = fakeelm.FakeCar()
    holder = [None]
    started = [0.0]
    fake = fakeelm.FakeElm(car, virtual=virtual)
    fake.on_time = demo_script(car, holder, lambda: started[0], fake.now)
    adapter = elm.Adapter(fake, now=fake.now)
    adapter.probe()
    session = Session(adapter, fake.now, say)
    session.scan()
    path, n = next_path(folder)
    rec = RecWriter(path, n, adapter.name + " (simulated)", adapter.protocol, fake.now)
    say(f"recording {DEMO_S:.0f} s of a scripted session into {path}")
    started[0] = fake.now()
    holder[0] = rec
    session.record(rec, lambda: fake.now() - started[0] >= DEMO_S)
    rec.close()
    return path


def run_live(adapter, link, folder):
    now = time.perf_counter     # time.monotonic ticks every ~15 ms on Windows
    adapter.probe()
    print(f"{adapter.name}, protocol {adapter.protocol}: silent monitoring on")
    session = Session(adapter, now)
    session.scan()
    if not session.ids:
        print("nothing heard: the OBD port may sit behind a gateway. See docs/sessions.md.")
        return 1
    path, n = next_path(folder)
    rec = RecWriter(path, n, adapter.name, adapter.protocol, now)
    done = threading.Event()

    def keys():
        for line in sys.stdin:
            if line.strip().lower() == "q":
                break
            print(f"MARK {rec.mark()}")
        done.set()

    threading.Thread(target=keys, daemon=True).start()
    print(f"recording into {path}. Enter = MARK, q then Enter = stop.")
    try:
        session.record(rec, done.is_set)
    except KeyboardInterrupt:
        pass
    finally:
        rec.close()
        link.close()
    print(f"done: python -m tools.sniff.analyze {path}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Listen-only CAN sniffer over an ELM327.")
    how = ap.add_mutually_exclusive_group(required=True)
    how.add_argument("--simulate", action="store_true", help="a scripted session on a simulated car")
    how.add_argument("--port", help="serial port: COM5, /dev/ttyUSB0, /dev/rfcomm0")
    how.add_argument("--ble", action="store_true", help="find a BLE adapter")
    ap.add_argument("--baud", type=int, help="serial speed (default: tries the usual ones)")
    ap.add_argument("--address", help="BLE address, instead of scanning")
    ap.add_argument("--out", default="recordings", help="folder for recordings (default: recordings)")
    args = ap.parse_args(argv)
    try:
        if args.simulate:
            print(f"done: python -m tools.sniff.analyze {run_demo(args.out)}")
            return 0
        if args.port:
            adapter, link = open_serial(args.port, args.baud)
        else:
            link = links.BleLink(args.address)
            adapter = elm.Adapter(link)
        return run_live(adapter, link, args.out)
    except (elm.Refused, elm.LinkLost) as e:
        print(f"stopped: {e}", file=sys.stderr)
        return 1
    except elm.Reset:
        print("stopped: the adapter restarted", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
