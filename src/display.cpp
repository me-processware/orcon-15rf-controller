// display.cpp — Guition ESP32-4848S040 on-screen UI (ST7701 480x480 RGB + GT911).
// Only built for -DBOARD_4848S040; a no-op stub otherwise so the Heltec builds
// don't need Arduino_GFX.
#include "display.h"

#if defined(BOARD_4848S040)
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "orcon.h"
#include "ramses_radio.h"
#include "net.h"

namespace display {

// ST7701 init for the GUITION 4848S040 panel — verbatim from the known-working
// aquaElectronics/esp32-4848s040-st7701 project (the library's generic types
// garble this panel).
static const uint8_t st7701_4848S040_init[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0D, 0x02,
    WRITE_C8_D16, 0xC2, 0x31, 0x05,
    WRITE_C8_D8,  0xCD, 0x00,
    WRITE_COMMAND_8, 0xB0, WRITE_BYTES, 16,
    0x00,0x11,0x18,0x0E, 0x11,0x06,0x07,0x08, 0x07,0x22,0x04,0x12, 0x0F,0xAA,0x31,0x18,
    WRITE_COMMAND_8, 0xB1, WRITE_BYTES, 16,
    0x00,0x11,0x19,0x0E, 0x12,0x07,0x08,0x08, 0x08,0x22,0x04,0x11, 0x11,0xA9,0x32,0x18,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
    WRITE_C8_D8, 0xB0, 0x60,
    WRITE_C8_D8, 0xB1, 0x32,
    WRITE_C8_D8, 0xB2, 0x07,
    WRITE_C8_D8, 0xB3, 0x80,
    WRITE_C8_D8, 0xB5, 0x49,
    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x21,
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,
    WRITE_COMMAND_8, 0xE0, WRITE_BYTES, 3, 0x00, 0x1B, 0x02,
    WRITE_COMMAND_8, 0xE1, WRITE_BYTES, 11,
    0x08,0xA0,0x00,0x00, 0x07,0xA0,0x00,0x00, 0x00,0x44,0x44,
    WRITE_COMMAND_8, 0xE2, WRITE_BYTES, 12,
    0x11,0x11,0x44,0x44, 0xED,0xA0,0x00,0x00, 0xEC,0xA0,0x00,0x00,
    WRITE_COMMAND_8, 0xE3, WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,
    WRITE_C8_D16, 0xE4, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 16,
    0x0A,0xE9,0xD8,0xA0, 0x0C,0xEB,0xD8,0xA0, 0x0E,0xED,0xD8,0xA0, 0x10,0xEF,0xD8,0xA0,
    WRITE_COMMAND_8, 0xE6, WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,
    WRITE_C8_D16, 0xE7, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE8, WRITE_BYTES, 16,
    0x09,0xE8,0xD8,0xA0, 0x0B,0xEA,0xD8,0xA0, 0x0D,0xEC,0xD8,0xA0, 0x0F,0xEE,0xD8,0xA0,
    WRITE_COMMAND_8, 0xEB, WRITE_BYTES, 7, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40,
    WRITE_C8_D16, 0xEC, 0x3C, 0x00,
    WRITE_COMMAND_8, 0xED, WRITE_BYTES, 16,
    0xAB,0x89,0x76,0x54, 0x02,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0x20, 0x45,0x67,0x98,0xBA,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    WRITE_C8_D8, 0xE5, 0xE4,
    WRITE_COMMAND_8, 0xFF, WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
    WRITE_C8_D8, 0x3A, 0x60,        // 0x60 = RGB666
    DELAY, 10,
    WRITE_COMMAND_8, 0x11,          // Sleep Out
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,          // Display On
    END_WRITE
};

static Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /*DC*/, 39 /*CS*/, 48 /*SCK*/, 47 /*MOSI*/, GFX_NOT_DEFINED /*MISO*/);
static Arduino_ESP32RGBPanel *panel = new Arduino_ESP32RGBPanel(
    18 /*DE*/, 17 /*VSYNC*/, 16 /*HSYNC*/, 21 /*PCLK*/,
    11, 12, 13, 14, 0,       /*R0..R4*/
    8, 20, 3, 46, 9, 10,     /*G0..G5*/
    4, 5, 6, 7, 15,          /*B0..B4*/
    1 /*hsync_pol*/, 10, 8, 50,
    1 /*vsync_pol*/, 10, 8, 20,
    1 /*pclk_active_neg = true*/,
    12000000 /*prefer PCLK ~12MHz — lower = less PSRAM-bandwidth banding*/);
static Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480, 480, panel, 0 /*rotation*/, true /*auto_flush*/,
    bus, GFX_NOT_DEFINED /*RST*/, st7701_4848S040_init, sizeof(st7701_4848S040_init));

#define PIN_BL 38

// ---- colours (RGB565) ------------------------------------------------------
#ifndef RGB565   // Arduino_GFX.h already defines this; keep ours only if it doesn't
#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
#endif
static const uint16_t C_BG   = RGB565(0x10,0x14,0x1a);
static const uint16_t C_CARD = RGB565(0x26,0x30,0x3c);
static const uint16_t C_FG   = RGB565(0xe7,0xed,0xf3);
static const uint16_t C_MUT  = RGB565(0x8b,0x97,0xa7);
static const uint16_t C_ACC  = RGB565(0x3d,0xa9,0xfc);
static const uint16_t C_EXH  = RGB565(0xe9,0x88,0x5a);
static const uint16_t C_OK   = RGB565(0x2e,0xcc,0x71);

// ---- GT911 capacitive touch (I2C: SDA=19, SCL=45, addr 0x5D) ----------------
#define GT911_ADDR 0x5D
// Persistent touch state. The GT911 only flags "new data" (bit7) every ~10ms;
// between flags we must KEEP the current down-state, otherwise a slow drag looks
// like a stream of taps and swipes never accumulate.
static bool    g_tDown = false;
static int16_t g_tX = 0, g_tY = 0;
static void touchBegin() { Wire.begin(19, 45); Wire.setClock(400000); }
static void touchPoll() {
    Wire.beginTransmission(GT911_ADDR); Wire.write(0x81); Wire.write(0x4E);
    if (Wire.endTransmission() != 0) return;
    if (Wire.requestFrom(GT911_ADDR, 1) != 1) return;
    uint8_t st = Wire.read();
    if (!(st & 0x80)) return;             // no fresh sample -> keep current state
    if ((st & 0x0F) > 0) {                // finger down -> update coordinates
        Wire.beginTransmission(GT911_ADDR); Wire.write(0x81); Wire.write(0x50);
        Wire.endTransmission();
        if (Wire.requestFrom(GT911_ADDR, 4) == 4) {
            uint8_t xl = Wire.read(), xh = Wire.read(), yl = Wire.read(), yh = Wire.read();
            g_tX = (int16_t)(xl | (xh << 8));
            g_tY = (int16_t)(yl | (yh << 8));
            g_tDown = true;
        }
    } else {
        g_tDown = false;                  // GT911 reported a release
    }
    Wire.beginTransmission(GT911_ADDR); Wire.write(0x81); Wire.write(0x4E); Wire.write(0x00);
    Wire.endTransmission();               // clear the buffer-ready flag
}

// ---- buttons ----------------------------------------------------------------
struct Btn { int16_t x, y, w, h; const char *label; const char *arg; };

// ---- page 0: control --------------------------------------------------------
static Btn modeBtns[6] = {
    {  10, 232, 148, 64, "Away", "away" }, { 166, 232, 148, 64, "Auto", "auto" },
    { 322, 232, 148, 64, "1", "low" },     {  10, 304, 148, 64, "2", "medium" },
    { 166, 304, 148, 64, "3", "high" },    { 322, 304, 148, 64, "Boost", "boost" },
};
static Btn bypBtns[3] = {
    {  10, 380, 148, 62, "Byp open", "open" }, { 166, 380, 148, 62, "Auto", "auto" },
    { 322, 380, 148, 62, "Close", "close" },
};

// ---- page 1: custom AC profile ----------------------------------------------
static int16_t acSup = 20, acExh = 40;
static Btn profBtns[] = {
    { 190, 120, 56, 56, "-", "s-" }, { 360, 120, 56, 56, "+", "s+" },
    { 190, 196, 56, 56, "-", "e-" }, { 360, 196, 56, 56, "+", "e+" },
    {  14, 268, 452, 58, "Apply custom", "apply" },
    {  10, 344, 148, 64, "Exh out", "exhaust" }, { 166, 344, 148, 64, "Sup in", "supply" },
    { 322, 344, 148, 64, "Balanced", "off" },
};

