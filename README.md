# Orcon 15RF controller (ESP32 + RAMSES-II)

Wi-Fi / MQTT / touch-screen controller for **Orcon HRC** heat-recovery ventilation
units (HRC300 / **HRC400** / MVS-15R and relatives). It emulates the **Orcon 15RF**
CO₂/RH wall controller over 868.3 MHz **RAMSES-II**, so you can set fan modes,
timed boost, the summer **bypass**, and an unbalanced **AC / night-cooling** mode
from a web page, MQTT/Home Assistant, or an on-device touch display — while the
unit's own safety logic stays untouched (everything goes over the RF control
path, not the motor).

> Unofficial and reverse-engineered from community work. Not affiliated with
> Orcon. See [`CREDITS.md`](CREDITS.md). MIT licensed — see [`LICENSE`](LICENSE).

---

## What it does

- **Modes**: `Away / Auto / 1 / 2 / 3 / Boost` (`22F1`) and timed boost
  `15 / 30 / 60 min` (`22F3`).
- **Bypass**: open / auto / close (`22F7`) — force the heat-exchanger bypass for
  night cooling.
- **AC mode**: one-tap unbalanced supply/exhaust (reconfigures the High preset via
  `2411`) for quiet cooling, e.g. exhaust > supply to pull warm air out.
- **Live status**: decodes the unit's broadcasts (`31DA` extended status, `31D9`
  mode, `12A0` humidity, `1298` CO₂, `10D0` filter, `042F` power-cycle) into a
  live model — temps, RH, CO₂, supply/exhaust %, bypass, filter, faults.
- **Deliberate pairing**: mimics the real 15RF "Auto+1" bind (`1FC9`) — power-cycle
  the HRC, tap **Pair**, and it learns the fan's ID from the handshake. Each unit
  auto-generates a unique controller ID on first boot.
- **Interfaces**: onboard web UI, MQTT (with Home Assistant auto-discovery), an
  embeddable airflow widget, and — on the Guition panel — an on-screen touch UI.

## Radio options (important)

RAMSES-II is 2-FSK, 38.4 kbps, Manchester-framed. The chip must expose the raw
demodulated bitstream (transparent / continuous mode); we feed it through a UART
and decode in software (the evofw3 method). **Not every LoRa chip can do this:**

| Radio | Works? | Notes |
|-------|--------|-------|
| **CC1101** | ✅ **Recommended** | The community standard. RX **and** TX via async serial. ~€2 external module. |
| **SX1276 / SX1279** (SX127x) | ✅ | Has direct/continuous mode → same trick on the *on-board* radio (e.g. Heltec WiFi LoRa 32 **V2**). SX1278 is 433 MHz — not 868. |
| SX1262 / SX1268 (SX126x) | ❌ | No transparent mode. Packet engine can't frame RAMSES. Kept only as an experiment. |
| SX1302/1303 | ❌ | LoRaWAN gateway concentrators, wrong class of chip. |

For a clean build, use **CC1101** (any USB-C ESP32-S3 + a CC1101 module) or an
SX1276 board. See [`docs/PROTOCOL.md`](docs/PROTOCOL.md) for the SX1276 direct-mode
write-up.

## Supported boards / build envs

Defined in `platformio.ini`:

| Env | Board | Radio |
|-----|-------|-------|
| `heltec_v3_cc1101_ota` *(default)* | Heltec WiFi LoRa 32 V3/V4 (ESP32-S3) | external CC1101, RX+TX |
| `heltec_v2_sx1276` | Heltec WiFi LoRa 32 **V2** (ESP32) | on-board SX1276, RX+TX |
| `esp32-480480s040` | Guition ESP32-4848S040 (480×480 touch) | CC1101 on shared SPI |
| `heltec_v3_1262_ota` | Heltec V3/V4 | on-board SX1262 (experimental) |
| `native` | host | unit tests (no hardware) |

## Build & flash

