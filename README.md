# Esp.rf.443
RF 433 MHz + IR Controller with TFT 3.5" display (TFT_eSPI, no touch)

## Opis / Description

Sterownik IR i RF 433 MHz na ESP32 z wyświetlaczem TFT 3.5" (ILI9488 / ST7796).
Interfejs obsługiwany trzema przyciskami fizycznymi (GÓRA / DÓŁ / OK).
Bez dotyku. Biblioteka wyświetlacza: **TFT_eSPI** (Bodmer) – nie Adafruit.

ESP32 IR + RF 433 MHz controller with a 3.5" TFT display (ILI9488 / ST7796).
Interface is driven by three physical buttons (UP / DOWN / OK).
No touchscreen. Display library: **TFT_eSPI** (by Bodmer) – not Adafruit.

## Potrzebne biblioteki / Required Libraries

Install via Arduino Library Manager or PlatformIO:

| Library | Author |
|---|---|
| **TFT_eSPI** | Bodmer |
| **IRremoteESP8266** | crankyoldgit |
| **rc-switch** | sui77 |

> ⚠️ Copy `User_Setup.h` (in this repo) into your
> `Arduino/libraries/TFT_eSPI/` folder and **rename** the original
> `User_Setup.h` (e.g. to `User_Setup.h.bak`) before replacing it.

## Podłączenie / Wiring (ESP32)

### TFT 3.5" SPI (ILI9488)
| TFT pin | ESP32 GPIO |
|---------|-----------|
| MOSI    | 23 |
| SCK     | 18 |
| CS      | 5  |
| DC      | 2  |
| RST     | 4  |
| VCC     | 3.3 V |
| GND     | GND |

### Pozostałe / Other
| Function | GPIO |
|----------|------|
| IR TX    | 32 |
| IR RX    | 35 |
| RF TX    | 17 |
| RF RX    | 16 |
| BTN UP   | 12 (INPUT_PULLUP) |
| BTN DOWN | 13 (INPUT_PULLUP) |
| BTN OK   | 14 (INPUT_PULLUP) |

> Buttons connect between GPIO and GND (active LOW, internal pull-up used).

## Funkcje / Features

* Menu nawigowane 3 przyciskami: GÓRA, DÓŁ, OK/POWRÓT
* IR – wysyłanie kodów TV, LED Strip, Klimatyzacja
* IR – nauka kodu (receive) i replay ostatniego
* RF 433 MHz – skanowanie i zapis 10 kodów w EEPROM
* RF 433 MHz – odtwarzanie i usuwanie zapisanych kodów
