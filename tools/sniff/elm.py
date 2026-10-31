"""Talks to an ELM327 from a PC (spec 2026-10-30): the listen-only gate, the line and frame parsers
and the blocking monitor. The firmware's src/util/obd, elmmon and monplan say the same in C++.

Adapter.send is the only way to a link, and it goes through allowed(), so nothing that transmits a
frame can leave the PC. test/test_obd/gate.inc holds both gates to the same list.
"""
import re
import time

_SETUP = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATH1", "ATSP0", "ATRV", "ATI", "ATDPN", "ATMA",
          "ATCSM1", "ATSP6", "ATCRA", ""}
_MODE01 = re.compile(r"01[0-9A-F]{2}")
_FILTER = re.compile(r"ATC[FM][0-9A-F]{3}")
_CRA = re.compile(r"ATCRA[0-9A-FX]{3}")
_FRAME = re.compile(r"[0-9A-Fa-f]+")
BANNER_REPLIES = ("ATZ\r", "ATI\r")   # the only commands answered with the "ELM327 ..." banner
RESET_TRIES = 3

PROBE = ["ATE0\r", "ATL0\r", "ATS0\r", "ATH1\r", "ATSP0\r"]
REPLY_S, SLOW_REPLY_S = 2.0, 8.0     # slow: ATZ, 0100 (SEARCHING...)
STOP_TRIES = 3
FILTER_TRIES = 3


class Refused(Exception):
    """The adapter or the car can't be used; the message says why, in words."""


class Reset(Exception):
    """The adapter restarted by itself and lost its settings, silent monitoring among them."""


class LinkLost(Exception):
    """The connection to the adapter is gone."""


def allowed(cmd):
    """True for exactly the commands the firmware's obd::allowed accepts."""
    if not cmd or not cmd.endswith("\r"):
        return False
    body = cmd[:-1]
    return body in _SETUP or bool(_MODE01.fullmatch(body) or _FILTER.fullmatch(body) or _CRA.fullmatch(body))


def parse_frame(line):
    """(id, data) for a monitor line (ATH1 ATS0: "1A6000400") or with spaces; None for anything else."""
    digits = line.replace(" ", "")
    if len(digits) < 3 or len(digits) > 19 or (len(digits) - 3) % 2 or not _FRAME.fullmatch(digits):
        return None
    cid = int(digits[:3], 16)
    return None if cid > 0x7FF else (cid, bytes.fromhex(digits[3:]))


class Lines:
    """Bytes from the adapter into lines; the '>' prompt is a line of its own."""

    def __init__(self):
        self._cur = ""
        self._out = []

    def push(self, data):
        for c in data.decode("ascii", "replace"):
            if c in "\r\n":
                self._finish()
            elif c == ">" and not self._cur:
                self._out.append(">")
            elif c != "\0":
                self._cur += c

    def _finish(self):
        line = self._cur.strip()
        self._cur = ""
        if line:
            self._out.append(line)

    def take(self):
        out, self._out = self._out, []
        return out

    def unread(self, lines):
        self._out = list(lines) + self._out


def pid_value(replies, pid):
    """The data bytes after "41 <pid>" in a reply's lines, or None."""
    want = f"41{pid:02X}"
    for line in replies:
        digits = line.replace(" ", "").upper()
        at = digits.find(want)
        if at >= 0 and _FRAME.fullmatch(digits):
            rest = digits[at + 4:]
            return bytes.fromhex(rest[:len(rest) & ~1])
    return None


def supports(replies, pid):
    """Whether a reply to 0100 lists pid (0x01-0x20); every ECU answers, so any of them counts."""
    want = "4100"
    for line in replies:
        digits = line.replace(" ", "").upper()
        at = digits.find(want)
        if at >= 0 and len(digits) >= at + 12 and _FRAME.fullmatch(digits):
            if int(digits[at + 4:at + 12], 16) >> (32 - pid) & 1:
                return True
    return False


