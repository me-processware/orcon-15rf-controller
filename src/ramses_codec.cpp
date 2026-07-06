// ramses_codec.cpp — see ramses_codec.h
#include "ramses_codec.h"

namespace ramses {

// ---------- small helpers ----------------------------------------------------
static int manchIndex(uint8_t wire) {
    for (int i = 0; i < 16; i++) if (MANCH_ENC[i] == wire) return i;
    return -1;
}

uint8_t headerForType(MsgType t, bool a0, bool a1, bool a2, bool p0, bool p1) {
    uint8_t flags = 0;
    switch (t) {
        case MsgType::I:  flags |= F_I;  break;
        case MsgType::RQ: flags |= F_RQ; break;
        case MsgType::RP: flags |= F_RP; break;
        case MsgType::W:  flags |= F_W;  break;
    }
    if (a0) flags |= F_ADDR0;
    if (a1) flags |= F_ADDR1;
    if (a2) flags |= F_ADDR2;
    // find flags in HEADER_FLAGS table
    int idx = -1;
    for (int i = 0; i < 16; i++) if (HEADER_FLAGS[i] == flags) { idx = i; break; }
    if (idx < 0) idx = 4;  // fall back to I+3addr
    uint8_t h = (uint8_t)(idx << 2);
    if (p0) h |= 0x02;
    if (p1) h |= 0x01;
    return h;
}

// ---------- Layer A: Frame <-> logical --------------------------------------
size_t buildLogical(const Frame& f, uint8_t* out, size_t cap) {
    uint8_t buf[MAX_LOGICAL];
    size_t n = 0;
    bool a0 = f.has_addr[0], a1 = f.has_addr[1], a2 = f.has_addr[2];
    buf[n++] = headerForType(f.type, a0, a1, a2, f.has_param0, f.has_param1);
    for (int i = 0; i < 3; i++) {
        if (!f.has_addr[i]) continue;
        uint32_t r = f.addr[i].raw();
        buf[n++] = (uint8_t)((r >> 16) & 0xFF);
        buf[n++] = (uint8_t)((r >> 8)  & 0xFF);
        buf[n++] = (uint8_t)(r & 0xFF);
    }
    if (f.has_param0) buf[n++] = f.param0;
    if (f.has_param1) buf[n++] = f.param1;
    buf[n++] = (uint8_t)((f.cmd >> 8) & 0xFF);
    buf[n++] = (uint8_t)(f.cmd & 0xFF);
    buf[n++] = f.len;
    for (uint8_t i = 0; i < f.len; i++) buf[n++] = f.payload[i];
    // checksum: total incl. checksum == 0 (mod 256)
    uint8_t sum = 0;
    for (size_t i = 0; i < n; i++) sum = (uint8_t)(sum + buf[i]);
    buf[n++] = (uint8_t)((256 - sum) & 0xFF);
    if (n > cap) return 0;
    for (size_t i = 0; i < n; i++) out[i] = buf[i];
    return n;
}

bool parseLogical(const uint8_t* d, size_t n, Frame& f) {
    if (n < 5) return false;
    // verify checksum
    uint8_t sum = 0;
    for (size_t i = 0; i < n; i++) sum = (uint8_t)(sum + d[i]);
    if (sum != 0) return false;

    size_t p = 0;
    uint8_t header = d[p++];
    uint8_t flags  = HEADER_FLAGS[(header >> 2) & 0x0F];
    if      (flags & F_I)  f.type = MsgType::I;
    else if (flags & F_RQ) f.type = MsgType::RQ;
    else if (flags & F_RP) f.type = MsgType::RP;
    else if (flags & F_W)  f.type = MsgType::W;

    f.has_addr[0] = flags & F_ADDR0;
    f.has_addr[1] = flags & F_ADDR1;
    f.has_addr[2] = flags & F_ADDR2;
    for (int i = 0; i < 3; i++) {
        if (!f.has_addr[i]) { f.addr[i] = DeviceId(); continue; }
        if (p + 3 > n) return false;
        uint32_t r = ((uint32_t)d[p] << 16) | ((uint32_t)d[p+1] << 8) | d[p+2];
        p += 3;
        f.addr[i] = DeviceId::fromRaw(r);
    }
    f.has_param0 = header & 0x02;
    f.has_param1 = header & 0x01;
    if (f.has_param0) { if (p >= n) return false; f.param0 = d[p++]; }
    if (f.has_param1) { if (p >= n) return false; f.param1 = d[p++]; }
    if (p + 3 > n) return false;
    f.cmd = ((uint16_t)d[p] << 8) | d[p+1]; p += 2;
    f.len = d[p++];
    if (f.len > MAX_PAYLOAD) return false;
    if (p + f.len + 1 > n) return false;        // +1 for checksum
    for (uint8_t i = 0; i < f.len; i++) f.payload[i] = d[p++];
    return true;
}

// ---------- Layer B: logical <-> wire ---------------------------------------
size_t wireFromLogical(const uint8_t* logical, size_t n, uint8_t* out, size_t cap) {
    size_t w = 0;
    if (5 + 2*n + 1 > cap) return 0;
    for (int i = 0; i < 5; i++) out[w++] = SYNC[i];
    for (size_t i = 0; i < n; i++) {
        out[w++] = MANCH_ENC[(logical[i] >> 4) & 0x0F];
        out[w++] = MANCH_ENC[logical[i] & 0x0F];
    }
    out[w++] = STOP_WIRE;
    return w;
}

size_t logicalFromWire(const uint8_t* wire, size_t n, uint8_t* out, size_t cap) {
    if (n < 6) return 0;
    for (int i = 0; i < 5; i++) if (wire[i] != SYNC[i]) return 0;
    size_t i = 5, o = 0;
    while (i + 1 < n) {
        if (wire[i] == STOP_WIRE) break;
        int hi = manchIndex(wire[i]);
        int lo = manchIndex(wire[i+1]);
        if (hi < 0 || lo < 0) return 0;
        if (o >= cap) return 0;
        out[o++] = (uint8_t)((hi << 4) | lo);
        i += 2;
    }
    return o;
}

// ---------- Layer C: bit packing --------------------------------------------
// MSB-first bit accumulator into a byte buffer.
struct BitWriter {
    uint8_t* buf; size_t cap; size_t bits = 0;
    BitWriter(uint8_t* b, size_t c) : buf(b), cap(c) {}
    bool put(int bit) {
        size_t byte = bits >> 3; int off = 7 - (int)(bits & 7);
        if (byte >= cap) return false;
        if (bit) buf[byte] |= (uint8_t)(1 << off);
        else     buf[byte] &= (uint8_t)~(1 << off);
        bits++;
        return true;
    }
    // UART-frame one wire byte: start(0) + 8 data LSB-first + stop(1)
    bool putFramed(uint8_t v) {
        if (!put(0)) return false;
        for (int i = 0; i < 8; i++) if (!put((v >> i) & 1)) return false;
        return put(1);
    }
};

static int getBit(const uint8_t* buf, size_t idx) {
    return (buf[idx >> 3] >> (7 - (int)(idx & 7))) & 1;
}

size_t encodeOnAir(const uint8_t* wire, size_t n, uint8_t* out, size_t cap,
                   int preamble_bytes, size_t* bit_len) {
    for (size_t i = 0; i < cap; i++) out[i] = 0;
    BitWriter bw(out, cap);
    for (int i = 0; i < preamble_bytes; i++) if (!bw.putFramed(PREAMBLE_BYTE)) return 0;
    for (size_t i = 0; i < n; i++) if (!bw.putFramed(wire[i])) return 0;
    // pad final partial byte with stop-level 1s (idle high)
    while (bw.bits & 7) bw.put(1);
    if (bit_len) *bit_len = bw.bits;
    return (bw.bits + 7) >> 3;
}

// deframe a 10-bit group at bit offset; return wire byte or -1 if framing bad.
static int deframeAt(const uint8_t* buf, size_t total_bits, size_t off) {
    if (off + 10 > total_bits) return -1;
    if (getBit(buf, off) != 0) return -1;          // start bit
    if (getBit(buf, off + 9) != 1) return -1;       // stop bit
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) if (getBit(buf, off + 1 + i)) v |= (uint8_t)(1 << i);
    return v;
}

