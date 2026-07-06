#include "ramses_codec.h"
#include "orcon_frames.h"
#include "orcon_log.h"
#include <cstdio>
#include <cstring>
unsigned long g_ms = 12345; unsigned long millis() { return g_ms; }
using namespace ramses; using namespace orcon;
static int failures=0;
#define CHECK(c,m) do{ if(!(c)){printf("  [FAIL] %s\n",m);failures++;} else printf("  [ ok ] %s\n",m);}while(0)
int main(){
    printf("RF log formatter tests\n");
    Ids id; id.us=DeviceId(0x1D,0x012345); id.fan=DeviceId(0x20,0x02BCDE);
    char buf[96];
    Frame f = make22F1(id, Mode::High);
    rlog::format(f, buf, sizeof(buf));
    printf("  fmt: %s\n", buf);
    CHECK(strstr(buf,"I  --- 29:074565 32:179422 --:------ 22F1 003 000307")!=nullptr,
          "22F1 formats with 2-address + payload");
    // self-broadcast 1298
    Frame c = make1298(id, 639);
    rlog::format(c, buf, sizeof(buf));
    printf("  fmt: %s\n", buf);
    CHECK(strstr(buf,"--:------")!=nullptr && strstr(buf,"1298 003 00027F")!=nullptr,
          "1298 formats self-broadcast with empty addr1");
    // log add + json + clone detect (simulate hearing a real remote->fan 22F1)
    Frame heard = make22F1(id, Mode::Low);   // addr0=29(REM), addr1=32(FAN)
    rlog::add('R', heard, -73);
    rlog::add('T', make31E0(id,50), 0);
    char j[2048]; rlog::toJson(j,sizeof(j));
    CHECK(strstr(j,"\"d\":\"R\"")&&strstr(j,"\"d\":\"T\"")&&strstr(j,"\"r\":-73"), "toJson has RX+TX entries with rssi");
    CHECK(rlog::seen().haveRemote && rlog::seen().remote.cls==0x1D, "clone-detect found the remote id");
    CHECK(rlog::seen().haveFan && rlog::seen().fan.cls==0x20, "clone-detect found the fan id");
    const char* v=failures?"FAILED":"ALL PASSED"; const char* pl=failures==1?"":"s";
    printf("\n%s (%d failure%s)\n",v,failures,pl);
    return failures?1:0;
}
