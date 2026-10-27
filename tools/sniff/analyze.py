"""Turns a sniff recording into suspects (spec 2026-10-27 "The PC script").

  python -m tools.sniff.analyze recordings/001.log

- Baseline: bytes that change with no MARK nearby are noisy (counters, checksums) and left out.
- Per MARK: the ID/byte pairs that changed within a short window after it and were quiet before
  it, best first, with the old and new values.
- Ground truth: for each O value (rpm, coolant), the single bytes and 16-bit pairs (both byte
  orders) whose value tracks it best (Pearson correlation).
Only changes of an ID while it was being watched count (the S window lines; a recording without
them, such as an ICSim capture, watches everything).
Plain text out, to paste back into a conversation.
"""
import bisect
import math
import sys
from dataclasses import dataclass

from tools.sniff import recording


@dataclass
class Suspect:
    id: int
    byte: int
    old: int
    new: int
    score: float


@dataclass
class Track:
    id: int
    start_byte: int
    width: int
    order: str   # "be", "le", or "-" for a single byte
    corr: float


def _watch_index(rec):
    """(times, windows): from each time on, the (filter, mask) watched, or None (adapter lost, end)."""
    idx = getattr(rec, "_watch", None)
    if idx is None:
        times, wins = [], []
        for t, text in rec.states:
            if text.startswith("window "):
                f, m = text[len("window "):].split("/")
                times.append(t)
                wins.append((int(f, 16), int(m, 16)))
            elif text in ("adapter lost", "end"):
                times.append(t)
                wins.append(None)
        idx = rec._watch = (times, wins)
    return idx


def watched(rec, cid, ms):
    """The ID passed the adapter's filter at ms (the last window line before it), and the adapter was up."""
    if not rec.windows:
        return True
    times, wins = _watch_index(rec)
    k = bisect.bisect_right(times, ms) - 1
    if k < 0 or wins[k] is None:
        return False
    filt, mask = wins[k]
    return (cid & mask) == filt


def changes(rec):
    """(ms, id, byte, old, new) for every byte that changed, watched only (first sightings are not changes)."""
    last = {}
    out = []
    for ms, cid, data in rec.frames:
        prev = last.get(cid)
        last[cid] = data
        if prev is None or not watched(rec, cid, ms):
            continue
        for i in range(max(len(prev), len(data))):
            a = prev[i] if i < len(prev) else None
            b = data[i] if i < len(data) else None
            if a != b:
                out.append((ms, cid, i, a, b))
    return out


