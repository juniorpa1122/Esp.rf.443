# Esp.rf.443

RF 433MHz + IR + WiFi Controller with ST7789 TFT Display

## Required Libraries

Install the following libraries via the Arduino Library Manager or by downloading them manually:

| Library | Purpose | Source |
|---------|---------|--------|
| **RCSwitch** | 433MHz RF signal receive/transmit | [GitHub](https://github.com/sui77/rc-switch) |
| **Adafruit ST7789** | ST7789 TFT display driver (part of the Adafruit ST7735 + ST7789 library) | [GitHub](https://github.com/adafruit/Adafruit-ST7735-Library) |
| **Adafruit GFX Library** | Graphics primitives (dependency of ST7789) | [GitHub](https://github.com/adafruit/Adafruit-GFX-Library) |

## Hardware Wiring

### ST7789 TFT Display (SPI)

| Display Pin | ESP Pin |
|-------------|---------|
| CS          | GPIO 15 |
| DC          | GPIO 2  |
| RST         | GPIO 4  |
| SCK         | GPIO 14 (HSPI CLK) |
| MOSI        | GPIO 13 (HSPI MOSI) |
| VCC         | 3.3V    |
| GND         | GND     |

### RF 433MHz Module

| Module Pin | ESP Pin |
|------------|---------|
| DATA (RX)  | GPIO 5  |
| DATA (TX)  | GPIO 0  |
| VCC        | 3.3V / 5V |
| GND        | GND     |

## Usage

1. Install all required libraries listed above.
2. Open `Esp_rf_443/Esp_rf_443.ino` in the Arduino IDE.
3. Select your ESP board (e.g. ESP8266 or ESP32).
4. Upload the sketch.
5. The ST7789 display will show received 433MHz RF codes in real time.
6. Received codes are also printed to the Serial Monitor at 115200 baud.
