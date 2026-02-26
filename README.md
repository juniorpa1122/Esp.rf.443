# Esp.rf.443

ESP32 controller with **ST7789 TFT display**, **433 MHz RF** (RCSwitch) and
**infrared** (IRremote) with a 3-function on-screen menu.

---

## Functions

| # | Name | Description |
|---|------|-------------|
| 1 | **IR TV Baza** | Learn IR codes from any TV remote, store them and retransmit them |
| 2 | **Skaner sygnałów** | Passively sweep all RF protocols (1-12) and all IR protocols; display every signal found nearby |
| 3 | **Nadajnik sygnałów** | Blast test RF + IR codes, then listen for any echo or response – useful when the passive scan found nothing |

---

## Required Libraries (Arduino Library Manager)

| Library | Version |
|---------|---------|
| Adafruit GFX Library | ≥ 1.11 |
| Adafruit ST7789 | ≥ 1.10 |
| RCSwitch | ≥ 2.6 |
| IRremote | ≥ 4.0 |

---

## Hardware Wiring (ESP32 DevKit)

### ST7789 240×240 TFT

| TFT pin | ESP32 pin |
|---------|-----------|
| CS  | GPIO 5 |
| DC  | GPIO 2 |
| RST | GPIO 4 |
| SDA (MOSI) | GPIO 23 |
| SCL (SCK)  | GPIO 18 |
| VCC | 3.3 V |
| GND | GND |

### 433 MHz RF module

| RF module pin | ESP32 pin |
|---------------|-----------|
| TX DATA | GPIO 17 |
| RX DATA | GPIO 16 |
| VCC | 5 V |
| GND | GND |

### IR LED + receiver

| Component | ESP32 pin | Notes |
|-----------|-----------|-------|
| IR LED (anode) | GPIO 14 | via 33 Ω resistor |
| IR receiver (OUT) | GPIO 15 | TSOP4838 or equivalent |

### Navigation buttons (active-LOW, internal pull-up)

| Button | ESP32 pin |
|--------|-----------|
| UP | GPIO 32 |
| DOWN | GPIO 33 |
| SELECT | GPIO 25 |
| BACK | GPIO 26 |

---

## Building

Open `Esp_RF_IR_Controller/Esp_RF_IR_Controller.ino` in **Arduino IDE 2** (or
PlatformIO), select *ESP32 Dev Module* as your board, and click **Upload**.

---

## Usage

1. **Main menu** – use UP/DOWN to highlight a function, SELECT to enter.
2. **IR TV Baza**
   - *Ucz sie (Learn)* – point a remote at the IR receiver and press buttons;
     each unique code is stored (up to 20 codes, duplicates skipped).
   - *Wyslij (Send)* – scroll through stored codes, press SELECT to transmit.
3. **Skaner sygnałów** – the device automatically cycles RF protocols
   (1-12) while simultaneously listening on the IR receiver.
   Any detected signal is shown on the display.
4. **Nadajnik sygnałów** – test signals are sent every ~1.2 s on RF (all 12
   protocols) and IR (common TV power codes).
   Any response or echo is shown immediately.
   Press BACK to return to the menu.
