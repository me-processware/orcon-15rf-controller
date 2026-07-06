// net.cpp — WiFi (STA + SoftAP), web UI + setup + RF log, MQTT, OLED.
// WiFi/MQTT settings AND device IDs persist in NVS (Preferences).
#include "net.h"
#include "config.h"
#include "orcon.h"
#include "orcon_log.h"
#include "ramses_radio.h"
#if defined(USE_CC1101_TX) || defined(RADIO_CC1101_ASYNC)
#include "cc1101_tx.h"
#endif
#include "web_page.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>

namespace net {

static WebServer    server(80);
static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);
static Preferences  prefs;

#if !defined(NO_OLED)
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C
    oled(U8G2_R0, PIN_OLED_RST, PIN_OLED_SCL, PIN_OLED_SDA);
#endif

static bool     g_apMode    = false;
static bool     g_dirty     = true;
static uint32_t g_lastPub   = 0;
static uint32_t g_lastMqtt  = 0;
static uint32_t g_rebootAt  = 0;
static char     g_ip[24]    = "0.0.0.0";

static String   g_ssid, g_pass, g_mqttHost;

// ---------------------------------------------------------------------------
// Persistent config (NVS): WiFi, MQTT host, device IDs
// ---------------------------------------------------------------------------
static void saveIds(uint8_t uc, uint32_t us, uint8_t fc, uint32_t fs);  // fwd

static void loadConfig() {
    prefs.begin("orcon", true);
    g_ssid     = prefs.getString("ssid", WIFI_SSID);
    g_pass     = prefs.getString("pass", WIFI_PASSWORD);
    g_mqttHost = prefs.getString("mqtt", MQTT_HOST);
    uint8_t  uc = prefs.getUInt("uc", DEV_US_CLASS);
    uint32_t us = prefs.getUInt("us", 0);        // 0 => never stored -> generate a unique one
    uint8_t  fc = prefs.getUInt("fc", DEV_FAN_CLASS);
    uint32_t fs = prefs.getUInt("fs", DEV_FAN_SERIAL);
    int16_t fsv[7]; char key[4];
    for (int i = 0; i < 7; i++) { snprintf(key, sizeof(key), "f%d", i); fsv[i] = (int16_t)prefs.getInt(key, -1); }
    int16_t bpv = (int16_t)prefs.getInt("bp", -1);
    bool     ar = prefs.getBool("ar", false);
    bool     pl = prefs.getBool("pl", false);
    prefs.end();
    bool freshId = (us == 0);
    if (freshId) {                               // first boot: a unique serial from the chip ID
        us = (uint32_t)(ESP.getEfuseMac() & 0x3FFFFUL);   // 18-bit RAMSES serial space
        if (us == 0) us = 1;
        saveIds(uc, us, fc, fs);                 // lock the generated identity into NVS
    }
    orcon::setIds(uc, us, fc, fs);               // apply stored / generated identities
    for (int i = 0; i < 7; i++) orcon::setFanSaved(i, fsv[i]);
    orcon::setBypassSaved(bpv);
    orcon::setAutoRestore(ar);
    orcon::setPassiveLearn(pl);
}
static void saveKeeper() {
    prefs.begin("orcon", false);
    char key[4];
    for (int i = 0; i < 7; i++) { snprintf(key, sizeof(key), "f%d", i); prefs.putInt(key, orcon::fanSaved(i)); }
    prefs.putInt("bp", orcon::bypassSaved());
    prefs.putBool("ar", orcon::autoRestore());
    prefs.end();
}
static void saveWifi(const char* ssid, const char* pass, const char* mqtt) {
    // Only overwrite a field that was actually provided — otherwise a blank
    // (e.g. saving just the MQTT host) would wipe the stored WiFi credentials.
    prefs.begin("orcon", false);
    if (ssid && strlen(ssid)) prefs.putString("ssid", ssid);
    if (pass && strlen(pass)) prefs.putString("pass", pass);
    if (mqtt && strlen(mqtt)) prefs.putString("mqtt", mqtt);
    prefs.end();
}
static void saveIds(uint8_t uc, uint32_t us, uint8_t fc, uint32_t fs) {
    prefs.begin("orcon", false);
    prefs.putUInt("uc", uc); prefs.putUInt("us", us);
    prefs.putUInt("fc", fc); prefs.putUInt("fs", fs);
    prefs.end();
}

