// ramses_radio.cpp — see ramses_radio.h
#include "ramses_radio.h"
#include "config.h"
#include <Arduino.h>
#include <RadioLib.h>
#if defined(USE_CC1101_TX) || defined(RADIO_CC1101_ASYNC)
#include "cc1101_tx.h"
#endif

namespace radio {

static volatile bool   g_rxFlag = false;
static float           g_rssi   = -127.0f;
static uint8_t         g_onair[ramses::MAX_ONAIR];

#if defined(ESP32)
IRAM_ATTR
#endif
static void onDio() { g_rxFlag = true; }

// ===========================================================================
//  SX1262 back-end (built-in Heltec radio). RX always; TX via SX1262 unless
//  USE_CC1101_TX is set, in which case an external CC1101 does the transmit.
// ===========================================================================
#if defined(RADIO_SX1262)

static SX1262 g_radio = new Module(PIN_SX_NSS, PIN_SX_DIO1, PIN_SX_RST, PIN_SX_BUSY);

static const size_t  RX_WINDOW = 96;
static uint8_t       g_syncWord[2] = { 0x55, 0x53 };

#if defined(USE_CC1101_TX)
const char* name() { return "SX1262rx+CC1101tx"; }
#else
const char* name() { return "SX1262"; }
#endif

static bool configure() {
    int st = g_radio.beginFSK(RF_FREQ_MHZ, RF_BITRATE_KBPS, RF_DEVIATION_KHZ,
                              RF_RXBW_KHZ, 10 /*dBm*/, 16 /*preamble bits*/);
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("[radio] beginFSK=%d\n", st); return false; }
    // Heltec V3/V4: the antenna is connected to the SX1262 through an RF switch
    // driven by DIO2. Without this, the receiver is electrically disconnected
    // from the antenna -> dead-flat ~-112 dBm RSSI no matter the antenna. REQUIRED.
    g_radio.setDio2AsRfSwitch(true);
    g_radio.setTCXO(1.6);                       // Heltec uses a DIO3-powered TCXO
    g_radio.setSyncWord(g_syncWord, sizeof(g_syncWord));
    g_radio.setCRC(0);
    g_radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    g_radio.setEncoding(RADIOLIB_ENCODING_NRZ);
    g_radio.fixedPacketLengthMode(RX_WINDOW);
    g_radio.setDio1Action(onDio);
    return true;
}

void startReceive() {
    g_rxFlag = false;
    g_radio.startReceive();
}

bool begin() {
    SPI.begin(PIN_SX_SCK, PIN_SX_MISO, PIN_SX_MOSI, PIN_SX_NSS);
    bool ok = configure();
#if defined(USE_CC1101_TX)
    cc1101tx::begin();          // CC1101 handles transmit; SX1262 keeps RX
#endif
    return ok;
}

bool sendFrame(const ramses::Frame& f) {
#if defined(USE_CC1101_TX)
    bool ok = cc1101tx::send(f);    // transmit via CC1101 async UART
    startReceive();                 // keep the SX1262 listening
    return ok;
#else
    size_t bits = 0;
    size_t n = ramses::encodeFrame(f, g_onair, sizeof(g_onair), RF_PREAMBLE_BYTES, &bits);
    if (!n) return false;
    static uint8_t txSync[2] = { 0x55, 0x55 };
    g_radio.setSyncWord(txSync, sizeof(txSync));
    g_radio.fixedPacketLengthMode((uint8_t)n);
    int st = g_radio.transmit(g_onair, n);
    g_radio.setSyncWord(g_syncWord, sizeof(g_syncWord));
    g_radio.fixedPacketLengthMode(RX_WINDOW);
    startReceive();
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("[radio] tx=%d\n", st); return false; }
    return true;
#endif
}

