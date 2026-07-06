# Technische Knowhow: Custom Stuurprint voor Orcon HRC400 WTW-unit

**Auteur:** Processware
**Datum:** Juni 2026

---

## 1. Introductie en Doelstelling

Dit document bevat de technische bevindingen rondom de haalbaarheid van een custom stuurprint voor de **Orcon HRC400** warmteterugwinningsunit (WTW). De originele sturing wordt door gebruikers vaak als inefficiënt ervaren, met beperkte toerentalregeling, moeilijke bypass-aansturing en de onmogelijkheid om de toevoer- en afvoerventilator onafhankelijk te schakelen.

De doelstelling was om te onderzoeken of een custom stuurprint op basis van een ESP32 (specifiek de Heltec WiFi LoRa 32 V3) haalbaar is.

---

## 2. Hardware Analyse van de Orcon HRC400

### 2.1 Ventilatormotoren

De Orcon HRC400 maakt gebruik van EC-ventilatoren (Electronically Commutated). Dit zijn borstelloze wisselstroommotoren met ingebouwde besturingselektronica.

*   **Voeding:** 230V AC / 50Hz (rechtstreeks vanaf het lichtnet via connectoren X13 en X14 op de hoofdprint). [1]
*   **Aansturing:** De toerentalregeling gebeurt **niet** door het variëren van de voedingsspanning (bijv. via PWM of een triac op de 230V-lijn). De aansturing verloopt via een **Modbus RTU (RS485)** datakanaal dat parallel aan de 230V-voeding loopt. [1]

**Conclusie voor custom hardware:** Een directe PWM- of 0-10V-regeling is niet mogelijk. Om de ventilatoren (onafhankelijk) aan te sturen op het diepste niveau, is een RS485-transceiver (zoals een MAX3485) nodig, gekoppeld aan de ESP32, om Modbus-commando's naar de motoren te sturen. Hierbij moet de custom software echter wel kritieke veiligheidsfuncties, zoals de vorstbeveiliging van de warmtewisselaar, zelfstandig afhandelen.

---

## 3. Analyse van het Orcon 15RF 868MHz Protocol

Een veelgebruikte en eenvoudigere methode om de Orcon-units aan te sturen zonder de interne Modbus-sturing en veiligheidsmechanismen te omzeilen, is via het draadloze 868MHz protocol van de 15RF afstandsbediening.

### 3.1 Protocol Specificaties

Het protocol dat Orcon gebruikt is gebaseerd op het **Honeywell RAMSES II** protocol (ook gebruikt door Itho, Evohome, etc.). [2]

*   **Frequentie:** 868.3 MHz
*   **Modulatie:** 2-FSK
*   **Bitrate:** 38.4 kbaud
*   **Frequentiedeviatie:** 50.8 kHz
*   **RX Filter Bandbreedte:** 325 kHz
*   **Datacodering:** Manchester-codering [3]
*   **Encryptie:** Het protocol is **niet** versleuteld (geen AES, geen rolling code). Het is een proprietary protocol met een specifieke framing en Manchester-codering, maar de data is in plain text (unencrypted RF traffic) te lezen en te injecteren. [2]

### 3.2 Pairing Mechanisme

Het systeem gebruikt Device ID's voor authenticatie in plaats van encryptie. Een nieuwe zender moet zich aanmelden (pairen) bij de WTW-unit.
1.  Stroom van de WTW-unit halen.
2.  Stroom inschakelen (activeert een pairing-venster van 3 minuten).
3.  De nieuwe zender (bijv. de ESP32) stuurt een join-commando met zijn eigen gegenereerde Device ID.
4.  De WTW-unit accepteert voortaan commando's van dit ID. [4]

### 3.3 Bestaande Implementaties

De community heeft het protocol succesvol reverse-engineered en geïmplementeerd:
*   **Python Decoder:** `peeter123/orcon-15rf-protocol-decoder` (voor Raspberry Pi + CC1101). [3]
*   **ESPEasy Plugin:** De `P118` (Itho/Orcon) plugin voor ESPEasy, origineel geschreven voor Itho, is aangepast en werkt betrouwbaar voor Orcon met een CC1101 module. [5]

**Beperking van de RF-methode:** Via deze route kunnen alleen de standaard modi (stand 1, 2, 3, auto, timer, away) worden aangestuurd. Onafhankelijke ventilatorsturing of traploze regeling is via de RF-interface niet mogelijk.

---

## 4. Hardware Platform: Heltec WiFi LoRa 32 V3

De Heltec WiFi LoRa 32 V3 is een ontwikkelbord gebaseerd op de ESP32-S3, uitgerust met een OLED-scherm en een geïntegreerde Semtech SX1262 LoRa/FSK transceiver. [6]

### 4.1 Ingebouwde SX1262 Transceiver

De ingebouwde SX1262 chip ondersteunt FSK-modulatie op 868MHz. Theoretisch zou deze gebruikt kunnen worden om het Orcon-protocol te zenden. Er is echter een fundamenteel hardware-verschil met de traditioneel gebruikte CC1101:

