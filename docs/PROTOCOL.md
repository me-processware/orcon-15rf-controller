# Orcon 15RF / RAMSES‑II protocol — implementation notes

This is the working spec the firmware is built against. It targets the **Orcon
HRC400** (and the wider MVS‑15R / HRC‑3xx/4xx family) which speaks the Honeywell
**RAMSES‑II** RF protocol — the same protocol used by evohome, Itho, Nuaire, etc.

Sources cross‑checked: `peeter123/orcon-15rf-protocol-decoder` (frame constants &
Manchester table), `tyz/orcon-mvs15` (Orcon code table), `ramses_rf` /
`ramses_protocol` wiki (command semantics), and the existing
`orcon_hrc400_technische_knowhow.md` in this repo.

> The goal is to emulate the **15RF display controller** — the CO₂/RH wall unit,
> *not* the plain 6‑button remote. That unit is **bidirectional**: it transmits
> fan commands *and* sensor demand, and it receives the fan's status broadcasts.

---

## 1. Physical layer (RF)

| Parameter            | Value                                   |
|----------------------|-----------------------------------------|
| Carrier              | 868.300 MHz (868.299866 measured)       |
| Modulation           | 2‑FSK / GFSK                            |
| Bit rate             | 38.4 kbaud (38.385 k)                   |
| FSK deviation        | 50.78 kHz                               |
| RX filter bandwidth  | 325 kHz                                 |
| Encoding             | Manchester (nibble table, see §3)       |
| Byte framing         | UART‑style: 1 start (0) + 8 data LSB‑first + 1 stop (1) |
| Encryption           | None (plain, device‑ID authenticated)   |

The on‑air stream is a continuous 38.4 kbps FSK bitstream in which every logical
byte is wrapped in start/stop bits exactly like a UART character. This is why
community projects drive a **CC1101 in async/serial mode** and bit‑bang the
framing. The SX1262 cannot bit‑bang raw RX bits, so we instead **pre‑compute the
whole on‑air byte buffer** and use the radio in raw packet mode (see
`ramses_radio` and §6).

---

## 2. Frame structure (logical, after de‑framing & Manchester decode)

```
+--------+------------------+----------------+--------+--------+---------+----------+
| header | addr0 addr1 addr2| param0  param1 | cmd16  | length | payload | checksum |
| 1 byte | 0/3 bytes each   | 0/1 byte each  | 2 bytes| 1 byte | n bytes | 1 byte   |
+--------+------------------+----------------+--------+--------+---------+----------+
```

### 2.1 Header byte
The header encodes a *flags* index and two "has‑param" bits:

* `header & 0x02` -> param0 present
* `header & 0x01` -> param1 present
* `flags = HEADER_FLAGS[(header >> 2) & 0x0F]`

`HEADER_FLAGS` (16‑entry lookup, from the decoder):
```
0x0F 0x0C 0x0D 0x0B 0x27 0x24 0x25 0x23 0x47 0x44 0x45 0x43 0x17 0x14 0x15 0x13
```

Flags bit meaning:

| Bit  | Meaning            |
|------|--------------------|
| 0x01 | addr0 present      |
| 0x02 | addr1 present      |
| 0x04 | addr2 present      |
| 0x08 | **request**  (RQ)  |
| 0x10 | **response** (RP)  |
| 0x20 | **information** (I) |
| 0x40 | **write** (W)      |

The four message *types* map to RAMSES‑II `I / RQ / RP / W`. All messages we care
about use **3 address fields**. The codec stores a small table mapping
`(type, n_addr, has_param0/1)` to the header byte, and the host unit test proves
the round trip, so we never reason about the bit math by hand.

### 2.2 Device addresses (3 bytes each)
A device ID is 3 bytes -> `(class << 18) | serial`, printed as `CC:SSSSSS`.

* Top 6 bits = device **class** (e.g. `18` HGI gateway, `32` FAN/HVAC,
  `29` HRU/REM, `37` CO₂ sensor ...).
* Low 18 bits = unit serial.

For a 3‑address `I` broadcast the convention is `addr0 = source (us)`,
`addr1 = dest (fan or broadcast 63:262143)`, `addr2 = us`. Device IDs are set in
`config.h`.

### 2.3 Command (2 bytes, big‑endian)
The 16‑bit command code, e.g. `0x22F1`. See §4.

### 2.4 Length + payload
`length` = number of payload bytes that follow.

### 2.5 Checksum (1 byte)
`checksum = (-(sum of all bytes header..last_payload)) & 0xFF`, i.e. the sum of
**every decoded byte including the checksum** is `0 (mod 256)`.

---

