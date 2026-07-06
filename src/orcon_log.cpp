// orcon_log.cpp — see orcon_log.h
#include "orcon_log.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace rlog {

struct Entry {
    uint32_t t;
    char     dir;
    int16_t  rssi;
    char     str[96];
};

static Entry  g_buf[CAP];
static size_t g_head = 0;
static size_t g_count = 0;
static Seen   g_seen;

static const char* msgType(ramses::MsgType t) {
    switch (t) {
        case ramses::MsgType::I:  return "I ";
        case ramses::MsgType::RQ: return "RQ";
        case ramses::MsgType::RP: return "RP";
        case ramses::MsgType::W:  return "W ";
    }
    return "??";
}

static void addrStr(const ramses::Frame& f, int i, char* out) {
    if (f.has_addr[i])
        snprintf(out, 12, "%02u:%06lu", f.addr[i].cls, (unsigned long)f.addr[i].serial);
    else
        strcpy(out, "--:------");
}

size_t format(const ramses::Frame& f, char* out, size_t cap) {
    char a0[12], a1[12], a2[12];
    addrStr(f, 0, a0); addrStr(f, 1, a1); addrStr(f, 2, a2);
    int n = snprintf(out, cap, "%s --- %s %s %s %04X %03u ",
                     msgType(f.type), a0, a1, a2, f.cmd, f.len);
    for (uint8_t i = 0; i < f.len && (size_t)n < cap - 3; i++)
        n += snprintf(out + n, cap - n, "%02X", f.payload[i]);
    return (size_t)n;
}

static void learn(const ramses::Frame& f) {
    // Class 0x20 = FAN; controllers are class 0x1D (29, REM) or 0x25 (37, CO2 /
    // 15RF display). Capture both so the UI can offer "clone this controller".
    for (int i = 0; i < 3; i++) {
        if (!f.has_addr[i]) continue;
        uint8_t c = f.addr[i].cls;
        if (c == 0x20)                   { g_seen.fan    = f.addr[i]; g_seen.haveFan    = true; }
        else if (c == 0x1D || c == 0x25) { g_seen.remote = f.addr[i]; g_seen.haveRemote = true; }
    }
}

void add(char dir, const ramses::Frame& f, int rssi) {
    Entry& e = g_buf[g_head];
    e.t = millis();
    e.dir = dir;
    e.rssi = (int16_t)rssi;
    format(f, e.str, sizeof(e.str));
    g_head = (g_head + 1) % CAP;
    if (g_count < CAP) g_count++;
    if (dir == 'R') learn(f);
}

size_t toJson(char* out, size_t cap) {
    size_t n = 0;
    out[n++] = '[';
    size_t start = (g_head + CAP - g_count) % CAP;
    for (size_t k = 0; k < g_count; k++) {
        size_t idx = (start + k) % CAP;
        Entry& e = g_buf[idx];
        if (k) out[n++] = ',';
        n += snprintf(out + n, cap - n,
                      "{\"t\":%lu,\"d\":\"%c\",\"r\":%d,\"s\":\"%s\"}",
                      (unsigned long)e.t, e.dir, e.rssi, e.str);
        if (n > cap - 120) break;
    }
    out[n++] = ']';
    out[n] = '\0';
    return n;
}

const Seen& seen() { return g_seen; }

} // namespace rlog
