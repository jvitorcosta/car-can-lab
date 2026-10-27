// Copied from spotify-pokemon-deck src/ui/backlight.h @ 9ced38d.
#pragma once
// Display backlight on GPIO 21 (PIN_BL) through LEDC PWM: full by day, dimmer at night.
namespace backlight {
void begin();            // after tft.init() (which drives the pin high); starts at day level
void set(bool night);
}
