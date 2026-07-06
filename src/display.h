// display.h — on-screen UI for the Guition ESP32-4848S040 (480x480 ST7701 RGB
// panel + GT911 touch). Compiles to no-ops on boards without -DBOARD_4848S040.
#pragma once

namespace display {
void begin();   // init panel + touch, draw the static UI
void loop();    // poll touch, refresh live values
}
