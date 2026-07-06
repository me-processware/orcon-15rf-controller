// cc1101_tx.h — CC1101 in ASYNC SERIAL mode, driven by a hardware UART (the
// evofw3 method). The RAMSES per-byte framing is literally 38400-baud 8N1, so:
//   TX: UART TX -> GDO0 (data in)  -> CC1101 FSK-modulates it.
//   RX: CC1101 demodulates -> GDO2 (data out) -> UART RX -> software decode.
// Used for transmit alongside SX1262 RX (-DUSE_CC1101_TX), OR for BOTH
// directions on a single CC1101 (-DRADIO_CC1101_ASYNC).
#pragma once
#include "ramses_codec.h"

namespace cc1101tx {
bool    begin();                       // SPI-config the CC1101 + open the UART (RX+TX)
bool    send(const ramses::Frame& f);  // transmit one frame (leaves chip in IDLE)
uint8_t version();                     // CC1101 VERSION reg read at begin (~0x14)

// --- receive (async serial) -------------------------------------------------
void    startRx();                     // strobe RX, flush the UART/byte buffer
bool    poll(ramses::Frame& out);      // drain UART, return one frame if found
int     rssiDbm();                     // current CC1101 RSSI (approx, dBm)
uint32_t uartBytes();                  // total bytes received on GDO2/UART (diagnostic)
}
