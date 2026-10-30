"""Tests for the PC side (spec 2026-10-30): the gate, the monitor, the demo, the adapter check."""
import os
import re

import pytest

from tools.sniff import adaptercheck, analyze, elm, fakeelm, links, recording, sniffer, sweep

VECTORS = os.path.join(os.path.dirname(__file__), "..", "..", "..", "test", "test_obd", "gate.inc")


def _vectors(kind):
    with open(VECTORS, encoding="utf-8") as f:
        return [m.group(1).replace("\\r", "\r").replace("\\n", "\n") for m in re.finditer(kind + r'\("(.*)"\)', f.read())]


def _fake(**kw):
    fake = fakeelm.FakeElm(virtual=True, **kw)
    return fake, elm.Adapter(fake, now=fake.now)


def _monitor(adapter, seconds):
    gen = adapter.monitor(seconds)
    frames = []
    while True:
        try:
            frames.append(next(gen))
        except StopIteration as stop:
            return frames, stop.value


def test_the_gate_matches_the_firmwares_vectors():
    assert len(_vectors("ALLOW")) > 20 and len(_vectors("REFUSE")) > 30
    for c in _vectors("ALLOW"):
        assert elm.allowed(c), repr(c)
    for c in _vectors("REFUSE"):
        assert not elm.allowed(c), repr(c)


def test_send_refuses_what_the_gate_refuses():
    fake, adapter = _fake()
    with pytest.raises(ValueError):
        adapter.send("ATSH7DF\r")
    assert fake.sent == []


@pytest.mark.parametrize("line,want", [
    ("1A6000400", (0x1A6, bytes([0, 4, 0]))),
    ("1A6 00 04", (0x1A6, bytes([0, 4]))),
    ("1A6", (0x1A6, b"")),
    ("800AA", None),
    ("1A6000", None),
    ("SEARCHING...", None),
    ("18DAF110AABBCCDDEEFF0011", None),     # 29-bit: longer than any 11-bit frame
    ("", None),
])
def test_parse_frame(line, want):
    assert elm.parse_frame(line) == want


def test_lines_splits_and_keeps_the_prompt_alone():
    lines = elm.Lines()
    lines.push(b"OK\r\r>1A")
    assert lines.take() == ["OK", ">"]
    lines.push(b"60004\r")
    assert lines.take() == ["1A60004"]


def test_probe_picks_coolant_05_when_listed_else_67():
    _, adapter = _fake()
    adapter.probe()
    assert adapter.coolant_pid == 0x05
    assert adapter.name == "ELM327 v1.5"
    assert not elm.supports(["7E8064100BFFF0000"], 0x05) is False
    assert elm.supports(["7E8064100F7FF0000"], 0x05) is False


def test_probe_refuses_a_clone_that_rejects_silent_monitoring_and_never_monitors():
    fake, adapter = _fake(csm_answer="?")
    with pytest.raises(elm.Refused, match="silently"):
        adapter.probe()
    assert "ATMA\r" not in fake.sent


@pytest.mark.parametrize("proto,why", [("7", "29-bit"), ("A9", "29-bit"), ("3", "not a CAN")])
def test_probe_names_unusable_buses(proto, why):
    fake, adapter = _fake(proto=proto)
    with pytest.raises(elm.Refused, match=why):
        adapter.probe()
    assert "ATMA\r" not in fake.sent


def test_a_car_at_250k_is_fine():
    _, adapter = _fake(proto="A8")
    adapter.probe()
    assert adapter.protocol == "A8"


def test_monitor_hears_the_window_and_stops_with_a_bare_return():
    fake, adapter = _fake()
    adapter.probe()
    adapter.set_window(0x1A0, 0x7FF)
    frames, overflowed = _monitor(adapter, 1.0)
    assert not overflowed
    assert frames and {cid for _, cid, _ in frames} == {0x1A0}
    assert fake.sent[-1] == "\r"