*   **CC1101:** Beschikt over **hardwarematige ondersteuning voor Manchester-codering**. [7] De microcontroller hoeft slechts ruwe bytes te sturen.
*   **SX1262:** Ondersteunt in FSK-modus uitsluitend **NRZ** (Non-Return-to-Zero) codering met optionele LFSR-gebaseerde *whitening*. De chip heeft **geen** hardwarematige Manchester-codering. [8]

Om de SX1262 te gebruiken voor het Orcon-protocol, moet de Manchester-codering (en decodering bij ontvangst) volledig in software (firmware) worden geïmplementeerd bovenop de ruwe FSK-bitstream. Er zijn momenteel geen kant-en-klare bibliotheken (zoals RadioLib) die dit out-of-the-box ondersteunen voor het RAMSES II protocol.

### 4.2 Externe CC1101 Toevoegen

Gezien de complexiteit van softwarematige Manchester-codering, is de meest pragmatische oplossing om een externe CC1101 module aan de Heltec V3 toe te voegen.

*   De SX1262 op de Heltec V3 gebruikt de SPI-pinnen GPIO9 (CLK), GPIO10 (MOSI) en GPIO11 (MISO). [9]
*   De ESP32-S3 ondersteunt meerdere apparaten op dezelfde SPI-bus, mits elk apparaat een eigen Chip Select (CS) pin heeft.
*   Een externe CC1101 kan worden aangesloten op dezelfde SPI-pinnen (9, 10, 11), waarbij een vrije GPIO (bijv. GPIO26 of GPIO47) als CS-pin wordt gebruikt.
*   De GDO2-pin (interrupt) van de CC1101 kan op een vrije GPIO (bijv. GPIO5 of GPIO6) worden aangesloten.

---

## 5. Conclusie en Advies

Het ontwikkelen van een eigen sturing voor de Orcon HRC400 is haalbaar, maar vereist een keuze in aanpak op basis van de gewenste functionaliteit:

1.  **Volledige Controle (Modbus RS485):**
    *   *Doel:* Traploze regeling, onafhankelijke ventilatorsturing, bypass-controle.
    *   *Methode:* Aftappen van de RS485-bus tussen het moederbord en de 230V EC-ventilatoren met een MAX3485 transceiver.
    *   *Nadeel:* Zeer complex. Het vereist het reverse-engineeren van de Modbus-registers en het zelfstandig programmeren van kritieke veiligheidslogica (vorstbeveiliging).

2.  **Standaard Automatisering (868MHz RF met CC1101):**
    *   *Doel:* Betrouwbare integratie in domotica (Home Assistant, sensoren) met behoud van de fabrieksveiligheden.
    *   *Methode:* Gebruik van een ESP32 (bijv. de Heltec V3) in combinatie met een externe **CC1101** module. Bestaande software zoals de ESPEasy P118 plugin kan (met aanpassing van de SPI-pinnen voor de ESP32-S3) direct worden gebruikt.
    *   *Nadeel:* Beperkt tot de standaard standen (1, 2, 3, auto).

**Advies voor de Heltec V3:** Hoewel het technisch een interessante uitdaging is om de ingebouwde SX1262 via softwarematige Manchester-codering te laten praten met de Orcon, is dit een aanzienlijk software-ontwikkeltraject. De meest efficiënte route is het aansluiten van een goedkope externe CC1101-module op de vrije GPIO- en gedeelde SPI-pinnen van de Heltec V3.

---

## 6. Referenties

[1] Orcon Installatiehandleiding HRC-EcoMax/MaxComfort (Nederlands).
[2] GitHub: ramses-rf/ramses_protocol - RAMSES-II is a Honeywell RF protocol for HVAC and CH/DHW. https://github.com/ramses-rf/ramses_protocol
[3] GitHub: peeter123/orcon-15rf-protocol-decoder - ORCON RF15 Protocol Decoder. https://github.com/peeter123/orcon-15rf-protocol-decoder
[4] vd Brink Home Automations: Control an Orcon mechanic ventilation system from Home Assistant. https://vdbrink.github.io/esphome/orcon_mechanic_ventilation.html
[5] GitHub: svollebregt/ESPEASY_Plugin_ITHO - Plugin for ESPEasy regarding a ITHO/Orcon Fan remote. https://github.com/svollebregt/ESPEASY_Plugin_ITHO
[6] Heltec Automation: WiFi LoRa 32 V3 Product Overview. https://heltec.org/project/wifi-lora-32-v3/
[7] Texas Instruments: CC1101 Low-Power Sub-1 GHz RF Transceiver Datasheet.
[8] Semtech: SX1261/2 Long Range, Low Power, sub-GHz RF Transceiver Datasheet.
[9] ESPHome Devices: Heltec WiFi LoRa 32 V3 GPIO Pinout. https://devices.esphome.io/devices/heltec-wifi-lora-32-v3/
