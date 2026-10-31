# Safety and security

## Using it on a car

car-can-lab is a research tool, used at your own risk, on vehicles you own or have permission to
work on. The MIT licence's "no warranty" applies in full.

- Run sessions **parked**, on the handbrake, foot on the brake. Don't operate a screen or laptop
  while driving.
- Run the engine outdoors only.
- A clone adapter can misbehave on the bus even in silent mode. If the car shows warnings, unplug
  the adapter.
- Don't leave an adapter plugged in for days: it can drain the battery.

## What the software does to protect the car

The firmware and the PC tools refuse every command outside a short list that excludes anything
that sends a frame, apart from standard mode 01 OBD readings, and refuse to monitor unless the
adapter accepts silent mode. See "Listen only" in the [README](README.md). This reduces risk; it
can't remove what a faulty adapter does on its own.

## Reporting a problem

If you find a way to make any part of this project transmit on the bus outside those mode 01
readings, or a problem with the firmware's download server or WiFi updates, please use
[a private report](https://github.com/jvitorcosta/car-can-lab/security/advisories/new) instead of a
public issue.

The firmware's download server speaks plain HTTP with a key in the URL, which suits a phone hotspot
and doesn't suit any network you don't trust.
