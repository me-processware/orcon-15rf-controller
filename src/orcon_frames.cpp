// orcon_frames.cpp — PURE RAMSES frame builders for the Orcon 15RF emulation.
// Addressing & payloads verified against real packet captures from a working
// Orcon remote (ramses_cc issue #178, sandervandegeijn/OrconRamsesRFCommand).
#include "orcon_frames.h"
#include <string.h>

namespace orcon {

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Away:    return "away";
        case Mode::Low:     return "low";
        case Mode::Medium:  return "medium";
        case Mode::High:    return "high";
        case Mode::Auto:    return "auto";
        case Mode::Auto2:   return "auto2";
        case Mode::Boost:   return "boost";
        case Mode::Disable: return "disable";
        default:            return "unknown";
    }
}

Mode modeFromName(const char* s) {
    if (!s) return Mode::Unknown;
    struct { const char* n; Mode m; } tbl[] = {
        {"away",Mode::Away},{"low",Mode::Low},{"medium",Mode::Medium},
        {"high",Mode::High},{"auto",Mode::Auto},{"auto2",Mode::Auto2},
        {"boost",Mode::Boost},{"disable",Mode::Disable},
        {"1",Mode::Low},{"2",Mode::Medium},{"3",Mode::High},
    };
    for (auto& e : tbl) if (strcmp(e.n, s) == 0) return e.m;
    return Mode::Unknown;
}

// --- addressing helpers (verified from captures) ----------------------------
// To the fan:        I --- us 32:fan --:------  (addr0 + addr1, addr2 absent)
static ramses::Frame toFan(const Ids& id, ramses::MsgType t, uint16_t code) {
    ramses::Frame f;
    f.type = t;
    f.has_addr[0] = true;  f.addr[0] = id.us;
    f.has_addr[1] = true;  f.addr[1] = id.fan;
    f.has_addr[2] = false;
    f.cmd = code;
    return f;
}
// Self broadcast:    I --- us --:------ us       (addr0 + addr2, addr1 absent)
static ramses::Frame selfBcast(const Ids& id, ramses::MsgType t, uint16_t code) {
    ramses::Frame f;
    f.type = t;
    f.has_addr[0] = true;  f.addr[0] = id.us;
    f.has_addr[1] = false;
    f.has_addr[2] = true;  f.addr[2] = id.us;
    f.cmd = code;
    return f;
}

// 22F1 fan mode:  I --- us 32:fan --:------ 22F1 003 00 NN 07
ramses::Frame make22F1(const Ids& id, Mode m) {
    ramses::Frame f = toFan(id, ramses::MsgType::I, 0x22F1);
    f.len = 3;
    f.payload[0] = 0x00; f.payload[1] = (uint8_t)m; f.payload[2] = 0x07;
    return f;
}

// 22F3 timed boost (high for N min):
//   I --- us 32:fan --:------ 22F3 007 00 12 <min> 03 04 04 04
ramses::Frame make22F3(const Ids& id, uint16_t minutes) {
    ramses::Frame f = toFan(id, ramses::MsgType::I, 0x22F3);
    f.len = 7;
    f.payload[0] = 0x00;
    f.payload[1] = 0x12;
    f.payload[2] = (uint8_t)(minutes > 255 ? 255 : minutes);  // 15/30/60
    f.payload[3] = 0x03;                                       // 03 = high
    f.payload[4] = 0x04; f.payload[5] = 0x04; f.payload[6] = 0x04;
    return f;
}

// 31E0 vent demand:
//   I --- us 32:fan --:------ 31E0 008 00 00 00 00 01 00 <pct> 00
ramses::Frame make31E0(const Ids& id, uint8_t percent) {
    if (percent > 100) percent = 100;
    ramses::Frame f = toFan(id, ramses::MsgType::I, 0x31E0);
    f.len = 8;
    f.payload[0]=0x00; f.payload[1]=0x00; f.payload[2]=0x00; f.payload[3]=0x00;
    f.payload[4]=0x01; f.payload[5]=0x00; f.payload[6]=percent; f.payload[7]=0x00;
    return f;
}

// 1298 CO2 (ppm):  I --- us --:------ us 1298 003 00 <hi> <lo>
ramses::Frame make1298(const Ids& id, uint16_t ppm) {
    ramses::Frame f = selfBcast(id, ramses::MsgType::I, 0x1298);
    f.len = 3;
    f.payload[0] = 0x00;
    f.payload[1] = (uint8_t)(ppm >> 8);
    f.payload[2] = (uint8_t)(ppm & 0xFF);
    return f;
}

