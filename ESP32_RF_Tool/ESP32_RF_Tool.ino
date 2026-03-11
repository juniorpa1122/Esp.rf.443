/*
 * ESP32 RF Tool
 * =============
 * Features:
 *   - ST7789 240x240 TFT display (Adafruit library)
 *   - WiFi network scanner
 *   - Bluetooth LE scanner
 *   - 433 MHz jammer (rapid random-code TX)
 *   - 433 MHz receiver (store up to MAX_RF_CODES codes)
 *   - 433 MHz replay (retransmit stored codes)
 *
 * Hardware wiring (default pins):
 *   ST7789 TFT  | ESP32
 *   ------------|-------
 *   CS          | GPIO 15
 *   DC          | GPIO  2
 *   RST         | GPIO  4
 *   SDA (MOSI)  | GPIO 23
 *   SCL (SCK)   | GPIO 18
 *   VCC         | 3.3 V
 *   GND         | GND
 *
 *   433 MHz RX DATA | GPIO 16
 *   433 MHz TX DATA | GPIO 17
 *
 *   Button UP     | GPIO 35  (pulled-up, active LOW – external 10 kΩ to 3.3 V)
 *   Button DOWN   | GPIO 34  (pulled-up, active LOW – external 10 kΩ to 3.3 V)
 *   Button SELECT | GPIO 32  (active LOW – internal pull-up enabled)
 *   Button BACK   | GPIO 33  (active LOW – internal pull-up enabled)
 *
 * Libraries required (install via Arduino Library Manager or platformio.ini):
 *   - Adafruit ST7789  (Adafruit)
 *   - Adafruit GFX     (Adafruit)
 *   - RCSwitch         (sui77)
 *   - ESP32 BLE Arduino (built-in for ESP32 Arduino core)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <RCSwitch.h>

// ---------------------------------------------------------------------------
// Pin definitions
// ---------------------------------------------------------------------------
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4

#define RF_TX_PIN  17
#define RF_RX_PIN  16

#define BTN_UP     35
#define BTN_DOWN   34
#define BTN_SELECT 32
#define BTN_BACK   33

#define DEBOUNCE_MS 50UL

// ---------------------------------------------------------------------------
// Display & RF objects
// ---------------------------------------------------------------------------
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
RCSwitch mySwitch = RCSwitch();

// ---------------------------------------------------------------------------
// Stored 433 MHz codes
// ---------------------------------------------------------------------------
#define MAX_RF_CODES 10
struct RFCode {
  unsigned long value;
  unsigned int  bits;
  unsigned int  protocol;
};
RFCode rfStore[MAX_RF_CODES];
int    rfCodeCount = 0;

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------
enum Screen {
  SCR_MAIN = 0,
  SCR_WIFI_SCAN,
  SCR_BT_SCAN,
  SCR_RF_JAMMER,
  SCR_RF_RECEIVE,
  SCR_RF_REPLAY
};
Screen currentScreen = SCR_MAIN;

const char* mainMenuItems[] = {
  "WiFi Scan",
  "BT Scan",
  "433 Jammer",
  "433 Receive",
  "433 Replay"
};
const int MAIN_MENU_COUNT = 5;
int menuIndex  = 0;

// ---------------------------------------------------------------------------
// Colour palette
// ---------------------------------------------------------------------------
#define C_BG        ST77XX_BLACK
#define C_HEADER    0x055F   // dark cyan
#define C_SELECT    ST77XX_GREEN
#define C_TEXT      ST77XX_WHITE
#define C_WARN      ST77XX_RED
#define C_INFO      ST77XX_YELLOW
#define C_HIGHLIGHT 0x2945   // dark blue-grey used for menu highlight row

// Maximum value for a 24-bit random jammer burst
#define JAMMER_MASK_24BIT 0x00FFFFFFUL

// ---------------------------------------------------------------------------
// Button debouncing
//
// KEY RULE: static Button members may NOT be initialised inside the struct
// body because the type is still incomplete at that point (compiler error:
// "in-class initialization of static data member of incomplete type").
// Instances must be defined as ordinary global variables AFTER the struct.
//
// Usage:
//   pollBtn(b) – call once per loop; returns true ONLY on the press edge.
//   isHeld(b)  – returns true while the button is stably held down.
//   waitRelease(b) – blocks until the button is released.
// ---------------------------------------------------------------------------
struct Button {
  uint8_t       pin;           // GPIO number
  int           stableState;   // last confirmed (debounced) logic level
  int           rawState;      // last raw digitalRead result
  unsigned long lastEdgeMs;    // millis() timestamp of the last raw-state change
};

// Global instances – defined OUTSIDE the struct body (type is complete here)
Button btnUp   = { BTN_UP,     HIGH, HIGH, 0 };
Button btnDown = { BTN_DOWN,   HIGH, HIGH, 0 };
Button btnSel  = { BTN_SELECT, HIGH, HIGH, 0 };
Button btnBack = { BTN_BACK,   HIGH, HIGH, 0 };

// Poll one button; returns true the moment it transitions to pressed (LOW).
// Call once per button per loop iteration for reliable edge detection.
// NOTE: unsigned long subtraction is used intentionally – it wraps correctly
// when millis() overflows (~49 days), which is the standard Arduino pattern.
bool pollBtn(Button &b) {
  int raw = digitalRead(b.pin);
  unsigned long now = millis();
  if (raw != b.rawState) {
    b.rawState    = raw;
    b.lastEdgeMs  = now;
  }
  if ((now - b.lastEdgeMs) >= DEBOUNCE_MS && b.rawState != b.stableState) {
    bool pressed  = (b.rawState == LOW);
    b.stableState = b.rawState;
    return pressed;   // true only on the falling edge (press event)
  }
  return false;
}

// Returns true while the button is stably held down.
bool isHeld(Button &b) {
  pollBtn(b);
  return b.stableState == LOW;
}

// Block until the button is released, then add a short settling delay.
void waitRelease(Button &b) {
  while (isHeld(b)) delay(10);
  delay(30);
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------
void drawHeader(const char* title) {
  tft.fillRect(0, 0, tft.width(), 20, C_HEADER);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 6);
  tft.print(title);
}

void clearBody() {
  tft.fillRect(0, 21, tft.width(), tft.height() - 21, C_BG);
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------
void drawMainMenu() {
  tft.fillScreen(C_BG);
  drawHeader("  ESP32 RF Tool  ");
  tft.setTextSize(1);
  for (int i = 0; i < MAIN_MENU_COUNT; i++) {
    int y = 30 + i * 20;
    if (i == menuIndex) {
      tft.fillRect(0, y - 2, tft.width(), 17, C_HIGHLIGHT);
      tft.setTextColor(C_SELECT);
    } else {
      tft.setTextColor(C_TEXT);
    }
    tft.setCursor(12, y);
    tft.print(i == menuIndex ? "> " : "  ");
    tft.print(mainMenuItems[i]);
  }
  tft.setTextColor(C_INFO);
  tft.setCursor(4, tft.height() - 14);
  tft.print("UP/DOWN=nav  SELECT=ok");
}

// ---------------------------------------------------------------------------
// WiFi Scan
// ---------------------------------------------------------------------------
void doWifiScan() {
  tft.fillScreen(C_BG);
  drawHeader("  WiFi Scan  ");
  tft.setTextColor(C_INFO);
  tft.setCursor(4, 28);
  tft.println("Scanning networks...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(150);
  int n = WiFi.scanNetworks();

  clearBody();
  tft.setCursor(4, 28);

  if (n == 0) {
    tft.setTextColor(C_WARN);
    tft.println("No networks found!");
  } else {
    tft.setTextColor(C_INFO);
    tft.printf("Found %d network(s):\n\n", n);
    tft.setTextColor(C_TEXT);
    int maxShow = min(n, 9);
    for (int i = 0; i < maxShow; i++) {
      tft.setCursor(4, 46 + i * 16);
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 16) ssid = ssid.substring(0, 16);
      tft.printf("%-16s %4ddBm", ssid.c_str(), WiFi.RSSI(i));
    }
    if (n > maxShow) {
      tft.setCursor(4, 46 + maxShow * 16);
      tft.setTextColor(C_INFO);
      tft.printf("  ... +%d more", n - maxShow);
    }
  }
  WiFi.scanDelete();

  tft.setTextColor(C_WARN);
  tft.setCursor(4, tft.height() - 14);
  tft.print("[BACK] Return to menu");
  while (!isHeld(btnBack)) delay(30);
  waitRelease(btnBack);
}

// ---------------------------------------------------------------------------
// Bluetooth BLE Scan
// ---------------------------------------------------------------------------
void doBTScan() {
  tft.fillScreen(C_BG);
  drawHeader("  BT Scan  ");
  tft.setTextColor(C_INFO);
  tft.setCursor(4, 28);
  tft.println("Scanning BLE 5s...");

  BLEDevice::init("");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  BLEScanResults results = pScan->start(5, false);
  int cnt = results.getCount();

  clearBody();
  tft.setCursor(4, 28);

  if (cnt == 0) {
    tft.setTextColor(C_WARN);
    tft.println("No BLE devices found!");
  } else {
    tft.setTextColor(C_INFO);
    tft.printf("Found %d device(s):\n\n", cnt);
    tft.setTextColor(C_TEXT);
    int maxShow = min(cnt, 8);
    for (int i = 0; i < maxShow; i++) {
      BLEAdvertisedDevice dev = results.getDevice(i);
      tft.setCursor(4, 46 + i * 16);
      String label = dev.getName().c_str();
      if (label.length() == 0) label = dev.getAddress().toString().c_str();
      if (label.length() > 16) label = label.substring(0, 16);
      tft.printf("%-16s%4d", label.c_str(), dev.getRSSI());
    }
    if (cnt > maxShow) {
      tft.setCursor(4, 46 + maxShow * 16);
      tft.setTextColor(C_INFO);
      tft.printf("  ... +%d more", cnt - maxShow);
    }
  }
  pScan->clearResults();

  tft.setTextColor(C_WARN);
  tft.setCursor(4, tft.height() - 14);
  tft.print("[BACK] Return to menu");
  while (!isHeld(btnBack)) delay(30);
  waitRelease(btnBack);
}

// ---------------------------------------------------------------------------
// 433 MHz Jammer
// ---------------------------------------------------------------------------
void doRFJammer() {
  tft.fillScreen(C_BG);
  drawHeader("  433MHz Jammer  ");
  tft.setTextColor(C_WARN);
  tft.setTextSize(2);
  tft.setCursor(30, 36);
  tft.println("JAMMING");
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(4, 70);
  tft.println("Transmitting noise on");
  tft.setCursor(4, 84);
  tft.println("433MHz band...");
  tft.setTextColor(C_INFO);
  tft.setCursor(4, 108);
  tft.println("[BACK] Stop jammer");

  mySwitch.enableTransmit(RF_TX_PIN);
  mySwitch.setRepeatTransmit(1);

  unsigned long lastUpdate = millis();
  unsigned long pktCount   = 0;

  while (!isHeld(btnBack)) {
    // Send rapidly varying random codes to saturate the 433 MHz channel
    mySwitch.send(random(JAMMER_MASK_24BIT), 24);
    pktCount++;
    if (millis() - lastUpdate >= 1000) {
      tft.fillRect(4, 120, tft.width() - 8, 14, C_BG);
      tft.setCursor(4, 120);
      tft.setTextColor(C_SELECT);
      tft.printf("Bursts: %lu", pktCount);
      lastUpdate = millis();
    }
  }

  mySwitch.disableTransmit();
  waitRelease(btnBack);
}

// ---------------------------------------------------------------------------
// 433 MHz Receive
// ---------------------------------------------------------------------------
void doRFReceive() {
  tft.fillScreen(C_BG);
  drawHeader("  433 Receive  ");
  tft.setCursor(4, 26);
  tft.setTextColor(C_INFO);
  tft.printf("Stored: %d / %d\n", rfCodeCount, MAX_RF_CODES);
  tft.setTextColor(C_TEXT);
  tft.setCursor(4, 42);
  tft.println("Listening for signals");
  tft.setCursor(4, 56);
  tft.println("Press remote now...");
  tft.setTextColor(C_WARN);
  tft.setCursor(4, tft.height() - 14);
  tft.print("[BACK] Return to menu");

  mySwitch.enableReceive(RF_RX_PIN);

  while (!isHeld(btnBack)) {
    if (mySwitch.available()) {
      unsigned long val   = mySwitch.getReceivedValue();
      unsigned int  bits  = mySwitch.getReceivedBitlength();
      unsigned int  proto = mySwitch.getReceivedProtocol();
      mySwitch.resetAvailable();

      // Show received code
      tft.fillRect(0, 21, tft.width(), tft.height() - 35, C_BG);
      tft.setCursor(4, 26);
      tft.setTextColor(C_INFO);
      tft.printf("Code:  %lu\n", val);
      tft.setTextColor(C_TEXT);
      tft.printf("Bits:  %u\n", bits);
      tft.printf("Proto: %u\n", proto);

      // Check for duplicate
      bool isDup = false;
      for (int i = 0; i < rfCodeCount; i++) {
        if (rfStore[i].value == val) { isDup = true; break; }
      }

      if (isDup) {
        tft.setTextColor(C_WARN);
        tft.println("Duplicate - skipped");
      } else if (rfCodeCount >= MAX_RF_CODES) {
        tft.setTextColor(C_WARN);
        tft.println("Storage full!");
      } else {
        rfStore[rfCodeCount++] = { val, bits, proto };
        tft.setTextColor(C_SELECT);
        tft.printf("Saved! (%d/%d)", rfCodeCount, MAX_RF_CODES);
      }

      tft.setTextColor(C_WARN);
      tft.setCursor(4, tft.height() - 14);
      tft.print("[BACK] Return to menu");
    }
    delay(30);
  }

  mySwitch.disableReceive();
  waitRelease(btnBack);
}

// ---------------------------------------------------------------------------
// 433 MHz Replay
// ---------------------------------------------------------------------------
void drawReplayScreen(int idx) {
  clearBody();
  tft.setCursor(4, 26);
  tft.setTextColor(C_INFO);
  tft.printf("Code %d / %d\n", idx + 1, rfCodeCount);
  tft.setTextColor(C_TEXT);
  tft.printf("Value:   %lu\n", rfStore[idx].value);
  tft.printf("Bits:    %u\n",  rfStore[idx].bits);
  tft.printf("Protocol:%u\n",  rfStore[idx].protocol);
  tft.setTextColor(C_SELECT);
  tft.setCursor(4, 108);
  tft.println("UP/DOWN = select code");
  tft.println("SELECT  = send code");
  tft.setTextColor(C_WARN);
  tft.setCursor(4, tft.height() - 14);
  tft.print("[BACK] Return to menu");
}

void doRFReplay() {
  tft.fillScreen(C_BG);
  drawHeader("  433 Replay  ");

  if (rfCodeCount == 0) {
    tft.setTextColor(C_WARN);
    tft.setCursor(4, 28);
    tft.println("No codes stored!");
    tft.println("Use '433 Receive' first.");
    tft.setCursor(4, tft.height() - 14);
    tft.print("[BACK] Return to menu");
    while (!isHeld(btnBack)) delay(30);
    waitRelease(btnBack);
    return;
  }

  int replayIdx = 0;

  mySwitch.enableTransmit(RF_TX_PIN);
  mySwitch.setRepeatTransmit(3);
  drawReplayScreen(replayIdx);

  while (true) {
    // UP – previous code
    if (pollBtn(btnUp)) {
      replayIdx = (replayIdx - 1 + rfCodeCount) % rfCodeCount;
      drawReplayScreen(replayIdx);
    }
    // DOWN – next code
    if (pollBtn(btnDown)) {
      replayIdx = (replayIdx + 1) % rfCodeCount;
      drawReplayScreen(replayIdx);
    }
    // SELECT – send selected code
    if (pollBtn(btnSel)) {
      mySwitch.setProtocol(rfStore[replayIdx].protocol);
      mySwitch.send(rfStore[replayIdx].value, rfStore[replayIdx].bits);

      // Visual feedback
      tft.fillRect(4, 86, tft.width() - 8, 16, C_SELECT);
      tft.setCursor(50, 88);
      tft.setTextColor(ST77XX_BLACK);
      tft.setTextSize(1);
      tft.print("  ** SENT! **");
      delay(600);
      drawReplayScreen(replayIdx);
    }
    // BACK – return to menu
    if (pollBtn(btnBack)) {
      break;
    }
    delay(20);
  }

  mySwitch.disableTransmit();
  waitRelease(btnBack);
}

// ===========================================================================
// setup()
// ===========================================================================
void setup() {
  Serial.begin(115200);

  // GPIO 34 and 35 are input-only pins with no internal pull-up;
  // external 10 kΩ pull-ups to 3.3 V are required on those lines.
  // GPIO 32 and 33 support INPUT_PULLUP normally.
  pinMode(BTN_UP,     INPUT);         // GPIO 35 – external pull-up required
  pinMode(BTN_DOWN,   INPUT);         // GPIO 34 – external pull-up required
  pinMode(BTN_SELECT, INPUT_PULLUP);  // GPIO 32 – internal pull-up enabled
  pinMode(BTN_BACK,   INPUT_PULLUP);  // GPIO 33 – internal pull-up enabled

  // TFT
  tft.init(240, 240);
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  // Splash screen
  tft.setTextColor(C_SELECT);
  tft.setTextSize(2);
  tft.setCursor(14, 70);
  tft.println("ESP32 RF Tool");
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(22, 104);
  tft.println("WiFi | BT | 433MHz");
  tft.setTextColor(C_INFO);
  tft.setCursor(44, 122);
  tft.println("by juniorpa1122");
  delay(2000);

  drawMainMenu();
}

// ===========================================================================
// loop()
// ===========================================================================
void loop() {
  if (currentScreen == SCR_MAIN) {
    if (pollBtn(btnUp)) {
      menuIndex = (menuIndex - 1 + MAIN_MENU_COUNT) % MAIN_MENU_COUNT;
      drawMainMenu();
    }
    if (pollBtn(btnDown)) {
      menuIndex = (menuIndex + 1) % MAIN_MENU_COUNT;
      drawMainMenu();
    }
    if (pollBtn(btnSel)) {
      switch (menuIndex) {
        case 0: doWifiScan();    break;
        case 1: doBTScan();      break;
        case 2: doRFJammer();    break;
        case 3: doRFReceive();   break;
        case 4: doRFReplay();    break;
      }
      drawMainMenu();
    }
  }
  delay(20);
}