## 3. On‑air encoding (the part the SX1262 makes interesting)

Working **outwards** from the logical frame to RF:

1. **Manchester‑encode** each payload byte into **two** "wire bytes". Each nibble
   (4 bits) maps to one wire byte via `MANCH_ENC` (high nibble first):
   ```
   nibble:  0    1    2    3    4    5    6    7
   wire:   0xAA 0xA9 0xA6 0xA5 0x9A 0x99 0x96 0x95
   nibble:  8    9    A    B    C    D    E    F
   wire:   0x6A 0x69 0x66 0x65 0x5A 0x59 0x56 0x55
   ```
   Valid wire bytes are exactly these 16 values; anything else on RX = Manchester
   error.

2. Prepend the **sync words** (literal, *not* Manchester‑encoded):
   ```
   SYNC = 0xFF 0x00 0x33 0x55 0x53
   ```

3. Append the **stop/end** marker wire byte `0x35` after the payload.

4. **UART‑frame** every wire byte (sync, manchester data, stop): prepend a `0`
   start bit, emit the 8 bits **LSB‑first**, append a `1` stop bit -> 10 bits/byte.

5. Prepend a **preamble** of alternating bits. A UART‑framed `0x55` byte is
   `0 1 0 1 0 1 0 1 0 1` = perfectly alternating, so the preamble seamlessly
   merges into the framed sync. We emit several `0x55` training bytes.

The receiver does the reverse: lock to the alternating preamble, slice 10‑bit
groups, drop start/stop, reverse to LSB‑first, match SYNC, Manchester‑decode in
pairs until the `0x35` stop byte, verify checksum, parse.

---

## 4. Command set (Orcon HRC400 / MVS‑15)

From the `tyz/orcon-mvs15` support table plus `ramses_rf`:

| Code   | Meaning             | Who sends        | Notes |
|--------|---------------------|------------------|-------|
| `042F` | Power‑cycle counter | FAN on powerup   | used to auto‑discover the fan |
| `10E0` | Device info         | FAN, CO₂, us     | 24 h broadcast; RQ‑able |
| `10E1` | Device ID           | FAN, CO₂         | RQ‑able |
| `1298` | CO₂ level (ppm)     | **CO₂ sensor**   | ~10 min; u16/u24 ppm |
| `12A0` | Indoor humidity     | FAN              | RQ‑able; %RH + temps |
| `1FC9` | RF bind             | all              | binding handshake (offer/confirm) |
| `22F1` | Fan mode (set)      | **REM / us**     | the main "set speed" command |
| `22F3` | Fan mode with timer | **REM / us**     | timed boost (15/30/60 min) |
| `31D9` | Fan state           | FAN              | ~5 min; mode + fault flag |
| `31DA` | Fan extended status | FAN              | %, temps, bypass, filter, flow |
| `31E0` | Vent demand         | **CO₂ sensor**   | ~5 min; demand 0–100 % |
| `10D0` | Filter / remaining  | FAN              | filter life, reset |

### 4.1 `22F1` fan‑mode payload
3‑byte payload `00 NN 07`, `NN` selects the preset and `07` is the Orcon setting
count:

| Mode      | NN   | payload    |
|-----------|------|------------|
| Away      | 0x00 | `00 00 07` |
| Low (1)   | 0x01 | `00 01 07` |
| Medium (2)| 0x02 | `00 02 07` |
| High (3)  | 0x03 | `00 03 07` |
| Auto      | 0x04 | `00 04 07` |
| Auto2     | 0x05 | `00 05 07` |
| Boost     | 0x06 | `00 06 07` |
| Disable   | 0x07 | `00 07 07` |

### 4.2 `22F3` timed‑boost payload
`00 TT ZZ …` — high speed for `TT` minutes (15/30/60). Firmware builds it in
`make_22F3`; tail bytes are easy to tweak after a capture.

### 4.3 `31E0` vent demand (we transmit as the CO₂ sensor)
8 bytes, e.g. `00 00 00 DD 00 00 00 00`, `DD` ~ demand. Seen example:
`0000000001001E00`. We map 0–100 % demand to that byte.

### 4.4 `1298` CO₂ (we transmit as the CO₂ sensor)
`00` + u16 ppm; firmware builds it in `make_1298(ppm)`.

### 4.5 `31D9` / `31DA` (we *receive* from the fan)
`31D9`: fan mode + fault/status flag -> drives "current preset" feedback.
`31DA`: extended — supply/exhaust %, four temps, bypass position, filter
remaining, RH. Parsed best‑effort; byte offsets are documented inline in
`orcon.cpp` and easy to adjust once you capture real frames (`DEBUG_RX_HEX`).

