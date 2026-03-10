# Esp.rf.443

RF 433 MHz + IR + WiFi Controller with **ILI9488 TFT menu interface**

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32 (or ESP8266 with pin adjustments) |
| Display | ILI9488 3.5″ SPI TFT – 320 × 480 px |
| Touch | XPT2046 (optional) |
| RF | 433 MHz transceiver module |
| IR | IR transmitter + receiver |

## Project structure

```
esp_menu_ili9488/
├── esp_menu_ili9488.ino   # Main sketch – setup(), loop(), button & touch input
├── config.h               # Pin definitions, colours, layout constants
├── menu.h                 # Menu data structures (MenuItem, Menu) + MenuEngine API
└── menu.cpp               # MenuEngine implementation (rendering, navigation)
```

## Menu skeleton

The menu system supports four item types:

| Type | Description |
|------|-------------|
| `SUBMENU` | Opens a nested menu page |
| `ACTION` | Calls a `void()` callback |
| `TOGGLE` | Flips a `bool` variable (shown as **ON / OFF**) |
| `VALUE` | Edits an `int` variable within `[min, max]` with a given step |

### Default menu tree

```
Main Menu (ESP Controller)
├── RF 433 MHz
│   ├── RF Enable        [TOGGLE]
│   ├── RF Channel       [VALUE 1-8]
│   ├── Send Signal      [ACTION]
│   ├── Learn Code       [ACTION]
│   └── < Back
├── Infrared (IR)
│   ├── IR Enable        [TOGGLE]
│   ├── IR Protocol      [VALUE 0-2]
│   ├── Send Signal      [ACTION]
│   ├── Learn Code       [ACTION]
│   └── < Back
├── WiFi
│   ├── WiFi Enable      [TOGGLE]
│   ├── WiFi Channel     [VALUE 1-13]
│   ├── Scan Networks    [ACTION]
│   ├── Connect          [ACTION]
│   └── < Back
└── Settings
    ├── Brightness       [VALUE 10-100 %]
    ├── Sound            [TOGGLE]
    ├── Reboot           [ACTION]
    ├── About            [ACTION]
    └── < Back
```

## Navigation

| Button | Action |
|--------|--------|
| **UP** | Move selection up |
| **DOWN** | Move selection down |
| **SELECT / OK** | Confirm / toggle / enter sub-menu |
| **BACK** | Return to parent menu |

All buttons are active-LOW with internal pull-up resistors enabled.  
Touch input (XPT2046) is supported when `TOUCH_CS` is defined.

## Required libraries

Install via **Arduino Library Manager**:

- **TFT_eSPI** – configure `User_Setup.h` for ILI9488 + ESP32 VSPI pins
- **XPT2046_Touchscreen** *(optional)*

### TFT_eSPI `User_Setup.h` key settings

```cpp
#define ILI9488_DRIVER
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO 19
```

## Pin mapping (ESP32)

See `config.h` for all pin assignments.

```
TFT CS   → GPIO 5    RF TX  → GPIO 26
TFT DC   → GPIO 2    RF RX  → GPIO 36
TFT RST  → GPIO 4    IR TX  → GPIO 13
TFT BL   → GPIO 32   IR RX  → GPIO 14
TOUCH CS → GPIO 15
BTN UP   → GPIO 34   BTN SELECT → GPIO 33
BTN DOWN → GPIO 35   BTN BACK   → GPIO 25
```
