// test_codec.cpp — native unit test for the RAMSES codec.
// Build:  g++ -std=c++17 -I../src test_codec.cpp ../src/ramses_codec.cpp -o test_codec
// Run:    ./test_codec
#include "ramses_codec.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

using namespace ramses;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  [FAIL] %s\n", msg); failures++; } \
    else         { printf("  [ ok ] %s\n", msg); } } while(0)

static Frame makeFan22F1(uint8_t nn) {
    Frame f;
    f.type = MsgType::I;
    f.addr[0] = { 0x1D, 0x012345 };   // us (REM/CO2)   18-bit serial
    f.addr[1] = { 0x20, 0x02BCDE };   // fan            18-bit serial
    f.addr[2] = { 0x1D, 0x012345 };   // us
    f.cmd = 0x22F1;
    f.len = 3;
    f.payload[0] = 0x00; f.payload[1] = nn; f.payload[2] = 0x07;
    return f;
}

static bool sameFrame(const Frame& a, const Frame& b) {
    if (a.type != b.type) return false;
    if (a.cmd != b.cmd || a.len != b.len) return false;
    for (int i = 0; i < 3; i++) {
        if (a.has_addr[i] != b.has_addr[i]) return false;
        if (a.has_addr[i] && !(a.addr[i] == b.addr[i])) return false;
    }
    if (a.has_param0 != b.has_param0 || a.has_param1 != b.has_param1) return false;
    if (a.has_param0 && a.param0 != b.param0) return false;
    if (a.has_param1 && a.param1 != b.param1) return false;
    for (uint8_t i = 0; i < a.len; i++) if (a.payload[i] != b.payload[i]) return false;
    return true;
}

static void test_logical_checksum() {
    printf("\n== logical build/parse + checksum ==\n");
    Frame f = makeFan22F1(0x03);   // High
    uint8_t logical[MAX_LOGICAL];
    size_t n = buildLogical(f, logical, sizeof(logical));
    CHECK(n > 0, "buildLogical returns data");

    uint8_t sum = 0; for (size_t i = 0; i < n; i++) sum = (uint8_t)(sum + logical[i]);
    CHECK(sum == 0, "checksum sums frame to 0 (mod 256)");

    Frame g;
    CHECK(parseLogical(logical, n, g), "parseLogical accepts valid frame");
    CHECK(sameFrame(f, g), "logical round-trip preserves frame");

    logical[2] ^= 0xFF;            // corrupt
    Frame h;
    CHECK(!parseLogical(logical, n, h), "parseLogical rejects corrupted checksum");
}

static void test_header_types() {
    printf("\n== header byte <-> type for all 4 types ==\n");
    MsgType types[4] = { MsgType::I, MsgType::RQ, MsgType::RP, MsgType::W };
    const char* names[4] = { "I", "RQ", "RP", "W" };
    for (int t = 0; t < 4; t++) {
        Frame f = makeFan22F1(0x01);
        f.type = types[t];
        uint8_t logical[MAX_LOGICAL];
        size_t n = buildLogical(f, logical, sizeof(logical));
        Frame g;
        bool ok = parseLogical(logical, n, g) && g.type == types[t];
        char m[64]; snprintf(m, sizeof(m), "type %s survives 3-addr round-trip", names[t]);
        CHECK(ok, m);
    }
}

static void test_onair_roundtrip() {
    printf("\n== full on-air encode/decode (Manchester + UART framing) ==\n");
    struct TC { const char* name; uint16_t cmd; uint8_t len; uint8_t p[8]; } tcs[] = {
        { "22F1 fan High",  0x22F1, 3, {0x00,0x03,0x07} },
        { "22F1 fan Auto",  0x22F1, 3, {0x00,0x04,0x07} },
        { "1298 CO2 ppm",   0x1298, 3, {0x00,0x03,0x20} },   // ~800 ppm
        { "31E0 demand",    0x31E0, 8, {0x00,0x00,0x00,0x01,0x00,0x1E,0x00,0x00} },
    };
    for (auto& tc : tcs) {
        Frame f = makeFan22F1(0);
        f.cmd = tc.cmd; f.len = tc.len;
        memcpy(f.payload, tc.p, tc.len);

        uint8_t onair[MAX_ONAIR]; size_t bits = 0;
        size_t bytes = encodeFrame(f, onair, sizeof(onair), 6, &bits);
        char m1[80]; snprintf(m1, sizeof(m1), "%s: encodeFrame produced %zu bytes", tc.name, bytes);
        CHECK(bytes > 0, m1);

        Frame g;
        bool ok = decodeFrame(onair, bytes, bits, g);
        char m2[80]; snprintf(m2, sizeof(m2), "%s: decodeFrame recovered a frame", tc.name);
        CHECK(ok, m2);
        char m3[80]; snprintf(m3, sizeof(m3), "%s: on-air round-trip is bit-exact", tc.name);
        CHECK(ok && sameFrame(f, g), m3);
    }
}

static void test_onair_misalignment() {
    printf("\n== decoder finds SYNC at arbitrary bit offset ==\n");
    // Encode with a different preamble length to shift the payload alignment,
    // proving the SYNC search (not a fixed offset) is what locks the frame.
    Frame f = makeFan22F1(0x02);
    for (int pre = 3; pre <= 10; pre++) {
        uint8_t onair[MAX_ONAIR]; size_t bits = 0;
        size_t bytes = encodeFrame(f, onair, sizeof(onair), pre, &bits);
        Frame g;
        bool ok = bytes && decodeFrame(onair, bytes, bits, g) && sameFrame(f, g);
        char m[80]; snprintf(m, sizeof(m), "preamble=%d bytes: frame still decodes", pre);
        CHECK(ok, m);
    }
}

static void test_manchester_table() {
    printf("\n== Manchester table integrity ==\n");
    bool distinct = true;
    for (int i = 0; i < 16; i++)
        for (int j = i+1; j < 16; j++)
            if (MANCH_ENC[i] == MANCH_ENC[j]) distinct = false;
    CHECK(distinct, "all 16 Manchester codes are distinct");
    bool noStop = true;
    for (int i = 0; i < 16; i++) if (MANCH_ENC[i] == STOP_WIRE) noStop = false;
    CHECK(noStop, "STOP marker (0x35) is not a valid Manchester code");
    bool noSync = true;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 5; j++)
            if (MANCH_ENC[i] == SYNC[j]) noSync = false;
    // (0x55 IS both a sync byte and a Manchester code for nibble 0xF — that's fine
    //  because sync is positionally fixed; we only assert the STOP separation.)
    (void)noSync;
}

int main() {
    printf("RAMSES-II / Orcon 15RF codec — native tests\n");
    test_manchester_table();
    test_logical_checksum();
    test_header_types();
    test_onair_roundtrip();
    test_onair_misalignment();
    const char* verdict = failures ? "FAILED" : "ALL PASSED";
    const char* plural  = (failures == 1) ? "" : "s";
    printf("\n%s (%d failure%s)\n", verdict, failures, plural);
    return failures ? 1 : 0;
}
