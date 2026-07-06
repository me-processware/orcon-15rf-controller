// cc1101_tx.cpp — see cc1101_tx.h
#include "cc1101_tx.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <string.h>

// Only compile the CC1101 driver when a CC1101 is actually in use. Other radio
// back-ends (SX1262, SX1276) don't define the PIN_CC_* pins.
#if defined(USE_CC1101_TX) || defined(RADIO_CC1101_ASYNC)

namespace cc1101tx {

// CC1101 gets its OWN SPI bus (the SX1262's SPI on GPIO 8-14 is not broken out
// on the Heltec V3/V4 headers). Use the spare SPI3 (HSPI) peripheral.
static SPIClass ccSPI(HSPI);

// UART1 carries the async serial data: TX -> GDO0 (data in), RX <- GDO2 (data out).
static HardwareSerial ccUart(1);

// CC1101 register config for RAMSES-II / evohome, copied verbatim from evofw3
// (cc1101_param.c). Registers 0x00..0x2E:
//  IOCFG2=0D (GDO2 = async serial data OUTPUT, used for RX)
//  IOCFG0=2E  PKTCTRL0=31 (async serial; GDO0 = serial data INPUT for TX)
//  FREQ=21 65 6A (868.3MHz)  MDMCFG4/3=6A 83 (38.383kbps)
//  MDMCFG2=10 (GFSK, no sync/manchester — transparent)  DEVIATN=50 (~50.8kHz)
static const uint8_t CFG[] = {
  0x0D,0x2E,0x2E,0x07,0xD3,0x91,0xFF,0x04,0x31,0x00,0x00,0x0F,0x00,0x21,0x65,0x6A,
  0x6A,0x83,0x10,0x22,0xF8,0x50,0x07,0x30,0x18,0x16,0x6C,0x43,0x40,0x91,0x87,0x6B,
  0xF8,0x56,0x10,0xE9,0x21,0x00,0x1F,0x41,0x00,0x59,0x7F,0x3F,0x81,0x35,0x09
};
static const uint8_t PA = 0xC3;   // ~+10 dBm at 868 MHz
static uint8_t g_ver = 0;         // CC1101 VERSION read at begin

enum { SRES=0x30, SRX=0x34, STX=0x35, SIDLE=0x36, SFRX=0x3A, SFTX=0x3B };

static SPISettings ccSpiCfg(4000000, MSBFIRST, SPI_MODE0);
static inline void cs(bool low) { digitalWrite(PIN_CC_NSS, low ? LOW : HIGH); }

static void strobe(uint8_t cmd) {
    ccSPI.beginTransaction(ccSpiCfg); cs(true);
    ccSPI.transfer(cmd);
    cs(false); ccSPI.endTransaction();
}
static void writeBurst(uint8_t addr, const uint8_t* d, uint8_t n) {
    ccSPI.beginTransaction(ccSpiCfg); cs(true);
    ccSPI.transfer(addr | 0x40);
    for (uint8_t i = 0; i < n; i++) ccSPI.transfer(d[i]);
    cs(false); ccSPI.endTransaction();
}
static uint8_t readStatus(uint8_t addr) {       // status regs need the burst bit
    ccSPI.beginTransaction(ccSpiCfg); cs(true);
    ccSPI.transfer(addr | 0xC0);
    uint8_t v = ccSPI.transfer(0x00);
    cs(false); ccSPI.endTransaction();
    return v;
}

bool begin() {
    pinMode(PIN_CC_NSS, OUTPUT); cs(false);
    ccSPI.begin(PIN_CC_SCK, PIN_CC_MISO, PIN_CC_MOSI, PIN_CC_NSS);
    delay(10);
    // Reset + configure with retries. On a SHARED SPI bus (the display inits the
    // pins first) the first reads can glitch to 0xFF before chip/bus settle, so
    // re-run the whole config until the VERSION register answers sanely.
    for (int attempt = 0; attempt < 6; attempt++) {
        strobe(SRES); delay(5);
        writeBurst(0x00, CFG, sizeof(CFG));
        writeBurst(0x3E, &PA, 1);               // PATABLE
        strobe(SIDLE);
        g_ver = readStatus(0x31);               // VERSION ~0x14 on a real CC1101
        if (g_ver != 0x00 && g_ver != 0xFF) break;
        delay(10);
    }
    Serial.printf("[cc1101] version=0x%02X (expect ~0x14)\n", g_ver);
    // UART: RX from GDO2 (CC1101 data out), TX to GDO0 (CC1101 data in).
    // Large RX buffer: in async mode the chip streams continuous noise (~3.8 kB/s)
    // and any loop stall (WiFi/MQTT) would otherwise overflow it and drop frames.
    // 8 kB absorbs ~2 s of stall — the ISR keeps buffering while the loop is busy.
    ccUart.setRxBufferSize(8192);
    ccUart.begin(38400, SERIAL_8N1, PIN_CC_GDO2 /*rx*/, PIN_CC_GDO0 /*tx*/);
    return (g_ver != 0x00 && g_ver != 0xFF);
}

bool send(const ramses::Frame& f) {
    uint8_t logical[ramses::MAX_LOGICAL];
    size_t ln = ramses::buildLogical(f, logical, sizeof(logical));
    if (!ln) return false;
    uint8_t wire[ramses::MAX_WIRE];
    size_t wn = ramses::wireFromLogical(logical, ln, wire, sizeof(wire));
    if (!wn) return false;

    strobe(SIDLE);
    strobe(SFTX);
    strobe(STX);                 // enter TX; GDO0 becomes the serial data input
    delayMicroseconds(300);      // PLL settle

    for (int i = 0; i < RF_PREAMBLE_BYTES + 2; i++) ccUart.write((uint8_t)0x55);
    ccUart.write(wire, wn);
    ccUart.flush();              // block until the last bit has been clocked out

    delayMicroseconds(200);
    strobe(SIDLE);
    return true;
}

uint8_t version() { return g_ver; }

// ---------------------------------------------------------------------------
// Receive: CC1101 async serial mode. In RX state the demodulated bitstream
// comes out on GDO2; the UART re-frames it into the wire bytes (the on-air
// per-byte framing IS 38400 8N1), and extractWireFrame() pulls out frames.
// ---------------------------------------------------------------------------
static uint8_t  g_rxbuf[320];
static size_t   g_rxlen = 0;
static int      g_rssi  = -127;
static uint32_t g_uartBytes = 0;   // total bytes seen on the GDO2/UART RX line (diag)
uint32_t uartBytes() { return g_uartBytes; }

void startRx() {
    strobe(SIDLE);
    strobe(SFRX);
    strobe(SRX);
    while (ccUart.available()) ccUart.read();    // flush stale/noise bytes
    g_rxlen = 0;
}

int rssiDbm() {
    uint8_t r = readStatus(0x34);                // RSSI status register
    int v = (r >= 128) ? (r - 256) : r;
    return v / 2 - 74;                            // CC1101 datasheet conversion
}

bool poll(ramses::Frame& out) {
    while (ccUart.available() && g_rxlen < sizeof(g_rxbuf)) {
        g_rxbuf[g_rxlen++] = (uint8_t)ccUart.read();
        g_uartBytes++;
    }
    if (g_rxlen < 8) return false;

    size_t consumed = 0;
    bool ok = ramses::extractWireFrame(g_rxbuf, g_rxlen, &consumed, out);
    if (ok) g_rssi = rssiDbm();
    if (consumed > 0 && consumed <= g_rxlen) {
        memmove(g_rxbuf, g_rxbuf + consumed, g_rxlen - consumed);
        g_rxlen -= consumed;
    } else if (g_rxlen >= sizeof(g_rxbuf)) {
        g_rxlen = 0;                              // overflow guard
    }
    return ok;
}

} // namespace cc1101tx
#endif // USE_CC1101_TX || RADIO_CC1101_ASYNC
