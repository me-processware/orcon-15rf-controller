// test_frames.cpp — verify Orcon command builders (addressing + payloads)
// against the real-capture-derived formats.
#include "ramses_codec.h"
#include "orcon_frames.h"
#include <cstdio>
#include <cstring>
using namespace ramses;
using namespace orcon;

static int failures = 0;
#define CHECK(c, m) do { if(!(c)){printf("  [FAIL] %s\n", m); failures++;} \
                         else {printf("  [ ok ] %s\n", m);} } while(0)

static Ids testIds() {
    Ids id;
    id.us  = DeviceId(0x1D, 0x012345);   // REM (29:...)
    id.fan = DeviceId(0x20, 0x02BCDE);   // FAN (32:...)
    return id;
}
static bool roundtrips(const Frame& f, Frame& out) {
    uint8_t onair[MAX_ONAIR]; size_t bits = 0;
    size_t n = encodeFrame(f, onair, sizeof(onair), 6, &bits);
    return n && decodeFrame(onair, n, bits, out);
}
static bool addr2(const Frame& f){ return f.has_addr[0]&&f.has_addr[1]&&!f.has_addr[2]; }
static bool addr02(const Frame& f){ return f.has_addr[0]&&!f.has_addr[1]&&f.has_addr[2]; }

static void test_22F1() {
    printf("\n== 22F1 fan mode (to-fan addressing) ==\n");
    Ids id = testIds();
    struct { Mode m; uint8_t nn; const char* nm; } cs[] = {
        {Mode::Away,0,"away"},{Mode::Low,1,"low"},{Mode::Medium,2,"medium"},
        {Mode::High,3,"high"},{Mode::Auto,4,"auto"},{Mode::Boost,6,"boost"},
    };
    for (auto& c : cs) {
        Frame f = make22F1(id, c.m);
        char m[64]; snprintf(m,sizeof(m),"22F1 %s -> 00 %02X 07", c.nm, c.nn);
        CHECK(f.cmd==0x22F1 && f.len==3 && f.payload[0]==0 && f.payload[1]==c.nn && f.payload[2]==7, m);
        Frame g;
        CHECK(roundtrips(f,g) && g.payload[1]==c.nn && addr2(g), "  ...round-trips as 2-address (addr2 empty)");
    }
}
static void test_22F3() {
    printf("\n== 22F3 timed boost ==\n");
    Ids id = testIds();
    Frame f = make22F3(id, 60);
    bool ok = f.cmd==0x22F3 && f.len==7 &&
              f.payload[0]==0x00 && f.payload[1]==0x12 && f.payload[2]==0x3C &&
              f.payload[3]==0x03 && f.payload[4]==0x04 && f.payload[5]==0x04 && f.payload[6]==0x04;
    CHECK(ok, "22F3 60min high -> 00 12 3C 03 04 04 04");
    Frame g; CHECK(roundtrips(f,g) && g.payload[2]==0x3C && addr2(g), "22F3 round-trips");
    Frame f15 = make22F3(id, 15);
    CHECK(f15.payload[2]==0x0F, "22F3 15min -> minutes byte 0x0F");
}
static void test_sensor() {
    printf("\n== 31E0 demand / 1298 CO2 ==\n");
    Ids id = testIds();
    Frame d = make31E0(id, 100);
    CHECK(d.cmd==0x31E0 && d.len==8 && d.payload[4]==0x01 && d.payload[6]==100 && addr2(d),
          "31E0 100% -> 00 00 00 00 01 00 64 00 (to fan)");
    Frame dg; CHECK(roundtrips(d,dg) && dg.payload[6]==100, "31E0 round-trips");
    Frame d30 = make31E0(id, 30);
    CHECK(d30.payload[6]==0x1E, "31E0 30% -> demand byte 0x1E");

    Frame c = make1298(id, 639);
    CHECK(c.cmd==0x1298 && c.len==3 && c.payload[1]==0x02 && c.payload[2]==0x7F && addr02(c),
          "1298 639ppm -> 00 02 7F (self-broadcast)");
    Frame cg; CHECK(roundtrips(c,cg) && ((cg.payload[1]<<8)|cg.payload[2])==639, "1298 round-trips");
}
static void test_bind_rq() {
    printf("\n== 1FC9 bind + RQ ==\n");
    Ids id = testIds();
    Frame b = make1FC9_bind(id);
    CHECK(b.cmd==0x1FC9 && b.len==24, "1FC9 offers 4 codes (24-byte payload, 6/tuple)");
    uint32_t r = id.us.raw();
    bool t0 = b.payload[0]==0x00 && b.payload[1]==0x22 && b.payload[2]==0xF1 &&
              b.payload[3]==((r>>16)&0xFF) && b.payload[4]==((r>>8)&0xFF) && b.payload[5]==(r&0xFF);
    CHECK(t0, "1FC9 tuple0 = 00 22F1 <our-id>");
    Frame bg; CHECK(roundtrips(b,bg) && bg.len==24, "1FC9 round-trips");
    Frame q = makeRequest(id, 0x31DA);
    CHECK(q.type==MsgType::RQ && addr2(q), "RQ 31DA is to-fan request");
    Frame qg; CHECK(roundtrips(q,qg) && qg.type==MsgType::RQ, "RQ 31DA round-trips");
}
static void test_modes() {
    printf("\n== mode names ==\n");
    CHECK(modeFromName("3")==Mode::High, "\"3\" -> High");
    CHECK(modeFromName("auto")==Mode::Auto, "\"auto\" -> Auto");
    CHECK(strcmp(modeName(Mode::Boost),"boost")==0, "Boost -> \"boost\"");
}
static void test_deep() {
    printf("\n== 22F7 bypass / 2411 fan-speed (capture-verified) ==\n");
    Ids id = testIds();
    Frame bo = make22F7(id, 0xC8);
    CHECK(bo.type==MsgType::W && bo.cmd==0x22F7 && bo.len==3 && bo.payload[0]==0 && bo.payload[1]==0xC8 && bo.payload[2]==0xEF && addr2(bo), "22F7 open -> W 00 C8 EF");
    Frame bc = make22F7(id, 0x00);
    CHECK(bc.payload[1]==0x00 && bc.payload[2]==0xEF, "22F7 close -> 00 00 EF");
    Frame ba = make22F7(id, 0xFF);
    CHECK(ba.payload[1]==0xFF, "22F7 auto -> 00 FF EF");

    Frame ls = makeFanSpeed(id, 0, 28);   // low supply 28%
    CHECK(ls.type==MsgType::W && ls.cmd==0x2411 && ls.len==23 && ls.payload[2]==0x3F && ls.payload[8]==0x38 && ls.payload[16]==0xA0 && addr2(ls), "2411 low-supply 28% -> pid 3F spd 38");
    Frame me = makeFanSpeed(id, 3, 50);   // med exhaust 50%
    CHECK(me.payload[2]==0x42 && me.payload[8]==0x64 && me.payload[12]==0x14 && me.payload[16]==0xC8, "2411 med-exhaust 50% -> pid 42 spd 64, flag 14");
    Frame bo2 = makeFanSpeed(id, 6, 100);  // boost 100%
    CHECK(bo2.payload[2]==0x95 && bo2.payload[8]==0xC8, "2411 boost 100% -> pid 95 spd C8");
    Frame g; CHECK(roundtrips(me,g) && g.payload[8]==0x64 && g.len==23, "2411 round-trips on-air");
}

int main() {
    printf("Orcon command-builder tests (capture-verified)\n");
    test_22F1(); test_22F3(); test_sensor(); test_bind_rq(); test_modes(); test_deep();
    const char* verdict = failures ? "FAILED" : "ALL PASSED";
    const char* plural  = (failures == 1) ? "" : "s";
    printf("\n%s (%d failure%s)\n", verdict, failures, plural);
    return failures ? 1 : 0;
}