---

## 5. Binding (pairing) the emulated 15RF to the fan

Two routes, both supported:

1. **Physical pairing window** (recommended first): power‑cycle the HRC400; it
   opens a ~3‑minute join window. Trigger "Pair" in the web UI, which sends our
   `1FC9` offer plus a couple of `22F1`s with our device ID. The fan then accepts
   commands from our ID.

2. **`1FC9` handshake**: offer (`I 1FC9`) -> fan confirm (`RP/W 1FC9`) -> ack.
   The payload lists `(code, our_device_id)` tuples for each code we send
   (`22F1`, `31E0`, `1298`). Implemented in `orcon.cpp::send_bind()`. Because the
   exact confirm varies by model, the physical window is the reliable path and
   `1FC9` is best‑effort.

Pick the device IDs in `config.h`. Use a class byte the fan expects for a
remote/CO₂ sensor and keep the serial unique.

---

## 6. Why SX1262 needs software Manchester (and the CC1101 fallback)

* **CC1101** has hardware Manchester + a transparent async serial mode -> it can
  stream raw RX bits on a GPIO. That's why every existing project uses it.
* **SX1262** (built into the Heltec V3) is a *packet* radio: GFSK only, **no**
  hardware Manchester, and **no** transparent raw‑bit RX. So:
  * **TX:** build the entire on‑air byte buffer (preamble + sync + framed
    Manchester payload + stop) in firmware and transmit it as a raw fixed‑length
    GFSK packet — the radio just FSK‑modulates our bits. Reliable.
  * **RX:** program the SX1262 hardware **sync word** to a slice of the known
    on‑air pattern (preamble tail + start of SYNC, <= 8 bytes), receive into the
    FIFO, then software‑deframe + Manchester‑decode. Works, but is the riskier
    half — hence the build‑time **CC1101 fallback** (`-D RADIO_CC1101`) that does
    framing in hardware if SX1262 RX proves flaky.

The `ramses_codec` layer is radio‑independent and host‑tested, so only the thin
`ramses_radio` layer differs between SX1262 and CC1101.

---

## 7. Radio back-ends in practice (CC1101, SX1276, SX1262)

The decisive capability is a **transparent / continuous FSK mode**: the chip
outputs the raw demodulated NRZ bitstream on a GPIO, which we clock into a
hardware UART at 38400 8N1 (the RAMSES per-byte framing *is* 38400 8N1) and hand
to `extractWireFrame()`. Chips without it cannot do RAMSES RX by this method.

### 7.1 CC1101 async serial (recommended) — `RADIO_CC1101_ASYNC`
The evofw3 method. Configure the CC1101 registers for 868.3 MHz / 38.4 kbps /
~50 kHz deviation, `IOCFG2 = 0x0D` (GDO2 = async data out), `PKTCTRL0 = 0x31`
(async serial). TX: UART TX → GDO0. RX: GDO2 → UART RX. One CC1101 does both.

### 7.2 SX1276 direct mode (single-board alternative) — `RADIO_SX1276`
**This is the part worth sharing.** The SX127x family kept the "continuous /
direct" FSK mode that the SX126x (SX1262/1268) dropped, so the CC1101 trick runs
on the *on-board* radio of common boards (e.g. Heltec WiFi LoRa 32 **V2**):

* **RX:** `beginFSK(868.3, 38.4, 50.0, rxBw)`, then `receiveDirect()`. The
  demodulated data appears on **DIO2** exactly like the CC1101's GDO2 — wire DIO2
  to an ESP UART RX pin and reuse the same async decode. On the Heltec V2, DIO2
  is broken out to GPIO34. (Many bare SX1276 modules only expose DIO0/DIO1 — DIO2
  access is the gating requirement.)
* **TX:** FSK **packet** mode — hand the packet engine the full on-air frame as
  raw payload (sync `55 53`, CRC and whitening off). Direct-mode TX also works
  but needs the ESP to *drive* the data pin, so packet TX is cleaner.

RxBw is clamped to the SX1276 maximum of 250 kHz. As far as we know this
SX127x-as-CC1101-alternative hasn't been documented for RAMSES before.

### 7.3 SX1262 (not recommended) — `RADIO_SX1262`
Packet-only, no transparent mode. TX works (stuff the Manchester frame as a
fixed-length GFSK packet); RX via the hardware sync word is unreliable and never
framed real RAMSES in testing. Kept only as an experiment. Note the Heltec V3/V4
route the antenna through an RF switch on **DIO2** — `setDio2AsRfSwitch(true)` is
mandatory or the radio is electrically disconnected.