size_t decodeOnAir(const uint8_t* onair, size_t n_bytes, size_t n_bits,
                   uint8_t* wire_out, size_t cap) {
    if (n_bits == 0) n_bits = n_bytes * 8;
    // Build the expected framed-SYNC bit pattern (50 bits).
    uint8_t syncbits[8] = {0};
    BitWriter sw(syncbits, sizeof(syncbits));
    for (int i = 0; i < 5; i++) sw.putFramed(SYNC[i]);
    size_t synclen = sw.bits;                       // 50

    // slide to find the sync pattern
    for (size_t start = 0; start + synclen <= n_bits; start++) {
        bool match = true;
        for (size_t b = 0; b < synclen; b++)
            if (getBit(onair, start + b) != getBit(syncbits, b)) { match = false; break; }
        if (!match) continue;

        // found: emit SYNC then deframe wire bytes until STOP
        size_t w = 0;
        if (w + 5 > cap) return 0;
        for (int i = 0; i < 5; i++) wire_out[w++] = SYNC[i];
        size_t off = start + synclen;
        while (off + 10 <= n_bits) {
            int v = deframeAt(onair, n_bits, off);
            if (v < 0) break;
            if (w >= cap) return 0;
            wire_out[w++] = (uint8_t)v;
            off += 10;
            if (v == STOP_WIRE) return w;
        }
        return w;   // ran out / no explicit stop
    }
    return 0;
}