bool poll(ramses::Frame& out) {
    if (!g_rxFlag) return false;
    g_rxFlag = false;
    uint8_t buf[RX_WINDOW];
    int st = g_radio.readData(buf, RX_WINDOW);
    g_rssi = g_radio.getRSSI();
    startReceive();
    if (st != RADIOLIB_ERR_NONE && st != RADIOLIB_ERR_CRC_MISMATCH) return false;
    bool ok = ramses::decodeFrame(buf, RX_WINDOW, RX_WINDOW * 8, out);
#if DEBUG_RX_HEX
    if (ok) {
        Serial.printf("[rx %.0fdBm] cmd=%04X len=%u :", g_rssi, out.cmd, out.len);
        for (uint8_t i = 0; i < out.len; i++) Serial.printf("%02X", out.payload[i]);
        Serial.println();
    }
#endif
    return ok;
}

#endif // RADIO_SX1262

// ===========================================================================
//  CC1101 stand-alone packet back-end (legacy fallback, -D RADIO_CC1101)
// ===========================================================================
#if defined(RADIO_CC1101)

static CC1101 g_radio = new Module(PIN_CC_NSS, PIN_CC_GDO0, RADIOLIB_NC, PIN_CC_GDO2);
static const size_t RX_WINDOW = 96;

const char* name() { return "CC1101"; }

static bool configure() {
    int st = g_radio.begin(RF_FREQ_MHZ, RF_BITRATE_KBPS, RF_DEVIATION_KHZ,
                           RF_RXBW_KHZ, 10 /*dBm*/, RF_PREAMBLE_BYTES);
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("[radio] cc begin=%d\n", st); return false; }
    g_radio.setSyncWord(0x55, 0x53);
    g_radio.setCrcFiltering(false);
    g_radio.setEncoding(RADIOLIB_ENCODING_NRZ);
    g_radio.fixedPacketLengthMode(RX_WINDOW);
    g_radio.setGdo0Action(onDio, RISING);
    return true;
}

bool begin() {
    SPI.begin(PIN_SX_SCK, PIN_SX_MISO, PIN_SX_MOSI, PIN_CC_NSS);
    return configure();
}

void startReceive() { g_rxFlag = false; g_radio.startReceive(); }

bool sendFrame(const ramses::Frame& f) {
    size_t bits = 0;
    size_t n = ramses::encodeFrame(f, g_onair, sizeof(g_onair), RF_PREAMBLE_BYTES, &bits);
    if (!n) return false;
    int st = g_radio.transmit(g_onair, n);
    startReceive();
    return st == RADIOLIB_ERR_NONE;
}

bool poll(ramses::Frame& out) {
    if (!g_rxFlag) return false;
    g_rxFlag = false;
    uint8_t buf[RX_WINDOW];
    int st = g_radio.readData(buf, RX_WINDOW);
    g_rssi = g_radio.getRSSI();
    startReceive();
    if (st != RADIOLIB_ERR_NONE && st != RADIOLIB_ERR_CRC_MISMATCH) return false;
    return ramses::decodeFrame(buf, RX_WINDOW, RX_WINDOW * 8, out);
}

#endif // RADIO_CC1101

// ===========================================================================
//  CC1101 async-serial back-end — RX *and* TX on ONE CC1101 (the evofw3 way).
//  No SX1262. Receive = the mirror of transmit: CC1101 demodulates onto GDO2,
//  the UART re-frames the wire bytes, software decodes them.
// ===========================================================================
#if defined(RADIO_CC1101_ASYNC)
const char* name() { return "CC1101 async RX+TX"; }
bool begin() { return cc1101tx::begin(); }
void startReceive() { cc1101tx::startRx(); }
bool sendFrame(const ramses::Frame& f) {
    bool ok = cc1101tx::send(f);
    cc1101tx::startRx();                 // straight back to listening
    return ok;
}
bool poll(ramses::Frame& out) {
    bool ok = cc1101tx::poll(out);
    g_rssi = (float)cc1101tx::rssiDbm();
    return ok;
}
#endif // RADIO_CC1101_ASYNC

