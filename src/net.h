// net.h — WiFi + web UI + MQTT + OLED glue.
#pragma once
#include <stdint.h>

namespace net {
void begin();
void loop();
void publishState();          // force an MQTT state publish + OLED refresh
void notifyStateChanged();     // mark state dirty (publish on next loop)

// accessors for the on-screen System page
const char* ipAddr();
const char* ssidName();
bool        apMode();
const char* mqttHost();
bool        mqttConnected();
}