static const int NUM_PAGES = 4;   // 0=control 1=custom AC 2=airflow house 3=system
static int  g_page = 0;
// selMode = which mode button is lit (the user's commanded choice). lastConfirmed
// = the mode the fan last actually reported. The highlight follows selMode and is
// only moved by the periodic refresh when the *confirmed* mode genuinely changes,
// so a tap stays blue even before (or without) an ack from the fan.
static char selMode[12] = "";
static char lastConfirmed[12] = "";

// ---- helpers (cursor Y = baseline) -----------------------------------------
static void tL(int16_t x, int16_t baseline, const char *s, const GFXfont *f, uint16_t col) {
    gfx->setFont(f); gfx->setTextColor(col); gfx->setCursor(x, baseline); gfx->print(s);
}
static void tC(int16_t cx, int16_t cy, const char *s, const GFXfont *f, uint16_t col) {
    gfx->setFont(f);
    int16_t x1, y1; uint16_t w, h;
    gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    gfx->setTextColor(col);
    gfx->setCursor(cx - (int16_t)w / 2 - x1, cy - (int16_t)h / 2 - y1);
    gfx->print(s);
}
static void drawBtn(const Btn &b, bool active) {
    gfx->fillRoundRect(b.x, b.y, b.w, b.h, 12, active ? C_ACC : C_CARD);
    tC(b.x + b.w / 2, b.y + b.h / 2, b.label, &FreeSansBold12pt7b, active ? C_BG : C_FG);
}
static void flash(const Btn &b) { drawBtn(b, true); delay(110); drawBtn(b, false); }  // momentary-press feedback
static void metric(int16_t x, int16_t baseline, const char *lab, const char *val, uint16_t vcol) {
    gfx->fillRect(x, baseline - 20, 228, 28, C_BG);
    tL(x,       baseline, lab, &FreeSans12pt7b, C_MUT);
    tL(x + 120, baseline, val, &FreeSans12pt7b, vcol);
}
static void header() { tL(14, 36, "Orcon", &FreeSansBold18pt7b, C_FG); }
static void drawDots() {
    int16_t cx0 = 240 - (NUM_PAGES - 1) * 11;
    for (int i = 0; i < NUM_PAGES; i++)
        gfx->fillCircle(cx0 + i * 22, 466, 5, i == g_page ? C_ACC : C_CARD);
}
static void refreshMode() {
    gfx->fillRect(250, 12, 218, 38, C_BG);
    tC(362, 30, orcon::modeName(orcon::state().mode), &FreeSansBold18pt7b, C_ACC);
}

// ---- page 0 -----------------------------------------------------------------
static void drawControl() {
    header();
    for (int i = 0; i < 6; i++) drawBtn(modeBtns[i], strcmp(modeBtns[i].arg, selMode) == 0);
    for (int i = 0; i < 3; i++) drawBtn(bypBtns[i], false);
}
static void refreshControl() {
    const orcon::State &s = orcon::state();
    char b[20];
    refreshMode();
    snprintf(b, sizeof(b), "%.1fC", s.tempIndoor);  metric(14, 88,  "Indoor",  b, C_FG);
    snprintf(b, sizeof(b), "%.1fC", s.tempOutdoor); metric(14, 120, "Outdoor", b, C_FG);
    snprintf(b, sizeof(b), "%u%%",  s.supplyPct);   metric(14, 152, "Supply",  b, C_ACC);
    snprintf(b, sizeof(b), "%u%%",  s.exhaustPct);  metric(14, 184, "Exhaust", b, C_EXH);
    snprintf(b, sizeof(b), "%u",    s.co2);         metric(248, 88,  "CO2", b, C_FG);
    snprintf(b, sizeof(b), "%u%%",  s.humidity);    metric(248, 120, "RH",  b, C_FG);
    metric(248, 152, "Bypass", s.bypassOpen ? "open" : "closed", s.bypassOpen ? C_OK : C_MUT);
    snprintf(b, sizeof(b), "%ddBm", (int)radio::lastRssi()); metric(248, 184, "RF", b, C_MUT);
    const char *m = orcon::modeName(s.mode);
    if (strcmp(m, lastConfirmed) != 0) {       // fan's confirmed mode actually changed
        snprintf(lastConfirmed, sizeof(lastConfirmed), "%s", m);
        snprintf(selMode, sizeof(selMode), "%s", m);   // adopt it as the lit button
        for (int i = 0; i < 6; i++) drawBtn(modeBtns[i], strcmp(modeBtns[i].arg, selMode) == 0);
    }
}
static void hitControl(int16_t x, int16_t y) {
    for (int i = 0; i < 6; i++) { const Btn &b = modeBtns[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
            orcon::setMode(orcon::modeFromName(b.arg));
            snprintf(selMode, sizeof(selMode), "%s", b.arg);   // light it and keep it lit
            for (int j = 0; j < 6; j++) drawBtn(modeBtns[j], j == i);
            return; } }
    for (int i = 0; i < 3; i++) { const Btn &b = bypBtns[i];
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
            flash(b);
            uint8_t v = 0xFF;
            if (!strcmp(b.arg, "open")) v = 0xC8; else if (!strcmp(b.arg, "close")) v = 0x00;
            orcon::setBypass(v); return; } }
}

