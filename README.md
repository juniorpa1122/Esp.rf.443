# ESP32 All-in-One Multi-Tool

Połączony projekt ESP32 z dotykowym menu na wyświetlaczu TFT (ILI9488 3.5").

## Funkcje / Features

| Kafelek | Funkcja |
|---------|---------|
| **IR Pilot** | Przechwytywanie i nadawanie sygnałów IR (dowolny pilot) |
| **IR Klima** | Kopiowanie i nadawanie sygnałów klimatyzacji (AC) – Daikin, Mitsubishi, LG, Samsung, Panasonic + ręczna kontrola |
| **RF 433 MHz** | Przechwytywanie i nadawanie sygnałów 433 MHz (RCSwitch) |
| **BLE Skan** | Skanowanie urządzeń Bluetooth Low Energy – czytanie reklam BLE |
| **BLE Beacon** | Nadawanie jako BLE Beacon – wysyłanie okienek widocznych dla telefonów |
| **WiFi Skan** | Skanowanie sieci WiFi z poziomem sygnału |
| **GPS** | Wyświetlanie danych GPS (szerokość, długość, wysokość, czas) |
| **Ustawienia** | Mapa pinów i lista bibliotek |

## Wymagania sprzętowe / Hardware

| Komponent | Opis |
|-----------|------|
| ESP32 | Główny mikrokontroler (240 MHz, dwurdzeniowy) |
| ILI9488 3.5" | Wyświetlacz TFT 480×320 (SPI) |
| XPT2046 | Kontroler dotyku (SPI, wspólna magistrala z TFT) |
| IR LED + odbiornik | TX=GPIO33, RX=GPIO35 |
| Nadajnik/odbiornik 433 MHz | TX=GPIO25, RX=GPIO26 |
| Moduł GPS | UART2: RX2=GPIO16, TX2=GPIO17 |

## Przypisanie pinów / Pin Map

```
TFT:   CS=15  DC=2   RST=4   BL=32 (PWM)
Touch: CS=14  IRQ=27
SPI:   CLK=18  MOSI=23  MISO=19
IR:    TX=33   RX=35
RF:    TX=25   RX=26
GPS:   RX2=16  TX2=17
```

## Biblioteki / Libraries

- `adafruit/Adafruit GFX Library`
- `adafruit/Adafruit ILI9488`
- `paulstoffregen/XPT2046_Touchscreen`
- `crankyoldgit/IRremoteESP8266 ^2.8.6` – IR + AC (klimatyzacja)
- `sui77/rc-switch ^2.6.4` – RF 433 MHz
- `mikalhart/TinyGPSPlus ^1.0.3` – GPS
- ESP32 wbudowane: BLE + WiFi

## Kompilacja / Build

```bash
pio run
pio run --target upload
```
