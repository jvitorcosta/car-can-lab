# Contributing

Thanks for helping. The most useful contributions need no code at all:

- **Adapter reports.** Run `python -m tools.sniff.adaptercheck` and open an
  [adapter report](https://github.com/jvitorcosta/car-can-lab/issues/new?template=adapter-report.yml),
  even when the answer is "it doesn't work".
- **Car signals.** Found the gear on your car? Add a DBC file: see [dbc/README.md](dbc/README.md).
- **Session reports.** What worked on your car, what the analyzer got wrong. Bug reports with a
  short excerpt of a recording are the easiest to fix.

## The one rule

**Nothing here may transmit on a car's bus.** A change that adds a way to send a frame, or weakens
the check for silent monitoring, won't be merged, however useful it looks.

Both gates, `obd::allowed` (firmware) and `elm.allowed` (PC), accept the same commands. To change
what they accept, change `test/test_obd/gate.inc` and both gates together.

## Working on it

```bash
pip install -r requirements-dev.txt
python -m pytest
python -m tools.sniff.sniffer --simulate
```

The firmware's logic is in pure modules under `src/util/`, tested on the PC. These need PlatformIO
and g++:

```bash
pio pkg install -e native
pwsh tools/run_tests.ps1
cp src/config.example.h src/config.h
pio run -e sniff
```

- Keep device code thin: logic goes in a tested pure module.
- A change to the monitor's behaviour usually belongs in both `src/util/monplan` and
  `tools/sniff/elm.py`, and the recording format in both `src/util/reclog` and
  `tools/sniff/recording.py`.
- Commits follow Conventional Commits: `type(scope): subject`.

## Please don't commit

- Raw recordings. They can identify a car and where it was. Written-up signals and DBC files are
  welcome.
- Secrets or network names. `src/config.h` is git-ignored for this reason.
