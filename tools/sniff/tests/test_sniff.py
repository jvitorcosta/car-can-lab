"""Tests for the recording reader, the ICSim converter and the analyzer (spec 2026-10-27, "The PC script")."""
import os

from tools.sniff import analyze, candump2rec, recording

DATA = os.path.join(os.path.dirname(__file__), "data")


def _rec(tmp_path, lines, name="r.log"):
    p = tmp_path / name
    p.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return recording.read(str(p))


def test_read_gear_recording():
    rec = recording.read(os.path.join(DATA, "gear.log"))
    assert rec.header["number"] == 1
    assert rec.header["adapter"] == "ELM327 v2.2"
    assert (10, 0x191, bytes([0x0B, 0x00, 0x01, 0x00])) == rec.frames[0]
    assert rec.marks == [(5000, 1), (10000, 2)]
    assert rec.windows == [(0, 0x000, 0x000)]
    assert rec.skipped == 0


def test_a_frame_in_the_same_ms_as_the_next_window_was_heard():
    rec = recording.parse_lines(["S 0 window 1A0/7FF", "F 10 1A0 01", "S 500 window 300/7FF", "S 600 window 1A0/7FF",
                                 "M 1000 1", "F 1000 1A0 02", "S 1000 window 300/7FF", "S 1400 window 1A0/7FF"])
    assert analyze.watched(rec, 0x1A0, 1000) and analyze.watched(rec, 0x300, 1000)
    assert not analyze.watched(rec, 0x1A0, 550)
    assert [(s.id, s.old, s.new) for s in analyze.suspects(rec, 1)] == [(0x1A0, 1, 2)]


def test_byte_that_flips_after_a_mark_is_the_top_suspect():
    rec = recording.read(os.path.join(DATA, "gear.log"))
    s = analyze.suspects(rec, 2)
    assert (s[0].id, s[0].byte) == (0x191, 2)
    assert (s[0].old, s[0].new) == (0x02, 0x04)
    assert (0x1A6, 1) in [(x.id, x.byte) for x in s]
    assert (analyze.suspects(rec, 1)[0].id, analyze.suspects(rec, 1)[0].byte) == (0x191, 2)


def test_counter_is_noisy_and_never_a_suspect():
    rec = recording.read(os.path.join(DATA, "gear.log"))
    assert (0x158, 7) in analyze.noisy(rec)
    assert (0x191, 2) not in analyze.noisy(rec)
    for n in (1, 2):
        assert (0x158, 7) not in [(x.id, x.byte) for x in analyze.suspects(rec, n)]


def test_unknown_mark_has_no_suspects():
    rec = recording.read(os.path.join(DATA, "gear.log"))
    assert analyze.suspects(rec, 9) == []


def test_sixteen_bit_value_tracks_rpm_above_every_single_byte(tmp_path):
    lines = ["# car-can-lab rec 2 x adapter=a proto=6 fw=f"]
    t = 0
    for rpm in (800, 1200, 2500, 3100, 1800, 950, 4000, 2200):
        raw = rpm * 4
        lines.append(f"F {t} 1DC {raw >> 8:02X} {raw & 0xFF:02X} {(rpm // 7) % 256:02X} 00")
        lines.append(f"F {t + 5} 300 {(t // 10) % 256:02X} 11")
        lines.append(f"O {t + 100} rpm {rpm}")
        t += 1000
    rec = _rec(tmp_path, lines)
    tracks = analyze.track(rec, "rpm")
    best = tracks[0]
    assert (best.id, best.start_byte, best.width, best.order) == (0x1DC, 0, 2, "be")
    assert best.corr > 0.99
    singles = [x for x in tracks if x.width == 1]
    assert all(abs(x.corr) < best.corr for x in singles)


def test_truncated_last_line_is_skipped(tmp_path):
    rec = _rec(tmp_path, ["# car-can-lab rec 3 x adapter=a proto=6 fw=f", "F 10 1A6 00 04", "M 20 1", "F 30 1A6 00 0"])
    assert rec.frames == [(10, 0x1A6, bytes([0x00, 0x04]))]
    assert rec.skipped == 1
    rec = _rec(tmp_path, ["# car-can-lab rec 3 x adapter=a proto=6 fw=f", "F 10 1A6 00 04", "F 3"])
    assert rec.skipped == 1


