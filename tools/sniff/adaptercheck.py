"""Says whether an adapter can be used, before anyone goes out to the car.

  python -m tools.sniff.adaptercheck --port COM5
  python -m tools.sniff.adaptercheck --ble

The adapter only needs power, which on most cars means being in the OBD port with the ignition on.
With the engine running it also tries a short silent monitor. It ends with a line for
docs/adapters.md. Listen only: it uses the same gate as the sniffer.
"""
import argparse
import sys
from dataclasses import dataclass

from tools.sniff import elm, links, sniffer

MONITOR_S = 3.0


@dataclass
class Result:
    name: str = ""
    silent: bool = False
    silent_answer: str = ""
    car: bool = False
    protocol: str = ""
    frames: int = 0
    ids: int = 0
    seconds: float = 0.0


def check(adapter):
    """Runs the checks in order; each needs the one before it. Never monitors without ATCSM1 OK."""
    r = Result()
    if not adapter.hello():
        return r
    for cmd in elm.PROBE:       # ATE0 first: with echo on, ATI's first line is its own echo
        adapter.command(cmd)
    r.name = "".join(adapter.command("ATI\r")[:1]).strip()
    reply = adapter.command("0100\r", elm.SLOW_REPLY_S)
    r.car = any("4100" in line.replace(" ", "") for line in reply)
    r.protocol = "".join(adapter.command("ATDPN\r")[:1]).strip()
    answer = adapter.command("ATCSM1\r")
    r.silent_answer = " ".join(answer) or "nothing"
    r.silent = any("OK" in line for line in answer)
    if r.silent and r.car and r.protocol[-1:] in ("6", "8"):
        ids = set()
        adapter.set_window(0, 0)
        t0 = adapter.now()
        for _, cid, _ in adapter.monitor(MONITOR_S):     # a busy bus overflows early, which is fine
            r.frames += 1
            ids.add(cid)
        r.seconds = adapter.now() - t0
        r.ids = len(ids)
    return r


def verdict(r):
    if not r.name:
        return "NO ADAPTER: nothing answered ATZ"
    if not r.silent:
        return f"NOT USABLE: it can't monitor silently (ATCSM1 answered {r.silent_answer})"
    if not r.car:
        return "PROMISING: it accepts silent monitoring. Try again in the car, engine running"
    if r.protocol[-1:] in ("7", "9"):
        return "NOT USABLE ON THIS CAR: 29-bit CAN isn't supported"
    if r.protocol[-1:] not in ("6", "8"):
        return f"NOT USABLE ON THIS CAR: not CAN (protocol {r.protocol or 'unknown'})"
    if not r.frames:
        return "NOTHING HEARD: it monitors, but no frames came. A gateway, or a clone that ignores ATMA"
    return f"USABLE: heard {r.frames} frames from {r.ids} IDs in {r.seconds:.1f} s"


def report_line(r, link_name):
    silent = "yes" if r.silent else "no"
    frames = f"{r.frames} in {r.seconds:.1f} s" if r.car else "not tried"
    return f"| ? | {r.name or '?'} | {link_name} | {silent} | {frames} | ? |"


def main(argv=None):
    ap = argparse.ArgumentParser(description="Checks an ELM327 can monitor silently.")
    how = ap.add_mutually_exclusive_group(required=True)
    how.add_argument("--port", help="serial port: COM5, /dev/ttyUSB0, /dev/rfcomm0")
    how.add_argument("--ble", action="store_true", help="find a BLE adapter")
    ap.add_argument("--baud", type=int, help="serial speed (default: tries the usual ones)")
    ap.add_argument("--address", help="BLE address, instead of scanning")
    args = ap.parse_args(argv)
    try:
        if args.port:
            adapter, link = sniffer.open_serial(args.port, args.baud)
            name = "serial"
        else:
            link = links.BleLink(args.address)
            adapter = elm.Adapter(link)
            name = "BLE"
        r = check(adapter)
        link.close()
    except (elm.Refused, elm.LinkLost) as e:
        print(f"stopped: {e}", file=sys.stderr)
        return 1
    except elm.Reset:
        print("stopped: the adapter restarted during the check; try again", file=sys.stderr)
        return 1
    print(verdict(r))
    print("\nFor docs/adapters.md (fill in the ? columns: the product, and your car):")
    print(report_line(r, name))
    return 0 if r.silent else 1


if __name__ == "__main__":
    sys.exit(main())
