// Copied from spotify-pokemon-deck src/util/version.h @ 9ced38d.
#pragma once
// Firmware version (semantic versioning, from the git tag vMAJOR.MINOR.PATCH). tools/version.py
// writes include/fw_version.h before each firmware build; host tests and builds without it use
// the fallback below.
//   FW_VERSION        full, for the serial log: "1.2.0", "1.2.0+3.gabc1234", "1.2.0+dirty"
//   FW_VERSION_SHORT  fits the boot screen: "1.2.0", or "1.2.0+" past the tag / with local edits
//   FW_LABEL          "FW " + FW_VERSION, the build on a screen (the yellow deck's boot typing, the
//                     black deck's status screens), so an update over WiFi shows which one came up
#if __has_include("fw_version.h")
#include "fw_version.h"
#endif
#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#define FW_VERSION_SHORT "0.0.0-dev"
#endif
#define FW_LABEL "FW " FW_VERSION