// ---- page 1 -----------------------------------------------------------------
static void drawProfVal() {
    char b[8];
    gfx->fillRect(250, 120, 108, 56, C_BG); snprintf(b, sizeof(b), "%d%%", acSup);
    tC(304, 148, b, &FreeSansBold18pt7b, C_ACC);
    gfx->fillRect(250, 196, 108, 56, C_BG); snprintf(b, sizeof(b), "%d%%", acExh);
    tC(304, 224, b, &FreeSansBold18pt7b, C_EXH);
}
static void drawProfiles() {
    header();
    tL(14, 84,  "Custom AC profile", &FreeSans12pt7b, C_MUT);
    tL(14, 156, "Supply",  &FreeSans12pt7b, C_FG);
    tL(14, 232, "Exhaust", &FreeSans12pt7b, C_FG);
    for (unsigned i = 0; i < sizeof(profBtns) / sizeof(profBtns[0]); i++) drawBtn(profBtns[i], false);
    drawProfVal();
}
static void hitProfiles(int16_t x, int16_t y) {
    for (unsigned i = 0; i < sizeof(profBtns) / sizeof(profBtns[0]); i++) {
        const Btn &b = profBtns[i];
        if (!(x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h)) continue;
        flash(b);
        const char *a = b.arg;
        if      (!strcmp(a, "s-")) { acSup = acSup > 0   ? acSup - 5 : 0;   drawProfVal(); }
        else if (!strcmp(a, "s+")) { acSup = acSup < 100 ? acSup + 5 : 100; drawProfVal(); }
        else if (!strcmp(a, "e-")) { acExh = acExh > 0   ? acExh - 5 : 0;   drawProfVal(); }
        else if (!strcmp(a, "e+")) { acExh = acExh < 100 ? acExh + 5 : 100; drawProfVal(); }
        else if (!strcmp(a, "apply"))   orcon::setAcCustom((uint8_t)acSup, (uint8_t)acExh, true);
        else if (!strcmp(a, "exhaust")) orcon::setAcMode(1);
        else if (!strcmp(a, "supply"))  orcon::setAcMode(2);
        else if (!strcmp(a, "off"))     orcon::setAcMode(0);
        return;
    }
}

