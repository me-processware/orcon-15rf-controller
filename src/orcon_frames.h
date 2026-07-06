// orcon_frames.h — PURE RAMSES frame builders for the Orcon 15RF emulation.
// No Arduino / radio dependencies, so this is host-unit-tested alongside the
// codec. Stateful sending lives in orcon.{h,cpp}.
#pragma once
#include <stdint.h>
#include "ramses_codec.h"

namespace orcon {

enum class Mode : uint8_t {
    Away = 0, Low = 1, Medium = 2, High = 3,
    Auto = 4, Auto2 = 5, Boost = 6, Disable = 7,
    Unknown = 0xFF
};

const char* modeName(Mode m);
Mode        modeFromName(const char* s);

// Identity used by the builders. addr2 mirrors addr0 for I-broadcasts.
struct Ids {
    ramses::DeviceId us;     // our emulated 15RF (REM + CO2)
    ramses::DeviceId fan;    // the HRC400 (may be broadcast until learned)
};

// --- REM role ---------------------------------------------------------------
ramses::Frame make22F1(const Ids& id, Mode m);                 // set fan mode
ramses::Frame make22F3(const Ids& id, uint16_t minutes);       // timed boost
ramses::Frame make1FC9_bind(const Ids& id);                    // bind offer

// --- CO2/RH sensor role -----------------------------------------------------
ramses::Frame make31E0(const Ids& id, uint8_t percent);        // vent demand
ramses::Frame make1298(const Ids& id, uint16_t ppm);           // CO2 ppm
ramses::Frame make10E0(const Ids& id);                         // device info

// --- request helper ---------------------------------------------------------
ramses::Frame makeRequest(const Ids& id, uint16_t code);       // RQ <code>

// --- deep control (write) ---------------------------------------------------
ramses::Frame make22F7(const Ids& id, uint8_t val);            // bypass C8/00/FF
ramses::Frame makeFanSpeed(const Ids& id, uint8_t paramIndex, uint8_t percent);

} // namespace orcon