def test_a_full_buffer_ends_the_monitor_without_a_stray_return():
    fake, adapter = _fake()
    fake.LINK_FPS = 10
    adapter.probe()
    adapter.set_window(0, 0)
    _, overflowed = _monitor(adapter, 5.0)
    assert overflowed
    assert fake.sent[-1] == "ATMA\r"    # a '\r' to an idle ELM would repeat ATMA


def test_an_adapter_reset_mid_monitor_is_seen():
    fake, adapter = _fake()
    adapter.probe()
    adapter.set_window(0x1A0, 0x7FF)
    original = fake.read

    def banner_after_a_while(wait):
        out = original(wait)
        return out + b"ELM327 v1.5\r" if fake.t > 0.3 else out

    fake.read = banner_after_a_while
    with pytest.raises(elm.Reset):
        _monitor(adapter, 2.0)


def _resets_once(fake, when):
    """The adapter restarts, losing its settings, when `when(fake)` first holds."""
    original = fake.read
    done = []

    def read(wait):
        out = original(wait)
        if not done and when(fake):
            done.append(True)
            fake._monitoring = False
            fake._echo = True
            return out + b"\rELM327 v1.5\r\r>"
        return out

    fake.read = read


def test_a_reset_outside_the_monitor_is_seen_too():
    fake, adapter = _fake()
    adapter.probe()
    _resets_once(fake, lambda f: f.sent[-1].startswith("ATCF"))
    with pytest.raises(elm.Reset):
        adapter.set_window(0x1A0, 0x7FF)
    assert "ATMA\r" not in fake.sent


def test_a_reset_during_the_probe_starts_it_again_and_silent_mode_is_last():
    fake, adapter = _fake()
    _resets_once(fake, lambda f: f.sent[-1] == "ATSP0\r")
    adapter.probe()
    assert fake.sent.count("ATZ\r") == 2
    assert fake.sent[-1] == "ATCSM1\r"


def test_an_adapter_that_keeps_restarting_is_refused_in_words():
    fake, adapter = _fake()
    original = fake.read
    fake.read = lambda wait: original(wait) + (b"ELM327 v1.5\r\r>" if fake.sent[-1:] == ["ATE0\r"] else b"")
    with pytest.raises(elm.Refused, match="restarting"):
        adapter.probe()


def test_a_reset_during_the_sweep_probes_again(tmp_path):
    fake = fakeelm.FakeElm(virtual=True)
    adapter = elm.Adapter(fake, now=fake.now)
    adapter.probe()
    session = sniffer.Session(adapter, fake.now, lambda *_: None)
    _resets_once(fake, lambda f: f.t > 3.0)
    assert 0x0C8 in session.scan()
    assert fake.sent.count("ATCSM1\r") == 2


def test_an_overflow_racing_the_stop_still_counts_and_sends_no_second_return():
    fake, adapter = _fake()
    adapter.probe()
    adapter.set_window(0x1A0, 0x7FF)
    original = fake.write

    def write(data):
        if data == b"\r":
            fake.sent.append("\r")
            fake._monitoring = False
            fake._out += b"BUFFER FULL\r\r>"
            return
        original(data)

    fake.write = write
    _, overflowed = _monitor(adapter, 0.3)
    assert overflowed
    assert fake.sent.count("\r") == 1


def test_read_obd_opens_the_filter_to_the_engine_first():
    fake, adapter = _fake()
    adapter.probe()
    fake.car.rpm = 812
    assert adapter.read_obd() == {"rpm": 812, "coolant": 91}
    assert fake.sent[-4:] == ["ATCF7E8\r", "ATCM7F8\r", "010C\r", "0105\r"]


def test_read_obd_decodes_0167_and_skips_a_missing_sensor():
    fake, adapter = _fake()
    adapter.probe()
    adapter.coolant_pid = 0x67
    assert "coolant" not in adapter.read_obd()      # NO DATA
    original = fake._execute
    fake._execute = lambda c: fake._reply("7E8054167037E00") if c == "0167" else original(c)
    assert adapter.read_obd()["coolant"] == 86
    fake._execute = lambda c: fake._reply("7E8054167027E00") if c == "0167" else original(c)
    assert "coolant" not in adapter.read_obd()      # A says sensor 1 isn't there


