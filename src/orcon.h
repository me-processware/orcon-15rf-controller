// orcon.h — stateful Orcon 15RF emulation.
#pragma once
#include <stdint.h>
#include "ramses_codec.h"
#include "orcon_frames.h"

namespace orcon {

struct State {
    Mode     mode           = Mode::Unknown; // FAN-reported (31D9) only
    uint8_t  supplyPct      = 0;
    uint8_t  exhaustPct     = 0;
    float    tempSupply     = 0;
    float    tempExhaust    = 0;
    float    tempOutdoor    = 0;
    float    tempIndoor     = 0;
    uint8_t  humidity       = 0;
    uint16_t co2            = 0;
    uint8_t  demand         = 0;
    bool     bypassOpen     = false;
    uint8_t  filterRemaining= 0xFF;
    bool     fault          = false;
    bool     fanKnown       = false;
    uint32_t lastRxMs       = 0;
    uint32_t lastFanMs      = 0;
    uint32_t lastCmdMs      = 0;
    uint32_t txAckMs        = 0;   // unit answered us (RP / 31D9 soon after a TX)
    bool     selftest       = false;
    Mode     cmdMode        = Mode::Unknown;
    // --- debug / keeper ---
    uint32_t rxCount        = 0;
    uint32_t txCount        = 0;
    char     raw31DA[65]    = "";  // last 31DA payload as hex (for offset calibration)
    uint32_t raw31DAms      = 0;
    char     lastRx[80]     = "";  // last decoded RX frame, human string
    char     lastTx[80]     = "";  // last TX frame, human string
    bool     autoRestore    = false;
};

void          begin(uint8_t usClass, uint32_t usSerial, uint8_t fanClass, uint32_t fanSerial);
void          setIds(uint8_t usClass, uint32_t usSerial, uint8_t fanClass, uint32_t fanSerial);
const State&  state();
const Ids&    ids();

bool setMode(Mode m);
bool setTimer(uint16_t minutes);
bool sendBind();

// --- pairing (1FC9 bind, mimics the real 15RF "Auto+1" procedure) -----------
enum class PairState : uint8_t { Idle, Searching, Paired, Timeout };
void      startPairing();       // open a 3-min bind window + send the 1FC9 offer
PairState pairState();          // current status (computes the timeout)
uint32_t  pairWindowLeftMs();   // ms left in the pairing window (0 if not pairing)
bool      idsDirty();           // pairing learned a fan -> caller should persist ids()
void      clearIdsDirty();
void      setPassiveLearn(bool on);  // ADVANCED: latch onto any fan heard (default off)
bool      passiveLearn();
void setSelftest(bool on);
bool setBypass(uint8_t val);
bool setFanSpeed(uint8_t paramIndex, uint8_t percent);
void setAcMode(uint8_t which);    // 0=balanced 1=exhaust-only 2=supply-only
void setAcCustom(uint8_t supplyPct, uint8_t exhaustPct, bool openBypass);

// "Settings keeper": remember per-preset fan speeds (and bypass) and re-push
// them automatically when the fan power-cycles (announces 042F). Solves the
// "custom settings vanish when the Orcon loses power" problem.
void    setFanSaved(uint8_t paramIndex, int16_t pct);  // -1 = clear
int16_t fanSaved(uint8_t paramIndex);
void    setBypassSaved(int16_t val);                   // -1 none, else C8/00/FF
int16_t bypassSaved();
void    setAutoRestore(bool on);
bool    autoRestore();
uint8_t reapplySaved();                                // returns count re-pushed

void sendConnect();                                    // RQ 0001 to the unit
void sendRaw(uint8_t type, uint16_t code,
             const uint8_t* pl, uint8_t len);          // arbitrary frame us->fan

bool sendDemand(uint8_t percent);
bool sendCo2(uint16_t ppm);
bool sendDeviceInfo();
bool requestStatus();

void tick(uint32_t nowMs);
bool onFrame(const ramses::Frame& f);

} // namespace orcon
