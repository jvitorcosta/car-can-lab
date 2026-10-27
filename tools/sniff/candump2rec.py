"""Turns a candump log (ICSim on a virtual CAN bus, Session 0) into a sniff recording.

  candump -l vcan0                      -> candump-....log
  python -m tools.sniff.candump2rec candump-....log > recordings/icsim.log

Lines like "(1698400000.123456) vcan0 244#0000000123" become "F <ms> 244 00 00 00 01 23", ms from
the first frame, written on an ID's first sighting and each change (as the deck logs them).
Extended (29-bit) IDs and remote frames are left out: the deck only sees 11-bit IDs.
"""
import re
import sys
from datetime import datetime, timezone

_LINE = re.compile(r"^\((\d+\.\d+)\)\s+\S+\s+([0-9A-Fa-f]+)#([0-9A-Fa-f]*)$")


def convert(lines):
    out = []
    t0 = None
    last = {}
    for raw in lines:
        m = _LINE.match(raw.strip())
        if not m:
            continue
        ts, ident, data = float(m.group(1)), m.group(2), m.group(3)
        if len(ident) > 3 or int(ident, 16) > 0x7FF or len(data) % 2 or len(data) > 16:
            continue
        if t0 is None:
            t0 = ts
            when = datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
            out.append(f"# car-can-lab rec 0 {when} adapter=candump proto=vcan fw=-")
        cid = int(ident, 16)
        payload = bytes.fromhex(data)
        if last.get(cid) == payload:
            continue
        last[cid] = payload
        ms = int(round((ts - t0) * 1000))
        out.append(" ".join([f"F {ms} {cid:03X}"] + [f"{b:02X}" for b in payload]))
    return out


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print("usage: python -m tools.sniff.candump2rec <candump.log>", file=sys.stderr)
        return 2
    with open(argv[0], encoding="utf-8", errors="replace") as f:
        for line in convert(f):
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