static void idStr(const ramses::DeviceId& d, char* out, size_t cap) {
    snprintf(out, cap, "%02u:%06lu", d.cls, (unsigned long)d.serial);
}

// ---------------------------------------------------------------------------
// State -> JSON
// ---------------------------------------------------------------------------
static void buildStateJson(char* out, size_t cap) {
    const orcon::State& s = orcon::state();
    JsonDocument d;
    d["mode"]      = orcon::modeName(s.mode);
    d["supply"]    = s.supplyPct;
    d["exhaust"]   = s.exhaustPct;
    d["t_indoor"]  = s.tempIndoor;
    d["t_outdoor"] = s.tempOutdoor;
    d["t_supply"]  = s.tempSupply;
    d["t_exhaust"] = s.tempExhaust;
    d["humidity"]  = s.humidity;
    d["co2"]       = s.co2;
    d["demand"]    = s.demand;
    d["bypass"]    = s.bypassOpen;
    d["filter"]    = s.filterRemaining;
    d["fault"]     = s.fault;
    d["fan_known"] = s.fanKnown;
    const char* ps = "idle";
    switch (orcon::pairState()) {
        case orcon::PairState::Searching: ps = "searching"; break;
        case orcon::PairState::Paired:    ps = "paired";    break;
        case orcon::PairState::Timeout:   ps = "timeout";   break;
        default: break;
    }
    d["pair"]      = ps;
    d["pair_left"] = (uint32_t)(orcon::pairWindowLeftMs() / 1000);
    d["plearn"]    = orcon::passiveLearn();
    char uid[16], fid[16];
    idStr(orcon::ids().us,  uid, sizeof(uid));
    idStr(orcon::ids().fan, fid, sizeof(fid));
    d["us_id"]     = uid;
    d["fan_id"]    = fid;
    d["rssi"]      = (int)radio::lastRssi();
    d["radio"]     = radio::name();
    d["online"]    = (millis() - s.lastRxMs) < 600000UL && s.lastRxMs != 0;
    d["cmd_mode"]  = orcon::modeName(s.cmdMode);
    d["selftest"]  = s.selftest;
    d["fan_age"]   = s.lastFanMs ? (int)((millis() - s.lastFanMs) / 1000) : -1;
    d["tx_ack"]    = s.txAckMs && (millis() - s.txAckMs) < 15000;
    d["autorestore"] = orcon::autoRestore();
    d["rx_n"]      = s.rxCount;
    d["tx_n"]      = s.txCount;
    d["raw31da"]   = s.raw31DA;
    d["raw31da_age"] = s.raw31DAms ? (int)((millis() - s.raw31DAms) / 1000) : -1;
    d["last_rx"]   = s.lastRx;
    d["last_tx"]   = s.lastTx;
    JsonArray sv = d["saved"].to<JsonArray>();
    for (int i = 0; i < 7; i++) sv.add(orcon::fanSaved(i));
    d["byp_saved"] = orcon::bypassSaved();
#if defined(USE_CC1101_TX) || defined(RADIO_CC1101_ASYNC)
    d["cc1101"]    = cc1101tx::version();
    d["urx"]       = cc1101tx::uartBytes();   // bytes seen on the GDO2 RX line (diag)
#endif
    serializeJson(d, out, cap);
}

