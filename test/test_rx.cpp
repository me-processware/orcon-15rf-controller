// test_rx.cpp — CC1101 async-RX decode chain: wire bytes -> Frame, in noise.
#include "ramses_codec.h"
#include <cstdio>
#include <cstring>
using namespace ramses;
static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){printf("  [FAIL] %s\n",m);fails++;} else printf("  [ ok ] %s\n",m);}while(0)

static Frame mk(MsgType t, uint16_t cmd, const uint8_t* pl, uint8_t len) {
    Frame f; f.type=t;
    f.has_addr[0]=true; f.addr[0]=DeviceId(0x1D,0x012345);
    f.has_addr[1]=true; f.addr[1]=DeviceId(0x20,0x02BCDE);
    f.has_addr[2]=false;
    f.cmd=cmd; f.len=len; for(uint8_t i=0;i<len;i++) f.payload[i]=pl[i];
    return f;
}
// frame -> wire bytes (what the UART would recover on RX)
static size_t toWire(const Frame& f, uint8_t* wire, size_t cap) {
    uint8_t logical[MAX_LOGICAL];
    size_t ln = buildLogical(f, logical, sizeof(logical));
    return wireFromLogical(logical, ln, wire, cap);
}
static void test_one(const char* nm, const Frame& f) {
    uint8_t wire[MAX_WIRE]; size_t wn = toWire(f, wire, sizeof(wire));
    // build a noisy stream: junk + preamble + wire + trailing junk
    uint8_t s[400]; size_t n=0;
    uint8_t junk[]={0x12,0xC4,0xAB,0x07,0x55,0x55,0x55};
    for(unsigned i=0;i<sizeof(junk);i++) s[n++]=junk[i];
    memcpy(s+n, wire, wn); n+=wn;
    s[n++]=0x88; s[n++]=0x31; s[n++]=0x44;
    Frame g; size_t consumed=0;
    bool ok = extractWireFrame(s, n, &consumed, g);
    char m[96];
    snprintf(m,sizeof(m),"%s: extracted cmd=%04X len=%u (consumed=%zu)", nm, g.cmd, g.len, consumed);
    bool match = ok && g.cmd==f.cmd && g.len==f.len && g.type==f.type && memcmp(g.payload,f.payload,f.len)==0;
    CHECK(match, m);
}
int main(){
    printf("CC1101 RX decode-chain tests\n");
    uint8_t p1[]={0x00,0x01,0x07};                 test_one("22F1", mk(MsgType::I,0x22F1,p1,3));
    uint8_t p2[]={0x00,0xC8,0xEF};                 test_one("22F7", mk(MsgType::W,0x22F7,p2,3));
    uint8_t p3[]={0x00,0x00,0x00,0x00,0x01,0x00,0x64,0x00}; test_one("31E0", mk(MsgType::I,0x31E0,p3,8));
    uint8_t p4[]={0x00,0x10,0x40,0x07,0x12,0x34,0x56,0x78,0x07,0xD0,0x07,0xD0,0x09,0xC4}; test_one("31DA-ish", mk(MsgType::I,0x31DA,p4,14));

    // no-frame stream (pure noise) must not false-decode
    uint8_t noise[60]; for(int i=0;i<60;i++) noise[i]=(uint8_t)(i*37+11);
    Frame g; size_t cc=0; CHECK(!extractWireFrame(noise,sizeof(noise),&cc,g), "pure noise -> no frame");

    printf("\n%s (%d failures)\n", fails?"FAILED":"ALL PASSED", fails);
    return fails?1:0;
}
