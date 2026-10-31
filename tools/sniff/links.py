"""Byte pipes to an adapter: a serial port (USB, or a Bluetooth Classic COM port) or BLE.

A link only moves bytes: write(data), read(wait) -> bytes, close(). Commands go through
elm.Adapter, which holds the listen-only gate. pyserial and bleak load only when used, so the
analyzer needs neither.
"""
import asyncio
import queue
import threading

from tools.sniff.elm import LinkLost

BAUDS = (38400, 115200, 9600, 230400, 500000)   # 38400 is the ELM327's usual default
SERIAL_SERVICES = (0xFFF0, 0xFFE0, 0x18F0)      # the same list as the firmware's util/obdble
NAME_WORDS = ("OBD", "ELM", "VLINK", "V-LINK", "VGATE", "ICAR")
BLE_CHUNK = 20                                  # the smallest payload every BLE link takes


class SerialLink:
    def __init__(self, port, baud):
        try:
            import serial
        except ImportError as e:
            raise LinkLost("pyserial is missing: pip install -r requirements.txt") from e
        self._serial_error = serial.SerialException
        self._wait = 0
        try:
            self._s = serial.Serial(port, baud, timeout=0)
        except self._serial_error as e:
            raise LinkLost(f"can't open {port}: {e}") from e

    def write(self, data):
        try:
            self._s.write(data)
        except self._serial_error as e:
            raise LinkLost(str(e)) from e

    def read(self, wait):
        try:
            if wait != self._wait:      # pyserial reconfigures the port on every set
                self._s.timeout = self._wait = wait
            first = self._s.read(1)
            return first + self._s.read(self._s.in_waiting) if first else b""
        except self._serial_error as e:
            raise LinkLost(str(e)) from e

    def close(self):
        self._s.close()


def uuid16(uuid):
    """0xFFF0 for "0000fff0-0000-1000-8000-00805f9b34fb" (a Bluetooth base UUID), else 0."""
    u = str(uuid).lower()
    return int(u[4:8], 16) if u.startswith("0000") and u.endswith("-0000-1000-8000-00805f9b34fb") else 0


def looks_like_adapter(name, services):
    up = (name or "").upper()
    return any(w in up for w in NAME_WORDS) or any(uuid16(s) in SERIAL_SERVICES for s in services)


def pick_characteristics(services):
    """(write, notify) of the first service having both, known services first; (None, None) if none.

    services: [(uuid16, [(characteristic, properties), ...]), ...].
    """
    def rank(item):
        return SERIAL_SERVICES.index(item[0]) if item[0] in SERIAL_SERVICES else len(SERIAL_SERVICES)

    for _, chars in sorted(services, key=rank):
        write = next((c for c, p in chars if "write" in p or "write-without-response" in p), None)
        notify = next((c for c, p in chars if "notify" in p or "indicate" in p), None)
        if write is not None and notify is not None:
            return write, notify
    return None, None


class BleLink:
    """bleak is asynchronous and Adapter blocks, so bleak runs on a thread of its own."""

    def __init__(self, address=None, scan_s=8.0):
        self._rx = queue.Queue()
        self._lost = threading.Event()
        self._loop = asyncio.new_event_loop()
        threading.Thread(target=self._loop.run_forever, daemon=True).start()
        try:
            self._run(self._connect(address, scan_s))
        except Exception:
            self._loop.call_soon_threadsafe(self._loop.stop)
            raise

    def _run(self, coro):
        try:
            return asyncio.run_coroutine_threadsafe(coro, self._loop).result()
        except LinkLost:
            raise
        except ImportError as e:
            raise LinkLost("bleak is missing: pip install -r requirements.txt") from e
        except Exception as e:      # BleakError and the OS's own Bluetooth errors
            raise LinkLost(str(e) or type(e).__name__) from e

    async def _connect(self, address, scan_s):
        from bleak import BleakClient, BleakScanner
        if address is None:
            found = await BleakScanner.discover(timeout=scan_s, return_adv=True)
            hits = [d for d, adv in found.values()
                    if looks_like_adapter(adv.local_name or d.name, adv.service_uuids)]
            if not hits:
                raise LinkLost("no BLE adapter found (is the car's ignition on?)")
            address = hits[0].address
        self._client = BleakClient(address, disconnected_callback=lambda _: self._lost.set())
        await self._client.connect()
        services = [(uuid16(s.uuid), [(c, c.properties) for c in s.characteristics])
                    for s in self._client.services]
        self._tx, rx = pick_characteristics(services)
        if self._tx is None:
            await self._client.disconnect()
            raise LinkLost(f"{address} has no serial service")
        self._response = "write-without-response" not in self._tx.properties
        await self._client.start_notify(rx, lambda _, data: self._rx.put(bytes(data)))

    def write(self, data):
        if self._lost.is_set():
            raise LinkLost("adapter disconnected")
        for i in range(0, len(data), BLE_CHUNK):
            self._run(self._client.write_gatt_char(self._tx, data[i:i + BLE_CHUNK], response=self._response))

    def read(self, wait):
        if self._lost.is_set():
            raise LinkLost("adapter disconnected")
        try:
            out = self._rx.get(timeout=wait) if wait > 0 else self._rx.get_nowait()
            while not self._rx.empty():
                out += self._rx.get_nowait()
            return out
        except queue.Empty:
            return b""

    def close(self):
        try:
            if not self._lost.is_set():
                self._run(self._client.disconnect())
        finally:
            self._loop.call_soon_threadsafe(self._loop.stop)
