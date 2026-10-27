# Copied from spotify-pokemon-deck tools/ota_auth.py @ 9ced38d.
# PlatformIO post script for the OTA envs (s3_ota, ledpanel_ota): espota's password is the deck's
# OTA_PASSWORD, read from src/config.h (git-ignored), so it never sits in platformio.ini.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os
import re
import sys

config = os.path.join(env.subst("$PROJECT_DIR"), "src", "config.h")  # noqa: F821
password = ""
if os.path.isfile(config):
    m = re.search(r'^\s*#define\s+OTA_PASSWORD\s+"([^"]*)"', open(config, encoding="utf-8").read(), re.M)
    password = m.group(1) if m else ""
if not password and "upload" in COMMAND_LINE_TARGETS:  # noqa: F821 (a plain build needs none)
    sys.stderr.write("ota_auth: no OTA_PASSWORD in src/config.h; the deck's OTA is off without it\n")
    env.Exit(1)  # noqa: F821
# espota's --debug logs its options, the password among them: keep its output to warnings and progress.
env.Replace(UPLOADERFLAGS=[f for f in env["UPLOADERFLAGS"] if f not in ("--debug", "-d")])  # noqa: F821
env.Append(UPLOADERFLAGS=["--auth=" + password])  # noqa: F821