def test_sweep_port_matches_the_firmware():
    p = sweep.Plan()
    assert len(p.windows) == 16 and p.current() == (0, 0x780)
    assert p.split() and p.windows[:2] == [(0, 0x7C0), (0x40, 0x7C0)] and len(p.windows) == 17
    p.only([0x1A0, 0x1A1, 0x300])
    assert p.windows == [(0x1A0, 0x7FC), (0x300, 0x7FF)]
    assert p.split() and len(p.windows) == 3
    p.only([0x300])
    assert not p.split()
    p.only([])
    assert len(p.windows) == 16
    while not p.next():
        pass
    assert p.cur == 0


def test_recwriter_is_the_recording_format(tmp_path):
    t = [0.0]
    path = tmp_path / "001.log"
    rec = sniffer.RecWriter(str(path), 1, "ELM327 v2.2", "6", lambda: t[0])
    rec.state("window 1A0/7F0")
    t[0] = 0.5
    rec.frame(0.5, 0x1A0, bytes([1, 2]))
    rec.frame(0.6, 0x1A0, bytes([1, 2]))            # unchanged: no line
    rec.frame(0.7, 0x1A0, bytes([1, 3]))
    assert rec.mark() == 1
    rec.obd("rpm", 812)
    rec.close()
    r = recording.read(str(path))
    assert r.skipped == 0
    assert r.header["adapter"] == "ELM327 v2.2" and r.header["fw"] == "pc"
    assert r.frames == [(500, 0x1A0, bytes([1, 2])), (700, 0x1A0, bytes([1, 3]))]
    assert r.marks == [(500, 1)] and r.obd["rpm"] == [(500, 812.0)]
    assert r.windows == [(0, 0x1A0, 0x7F0)]
    assert r.states[-1] == (500, "end")


def test_the_demo_finds_the_gear_and_the_rpm(tmp_path):
    path = sniffer.run_demo(str(tmp_path), virtual=True, say=lambda *_: None)
    rec = recording.read(path)
    assert rec.skipped == 0 and len(rec.marks) == 6
    assert (0x158, 3) in analyze.noisy(rec)
    for _, n in rec.marks:
        top = analyze.suspects(rec, n)[0]
        assert (top.id, top.byte) == (fakeelm.ID_GEAR, 2), f"mark {n}"
    best = analyze.track(rec, "rpm")[0]
    assert (best.id, best.start_byte, best.order) == (fakeelm.ID_RPM, 0, "be") and best.corr > 0.8


def test_the_session_only_ever_sends_what_the_gate_allows(tmp_path):
    fake = fakeelm.FakeElm(virtual=True)
    adapter = elm.Adapter(fake, now=fake.now)
    adapter.probe()
    session = sniffer.Session(adapter, fake.now, lambda *_: None)
    session.scan()
    path, n = sniffer.next_path(str(tmp_path))
    rec = sniffer.RecWriter(path, n, "x", "6", fake.now)
    t0 = fake.now()
    session.record(rec, lambda: fake.now() - t0 > 10)
    rec.close()
    assert all(elm.allowed(c) for c in fake.sent)
    assert "ATCSM1\r" in fake.sent
    assert fake.sent.index("ATCSM1\r") < fake.sent.index("ATMA\r")


def test_a_reset_probes_again_before_monitoring(tmp_path):
    fake = fakeelm.FakeElm(virtual=True)
    adapter = elm.Adapter(fake, now=fake.now)
    adapter.probe()
    session = sniffer.Session(adapter, fake.now, lambda *_: None)
    session.ids = {0x1A0}
    path, n = sniffer.next_path(str(tmp_path))
    rec = sniffer.RecWriter(path, n, "x", "6", fake.now)
    original = fake.read
    once = [True]

    def banner(wait):
        out = original(wait)
        if once[0] and fake.t > 1.0 and fake._monitoring:
            once[0] = False
            fake._monitoring = False
            return out + b"ELM327 v1.5\r\r>"
        return out

    fake.read = banner
    t0 = fake.now()
    session.record(rec, lambda: fake.now() - t0 > 3)
    rec.close()
    assert fake.sent.count("ATCSM1\r") == 2
    assert any(text == "adapter reset" for _, text in recording.read(path).states)