// ---------- convenience ------------------------------------------------------
size_t encodeFrame(const Frame& f, uint8_t* out, size_t cap,
                   int preamble_bytes, size_t* bit_len) {
    uint8_t logical[MAX_LOGICAL];
    size_t ln = buildLogical(f, logical, sizeof(logical));
    if (!ln) return 0;
    uint8_t wire[MAX_WIRE];
    size_t wn = wireFromLogical(logical, ln, wire, sizeof(wire));
    if (!wn) return 0;
    return encodeOnAir(wire, wn, out, cap, preamble_bytes, bit_len);
}

bool decodeFrame(const uint8_t* onair, size_t n_bytes, size_t n_bits, Frame& f) {
    uint8_t wire[MAX_WIRE];
    size_t wn = decodeOnAir(onair, n_bytes, n_bits, wire, sizeof(wire));
    if (!wn) return false;
    uint8_t logical[MAX_LOGICAL];
    size_t ln = logicalFromWire(wire, wn, logical, sizeof(logical));
    if (!ln) return false;
    return parseLogical(logical, ln, f);
}

bool extractWireFrame(const uint8_t* s, size_t n, size_t* consumed, Frame& f) {
    for (size_t i = 0; i + 5 <= n; i++) {
        bool sync = true;
        for (int k = 0; k < 5; k++) if (s[i + k] != SYNC[k]) { sync = false; break; }
        if (!sync) continue;
        // SYNC found at i; need a STOP marker to have a complete frame.
        size_t stop = 0; bool haveStop = false;
        for (size_t j = i + 5; j < n; j++) { if (s[j] == STOP_WIRE) { stop = j; haveStop = true; break; } }
        if (!haveStop) { if (consumed) *consumed = i; return false; }   // wait for more bytes
        uint8_t logical[MAX_LOGICAL];
        size_t ln = logicalFromWire(&s[i], stop - i + 1, logical, sizeof(logical));
        if (ln && parseLogical(logical, ln, f)) { if (consumed) *consumed = stop + 1; return true; }
        // bad/garbled frame: drop through this SYNC and keep scanning.
        i = stop;   // skip to the STOP we found
    }
    // No frame: drop everything but a possible partial SYNC at the tail.
    if (consumed) *consumed = (n >= 4) ? n - 4 : 0;
    return false;
}

} // namespace ramses