class Adapter:
    """One ELM327 on one link. Every call blocks; `now` is replaceable so tests run on a fake clock."""

    def __init__(self, link, now=time.perf_counter):
        self._link = link
        self._now = now
        self._lines = Lines()
        self.name = ""
        self.protocol = ""
        self.coolant_pid = 0x67

    def send(self, cmd):
        if not allowed(cmd):
            raise ValueError(f"refused by the listen-only gate: {cmd!r}")
        self._link.write(cmd.encode("ascii"))

    def now(self):
        return self._now()

    def _read(self, wait, banner_ok=False):
        """The lines available now, waiting up to `wait` s for the first.

        The banner anywhere it wasn't asked for means the adapter restarted, whatever we were doing.
        """
        self._lines.push(self._link.read(wait))
        lines = self._lines.take()
        if not banner_ok and any(line.startswith("ELM327") for line in lines):
            raise Reset()
        return lines

    def command(self, cmd, timeout=REPLY_S):
        """Sends cmd and returns its reply's lines (the prompt not included); [] when none came."""
        self.send(cmd)
        return self._until_prompt(timeout, cmd in BANNER_REPLIES)

    def _until_prompt(self, timeout, banner_ok=False):
        out = []
        end = self._now() + timeout
        while self._now() < end:
            for line in self._read(min(0.1, max(0.0, end - self._now())), banner_ok):
                if line == ">":
                    return out
                out.append(line)
        return out

    def hello(self):
        """True when an ELM answers ATZ. The first try may only end a monitor left running."""
        for _ in range(3):
            if any("ELM" in line for line in self.command("ATZ\r", SLOW_REPLY_S)):
                return True
        return False

    def probe(self):
        """Sets the adapter up. Raises Refused, with the reason, on anything unusable."""
        for _ in range(RESET_TRIES):
            try:
                return self._probe()
            except Reset:
                pass
        raise Refused("the adapter keeps restarting (a poor connection at the OBD port?)")

    def _probe(self):
        """The firmware's order, except ATI before ATCSM1: a reset in ATI's reply looks like its
        answer, and silent mode must be the last thing set."""
        if not self.hello():
            raise Refused("no ELM327 answered")
        for cmd in PROBE:
            if not any("OK" in line for line in self.command(cmd)):
                raise Refused(f"the adapter refused {cmd.strip()}")
        reply = self.command("0100\r", SLOW_REPLY_S)
        if not any("4100" in line.replace(" ", "") for line in reply):
            raise Refused("engine off? the car doesn't answer OBD")
        self.coolant_pid = 0x05 if supports(reply, 0x05) else 0x67
        self.protocol = "".join(self.command("ATDPN\r")[:1]).strip()
        digit = self.protocol[-1:]
        if digit in ("7", "9"):
            raise Refused("29-bit CAN: not supported")
        if digit not in ("6", "8"):
            raise Refused(f"not a CAN car? (protocol {self.protocol or 'unknown'})")
        self.name = "".join(self.command("ATI\r")[:1]).strip()
        self.require_silent()

    def require_silent(self):
        reply = self.command("ATCSM1\r")
        if not any("OK" in line for line in reply):
            raise Refused(f"adapter can't monitor silently (ATCSM1 answered {' '.join(reply) or 'nothing'})")

    def set_window(self, filt, mask):
        """Sets the receive filter. No OK may mean a stray '\\r' restarted a monitor: stop it and retry."""
        cmds = (f"ATCF{filt:03X}\r", f"ATCM{mask:03X}\r")
        for _ in range(FILTER_TRIES):
            if all(any("OK" in line for line in self.command(c)) for c in cmds):
                return
            self._stop()
        raise Refused("adapter can't monitor (it refused the receive filter)")

    def _stop(self, send=True):
        """Waits for the prompt that ends a monitor, stopping it with '\r' when `send`. A monitor
        that already ended by itself must not get one: it would repeat ATMA. Returns (frames, overflowed)."""
        frames, overflowed = [], False
        for _ in range(STOP_TRIES):
            if send:
                self.send("\r")
            end = self._now() + REPLY_S
            while self._now() < end:
                for line in self._read(0.05):
                    if line == ">":
                        return frames, overflowed
                    if "BUFFER FULL" in line:
                        overflowed = True       # the overflow raced the '\r': still an overflow
                    f = parse_frame(line)
                    if f:
                        frames.append((self._now(), f[0], f[1]))
            send = True
        raise LinkLost("the adapter stopped answering")

    def monitor(self, seconds):
        """ATMA for `seconds`. Yields (time, id, data); returns True when the window overflowed."""
        self.send("ATMA\r")
        end = self._now() + seconds
        while self._now() < end:
            lines = self._read(min(0.05, max(0.0, end - self._now())))
            for i, line in enumerate(lines):
                if "BUFFER FULL" in line:
                    self._lines.unread(lines[i + 1:])       # the prompt may have come with it
                    yield from self._stop(send=False)[0]
                    return True
                if line == "?":
                    raise Refused("adapter can't monitor (it refused ATMA)")
                if line == ">":
                    return False            # the ELM stopped by itself
                f = parse_frame(line)
                if f:
                    yield self._now(), f[0], f[1]
        frames, overflowed = self._stop()
        yield from frames
        return overflowed

    def read_obd(self):
        """{"rpm": value, "coolant": value}, whichever the car answered."""
        out = {}
        self.set_window(0x7E8, 0x7F8)       # the window's filter would hide the engine's replies
        rpm = pid_value(self.command("010C\r"), 0x0C)
        if rpm and len(rpm) >= 2:
            out["rpm"] = (rpm[0] * 256 + rpm[1]) / 4
        cool = pid_value(self.command(f"01{self.coolant_pid:02X}\r"), self.coolant_pid)
        if cool and self.coolant_pid == 0x05:
            out["coolant"] = cool[0] - 40
        elif cool and len(cool) >= 2 and cool[0] & 1:      # 0167: A says which sensors exist
            out["coolant"] = cool[1] - 40
        return out