1. Install [PlatformIO](https://platformio.org/) (`pip install platformio`).
2. First flash over USB (later updates go over Wi-Fi / OTA):
   ```bash
   pio run -e heltec_v3_cc1101_ota -t upload --upload-port COM5
   pio device monitor
   ```
3. On first boot with no Wi-Fi set, it starts a setup portal (`Orcon15RF-Setup` /
   `orconsetup` → http://192.168.4.1). Enter your Wi-Fi (and optional MQTT host).
4. Open the device's web UI at its IP. **Pair**: power-cycle your HRC, then tap
   *Pair with my Orcon* within 3 minutes.

Wiring for each board's radio is in `src/config.h` (per-board pin blocks).

## Controls & MQTT

The web UI keeps daily controls up front (modes, timed boost, live metrics,
bypass, AC mode, pairing) and tucks diagnostics behind a ⚙ Settings toggle.

| Topic | Dir | Payload |
|-------|-----|---------|
| `orcon15rf/state` | out (retained) | JSON: mode, supply/exhaust %, temps, humidity, co2, bypass, filter, fault, fan_id, rssi, pairing status |
| `orcon15rf/cmd` | in | `{"mode":"high"}`, `{"timer":30}`, `{"bypass":"open"}`, `{"ac_mode":"exhaust"}`, `{"ac_sup":20,"ac_exh":40,"ac_byp":1}`, `{"pair":1}` |

Home Assistant auto-discovery is published on connect (also works with Domoticz).

## Airflow widget

A self-contained page at **`/widget.html`** shows an animated house — supply air
routed **through** the heat-exchanger core (bypass closed) or **around** it
(bypass open) — with live temps/RH/CO₂ and a row of quick buttons
(Away · Bypass · 1 · 2 · 3 · Boost · AC). Embed it in a dashboard:

```html
<iframe src="http://orcon15rf.local/widget.html" style="width:320px;height:420px;border:0"></iframe>
```

## On-screen UI (Guition 4848S040)

The `esp32-480480s040` build adds a 480×480 touch UI: swipeable pages (control,
custom AC, airflow, system), a bypass/mode grid, and an idle **airflow
screensaver** (tap to wake to the controls).

## Settings keeper (survive Orcon power loss)

The HRC forgets custom fan-speed settings on mains loss. Every fan-speed / bypass
you set is remembered in flash; with **Auto-restore** on, the ESP re-pushes them
~20 s after it sees the fan power-cycle (`042F`). MQTT: `{"autorestore":1}`,
`{"reapply":1}`, `{"forget":1}`.

## Diagnostics

- `/log` — rolling RX/TX log in `ramses_rf` string form (RX green, TX amber, with
  RSSI), plus device-ID / manual-clone controls.
- `/debug` — link counters and **byte rulers** for every status frame, for dialling
  in `31DA` offsets against a real unit.
- `/update` — browser firmware upload (no CLI).
- `pintest_4848` env — GPIO mapper for verifying CC1101 wiring on the Guition board.

## Tests

The protocol layer is plain C++ and host-tested before it touches hardware:

```bash
cd test
g++ -std=c++17 -I../src test_codec.cpp  ../src/ramses_codec.cpp                          -o test_codec  && ./test_codec
g++ -std=c++17 -I../src test_frames.cpp ../src/ramses_codec.cpp ../src/orcon_frames.cpp  -o test_frames && ./test_frames
```

## Status & caveats

- **Proven on hardware**: CC1101 RX+TX (Heltec + Guition), SX1276 RX+TX (Heltec V2),
  full control of a real HRC400, `31DA` decoding calibrated against the unit.
- **Model-specific**: `31DA` offsets and some tail bytes may vary across the
  HRC/MVS family — the `/debug` byte rulers make these easy to re-check.
- Reverse-engineered, unofficial, no warranty. See [`CREDITS.md`](CREDITS.md).

Full protocol write-up: [`docs/PROTOCOL.md`](docs/PROTOCOL.md).
