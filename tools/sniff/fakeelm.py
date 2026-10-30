"""A simulated car and ELM327 behind the link interface: the demo (`sniffer --simulate`) and the tests.

The car broadcasts a counter, a static ID, the engine speed (0C8, bytes 0-1, big endian, rpm x 4)
and the gear (1A0, byte 2). It answers 0100, 010C and 0105, and honours ATCF/ATCM and ATMA with a
frame buffer that overflows, like the real chip. `sent` keeps everything it was told, so tests can
prove what left the PC.
"""
import time

GEARS = {"P": 0x01, "R": 0x02, "N": 0x04, "D": 0x08}
ID_RPM, ID_GEAR, ID_COUNTER, ID_STATIC = 0x0C8, 0x1A0, 0x158, 0x300
PERIOD_S = {ID_RPM: 0.02, ID_GEAR: 0.05, ID_COUNTER: 0.02, ID_STATIC: 0.5}


class FakeCar:
    def __init__(self):
        self.rpm, self.gear = 800, "P"
        self.counter = 0

    def frame(self, cid):
        if cid == ID_RPM:
            v = self.rpm * 4
            return bytes([v >> 8 & 255, v & 255, 0, 0])
        if cid == ID_GEAR:
            return bytes([0, 0x10, GEARS[self.gear], 0])
        if cid == ID_COUNTER:
            self.counter = (self.counter + 1) & 255
            return bytes([0, 0, 0, self.counter])
        return bytes([0x12, 0x34])


class FakeElm:
    """virtual: time only passes in read(), so a 40 s session takes no real time."""
    BUFFER = 24         # frames it holds for a link this slow before BUFFER FULL
    LINK_FPS = 200      # what the link carries a second

    def __init__(self, car=None, virtual=False, proto="6", csm_answer="OK", on_time=None, banner="ELM327 v1.5"):
        self.car = car or FakeCar()
        self.virtual, self.proto, self.csm_answer, self.banner = virtual, proto, csm_answer, banner
        self.on_time = on_time      # called with the time on each read: scripts the car
        self.sent = []
        self.t = 0.0
        self._t0 = time.perf_counter()
        self._out = b""
        self._cmd = ""
        self._echo = True
        self._filter, self._mask = 0, 0
        self._monitoring = False
        self._next = dict.fromkeys(PERIOD_S, 0.0)
        self._queue = 0.0
        self._last = 0.0

    def now(self):
        return self.t if self.virtual else time.perf_counter() - self._t0

    def write(self, data):
        for c in data.decode("ascii"):
            if self._monitoring:        # any character stops a monitor
                self._monitoring = False
                self.sent.append(c)
                self._reply("STOPPED")
            elif c == "\r":
                self.sent.append(self._cmd + "\r")
                if self._echo:
                    self._out += self._cmd.encode() + b"\r"
                self._execute(self._cmd)
                self._cmd = ""
            else:
                self._cmd += c

    def _reply(self, *lines):
        self._out += "".join(f"{x}\r" for x in lines).encode() + b"\r>"

    def _execute(self, cmd):
        if cmd == "ATZ":
            self._filter, self._mask = 0, 0
            self._echo = True
            self._reply(self.banner)
        elif cmd == "ATE0":
            self._echo = False
            self._reply("OK")
        elif cmd == "ATCSM1":
            self._reply(self.csm_answer)
        elif cmd == "ATI":
            self._reply(self.banner)
        elif cmd == "ATDPN":
            self._reply(self.proto)
        elif cmd.startswith("ATCF"):
            self._filter = int(cmd[4:], 16)
            self._reply("OK")
        elif cmd.startswith("ATCM"):
            self._mask = int(cmd[4:], 16)
            self._reply("OK")
        elif cmd == "ATMA":
            self._monitoring = True
            self._queue, self._last = 0.0, self.now()
            self._next = dict.fromkeys(PERIOD_S, self.now())
        elif cmd == "0100":
            self._reply("SEARCHING...", "7E8064100BE3EB811")    # 05 and 0C listed
        elif cmd == "010C":
            v = self.car.rpm * 4
            self._reply(f"7E804410C{v:04X}")
        elif cmd == "0105":
            self._reply("7E803410583")
        elif cmd == "":
            self._monitoring = True     # a bare '\r' repeats the last command
        elif cmd.startswith("AT"):
            self._reply("OK")
        else:
            self._reply("NO DATA")

    def _broadcast(self, now):
        for cid, period in PERIOD_S.items():
            while self._next[cid] <= now:
                self._next[cid] += period
                if cid & self._mask != self._filter:
                    continue
                data = self.car.frame(cid)
                self._out += f"{cid:03X}{data.hex().upper()}\r".encode()
                self._queue += 1
        self._queue = max(0.0, self._queue - (now - self._last) * self.LINK_FPS)
        self._last = now
        if self._queue > self.BUFFER:
            self._monitoring = False
            self._reply("BUFFER FULL")

    def read(self, wait):
        if self.virtual:
            self.t += max(wait, 0.001)
        elif not self._out:
            time.sleep(wait)
        if self.on_time:
            self.on_time(self.now())
        if self._monitoring:
            self._broadcast(self.now())
        out, self._out = self._out, b""
        return out

    def close(self):
        pass