// ---------------------------------------------------------------------------
// Command handling (web POST + MQTT)
// ---------------------------------------------------------------------------
static void applyCommand(JsonDocument& d) {
    if (!d["mode"].isNull()) {
        orcon::Mode m = orcon::modeFromName(d["mode"] | "");
        if (m != orcon::Mode::Unknown) orcon::setMode(m);
    }
    if (!d["timer"].isNull())  orcon::setTimer((uint16_t)(d["timer"] | 0));
    if (!d["pair"].isNull())   orcon::startPairing();      // open the bind window
    if (!d["plearn"].isNull()) {                            // advanced auto-learn toggle
        bool on = (int)(d["plearn"] | 0) != 0;
        orcon::setPassiveLearn(on);
        prefs.begin("orcon", false); prefs.putBool("pl", on); prefs.end();
    }
    if (!d["selftest"].isNull()) orcon::setSelftest((int)(d["selftest"] | 0) != 0);
    if (!d["connect"].isNull())  orcon::sendConnect();
    if (!d["status"].isNull())   orcon::requestStatus();
    if (!d["bypass"].isNull()) {
        const char* b = d["bypass"] | "";
        uint8_t v = 0xFF;
        if (!strcmp(b, "open")) v = 0xC8; else if (!strcmp(b, "close")) v = 0x00;
        orcon::setBypass(v);
        orcon::setBypassSaved(v); saveKeeper();          // keep across power loss
    }
    if (!d["ac_mode"].isNull()) {
        const char* a = d["ac_mode"] | "";
        uint8_t w = 0;
        if (!strcmp(a, "exhaust")) w = 1; else if (!strcmp(a, "supply")) w = 2;
        orcon::setAcMode(w); saveKeeper();
    }
    if (!d["ac_sup"].isNull() || !d["ac_exh"].isNull()) {
        uint8_t sup = (uint8_t)(d["ac_sup"] | 50);
        uint8_t exh = (uint8_t)(d["ac_exh"] | 50);
        bool    byp = (int)(d["ac_byp"] | 1) != 0;
        orcon::setAcCustom(sup, exh, byp); saveKeeper();
    }
    if (!d["autorestore"].isNull()) { orcon::setAutoRestore((int)(d["autorestore"] | 0) != 0); saveKeeper(); }
    if (!d["reapply"].isNull())      orcon::reapplySaved();
    if (!d["forget"].isNull())     { for (int i = 0; i < 7; i++) orcon::setFanSaved(i, -1); orcon::setBypassSaved(-1); saveKeeper(); }
    if (!d["raw_code"].isNull()) {
        const char* tn = d["raw_type"] | "I";
        uint8_t ty = 0;
        if (!strcmp(tn, "RQ")) ty = 1; else if (!strcmp(tn, "RP")) ty = 2;
        else if (!strcmp(tn, "W")) ty = 3;
        uint16_t code = (uint16_t)strtol(d["raw_code"] | "0000", nullptr, 16);
        uint8_t pl[64]; uint8_t n = 0;
        const char* h = d["raw_pl"] | "";
        while (h[0] && h[1] && n < 64) {
            char b[3] = { h[0], h[1], 0 };
            pl[n++] = (uint8_t)strtol(b, nullptr, 16);
            h += 2; while (*h == ' ') h++;
        }
        orcon::sendRaw(ty, code, pl, n);
    }
    g_dirty = true;
}

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------
static void handleRoot()  { server.send_P(200, "text/html", INDEX_HTML); }
static void handleWifi()  { server.send_P(200, "text/html", SETUP_HTML); }
static void handleLogPg() { server.send_P(200, "text/html", LOG_HTML);  }
static void handleWidget() { server.send_P(200, "text/html", WIDGET_HTML); }
static void handleFlow()   { server.send_P(200, "text/html", FLOW_HTML);   }
static void handleDebug()  { server.send_P(200, "text/html", DEBUG_HTML);  }
static void handleUpdatePage() { server.send_P(200, "text/html", UPDATE_HTML); }
static void handleUpdateUpload() {
    HTTPUpload& up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (up.status == UPLOAD_FILE_WRITE) {
        Update.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        Update.end(true);
    }
}
static void handleUpdateDone() {
    bool ok = !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0}");
    if (ok) g_rebootAt = millis() + 1500;   // reboot into the new firmware
}

static void handleState() {
    char buf[1400]; buildStateJson(buf, sizeof(buf));
    server.send(200, "application/json", buf);
}
static void handleCmd() {
    JsonDocument d;
    if (deserializeJson(d, server.arg("plain"))) {
        server.send(400, "application/json", "{\"err\":\"bad json\"}"); return;
    }
    applyCommand(d);
    server.send(200, "application/json", "{\"ok\":1}");
}

// GET /api/log -> { log:[...], seen_remote, seen_fan }
static void handleLog() {
    static char buf[7168];
    size_t n = snprintf(buf, sizeof(buf), "{\"log\":");
    n += rlog::toJson(buf + n, sizeof(buf) - n);
    const rlog::Seen& s = rlog::seen();
    char rem[16] = "", fan[16] = "";
    if (s.haveRemote) idStr(s.remote, rem, sizeof(rem));
    if (s.haveFan)    idStr(s.fan,    fan, sizeof(fan));
    snprintf(buf + n, sizeof(buf) - n,
             ",\"seen_remote\":\"%s\",\"seen_fan\":\"%s\"}", rem, fan);
    server.send(200, "application/json", buf);
}

