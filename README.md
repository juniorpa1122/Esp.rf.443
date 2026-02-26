# ESP32 RF/IR Multi-Tool

A feature-rich ESP32 project combining a 3.5″ ILI9488 touchscreen menu with
433 MHz RF (RCSwitch) and IR remote control (IRremote ≥ 4.0).

---

## Features

| Feature | Description |
|---------|-------------|
| **IR Remote – Send All TVs** | Sends power code to 16 TV brands (Samsung, LG, Sony, Philips, Panasonic, Sharp, Toshiba, Hisense, TCL, Haier, Hitachi, Vizio, Pioneer, JVC, Onkyo, Denon) |
| **IR Remote – Volume Up** | Volume-up for Samsung, LG, Sony, Philips |
| **IR Capture** | Captures any IR signal and displays protocol, address, command and raw timings |
| **IR Replay** | Retransmits the last captured IR signal |
| **433 MHz Scan** | Receives and displays any OOK/ASK 433 MHz signal (value, hex, binary, bit-length, protocol) |
| **433 MHz Replay** | Retransmits the last captured 433 MHz signal |
| **IR Activity Monitor** | Passive scroll-log of all IR signals (colour-coded by protocol) |
| **433 MHz Activity Monitor** | Passive scroll-log of all 433 MHz signals |
| **Settings** | Pin map, library versions |

---

## Hardware

### Bill of Materials

| Component | Notes |
|-----------|-------|
| ESP32 Dev Board | Any 38-pin WROOM/WROVER |
| ILI9488 3.5″ TFT + XPT2046 touch | 480 × 320, SPI |
| VS1838 / TSOP38238 IR receiver | 38 kHz demodulator |
| IR LED + NPN transistor | For IR transmit |
| FS1000A or similar 433 MHz TX | OOK/ASK |
| RXB6 or XY-MK-5V 433 MHz RX | Superheterodyne preferred |

### Pin Map

```
ESP32 GPIO  →  Function
──────────────────────────────────────────────
18           SPI CLK  (shared TFT + Touch)
23           SPI MOSI (shared TFT + Touch)
19           SPI MISO (shared TFT + Touch)
15           TFT CS
 2           TFT DC/RS
 4           TFT RST
32           TFT Backlight (PWM)
14           XPT2046 Touch CS
27           XPT2046 Touch IRQ
33           IR LED TX
35           IR Receiver RX
25           433 MHz TX
26           433 MHz RX
```

---

## Software Setup

### PlatformIO (recommended)

```bash
pio run --target upload
pio device monitor
```

`platformio.ini` pulls all required libraries automatically.

### Arduino IDE

Install these libraries via the Library Manager:

| Library | Author |
|---------|--------|
| Adafruit GFX Library | Adafruit |
| Adafruit ILI9488 | Adafruit |
| XPT2046_Touchscreen | Paul Stoffregen |
| IRremote ≥ 4.0 | Arduino-IRremote |
| rc-switch | sui77 |

Then open `src/main.cpp` (rename to `esp32_rf_ir_menu.ino` and place it inside a
folder of the same name).

---

## Touch Calibration

If the touch response is off, adjust these constants at the top of `main.cpp`:

```cpp
#define TS_MINX  300
#define TS_MAXX  3800
#define TS_MINY  300
#define TS_MAXY  3800
```

---

## Menu Structure

```
Main Menu
├── IR Remote
│   ├── Send POWER to ALL TV Brands  (16 brands)
│   ├── TV Volume UP (4 brands)
│   ├── Capture IR Signal
│   └── Replay Captured IR
├── 433 MHz RF
│   ├── Scan 433 MHz Signals
│   └── Replay Last Signal
├── Signal Scanner  (passive – no TX)
│   ├── IR Activity Monitor
│   └── 433 MHz Activity Monitor
└── Settings
    └── Pin Map & Library Info
```

---

## License

MIT