def cycle_ms(rec):
    """How long one round of the windows took (the median gap between a window's openings); 0 without windows."""
    last, gaps = {}, []
    for ms, f, m in rec.windows:
        if (f, m) in last:
            gaps.append(ms - last[(f, m)])
        last[(f, m)] = ms
    if not gaps:
        return 0
    gaps.sort()
    return gaps[len(gaps) // 2]


def _slack(rec, ms):
    """A time window widened to one and a half cycles: an ID is only heard when its window comes round."""
    return max(ms, cycle_ms(rec) * 3 // 2)


def noisy(rec, quiet_ms=2000):
    """(id, byte) pairs that changed at least twice with no MARK within quiet_ms (or 1.5 cycles)."""
    quiet_ms = _slack(rec, quiet_ms)
    marks = sorted(ms for ms, _ in rec.marks)
    count = {}
    for ms, cid, i, _, _ in changes(rec):
        k = bisect.bisect_left(marks, ms - quiet_ms)
        near = k < len(marks) and marks[k] <= ms + quiet_ms
        if not near:
            count[(cid, i)] = count.get((cid, i), 0) + 1
    return {k for k, n in count.items() if n >= 2}


def suspects(rec, mark_n, after_ms=1500, before_ms=2000):
    times = [ms for ms, n in rec.marks if n == mark_n]
    if not times:
        return []
    t = times[0]
    # The window after the mark: at least 1.5 cycles (an ID is logged only when its window comes
    # round), but never past the next mark.
    after_ms = _slack(rec, after_ms)
    later = [ms for ms, _ in rec.marks if ms > t]
    end = min([t + after_ms] + [ms - 1 for ms in later])
    after_ms = max(1, end - t)
    noise = noisy(rec)
    ch = changes(rec)
    before = {(cid, i) for ms, cid, i, _, _ in ch if t - before_ms <= ms < t}
    after = {}
    for ms, cid, i, a, b in ch:
        if t <= ms <= end:
            after.setdefault((cid, i), []).append((ms, a, b))
    out = []
    for key, evs in after.items():
        if key in before or key in noise:
            continue
        first_dt = evs[0][0] - t
        score = 1.0 / len(evs) + 0.5 * (after_ms - first_dt) / after_ms
        out.append(Suspect(key[0], key[1], evs[0][1], evs[-1][2], round(score, 3)))
    out.sort(key=lambda s: (-s.score, s.id, s.byte))
    return out


def _pearson(xs, ys):
    n = len(xs)
    if n < 3:
        return 0.0
    mx, my = sum(xs) / n, sum(ys) / n
    sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    sy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if sx == 0 or sy == 0:
        return 0.0
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / (sx * sy)


def track(rec, name):
    samples = rec.obd.get(name, [])
    if not samples:
        return []
    by_id = {}
    for ms, cid, data in rec.frames:
        by_id.setdefault(cid, []).append((ms, data))
    out = []
    for cid, frames in by_id.items():
        times = [ms for ms, _ in frames]
        pairs = []   # (O value, frame data at that time)
        for ms, v in samples:
            k = bisect.bisect_right(times, ms) - 1
            if k >= 0:
                pairs.append((v, frames[k][1]))
        if len(pairs) < 3:
            continue
        width = min(len(d) for _, d in pairs)
        ys = [v for v, _ in pairs]
        for i in range(width):
            out.append(Track(cid, i, 1, "-", _pearson([d[i] for _, d in pairs], ys)))
            if i + 1 < width:
                out.append(Track(cid, i, 2, "be", _pearson([d[i] << 8 | d[i + 1] for _, d in pairs], ys)))
                out.append(Track(cid, i, 2, "le", _pearson([d[i + 1] << 8 | d[i] for _, d in pairs], ys)))
    out.sort(key=lambda x: (-abs(x.corr), x.id, x.start_byte, x.width))
    return out


def report(rec, top=5):
    lines = []
    h = rec.header
    lines.append(f"recording {h.get('number', '?')} ({h.get('when', '?')}, adapter {h.get('adapter', '?')}): "
                 f"{len(rec.frames)} frame lines, {len({c for _, c, _ in rec.frames})} IDs, "
                 f"{len(rec.marks)} marks, {rec.skipped} lines skipped")
    noise = sorted(noisy(rec))
    lines.append(f"noisy (left out): {', '.join(f'{c:03X} byte {i}' for c, i in noise) or 'none'}")
    for ms, n in rec.marks:
        lines.append(f"MARK {n} at {ms / 1000:.1f} s:")
        s = suspects(rec, n)[:top]
        if not s:
            lines.append("  nothing changed")
        for x in s:
            lines.append(f"  {x.id:03X} byte {x.byte}  {_hex(x.old)} -> {_hex(x.new)}  score {x.score}")
    for name in sorted(rec.obd):
        lines.append(f"{name} ({len(rec.obd[name])} readings) tracks best:")
        for x in track(rec, name)[:top]:
            what = f"byte {x.start_byte}" if x.width == 1 else f"bytes {x.start_byte}-{x.start_byte + 1} {x.order}"
            lines.append(f"  {x.id:03X} {what}  corr {x.corr:+.3f}")
    return "\n".join(lines)


def _hex(v):
    return "--" if v is None else f"{v:02X}"


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print("usage: python -m tools.sniff.analyze <recording.log>", file=sys.stderr)
        return 2
    print(report(recording.read(argv[0])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