// ---- page 2: airflow house --------------------------------------------------
// A house with two animated ducts on the left wall: supply (blue, outside->house)
// and exhaust (orange, house->outside). Dot speed tracks the live fan %.
static float g_phaseS = 0, g_phaseE = 0;
static void drawFlow(int16_t lo, int16_t hi, int16_t y, int dir, uint8_t pct,
                     uint16_t col, float &phase) {
    gfx->fillRect(lo - 4, y - 8, (hi - lo) + 16, 16, C_BG);     // clear the channel strip
    if (dir > 0) gfx->fillTriangle(hi, y - 6, hi, y + 6, hi + 8, y, col);   // arrow into house
    else         gfx->fillTriangle(lo, y - 6, lo, y + 6, lo - 8, y, col);   // arrow out of house
    if (pct == 0) return;
    float len = hi - lo;
    phase += pct * 0.04f;                       // travel speed ~ fan %
    if (phase > len) phase -= len;
    const int N = 4; float spacing = len / N;
    for (int i = 0; i < N; i++) {
        float t = fmodf(phase + i * spacing, len);
        int16_t px = (dir > 0) ? (int16_t)(lo + t) : (int16_t)(hi - t);
        gfx->fillCircle(px, y, 3, col);
    }
}
static void houseAnim() {
    const orcon::State &s = orcon::state();
    drawFlow(8, 78, 255, +1, s.supplyPct,  C_ACC, g_phaseS);
    drawFlow(8, 78, 340, -1, s.exhaustPct, C_EXH, g_phaseE);
}
static void drawHouse() {
    header();
    tL(14, 84, "Airflow", &FreeSans12pt7b, C_MUT);
    gfx->fillTriangle(74, 222, 190, 148, 306, 222, C_CARD);     // roof
    gfx->fillRoundRect(80, 222, 220, 150, 8, C_CARD);           // body
    tL(316, 236, "Supply",  &FreeSans12pt7b, C_MUT);
    tL(316, 322, "Exhaust", &FreeSans12pt7b, C_MUT);
}
static void refreshHouse() {
    const orcon::State &s = orcon::state(); char b[16];
    refreshMode();
    gfx->fillRect(6, 178, 76, 26, C_BG);
    snprintf(b, sizeof(b), "Out %.0fC", s.tempOutdoor); tL(8, 198, b, &FreeSans12pt7b, C_MUT);
    gfx->fillRect(82, 246, 216, 40, C_CARD);
    snprintf(b, sizeof(b), "%.1fC", s.tempIndoor);  tC(190, 272, b, &FreeSansBold18pt7b, C_FG);
    gfx->fillRect(82, 292, 216, 60, C_CARD);
    snprintf(b, sizeof(b), "%u ppm", s.co2);        tC(190, 312, b, &FreeSans12pt7b, C_MUT);
    snprintf(b, sizeof(b), "RH %u%%", s.humidity);  tC(190, 344, b, &FreeSans12pt7b, C_MUT);
    gfx->fillRect(312, 256, 160, 30, C_BG);
    snprintf(b, sizeof(b), "%u%%", s.supplyPct);    tL(316, 280, b, &FreeSansBold18pt7b, C_ACC);
    gfx->fillRect(312, 342, 160, 30, C_BG);
    snprintf(b, sizeof(b), "%u%%", s.exhaustPct);   tL(316, 366, b, &FreeSansBold18pt7b, C_EXH);
    gfx->fillRect(312, 182, 160, 24, C_BG);
    tL(316, 200, s.bypassOpen ? "Bypass open" : "Bypass closed",
       &FreeSans12pt7b, s.bypassOpen ? C_OK : C_MUT);
}

// ---- page 3: system info ----------------------------------------------------
static const char *SYS_LABELS[] = {"WiFi", "IP", "MQTT", "Radio", "RF", "IDs",
                                   "Heap", "Uptime", "Build"};
static void drawSystem() {
    header();
    tL(14, 84, "System", &FreeSans12pt7b, C_MUT);
    for (int i = 0; i < 9; i++)
        tL(14, 124 + i * 34, SYS_LABELS[i], &FreeSans12pt7b, C_MUT);
}
static void sysVal(int row, const char *val, uint16_t col) {
    int16_t y = 124 + row * 34;
    gfx->fillRect(150, y - 22, 320, 30, C_BG);
    tL(150, y, val, &FreeSans12pt7b, col);
}
static void refreshSystem() {
    const orcon::State &s = orcon::state(); const orcon::Ids &id = orcon::ids();
    char b[44];
    refreshMode();
    sysVal(0, net::ssidName(), C_FG);
    sysVal(1, net::ipAddr(), C_FG);
    if (net::mqttConnected()) sysVal(2, net::mqttHost(), C_OK);
    else { snprintf(b, sizeof(b), "%s (off)", net::mqttHost()); sysVal(2, b, C_MUT); }
    sysVal(3, radio::name(), C_FG);
    snprintf(b, sizeof(b), "%ddBm  rx%lu tx%lu", (int)radio::lastRssi(),
             (unsigned long)s.rxCount, (unsigned long)s.txCount);                sysVal(4, b, C_FG);
    snprintf(b, sizeof(b), "%u:%lu > %u:%lu", id.us.cls, (unsigned long)id.us.serial,
             id.fan.cls, (unsigned long)id.fan.serial);                          sysVal(5, b, C_FG);
    snprintf(b, sizeof(b), "%u KB free", (unsigned)(ESP.getFreeHeap() / 1024));  sysVal(6, b, C_FG);
    uint32_t up = millis() / 1000;
    snprintf(b, sizeof(b), "%lud %02lu:%02lu:%02lu", (unsigned long)(up / 86400),
             (unsigned long)((up % 86400) / 3600), (unsigned long)((up % 3600) / 60),
             (unsigned long)(up % 60));                                          sysVal(7, b, C_FG);
    sysVal(8, __DATE__, C_MUT);
}

