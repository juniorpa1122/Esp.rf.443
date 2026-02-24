# ESP32 Multi-Tool – ILI9488 Touchscreen + IR + RF 433

An **ESP32** project that puts a touch-driven menu on a 3.5″ ILI9488 SPI
display and lets you:

| Feature | Description |
|---------|-------------|
| **IR Remote** | Record, replay and send pre-defined IR codes for Samsung, LG, Sony, Philips, Panasonic, Sharp, Pioneer and Denon TVs |
| **RF 433 MHz** | Scan incoming OOK packets (rc-switch protocols) and store up to 16 of them, then replay any stored code |
| **Signal Scanner** | Passive detector – shows whether the RF or IR receiver sees any signal at all, with event counters; no signals are transmitted |

---

## Hardware required

| Component | Notes |
|-----------|-------|
| ESP32 DevKit (30-pin or 38-pin) | Any variant |
| ILI9488 3.5″ SPI TFT display with XPT2046 touch | Landscape 480 × 320 |
| 433 MHz superheterodyne RX module (SRX882, RXB6, …) | Needs VCC 5 V, DATA output → GPIO 34 |
| 433 MHz TX module | DATA → GPIO 12 |
| TSOP38238 (or compatible) IR receiver | DATA → GPIO 35 |
| IR LED + NPN transistor driver | Base → GPIO 14 |

---

## Wiring

### ILI9488 Display (SPI)

| Display pin | ESP32 GPIO |
|-------------|-----------|
| VCC / LED | 3.3 V |
| GND | GND |
| CS | GPIO **15** |
| RESET | GPIO **4** |
| DC/RS | GPIO **2** |
| SDI (MOSI) | GPIO **23** |
| SCK | GPIO **18** |
| SDO (MISO) | GPIO **19** |

### XPT2046 Touch (same SPI bus)

| Touch pin | ESP32 GPIO |
|-----------|-----------|
| T_CS | GPIO **5** |
| T_IRQ | GPIO **27** |
| T_DIN | GPIO **23** (shared MOSI) |
| T_DO | GPIO **19** (shared MISO) |
| T_CLK | GPIO **18** (shared SCK) |

### RF / IR

| Signal | GPIO |
|--------|------|
| RF 433 RX data | **34** (input-only) |
| RF 433 TX data | **12** |
| IR RX data | **35** (input-only) |
| IR TX (LED driver) | **14** |

---

## Software / Libraries

Install these via the PlatformIO Library Manager or in Arduino IDE:

| Library | Version |
|---------|---------|
| [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) | ≥ 1.11 |
| [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341) | ≥ 1.6 (SPI driver compatible with ILI9488) |
| [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) | ≥ 1.4 |
| [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) | ≥ 2.8 |
| [rc-switch](https://github.com/sui77/rc-switch) | ≥ 2.6 |

> **PlatformIO users:** All dependencies are declared in `platformio.ini` –
> just run `pio run` and they are fetched automatically.

---

## Building

### PlatformIO (recommended)

```bash
# Install PlatformIO Core if you haven't already
pip install platformio

# Build
pio run

# Upload
pio run --target upload

# Serial monitor
pio device monitor
```

### Arduino IDE

1. Install all libraries listed above.
2. Open `src/main.cpp`, rename it to `ESP_RF_433.ino` and place it in a
   folder called `ESP_RF_433`.
3. Copy `ir_module.h`, `rf433_module.h`, `scanner_module.h` into the same
   folder.
4. Select **Board → ESP32 Dev Module**, upload.

---

## Touch calibration

If touch coordinates feel off, adjust the four constants at the top of
`src/main.cpp`:

```cpp
#define TOUCH_X_MIN  200
#define TOUCH_X_MAX  3700
#define TOUCH_Y_MIN  200
#define TOUCH_Y_MAX  3700
```

Run the sketch, tap each corner of the screen and read the raw values from
the serial monitor (`[TOUCH] raw(…)` lines), then set the min/max accordingly.

---

## Menu structure

```
Main Menu
├── IR Remote
│   ├── TV Codes   – sends POWER code for 8 popular brands
│   ├── Record IR  – captures next IR burst and stores it
│   └── Replay IR  – retransmits last captured signal
├── RF 433 MHz
│   ├── Scan 433   – listens for OOK packets; stores up to 16
│   └── Replay     – tap any stored code to retransmit it
└── Scanner (passive)
    – live view of RF 433 & IR activity with event counters
    – RESET CNT button clears counters
```

---

## Source layout

```
src/
├── main.cpp          – setup/loop, touchscreen menu
├── ir_module.h       – IR send / receive (IRremoteESP8266)
├── rf433_module.h    – 433 MHz scan / replay (rc-switch)
└── scanner_module.h  – passive signal presence detector
platformio.ini        – PlatformIO project config
```

---

## Notes

* The **Signal Scanner** is **purely passive** – it only reads the DATA pin
  of the receivers and reports whether a signal is present.  No jamming,
  replay or decoding is performed in that mode.
* RC-Switch supports protocols 1–12.  If your remote uses an unusual timing,
  you may need to call `rcSwitch.setReceiveTolerance()` in `rf433_module.h`.
* For the ILI9488 display the Adafruit ILI9341 driver is used because both
  chips share the same SPI command set up to initialisation.  If colours look
  inverted, call `tft.invertDisplay(true)` after `tft.begin()`.
