# Esp.rf.443
RF 433 MHz + IR Universal Remote Controller for ESP32 / ESP8266

## Funkcje / Features

| Funkcja | Opis |
|---|---|
| 📺 IR TV | Wyślij predefiniowane kody (Samsung NEC) |
| 💡 IR LED | Sterowanie paskiem LED RGB |
| ❄️ IR AC | Klimatyzacja (Midea/generyczne) |
| 🎓 IR Learn | Naucz się kodu z oryginalnego pilota |
| 🔁 IR Replay | Wyślij ostatnio nauczony kod |
| 📻 RF Scan | Zeskanuj i zapisz kod 433 MHz (do 10 slotów) |
| 📻 RF Replay | Odtwórz zapisany kod 433 MHz |
| 🗑 RF Delete | Usuń wybrany slot |
| 🖥 OLED | Menu na wyświetlaczu 128×64 (U8g2 lub Adafruit SSD1306) |

## Podłączenie / Wiring

```
ESP32 / ESP8266        Peryferia
──────────────────────────────────────────────
GPIO 21 (SDA)  ──►  OLED SDA
GPIO 22 (SCL)  ──►  OLED SCL
GPIO  4        ──►  IR LED Nadajnik (TX)
GPIO 15        ──►  IR Odbiornik   (RX) TSOP38238
GPIO 17        ──►  RF 433 Nadajnik (TX FS1000A)
GPIO 16        ──►  RF 433 Odbiornik (RX MX-RM-5V)
GPIO 12        ──►  Przycisk GÓRA  (do GND)
GPIO 13        ──►  Przycisk DÓŁ   (do GND)
GPIO 14        ──►  Przycisk OK    (do GND)
```
> ESP8266: użyj D1=SCL, D2=SDA, D5/D6/D7/D8 dla IR/RF/BTN.

## Wymagane biblioteki / Required Libraries

Zainstaluj przez Arduino Library Manager:

| Biblioteka | Zastosowanie |
|---|---|
| **U8g2** (lub Adafruit SSD1306 + Adafruit GFX) | Wyświetlacz OLED |
| **IRremoteESP8266** | IR nadajnik + odbiornik |
| **rc-switch** | RF 433 MHz |
| **EEPROM** | (wbudowana) zapis kodów RF |

## Konfiguracja / Configuration

Otwórz `Esp_RF_443.ino` i dostosuj:

```cpp
// Użyj Adafruit SSD1306 zamiast U8g2 – odkomentuj linię:
// #define USE_ADAFRUIT_SSD1306

// Piny – zmień według własnego układu:
#define PIN_IR_TX   4
#define PIN_IR_RX   15
#define PIN_RF_TX   17
#define PIN_RF_RX   16
#define PIN_BTN_UP  12
#define PIN_BTN_DN  13
#define PIN_BTN_SEL 14
```

## Obsługa menu / Menu Navigation

```
[GÓRA] / [DÓŁ]  – poruszaj się po liście
[OK]             – wybierz / wyślij / potwierdź
```

Menu główne → IR Piloty → TV / LED / Klimatyzacja / Learn / Replay  
Menu główne → RF 433 MHz → Skanuj / Odtwarzaj / Usuń

## Dostosowanie kodów IR / Customising IR Codes

W pliku `.ino` znajdź tablice `TV_CODES[]`, `LED_CODES[]`, `AC_CODES[]`.  
Każdy wpis to: `{ "Etykieta", 0xKOD, PROTOKOL, BITY }`.  
Protokoły: `NEC`, `SAMSUNG`, `SONY`, `RC5`, `RC6`, `LG`, `PANASONIC`, itd.  
(wszystkie wspierane przez bibliotekę IRremoteESP8266)

## Licencja / License

MIT
