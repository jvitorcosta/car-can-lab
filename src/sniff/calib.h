#pragma once
class TFT_eSPI;
// Touch calibration: tap five crosses with the pen, fit (util/touchcal), keep it. The fit is
// stored in sniff's own NVS namespace ("sniff"/"hom"); without one, the LED panel's ("touch"/"hom",
// read only) is used, and without either the panel's factory map (pins.h). Run with BOOT held at
// power-up or for 2 s (as on the LED panel).
namespace sniff {
namespace calib {
// The stored calibration into hw/touch; false when there is none (the factory map stays).
bool load();
void run(TFT_eSPI& tft);
}
}