// POST /api/ids {us_class,us_serial,fan_class,fan_serial}
static void handleIds() {
    JsonDocument d;
    if (deserializeJson(d, server.arg("plain"))) {
        server.send(400, "application/json", "{\"err\":\"bad json\"}"); return;
    }
    uint8_t  uc = (uint8_t)(d["us_class"]  | (int)DEV_US_CLASS);
    uint32_t us = (uint32_t)(d["us_serial"] | 0);
    uint8_t  fc = (uint8_t)(d["fan_class"] | (int)DEV_FAN_CLASS);
    uint32_t fs = (uint32_t)(d["fan_serial"]| 0);
    saveIds(uc, us, fc, fs);
    orcon::setIds(uc, us, fc, fs);
    g_dirty = true;
    server.send(200, "application/json", "{\"ok\":1}");
}

// GET /api/scan
static void handleScan() {
    int n = WiFi.scanNetworks();
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    for (int i = 0; i < n && i < 30; i++) {
        JsonObject o = a.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["lock"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    String out; serializeJson(d, out);
    server.send(200, "application/json", out);
    WiFi.scanDelete();
}

// POST /api/wifi {ssid,pass,mqtt} -> save + reboot
static void handleWifiSave() {
    JsonDocument d;
    if (deserializeJson(d, server.arg("plain"))) {
        server.send(400, "application/json", "{\"err\":\"bad json\"}"); return;
    }
    String ssid = d["ssid"] | "";
    String pass = d["pass"] | "";
    String mqtt = d["mqtt"] | "";
    if (ssid.length() == 0 && pass.length() == 0 && mqtt.length() == 0) {
        server.send(400, "application/json", "{\"err\":\"nothing to save\"}"); return;
    }
    saveWifi(ssid.c_str(), pass.c_str(), mqtt.c_str());
    server.send(200, "application/json", "{\"ok\":1}");
    g_rebootAt = millis() + 1200;
}

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------
static void mqttCallback(char* topic, byte* payload, unsigned int len) {
    (void)topic;
    JsonDocument d;
    if (deserializeJson(d, payload, len)) return;
    applyCommand(d);
}
#if MQTT_ENABLED
// ---- Home Assistant MQTT auto-discovery (Domoticz reads the same format) ----
static void addDev(JsonDocument& d) {
    JsonObject dev = d["dev"].to<JsonObject>();
    dev["ids"].to<JsonArray>().add("orcon15rf");
    dev["name"] = "Orcon 15RF";
    dev["mf"]   = "Orcon (DIY)";
    dev["mdl"]  = "HRC400 15RF";
}
static void pubCfg(const char* comp, const char* oid, JsonDocument& d) {
    char buf[640]; size_t n = serializeJson(d, buf, sizeof(buf));
    char topic[100];
    snprintf(topic, sizeof(topic), "homeassistant/%s/orcon15rf/%s/config", comp, oid);
    mqtt.publish(topic, (const uint8_t*)buf, n, true);
}
static void pubSensor(const char* oid, const char* name, const char* key,
                      const char* unit, const char* dcla) {
    JsonDocument d;
    d["name"]    = name;
    d["uniq_id"] = String("orcon_") + oid;
    d["stat_t"]  = MQTT_BASE_TOPIC "/state";
    d["val_tpl"] = String("{{ value_json.") + key + " }}";
    if (unit && *unit) d["unit_of_meas"] = unit;
    if (dcla && *dcla) d["dev_cla"]      = dcla;
    addDev(d);
    pubCfg("sensor", oid, d);
}
static void pubBinary(const char* oid, const char* name, const char* tpl, const char* dcla) {
    JsonDocument d;
    d["name"]    = name;
    d["uniq_id"] = String("orcon_") + oid;
    d["stat_t"]  = MQTT_BASE_TOPIC "/state";
    d["val_tpl"] = tpl;
    d["pl_on"]   = "ON";
    d["pl_off"]  = "OFF";
    if (dcla && *dcla) d["dev_cla"] = dcla;
    addDev(d);
    pubCfg("binary_sensor", oid, d);
}
static void pubSelect(const char* oid, const char* name, const char* valTpl,
                      const char* cmdTpl, const char** opts, int nopts) {
    JsonDocument d;
    d["name"]    = name;
    d["uniq_id"] = String("orcon_") + oid;
    d["stat_t"]  = MQTT_BASE_TOPIC "/state";
    d["val_tpl"] = valTpl;
    d["cmd_t"]   = MQTT_BASE_TOPIC "/cmd";
    d["cmd_tpl"] = cmdTpl;
    JsonArray o = d["options"].to<JsonArray>();
    for (int i = 0; i < nopts; i++) o.add(opts[i]);
    addDev(d);
    pubCfg("select", oid, d);
}
static void publishDiscovery() {
    pubSensor("t_indoor",  "Indoor temperature",  "t_indoor",  "\xC2\xB0""C", "temperature");
    pubSensor("t_outdoor", "Outdoor temperature", "t_outdoor", "\xC2\xB0""C", "temperature");
    pubSensor("t_supply",  "Supply temperature",  "t_supply",  "\xC2\xB0""C", "temperature");
    pubSensor("t_exhaust", "Exhaust temperature", "t_exhaust", "\xC2\xB0""C", "temperature");
    pubSensor("humidity",  "Humidity",            "humidity",  "%",   "humidity");
    pubSensor("co2",       "CO2",                 "co2",       "ppm", "carbon_dioxide");
    pubSensor("supply",    "Supply fan",          "supply",    "%",   "");
    pubSensor("exhaust",   "Exhaust fan",         "exhaust",   "%",   "");
    pubSensor("filter",    "Filter remaining",    "filter",    "%",   "");
    pubSensor("rssi",      "RF signal",           "rssi",      "dBm", "signal_strength");
    pubBinary("bypass", "Bypass open", "{{ 'ON' if value_json.bypass else 'OFF' }}", "opening");
    pubBinary("fault",  "Fault",       "{{ 'ON' if value_json.fault  else 'OFF' }}", "problem");
    static const char* MODES[] = {"away","auto","low","medium","high","boost"};
    pubSelect("mode", "Mode", "{{ value_json.mode }}", "{\"mode\":\"{{ value }}\"}", MODES, 6);
    static const char* BYP[] = {"open","auto","close"};
    pubSelect("bypass_set", "Bypass control",
              "{{ 'open' if value_json.bypass else 'close' }}",
              "{\"bypass\":\"{{ value }}\"}", BYP, 3);
}
#endif

static void mqttReconnect() {
#if MQTT_ENABLED
    if (mqtt.connected() || g_apMode) return;
    // Back off when the broker is unreachable so a blocking connect() doesn't
    // repeatedly stall the loop and starve the radio RX. Grows 5s -> 5min.
    static uint32_t backoff = 5000;
    if (millis() - g_lastMqtt < backoff) return;
    g_lastMqtt = millis();
    String cid = "orcon15rf-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    bool ok = strlen(MQTT_USER)
        ? mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS)
        : mqtt.connect(cid.c_str());
    if (ok) { mqtt.subscribe(MQTT_BASE_TOPIC "/cmd"); publishDiscovery(); g_dirty = true; backoff = 5000; }
    else    { backoff = backoff < 300000 ? backoff * 2 : 300000; }
#endif
}

// ---------------------------------------------------------------------------
// OLED
// ---------------------------------------------------------------------------
#if defined(NO_OLED)
static void oledInit() {}
static void oledDraw() {}
#else
static void oledInit() {
    pinMode(PIN_VEXT, OUTPUT); digitalWrite(PIN_VEXT, LOW);
    pinMode(PIN_OLED_RST, OUTPUT);
    digitalWrite(PIN_OLED_RST, LOW); delay(20); digitalWrite(PIN_OLED_RST, HIGH);
    oled.begin();
    oled.setFont(u8g2_font_6x12_tf);
}
static void oledDraw() {
    const orcon::State& s = orcon::state();
    char l1[24], l2[24], l3[24], l4[24];
    snprintf(l1, sizeof(l1), "Orcon 15RF  %s", radio::name());
    if (s.selftest)
        snprintf(l2, sizeof(l2), "TST cmd:%s fan:%s",
                 orcon::modeName(s.cmdMode), orcon::modeName(s.mode));
    else
        snprintf(l2, sizeof(l2), "Mode: %s", orcon::modeName(s.mode));
    int fage = s.lastFanMs ? (int)((millis() - s.lastFanMs) / 1000) : -1;
    snprintf(l3, sizeof(l3), "fan %ds  CO2 %u", fage, s.co2);
    snprintf(l4, sizeof(l4), g_apMode ? "AP %s" : "%s", g_ip);
    oled.clearBuffer();
    oled.drawStr(0, 11, l1); oled.drawStr(0, 26, l2);
    oled.drawStr(0, 41, l3); oled.drawStr(0, 56, l4);
    if (s.fault) oled.drawStr(104, 11, "!");
    oled.sendBuffer();
}
#endif

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
static void wifiConnect() {
    g_apMode = (g_ssid.length() == 0);
    if (!g_apMode) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(g_ssid.c_str(), g_pass.c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(250); oledDraw(); }
    }
    if (WiFi.status() == WL_CONNECTED) {
        g_apMode = false;
        snprintf(g_ip, sizeof(g_ip), "%s", WiFi.localIP().toString().c_str());
    } else {
        g_apMode = true;
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASS);
        snprintf(g_ip, sizeof(g_ip), "%s", WiFi.softAPIP().toString().c_str());
    }
}