def test_a_lost_link_ends_the_recording_cleanly(tmp_path):
    fake = fakeelm.FakeElm(virtual=True)
    adapter = elm.Adapter(fake, now=fake.now)
    adapter.probe()
    session = sniffer.Session(adapter, fake.now, lambda *_: None)
    session.ids = {0x1A0}
    path, n = sniffer.next_path(str(tmp_path))
    rec = sniffer.RecWriter(path, n, "x", "6", fake.now)
    original = fake.read

    def dies(wait):
        if fake.t > 1.0:
            raise elm.LinkLost("gone")
        return original(wait)

    fake.read = dies
    session.record(rec, lambda: False)
    rec.close()
    states = [text for _, text in recording.read(path).states]
    assert states[-2:] == ["adapter lost", "end"]


def test_next_path_numbers_recordings(tmp_path):
    folder = str(tmp_path / "new")
    path, n = sniffer.next_path(folder)
    assert n == 1 and path.endswith("001.log")
    open(path, "w").close()
    assert sniffer.next_path(folder)[1] == 2


def test_adapter_check_good():
    _, adapter = _fake()
    r = adaptercheck.check(adapter)
    assert r.silent and r.car and r.frames > 0 and r.ids == 4
    assert r.name == "ELM327 v1.5"      # not the echo of ATI
    assert r.seconds > 0
    assert adaptercheck.verdict(r).startswith("USABLE")
    assert adaptercheck.report_line(r, "serial").count("|") == 7


def test_adapter_check_clone_never_monitors():
    fake, adapter = _fake(csm_answer="?")
    r = adaptercheck.check(adapter)
    assert not r.silent and "ATMA\r" not in fake.sent
    assert adaptercheck.verdict(r).startswith("NOT USABLE")
    assert all(elm.allowed(c) for c in fake.sent)


def test_adapter_check_without_a_car(monkeypatch):
    fake, adapter = _fake()
    monkeypatch.setattr(fake, "_execute", _no_car(fake))
    r = adaptercheck.check(adapter)
    assert r.silent and not r.car and "ATMA\r" not in fake.sent
    assert adaptercheck.verdict(r).startswith("PROMISING")


def _no_car(fake):
    original = fake._execute

    def execute(cmd):
        if cmd == "0100":
            fake._reply("UNABLE TO CONNECT")
        elif cmd == "ATDPN":
            fake._reply("A0")
        else:
            original(cmd)
    return execute


def test_adapter_check_nothing_answers():
    class Silent:
        t = 0.0

        def now(self):
            return self.t

        def write(self, data):
            pass

        def read(self, wait):
            self.t += max(wait, 0.01)
            return b""

    s = Silent()
    r = adaptercheck.check(elm.Adapter(s, now=s.now))
    assert adaptercheck.verdict(r).startswith("NO ADAPTER")


def test_ble_helpers():
    fff0 = "0000fff0-0000-1000-8000-00805f9b34fb"
    assert links.uuid16(fff0) == 0xFFF0
    assert links.uuid16("6e400001-b5a3-f393-e0a9-e50e24dcca9e") == 0
    assert links.looks_like_adapter("Vgate iCar Pro", [])
    assert links.looks_like_adapter(None, [fff0])
    assert not links.looks_like_adapter("JBL Flip 5", [])
    other = (0x1234, [("w0", ["write"]), ("n0", ["notify"])])
    known = (0xFFE0, [("w1", ["write-without-response"]), ("n1", ["indicate"])])
    assert links.pick_characteristics([other, known]) == ("w1", "n1")
    assert links.pick_characteristics([other]) == ("w0", "n0")
    assert links.pick_characteristics([(0xFFE0, [("n", ["notify"])])]) == (None, None)
