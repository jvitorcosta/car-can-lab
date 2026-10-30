# Third-party code

car-can-lab's own code is under the [MIT licence](LICENSE). These parts come from elsewhere:

| What | Where | Licence |
|---|---|---|
| VT323 font | `src/fonts/`, generated from Google Fonts' VT323 | SIL OFL 1.1, [tools/fonts/VT323-OFL.txt](tools/fonts/VT323-OFL.txt) |
| XboxSeriesXControllerESP32 by asukiaaa, vendored | `lib/xboxpad/` | [lib/xboxpad/LICENSE](lib/xboxpad/LICENSE) |

Libraries fetched at build time (NimBLE-Arduino, TFT_eSPI, XPT2046_Touchscreen, Unity) and at run
time (pyserial, bleak) keep their own licences.