// ===========================================================================
//  SX1276 back-end (Heltec WiFi LoRa 32 V2) — single-board RX+TX, NO CC1101.
//  The SX127x family keeps the "continuous/direct" FSK mode the SX1262 dropped,
//  so we can mirror the CC1101 trick on the on-board radio:
//   RX: continuous FSK — the demodulated NRZ stream appears on DIO2; a hardware
//       UART at 38400 8N1 (== the RAMSES byte framing) re-frames it and
//       extractWireFrame() decodes it (the exact path proven on CC1101 GDO2).
//   TX: FSK packet mode — the full on-air frame is stuffed as raw payload.
// ===========================================================================
#if defined(RADIO_SX1276)
#include <HardwareSerial.h>

static SX1276 g_radio = new Module(PIN_SX_NSS, PIN_SX_DIO0, PIN_SX_RST, PIN_SX_DIO1);
static HardwareSerial rxUart(1);     // reads the demodulated data off DIO2
static uint8_t  g_rxbuf[320];
static size_t   g_rxlen = 0;

const char* name() { return "SX1276 direct RX + packet TX"; }

static bool configure() {
    int st = g_radio.beginFSK(RF_FREQ_MHZ, RF_BITRATE_KBPS, RF_DEVIATION_KHZ,
                              RF_RXBW_KHZ, 17 /*dBm*/, 16 /*preamble bits*/);
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("[radio] sx1276 beginFSK=%d\n", st); return false; }
    g_radio.setEncoding(RADIOLIB_ENCODING_NRZ);
    g_radio.setDataShaping(RADIOLIB_SHAPING_NONE);
    g_radio.setCRC(false);
    return true;
}

void startReceive() {
    g_radio.receiveDirect();                  // continuous FSK; demod streams on DIO2
    while (rxUart.available()) rxUart.read();  // flush noise/partial bytes
    g_rxlen = 0;
}

bool begin() {
    SPI.begin(PIN_SX_SCK, PIN_SX_MISO, PIN_SX_MOSI, PIN_SX_NSS);
    bool ok = configure();
    rxUart.setRxBufferSize(8192);
    rxUart.begin(38400, SERIAL_8N1, PIN_SX_DIO2 /*rx*/, -1 /*tx unused*/);
    return ok;
}

bool sendFrame(const ramses::Frame& f) {
    size_t bits = 0;
    size_t n = ramses::encodeFrame(f, g_onair, sizeof(g_onair), RF_PREAMBLE_BYTES, &bits);
    if (!n) return false;
    static uint8_t txSync[2] = { 0x55, 0x53 };
    g_radio.standby();                        // leave continuous/direct RX cleanly
    g_radio.setEncoding(RADIOLIB_ENCODING_NRZ);
    g_radio.setSyncWord(txSync, sizeof(txSync));
    g_radio.fixedPacketLengthMode((uint8_t)n);
    int st = g_radio.transmit(g_onair, n);
    startReceive();                           // straight back to listening
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("[radio] sx1276 tx=%d\n", st); return false; }
    return true;
}

bool poll(ramses::Frame& out) {
    while (rxUart.available() && g_rxlen < sizeof(g_rxbuf))
        g_rxbuf[g_rxlen++] = (uint8_t)rxUart.read();
    if (g_rxlen < 8) return false;
    size_t consumed = 0;
    bool ok = ramses::extractWireFrame(g_rxbuf, g_rxlen, &consumed, out);
    if (ok) g_rssi = g_radio.getRSSI();
    if (consumed > 0 && consumed <= g_rxlen) {
        memmove(g_rxbuf, g_rxbuf + consumed, g_rxlen - consumed);
        g_rxlen -= consumed;
    } else if (g_rxlen >= sizeof(g_rxbuf)) {
        g_rxlen = 0;                          // overflow guard
    }
    return ok;
}

#endif // RADIO_SX1276

float lastRssi() { return g_rssi; }

} // namespace radio
