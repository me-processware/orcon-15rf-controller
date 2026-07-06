// ramses_codec.h — RAMSES-II / Orcon 15RF codec (radio-independent, host-testable)
//
// Pure C++ (only <stdint.h>/<stddef.h>) so it compiles both in the firmware and
// in a native unit test. No Arduino, no radio dependencies here.
//
// Three layers (see docs/PROTOCOL.md):
//   A. Frame  <-> logical bytes   (header, addrs, params, cmd, len, payload, csum)
//   B. logical <-> wire bytes     (SYNC prefix + Manchester nibble pairs + STOP)
//   C. wire   <-> on-air bitstream(UART 10-bit framing + preamble, packed MSB-first)
//
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ramses {

// ---- on-air constants -------------------------------------------------------
static const uint8_t  PREAMBLE_BYTE = 0x55;          // alternating training byte
static const uint8_t  STOP_WIRE     = 0x35;          // end-of-packet wire marker
static const uint8_t  SYNC[5]       = {0xFF,0x00,0x33,0x55,0x53};

// Manchester nibble table: MANCH_ENC[nibble] -> wire byte (and reverse via find)
static const uint8_t  MANCH_ENC[16] = {
    0xAA,0xA9,0xA6,0xA5, 0x9A,0x99,0x96,0x95,
    0x6A,0x69,0x66,0x65, 0x5A,0x59,0x56,0x55 };

// Header-flag lookup (index = (header>>2)&0x0F)
static const uint8_t  HEADER_FLAGS[16] = {
    0x0F,0x0C,0x0D,0x0B, 0x27,0x24,0x25,0x23,
    0x47,0x44,0x45,0x43, 0x17,0x14,0x15,0x13 };

// flag bits
enum {
    F_ADDR0 = 0x01, F_ADDR1 = 0x02, F_ADDR2 = 0x04,
    F_RQ    = 0x08, F_RP    = 0x10, F_I     = 0x20, F_W = 0x40
};

enum class MsgType : uint8_t { I, RQ, RP, W };

static const size_t MAX_PAYLOAD = 64;
static const size_t MAX_LOGICAL = 96;
static const size_t MAX_WIRE    = 5 + 2*MAX_LOGICAL + 1;     // sync + manch + stop
static const size_t MAX_ONAIR   = 32 + (MAX_WIRE+8)*2;       // generous byte buffer

struct DeviceId {
    uint8_t  cls    = 0;        // 6-bit device class
    uint32_t serial = 0;        // 18-bit serial
    // Explicit constructors so `DeviceId{cls, serial}` works under -std=gnu++11
    // (the ESP32 core's default). With NSDMI present the type is not an
    // aggregate pre-C++14, so brace-init needs a real constructor.
    DeviceId() = default;
    DeviceId(uint8_t c, uint32_t s) : cls(c), serial(s) {}
    uint32_t raw() const { return ((uint32_t)(cls & 0x3F) << 18) | (serial & 0x3FFFF); }
    static DeviceId fromRaw(uint32_t r) {
        DeviceId d; d.cls = (uint8_t)((r >> 18) & 0x3F); d.serial = r & 0x3FFFF; return d;
    }
    bool operator==(const DeviceId& o) const { return cls==o.cls && serial==o.serial; }
};

struct Frame {
    MsgType  type = MsgType::I;
    bool     has_addr[3] = { true, true, true };
    DeviceId addr[3];
    bool     has_param0 = false, has_param1 = false;
    uint8_t  param0 = 0, param1 = 0;
    uint16_t cmd = 0;
    uint8_t  len = 0;
    uint8_t  payload[MAX_PAYLOAD] = {0};
};

// ---- Layer A: Frame <-> logical bytes --------------------------------------
// buildLogical: returns number of bytes written (incl. checksum), 0 on overflow.
size_t buildLogical(const Frame& f, uint8_t* out, size_t cap);
// parseLogical: validates checksum + structure. true on success.
bool   parseLogical(const uint8_t* data, size_t n, Frame& f);

// header byte helpers
uint8_t headerForType(MsgType t, bool a0, bool a1, bool a2, bool p0, bool p1);

// ---- Layer B: logical bytes <-> wire bytes ---------------------------------
// wireFromLogical: SYNC + manchester(logical) + STOP. returns wire length.
size_t wireFromLogical(const uint8_t* logical, size_t n, uint8_t* out, size_t cap);
// logicalFromWire: input begins at SYNC[0]. returns logical length, 0 on error.
size_t logicalFromWire(const uint8_t* wire, size_t n, uint8_t* out, size_t cap);
// Scan a stream of UART-recovered wire bytes (CC1101 async RX) for ONE frame.
// Finds SYNC[5], lets logicalFromWire read to STOP, parses it. Returns true and
// fills f on success. *consumed = #bytes the caller may drop from the front
// (keeps a partial SYNC / incomplete frame at the tail for the next call).
bool extractWireFrame(const uint8_t* s, size_t n, size_t* consumed, Frame& f);

// ---- Layer C: wire bytes <-> on-air bitstream ------------------------------
// encodeOnAir: full buffer incl. preamble, UART framing, packed MSB-first.
// returns number of *bytes* written; bit_len (optional) gets the exact bit count.
size_t encodeOnAir(const uint8_t* wire, size_t n, uint8_t* out, size_t cap,
                   int preamble_bytes = 6, size_t* bit_len = nullptr);
// decodeOnAir: software receiver over packed bytes. Finds SYNC, deframes,
// returns wire bytes (SYNC..STOP inclusive) so logicalFromWire can consume them.
// returns wire length, 0 if no valid frame found.
size_t decodeOnAir(const uint8_t* onair, size_t n_bytes, size_t n_bits,
                   uint8_t* wire_out, size_t cap);

// ---- convenience: Frame <-> full on-air ------------------------------------
size_t encodeFrame(const Frame& f, uint8_t* out, size_t cap,
                   int preamble_bytes = 6, size_t* bit_len = nullptr);
bool   decodeFrame(const uint8_t* onair, size_t n_bytes, size_t n_bits, Frame& f);

} // namespace ramses