def test_change_while_not_watched_is_not_counted(tmp_path):
    rec = _rec(tmp_path, [
        "# car-can-lab rec 4 x adapter=a proto=6 fw=f",
        "S 0 window 100/700",
        "F 10 1A6 00 00",
        "F 20 2B0 00 00",
        "M 1000 1",
        "F 1100 1A6 00 01",   # 1A6 is in 100/700
        "F 1200 2B0 00 01",   # 2B0 is not: the log contradicts the window, so it isn't counted
    ])
    assert analyze.watched(rec, 0x1A6, 1100)
    assert not analyze.watched(rec, 0x2B0, 1200)
    ids = [(x.id, x.byte) for x in analyze.suspects(rec, 1)]
    assert (0x1A6, 1) in ids
    assert (0x2B0, 1) not in ids


def test_adapter_lost_means_nothing_watched(tmp_path):
    rec = _rec(tmp_path, [
        "# car-can-lab rec 5 x adapter=a proto=6 fw=f",
        "S 0 window 000/000",
        "S 500 adapter lost",
        "S 900 adapter back",
        "S 900 window 000/000",
    ])
    assert analyze.watched(rec, 0x123, 100)
    assert not analyze.watched(rec, 0x123, 600)
    assert analyze.watched(rec, 0x123, 950)


def test_no_window_lines_watch_everything(tmp_path):
    rec = _rec(tmp_path, ["# car-can-lab rec 6 x adapter=a proto=6 fw=f", "F 1 123 00"])
    assert analyze.watched(rec, 0x7FF, 1)


def test_candump_converts_and_reads_back(tmp_path):
    out = candump2rec.convert([
        "(1698400000.123456) vcan0 244#0000000123",
        "(1698400000.223456) vcan0 244#0000000123",   # same data: no line
        "(1698400000.323456) vcan0 244#0000000124",
        "(1698400000.400000) vcan0 18DAF110#0102",    # extended ID: not 11-bit, skipped
        "(1698400001.123456) vcan0 19B#00000E000000",
        "garbage",
    ])
    assert out[0].startswith("# car-can-lab rec 0 ")
    assert out[0].endswith(" adapter=candump proto=vcan fw=-")
    assert out[1] == "F 0 244 00 00 00 01 23"
    assert out[2] == "F 200 244 00 00 00 01 24"
    assert out[3] == "F 1000 19B 00 00 0E 00 00 00"
    rec = _rec(tmp_path, out)
    assert len(rec.frames) == 3 and rec.skipped == 0


def test_cli_report(tmp_path, capsys):
    analyze.main([os.path.join(DATA, "gear.log")])
    out = capsys.readouterr().out
    assert "MARK 2" in out
    assert "191 byte 2" in out


def test_suspects_with_a_slow_cycle(tmp_path):
    # Review 1: two frozen windows, a 6 s cycle; the gear byte is only heard every 6 s, so its
    # change shows up to a cycle after the MARK, and must still be the suspect (and not noise).
    lines = ["# car-can-lab rec 7 x adapter=a proto=6 fw=f"]
    gear = 1
    for k in range(10):
        t = k * 6000
        lines.append(f"S {t} window 191/7FF")
        if k in (3, 6):
            gear *= 2
            lines.append(f"F {t + 100} 191 0B 00 {gear:02X} 00" if k else "")
        elif k == 0:
            lines.append(f"F {t + 100} 191 0B 00 {gear:02X} 00")
        lines.append(f"S {t + 3000} window 158/7FF")
        lines.append(f"F {t + 3100} 158 00 {k:02X}")
    lines.insert(1, "M 14000 1")
    lines.insert(2, "M 32000 2")
    rec = _rec(tmp_path, [l for l in lines if l])
    rec.marks.sort()
    assert analyze.cycle_ms(rec) == 6000
    for n, (old, new) in ((1, (1, 2)), (2, (2, 4))):
        s = analyze.suspects(rec, n)
        assert s, f"mark {n}: nothing"
        assert (s[0].id, s[0].byte, s[0].old, s[0].new) == (0x191, 2, old, new)
    assert (0x191, 2) not in analyze.noisy(rec)
    assert (0x158, 1) in analyze.noisy(rec)
