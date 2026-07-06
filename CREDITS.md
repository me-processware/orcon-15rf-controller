# Credits & attribution

This project is unofficial and reverse-engineered. It would not exist without
the community that decoded the Honeywell/RAMSES-II RF protocol and the
Orcon/Itho HVAC dialect. Huge thanks to everyone below.

## Protocol & prior art

- **[ramses_rf](https://github.com/zxdavb/ramses_rf)** — David Bonnes. The
  reference implementation and documentation of the RAMSES-II protocol. Licensed
  **MIT**. We used its message-layout knowledge, in particular the `31DA`
  extended-status byte offsets (temperatures, RH, CO₂, bypass, fan %). Protocol
  *facts* aren't copyrightable, but credit is due and gladly given.

- **[evofw3](https://github.com/ghoti57/evofw3)** — "ghoti57" (Peter Price). The
  original CC1101 **asynchronous-serial** gateway approach: put the CC1101 in
  transparent mode, feed the demodulated bitstream through a UART, and re-frame
  RAMSES in software. Our CC1101 back-end follows this method, and the CC1101
  register settings for 868.3 MHz / 38.4 kbps / ~50 kHz-deviation RAMSES are the
  same values evofw3 documents (they are dictated by the RF protocol and the
  CC1101 datasheet). evofw3 ships **without an explicit licence**; it is credited
  here as the reference for the method and register values. If you require a
  100% clean-room CC1101 config, use the RadioLib-based configuration instead.

- **[peeter123/orcon-15rf-protocol-decoder](https://github.com/peeter123)**,
  **tyz/orcon-mvs15**, and **[vd Brink](https://vdbrink.github.io/)** — Orcon-
  specific command decoding (`2411` service params, `22F7` bypass, `22F1` modes).

## Libraries

- **[RadioLib](https://github.com/jgromes/RadioLib)** (MIT) — SX126x / SX127x /
  CC1101 radio drivers.
- **[Arduino_GFX](https://github.com/moononournation/Arduino_GFX)** — ST7701 RGB
  panel driver for the Guition 4848S040 build.
- **[U8g2](https://github.com/olikraus/u8g2)** — SSD1306 OLED (Heltec builds).
- **[PubSubClient](https://github.com/knolleary/pubsubclient)** — MQTT.
- **[ArduinoJson](https://github.com/bblanchon/ArduinoJson)** — JSON.
- **Adafruit GFX fonts** — anti-aliased FreeSans headers (font data only).

## What is new here

Contributed back to the community as documentation (see `docs/PROTOCOL.md`):

- A standalone **Orcon 15RF emulator + HRC400 controller** (not just a gateway):
  device emulation, live state model, web UI, MQTT/HA discovery, on-screen touch
  UI, and a deliberate bind/pairing flow.
- **Using a Semtech SX1276 (SX127x) in direct/continuous FSK mode as a drop-in
  alternative to the CC1101** — the SX127x keeps the transparent mode the SX1262
  dropped, so the same async-serial trick runs on the on-board radio of common
  boards (e.g. Heltec WiFi LoRa 32 V2). As far as we know this hasn't been
  documented for RAMSES before.
- Calibrated Orcon **HRC400** `31DA` offsets and the `2411`/`22F7` control frames,
  verified against a real unit.

## Trademark / affiliation

"Orcon", "Honeywell", "evohome" and related marks belong to their respective
owners. This project is not affiliated with, endorsed by, or supported by any of
them. Use at your own risk.
