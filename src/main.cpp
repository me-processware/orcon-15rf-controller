// main.cpp — Orcon 15RF emulator for Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
//
// Emulates the Orcon 15RF display controller over 868.3 MHz RAMSES-II, exposes
// control via an onboard web UI + MQTT, and shows live status on the OLED.
//
// Layers:
//   ramses_codec   protocol (host-tested)      <- the hard part, proven correct
//   ramses_radio   SX1262 GFSK + SW Manchester (CC1101 fallback via -D RADIO_CC1101)
//   orcon          15RF device emulation + state
//   net            WiFi / web / MQTT / OLED
#include <Arduino.h>
#include "config.h"
#include "ramses_radio.h"
#include "orcon.h"
#include "net.h"
#include "display.h"

#if defined(PINTEST)
// ---- GPIO mapper: drives each CC1101 GPIO HIGH (others LOW) for 2.5s in turn.
// Probe each CC1101 solder point with a meter to confirm which GPIO it lands on,
// independent of the (suspect) board schematic.
static const int PT_PINS[] = {48, 47, 41, 40, 2, 1};
void setup() {
    Serial.begin(115200); delay(400);
    Serial.println("\n=== CC1101 GPIO MAPPER ===");
    Serial.println("Each pin goes HIGH (3.3V) for 2.5s; the rest stay LOW (0V).");
    for (unsigned i = 0; i < sizeof(PT_PINS)/sizeof(PT_PINS[0]); i++) {
        pinMode(PT_PINS[i], OUTPUT); digitalWrite(PT_PINS[i], LOW);
    }
}
void loop() {
    static unsigned i = 0;
    for (unsigned k = 0; k < sizeof(PT_PINS)/sizeof(PT_PINS[0]); k++) digitalWrite(PT_PINS[k], LOW);
    digitalWrite(PT_PINS[i], HIGH);
    Serial.printf("HIGH -> GPIO%d   (all others LOW)\n", PT_PINS[i]);
    i = (i + 1) % (sizeof(PT_PINS)/sizeof(PT_PINS[0]));
    delay(2500);
}
#else

void setup() {
    Serial.begin(DEBUG_SERIAL_BAUD);
    delay(200);
    Serial.println("\nOrcon 15RF emulator starting...");

    display::begin();          // init the panel (if any) before the radio takes the shared SPI

    if (!radio::begin()) {
        Serial.println("[FATAL] radio init failed - check wiring / build flags");
    } else {
        Serial.printf("[radio] %s up on %.3f MHz\n", radio::name(), RF_FREQ_MHZ);
    }

    orcon::begin(DEV_US_CLASS, DEV_US_SERIAL, DEV_FAN_CLASS, DEV_FAN_SERIAL);
    radio::startReceive();

    net::begin();
    Serial.println("Ready.");
}

void loop() {
    // 1) drain any received RAMSES frames into the state model
    ramses::Frame f;
    while (radio::poll(f)) {
        if (orcon::onFrame(f)) net::notifyStateChanged();
    }

    // 2) periodic sensor broadcasts (31E0 demand, 1298 CO2, 10E0 info)
    orcon::tick(millis());

    // 3) web / MQTT / OLED
    net::loop();

    // 4) on-screen UI + touch (4848S040 panel; no-op on Heltec)
    display::loop();
}
#endif  // PINTEST