// 10E0 device info (self broadcast). OEM blob + an ASCII descriptor.
ramses::Frame make10E0(const Ids& id) {
    ramses::Frame f = selfBcast(id, ramses::MsgType::I, 0x10E0);
    static const uint8_t info[] = {
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x01,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x4F,0x52,0x43,0x4F,0x4E,0x2D,0x31,0x35,0x52,0x46  // "ORCON-15RF"
    };
    f.len = (uint8_t)sizeof(info);
    if (f.len > ramses::MAX_PAYLOAD) f.len = ramses::MAX_PAYLOAD;
    for (uint8_t i = 0; i < f.len; i++) f.payload[i] = info[i];
    return f;
}

// 1FC9 RF bind offer (self broadcast). Tuples of [idx][code-hi][code-lo][id0..2]
// (6 bytes each) for each code we will send. idx 0x00 for our HVAC domain.
ramses::Frame make1FC9_bind(const Ids& id) {
    ramses::Frame f = selfBcast(id, ramses::MsgType::I, 0x1FC9);
    uint32_t r = id.us.raw();
    uint8_t i0=(uint8_t)((r>>16)&0xFF), i1=(uint8_t)((r>>8)&0xFF), i2=(uint8_t)(r&0xFF);
    const uint16_t codes[] = { 0x22F1, 0x31E0, 0x1298, 0x10E0 };
    uint8_t n = 0;
    for (uint16_t c : codes) {
        f.payload[n++] = 0x00;                  // domain/index
        f.payload[n++] = (uint8_t)(c >> 8);
        f.payload[n++] = (uint8_t)(c & 0xFF);
        f.payload[n++] = i0; f.payload[n++] = i1; f.payload[n++] = i2;
    }
    f.len = n;                                   // 4 codes * 6 = 24 bytes
    return f;
}

// RQ <code> to the fan (e.g. 31DA status, 12A0 humidity).
ramses::Frame makeRequest(const Ids& id, uint16_t code) {
    ramses::Frame f = toFan(id, ramses::MsgType::RQ, code);
    f.len = 1;
    f.payload[0] = 0x00;
    return f;
}

// 22F7 bypass:  W --- us 32:fan --:------ 22F7 003 00 <val> EF
// val: 0xC8 = open, 0x00 = closed, 0xFF = auto.
ramses::Frame make22F7(const Ids& id, uint8_t val) {
    ramses::Frame f = toFan(id, ramses::MsgType::W, 0x22F7);
    f.len = 3;
    f.payload[0] = 0x00; f.payload[1] = val; f.payload[2] = 0xEF;
    return f;
}

// 2411 fan-speed parameter write (per-preset supply/exhaust %, persistent).
// 23-byte payloads captured verbatim from a real Orcon remote.
// paramIndex: 0 low-sup 1 low-exh 2 med-sup 3 med-exh 4 high-sup 5 high-exh 6 boost
ramses::Frame makeFanSpeed(const Ids& id, uint8_t paramIndex, uint8_t percent) {
    static const uint8_t TMPL[7][23] = {
      {0x00,0x00,0x3F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x00,0x00,0x00,0x01,0x00,0x32},
      {0x00,0x00,0x40,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x00,0x00,0x00,0x01,0x00,0x32},
      {0x00,0x00,0x41,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC8,0x00,0x00,0x00,0x01,0x00,0x32},
      {0x00,0x00,0x42,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0x00,0xC8,0x00,0x00,0x00,0x01,0x00,0x32},
      {0x00,0x00,0x43,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC8,0x00,0x00,0x00,0x01,0x00,0x32},
      {0x00,0x00,0x44,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0x00,0xC8,0x00,0x00,0x00,0x01,0x00,0x32},
      {0x00,0x00,0x95,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC8,0x00,0x00,0x00,0x01,0x00,0x32},
    };
    if (paramIndex > 6) paramIndex = 6;
    if (percent > 100)  percent = 100;
    ramses::Frame f = toFan(id, ramses::MsgType::W, 0x2411);
    f.len = 23;
    for (int i = 0; i < 23; i++) f.payload[i] = TMPL[paramIndex][i];
    f.payload[8] = (uint8_t)(percent * 2);
    return f;
}

} // namespace orcon
