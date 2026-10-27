"""Reads a sniff recording (the deck's /rec/NNN.log, or candump2rec's output).

Lines (spec 2026-10-27 "The recording"; src/util/reclog.h):
  # car-can-lab rec <n> <when> adapter=<..> proto=<..> fw=<..>
  F <ms> <id> <b0> ... <bn>     first sighting / change of an ID's data
  M <ms> <n>                    MARK n
  O <ms> <name> <value>         a value read the normal way (rpm, coolant)
  S <ms> <text>                 a state change ("window 1A0/7F0", "adapter lost", "end", ...)
A line that doesn't parse (the last one, cut by a power cut) is skipped and counted.
"""
import re
from dataclasses import dataclass, field

_HEX2 = re.compile(r"^[0-9A-Fa-f]{2}$")
_ID = re.compile(r"^[0-7][0-9A-Fa-f]{2}$")
_WINDOW = re.compile(r"^window ([0-9A-Fa-f]{3})/([0-9A-Fa-f]{3})$")
_HEADER = re.compile(r"^# car-can-lab rec (\d+) (\S+) adapter=(.*) proto=(\S*) fw=(\S*)$")


@dataclass
class Recording:
    header: dict = field(default_factory=dict)
    frames: list = field(default_factory=list)    # (ms, id, bytes)
    marks: list = field(default_factory=list)     # (ms, n)
    obd: dict = field(default_factory=dict)       # name -> [(ms, value)]
    windows: list = field(default_factory=list)   # (ms, filter, mask)
    states: list = field(default_factory=list)    # (ms, text), the window lines included
    skipped: int = 0


def _ms(s):
    if not s.isdigit():
        raise ValueError(s)
    return int(s)


def parse_lines(lines):
    rec = Recording()
    for raw in lines:
        line = raw.rstrip("\r\n")
        if not line:
            continue
        if line.startswith("#"):
            m = _HEADER.match(line)
            if m:
                rec.header = {"number": int(m.group(1)), "when": m.group(2), "adapter": m.group(3),
                              "proto": m.group(4), "fw": m.group(5)}
            continue
        parts = line.split(" ")
        try:
            kind = parts[0]
            if kind == "F":
                if len(parts) < 3 or len(parts) > 11 or not _ID.match(parts[2]):
                    raise ValueError(line)
                if not all(_HEX2.match(b) for b in parts[3:]):
                    raise ValueError(line)
                rec.frames.append((_ms(parts[1]), int(parts[2], 16), bytes(int(b, 16) for b in parts[3:])))
            elif kind == "M":
                if len(parts) != 3:
                    raise ValueError(line)
                rec.marks.append((_ms(parts[1]), int(parts[2])))
            elif kind == "O":
                if len(parts) != 4:
                    raise ValueError(line)
                rec.obd.setdefault(parts[2], []).append((_ms(parts[1]), float(parts[3])))
            elif kind == "S":
                if len(parts) < 3:
                    raise ValueError(line)
                ms, text = _ms(parts[1]), " ".join(parts[2:])
                rec.states.append((ms, text))
                w = _WINDOW.match(text)
                if w:
                    rec.windows.append((ms, int(w.group(1), 16), int(w.group(2), 16)))
            else:
                raise ValueError(line)
        except ValueError:
            rec.skipped += 1
    return rec


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return parse_lines(f)