// ---------------------------------------------------------------------------
void begin() {
    loadConfig();          // also applies stored device IDs via orcon::setIds
    oledInit();
    wifiConnect();

    if (!g_apMode) {                    // OTA only makes sense on the home network
        ArduinoOTA.setHostname("orcon15rf");
        ArduinoOTA.begin();
    }

    server.on("/",          handleRoot);
    server.on("/wifi",      handleWifi);
    server.on("/log",       handleLogPg);
    server.on("/widget",    handleWidget);
    server.on("/widget.html", handleFlow);
    server.on("/flow",      handleFlow);
    server.on("/debug",     handleDebug);
    server.on("/update",    HTTP_GET,  handleUpdatePage);
    server.on("/update",    HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.on("/api/state", handleState);
    server.on("/api/cmd",   HTTP_POST, handleCmd);
    server.on("/api/log",   handleLog);
    server.on("/api/ids",   HTTP_POST, handleIds);
    server.on("/api/scan",  handleScan);
    server.on("/api/wifi",  HTTP_POST, handleWifiSave);
    server.begin();

#if MQTT_ENABLED
    mqtt.setServer(g_mqttHost.c_str(), MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(768);   // room for the discovery config payloads
    mqtt.setSocketTimeout(2);     // cap a blocking connect() at 2s (don't starve RX)
#endif
    oledDraw();
}

void notifyStateChanged() { g_dirty = true; }

// ---- accessors for the on-screen System page -------------------------------
const char* ipAddr()       { return g_ip; }
const char* ssidName()     { return g_apMode ? WIFI_AP_NAME : g_ssid.c_str(); }
bool        apMode()       { return g_apMode; }
const char* mqttHost()     { return g_mqttHost.c_str(); }
bool        mqttConnected() {
#if MQTT_ENABLED
    return mqtt.connected();
#else
    return false;
#endif
}

void publishState() {
    char buf[760]; buildStateJson(buf, sizeof(buf));
#if MQTT_ENABLED
    if (mqtt.connected()) mqtt.publish(MQTT_BASE_TOPIC "/state", buf, true);
#endif
    oledDraw();
    g_dirty = false;
    g_lastPub = millis();
}

void loop() {
    if (g_rebootAt && millis() > g_rebootAt) ESP.restart();
    if (orcon::idsDirty()) {                  // pairing learned a fan -> persist it
        const orcon::Ids& id = orcon::ids();
        saveIds(id.us.cls, id.us.serial, id.fan.cls, id.fan.serial);
        orcon::clearIdsDirty();
        g_dirty = true;
    }
    if (!g_apMode) ArduinoOTA.handle();
    server.handleClient();
#if MQTT_ENABLED
    mqttReconnect();
    mqtt.loop();
#endif
    if (g_dirty || millis() - g_lastPub > 5000) publishState();
}

} // namespace net
