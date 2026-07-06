// ramses_radio.h — thin RF transport for RAMSES-II frames.
//
// Two back-ends selected at compile time (see config.h):
//   RADIO_SX1262  : Heltec V3 built-in SX1262. GFSK + *software* Manchester.
//                   TX sends the fully pre-encoded on-air buffer; RX captures a
//                   raw window and runs the host-tested software decoder.
//   RADIO_CC1101  : external CC1101 with hardware Manchester/async framing.
//
// The protocol logic lives entirely in ramses_codec; this file is only "get
// these bytes on/off the air".
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ramses_codec.h"

namespace radio {

// Initialise the selected radio. Returns true on success.
bool begin();

// Transmit one RAMSES frame. Blocks until the packet is sent (~a few ms).
bool sendFrame(const ramses::Frame& f);

// Put the radio into continuous receive. Call once after begin().
void startReceive();

// Poll for a received frame. Returns true and fills `out` when a valid,
// checksum-clean RAMSES frame has been decoded since the last call.
bool poll(ramses::Frame& out);

// Last measured RSSI (dBm) of a received packet, for diagnostics.
float lastRssi();

// Human-readable back-end name ("SX1262" / "CC1101").
const char* name();

} // namespace radio
