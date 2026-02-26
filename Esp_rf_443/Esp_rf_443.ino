/*
 * Esp_rf_443.ino
 *
 * RF 433MHz + ST7789 TFT Display Controller
 * Requires:
 *   - RCSwitch library (https://github.com/sui77/rc-switch)
 *   - Adafruit ST7789 library (https://github.com/adafruit/Adafruit-ST7735-Library)
 *   - Adafruit GFX Library (https://github.com/adafruit/Adafruit-GFX-Library)
 */

#include <RCSwitch.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// --- ST7789 TFT Display pin definitions ---
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

// --- RF 433MHz receiver/transmitter pin definitions ---
#define RF_RECEIVE_PIN  5
#define RF_TRANSMIT_PIN 0

// --- Display resolution (240x240 for typical ST7789) ---
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
RCSwitch mySwitch = RCSwitch();

// Track the last received RF code for display
unsigned long lastReceivedValue = 0;
unsigned int  lastReceivedBitLength = 0;
unsigned int  lastReceivedProtocol  = 0;

void displayInit() {
  tft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("RF 433 Controller");
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Waiting for signal...");
}

void displayRFCode(unsigned long value, unsigned int bitLength, unsigned int protocol) {
  tft.fillRect(0, 30, SCREEN_WIDTH, SCREEN_HEIGHT - 30, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(1);

  tft.setCursor(10, 40);
  tft.print("Received RF signal:");

  tft.setCursor(10, 55);
  tft.print("Value:    ");
  tft.print(value);

  tft.setCursor(10, 70);
  tft.print("Bits:     ");
  tft.print(bitLength);

  tft.setCursor(10, 85);
  tft.print("Protocol: ");
  tft.print(protocol);
}

void setup() {
  Serial.begin(115200);
  Serial.println("RF 433MHz + ST7789 Controller starting...");

  // Initialize the ST7789 TFT display
  displayInit();

  // Enable RF receiver on defined pin
  mySwitch.enableReceive(RF_RECEIVE_PIN);

  // Enable RF transmitter on defined pin
  mySwitch.enableTransmit(RF_TRANSMIT_PIN);

  Serial.println("Setup complete. Listening for RF signals...");
}

void loop() {
  if (mySwitch.available()) {
    unsigned long value      = mySwitch.getReceivedValue();
    unsigned int  bitLength  = mySwitch.getReceivedBitlength();
    unsigned int  protocol   = mySwitch.getReceivedProtocol();

    if (value == 0) {
      Serial.println("Unknown encoding");
    } else {
      Serial.print("Received: ");
      Serial.print(value);
      Serial.print(" / ");
      Serial.print(bitLength);
      Serial.print("bit / Protocol: ");
      Serial.println(protocol);

      // Update display only when a new code is received
      if (value != lastReceivedValue ||
          bitLength != lastReceivedBitLength ||
          protocol != lastReceivedProtocol) {
        lastReceivedValue     = value;
        lastReceivedBitLength = bitLength;
        lastReceivedProtocol  = protocol;
        displayRFCode(value, bitLength, protocol);
      }
    }

    mySwitch.resetAvailable();
  }
}