// ---- page dispatch + input --------------------------------------------------
static void refreshPage() {
    switch (g_page) {
        case 0: refreshControl(); break;
        case 1: refreshMode();    break;   // custom AC page: keep the mode label fresh
        case 2: refreshHouse();   break;
        case 3: refreshSystem();  break;
    }
}
static void drawPage() {
    gfx->fillScreen(C_BG);
    lastConfirmed[0] = '\0';                  // re-sync the highlight to the live mode
    switch (g_page) {
        case 0: drawControl();  break;
        case 1: drawProfiles(); break;
        case 2: drawHouse();    break;
        case 3: drawSystem();   break;
    }
    drawDots();
    refreshPage();
}

static uint32_t g_lastDraw = 0;
static uint32_t g_animMs   = 0;
static uint32_t g_lastTouchMs = 0;
static bool     g_saver = false;        // screensaver (airflow) currently showing
static bool     g_wake  = false;        // swallow the gesture that woke the screen
static const uint32_t SCREENSAVER_MS = 60000;   // idle time before the airflow saver
static bool     g_down = false;
static int16_t  g_sx, g_sy, g_lx, g_ly;

void begin() {
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, LOW);   // backlight off during init
    gfx->begin();
    gfx->fillScreen(C_BG);
    touchBegin();
    drawPage();
    g_lastTouchMs = millis();
    digitalWrite(PIN_BL, HIGH);                            // backlight on
}

void loop() {
    uint32_t now = millis();
    touchPoll();
    if (g_tDown) {
        if (!g_down) {                                      // new touch begins
            g_down = true; g_sx = g_tX; g_sy = g_tY;
            g_lastTouchMs = now;
            if (g_saver) {                                  // wake -> jump to control page
                g_saver = false; g_page = 0; drawPage();
                g_wake = true;                              // ignore this whole gesture
            }
        }
        g_lx = g_tX; g_ly = g_tY;
    } else if (g_down) {
        g_down = false;
        if (g_wake) {
            g_wake = false;                                 // the tap only woke the screen
        } else {
            int dx = g_lx - g_sx, dy = g_ly - g_sy;
            if (abs(dx) > 70 && abs(dx) > abs(dy)) {        // horizontal swipe -> change page
                if (dx < 0 && g_page < NUM_PAGES - 1) g_page++;
                else if (dx > 0 && g_page > 0)        g_page--;
                drawPage();
            } else if (abs(dx) < 25 && abs(dy) < 25) {      // tap -> hit-test
                if (g_sx >= 0 && g_sx < 480 && g_sy >= 0 && g_sy < 480) {
                    if      (g_page == 0) hitControl(g_sx, g_sy);
                    else if (g_page == 1) hitProfiles(g_sx, g_sy);
                    // pages 2 (airflow) and 3 (system) are read-only
                }
            }
        }
    }
    // after a minute of no touch, drop into the airflow "screensaver"
    if (!g_saver && now - g_lastTouchMs > SCREENSAVER_MS) {
        g_saver = true; g_page = 2; drawPage();
    }
    if (g_page == 2 && now - g_animMs > 110) { g_animMs = now; houseAnim(); }
    if (now - g_lastDraw > 700) { g_lastDraw = now; refreshPage(); }
}

} // namespace display

#else  // not BOARD_4848S040 -> no-op stubs
namespace display { void begin() {} void loop() {} }
#endif
