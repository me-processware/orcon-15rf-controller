// orcon_log.h — in-RAM ring buffer of RAMSES frames (RX + TX), viewable over
// the web with no USB serial. Also auto-detects a REM (remote) controlling a
// FAN so the device can be "cloned" without binding.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ramses_codec.h"

namespace rlog {

static const size_t  CAP = 48;          // ring-buffer depth

// Format a frame as a ramses-style string, e.g.
//   "I --- 29:012345 32:178990 --:------ 22F1 003 000307"
// `out` should be >= 96 bytes. Returns length.
size_t format(const ramses::Frame& f, char* out, size_t cap);

// Record one frame. dir = 'R' (received) or 'T' (transmitted).
void add(char dir, const ramses::Frame& f, int rssi);

// Serialize recent entries (newest last) as a JSON array of
//   {"t":<ms>,"d":"R","s":"<frame str>","r":<rssi>}
size_t toJson(char* out, size_t cap);

// --- clone auto-detect ------------------------------------------------------
// When we hear an I 22F1/22F3/31D9 involving a REM (class 0x1D) and a FAN
// (class 0x20), remember those ids so the UI can offer "clone this remote".
struct Seen {
    bool             haveRemote = false;
    bool             haveFan    = false;
    ramses::DeviceId remote;
    ramses::DeviceId fan;
};
const Seen& seen();

} // namespace rlog
