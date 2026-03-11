# ESP32 RF Tool

An all-in-one ESP32 firmware that combines:

- **WiFi Scanner** – lists nearby networks with RSSI
- **Bluetooth BLE Scanner** – lists nearby BLE devices with RSSI
- **433 MHz Jammer** – floods the 433 MHz band with noise bursts
- **433 MHz Receiver** – captures and stores up to 10 OOK/ASK codes
- **433 MHz Replay** – retransmits any stored code on demand

All output is shown on a **240 × 240 ST7789 TFT** display, navigated with four push-buttons.

---

## Hardware

| Component | Notes |
|-----------|-------|
| ESP32 dev board | Any standard 38-pin board |
| ST7789 240×240 TFT | SPI, 3.3 V logic |
| 433 MHz RX module | e.g. MX-RM-5V / XY-MK-5V |
| 433 MHz TX module | e.g. MX-FS-03V / XY-FST |
| 3 × push-buttons | Normally-open, to GND |

### Wiring

#### ST7789 TFT → ESP32

| TFT Pin | ESP32 GPIO |
|---------|-----------|
| CS      | 15        |
| DC      | 2         |
| RST     | 4         |
| SDA (MOSI) | 23     |
| SCL (SCK)  | 18     |
| VCC     | 3.3 V     |
| GND     | GND       |

#### 433 MHz Modules → ESP32

| Module     | ESP32 GPIO |
|------------|-----------|
| RX DATA    | 16        |
| TX DATA    | 17        |

#### Navigation Buttons → ESP32

| Button | ESP32 GPIO | Notes |
|--------|-----------|-------|
| UP     | 35        | External 10 kΩ pull-up to 3.3 V required (GPIO 35 is input-only) |
| DOWN   | 34        | External 10 kΩ pull-up to 3.3 V required (GPIO 34 is input-only) |
| SELECT | 32        | Uses internal pull-up – confirms / sends in submenus |
| BACK   | 33        | Uses internal pull-up – exits any submenu back to main menu |

---

## Software dependencies

### Arduino IDE

Install the following libraries via **Sketch → Include Library → Manage Libraries**:

| Library | Author | Version |
|---------|--------|---------|
| Adafruit ST7789 | Adafruit | ≥ 1.10.3 |
| Adafruit GFX Library | Adafruit | ≥ 1.11.9 |
| rc-switch | sui77 | ≥ 2.6.4 |

The ESP32 BLE library ships with the **esp32** board support package (Espressif).

### PlatformIO

All dependencies are declared in `platformio.ini`. Just run:

```bash
pio run          # build
pio run -t upload  # build + flash
pio device monitor # serial monitor
```

---

## Usage

1. Power on the ESP32 – the **splash screen** appears for 2 seconds.
2. The **main menu** is shown with 5 options.
3. Use **UP / DOWN** to highlight an option, then press **SELECT** to enter it.
4. Press **BACK** from any submenu to return to the main menu.

### WiFi Scan
Scans all 2.4 GHz channels and lists SSIDs with signal strength (dBm).  
Press **BACK** to return to the menu.

### BT Scan
Performs a 5-second active BLE scan and lists device names (or MAC addresses) with RSSI.  
Press **BACK** to return to the menu.

### 433 Jammer
Continuously transmits random 24-bit codes to saturate the 433 MHz band.  
Press **BACK** to stop and return to the menu.

> ⚠️ **Legal notice** – intentional radio jamming is illegal in most countries. Use only in an RF-shielded environment and only for legitimate testing purposes.

### 433 Receive
Listens on the 433 MHz receiver for OOK/ASK signals.  
Each unique received code (value, bit-length, protocol) is stored in RAM (up to 10 codes).  
Press **BACK** to stop and return to the menu.

### 433 Replay
Browse the stored codes with **UP / DOWN**.  
Press **SELECT** to retransmit the selected code.  
Press **BACK** to return to the menu.

---

## File structure

```
ESP32_RF_Tool/
└── ESP32_RF_Tool.ino   ← main Arduino sketch
platformio.ini           ← PlatformIO config & library deps
README.md
```

---

## License

MIT – see individual library licences for their respective terms.
