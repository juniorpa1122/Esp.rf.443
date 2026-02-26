/*
 * ESP32/ESP8266 Universal Remote Control
 * =======================================
 * Features:
 *   - OLED menu (SSD1306 128x64 via U8G2 or Adafruit SSD1306)
 *   - IR: send predefined codes (TV, LED strip, Air-Conditioning)
 *   - IR: learn (receive) & replay any code
 *   - 433 MHz RF: scan / save up to 10 codes in EEPROM
 *   - 433 MHz RF: replay saved codes
 *
 * Hardware (ESP32 recommended, ESP8266 also supported):
 *   - OLED  SDA -> GPIO 21  (ESP32) / D2 (ESP8266)
 *   - OLED  SCL -> GPIO 22  (ESP32) / D1 (ESP8266)
 *   - IR TX     -> GPIO 4
 *   - IR RX     -> GPIO 15
 *   - RF TX     -> GPIO 17
 *   - RF RX     -> GPIO 16
 *   - BTN UP    -> GPIO 12  (INPUT_PULLUP)
 *   - BTN DOWN  -> GPIO 13  (INPUT_PULLUP)
 *   - BTN SEL   -> GPIO 14  (INPUT_PULLUP)
 *
 * Required libraries (install via Arduino Library Manager):
 *   - U8g2           (or use #define USE_ADAFRUIT_SSD1306 below)
 *   - IRremoteESP8266 (for ESP32/ESP8266) or IRremote (for classic AVR)
 *   - rc-switch
 *
 * To use Adafruit SSD1306 instead of U8g2:
 *   Uncomment:  #define USE_ADAFRUIT_SSD1306
 */

// ─── Display driver selection ────────────────────────────────────────────────
// Comment out to use U8g2 (default)
// #define USE_ADAFRUIT_SSD1306

// ─── Pin definitions ─────────────────────────────────────────────────────────
#define PIN_IR_TX   4
#define PIN_IR_RX   15
#define PIN_RF_TX   17
#define PIN_RF_RX   16
#define PIN_BTN_UP  12
#define PIN_BTN_DN  13
#define PIN_BTN_SEL 14

// ─── Includes ────────────────────────────────────────────────────────────────
#ifdef USE_ADAFRUIT_SSD1306
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #define SCREEN_WIDTH 128
  #define SCREEN_HEIGHT 64
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#else
  #include <U8g2lib.h>
  #include <Wire.h>
  // Hardware I2C 128x64 SSD1306
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <RCSwitch.h>
#include <EEPROM.h>

// ─── Constants ───────────────────────────────────────────────────────────────
#define EEPROM_SIZE       512
#define RF_SLOT_COUNT     10
#define RF_SLOT_SIZE      16   // bytes per slot: 4B value + 4B bitlen + 4B protocol + 4B valid
#define RF_EEPROM_BASE    0

#define BTN_DEBOUNCE_MS   50
#define BTN_HOLD_MS       600

// ─── IR predefined codes ──────────────────────────────────────────────────────
// Samsung TV example codes (NEC protocol, 32-bit)
struct IRCode { const char* label; uint32_t code; decode_type_t protocol; uint16_t bits; };

const IRCode TV_CODES[] = {
  { "Power",   0xE0E040BF, NEC, 32 },
  { "Vol+",    0xE0E0E01F, NEC, 32 },
  { "Vol-",    0xE0E0D02F, NEC, 32 },
  { "Mute",    0xE0E0F00F, NEC, 32 },
  { "CH+",     0xE0E048B7, NEC, 32 },
  { "CH-",     0xE0E008F7, NEC, 32 },
  { "Source",  0xE0E0807F, NEC, 32 },
};
const uint8_t TV_CODE_COUNT = sizeof(TV_CODES) / sizeof(TV_CODES[0]);

// Generic LED strip remote (NEC, common)
const IRCode LED_CODES[] = {
  { "On/Off",  0xFF02FD,   NEC, 24 },
  { "Bright+", 0xFF3AC5,   NEC, 24 },
  { "Bright-", 0xFFBA45,   NEC, 24 },
  { "Red",     0xFF1AE5,   NEC, 24 },
  { "Green",   0xFF9A65,   NEC, 24 },
  { "Blue",    0xFFA25D,   NEC, 24 },
  { "White",   0xFF22DD,   NEC, 24 },
  { "DIY1",    0xFF2AD5,   NEC, 24 },
};
const uint8_t LED_CODE_COUNT = sizeof(LED_CODES) / sizeof(LED_CODES[0]);

// Generic AC (Midea/generic NEC-style codes)
const IRCode AC_CODES[] = {
  { "Power",   0xFF58A7,   NEC, 24 },
  { "Temp+",   0xFF08F7,   NEC, 24 },
  { "Temp-",   0xFF8877,   NEC, 24 },
  { "Cool",    0xFF48B7,   NEC, 24 },
  { "Heat",    0xFFC837,   NEC, 24 },
  { "Fan",     0xFF28D7,   NEC, 24 },
  { "Sleep",   0xFFA857,   NEC, 24 },
  { "Timer",   0xFF6897,   NEC, 24 },
};
const uint8_t AC_CODE_COUNT = sizeof(AC_CODES) / sizeof(AC_CODES[0]);

// ─── Menu structure ───────────────────────────────────────────────────────────
enum MenuLevel {
  MENU_MAIN,
  MENU_IR,
  MENU_IR_TV,
  MENU_IR_LED,
  MENU_IR_AC,
  MENU_IR_LEARN,
  MENU_IR_REPLAY,
  MENU_RF,
  MENU_RF_SCAN,
  MENU_RF_REPLAY,
  MENU_RF_DELETE,
};

const char* MAIN_ITEMS[]    = { "IR Piloty", "RF 433 MHz" };
const char* IR_ITEMS[]      = { "Telewizor", "LED Strip", "Klimatyzacja", "Naucz (Learn)", "Replay ostatni", "< Powrot" };
const char* RF_ITEMS[]      = { "Skanuj/Zapisz", "Odtwarzaj", "Usun slot", "< Powrot" };

const uint8_t MAIN_COUNT    = 2;
const uint8_t IR_COUNT      = 6;
const uint8_t RF_COUNT      = 4;

// ─── Global state ────────────────────────────────────────────────────────────
IRsend   irsend(PIN_IR_TX);
IRrecv   irrecv(PIN_IR_RX, 1024, 50, true);
RCSwitch rfSwitch;

MenuLevel currentMenu = MENU_MAIN;
int8_t    menuIndex   = 0;
int8_t    subIndex    = 0;   // index within TV/LED/AC/RF-slot list

// Learned IR code
decode_results irResult;
bool           irLearned = false;
uint32_t       irLearnedCode     = 0;
decode_type_t  irLearnedProtocol = UNKNOWN;
uint16_t       irLearnedBits     = 0;

// RF slot management
struct RFSlot {
  uint32_t value;
  uint32_t bitlen;
  uint32_t protocol;
  uint32_t valid;   // 0xDEADBEEF = occupied
};
RFSlot rfSlots[RF_SLOT_COUNT];
int8_t rfScanSlot = -1;   // slot chosen for next scan

// Button state
uint32_t btnUpLast  = 0, btnDnLast  = 0, btnSelLast = 0;
bool     btnUpPrev  = HIGH, btnDnPrev  = HIGH, btnSelPrev = HIGH;

// ─── EEPROM helpers ───────────────────────────────────────────────────────────
void eepromLoadSlots() {
  for (uint8_t i = 0; i < RF_SLOT_COUNT; i++) {
    EEPROM.get(RF_EEPROM_BASE + i * RF_SLOT_SIZE, rfSlots[i]);
  }
}

void eepromSaveSlot(uint8_t idx) {
  EEPROM.put(RF_EEPROM_BASE + idx * RF_SLOT_SIZE, rfSlots[idx]);
  EEPROM.commit();
}

// ─── Display helpers ─────────────────────────────────────────────────────────
void dispClear() {
#ifdef USE_ADAFRUIT_SSD1306
  display.clearDisplay();
#else
  u8g2.clearBuffer();
#endif
}

void dispSend() {
#ifdef USE_ADAFRUIT_SSD1306
  display.display();
#else
  u8g2.sendBuffer();
#endif
}

void dispSetFont() {
#ifndef USE_ADAFRUIT_SSD1306
  u8g2.setFont(u8g2_font_6x10_tr);
#endif
}

// Draw a scrollable list menu with title
void drawMenu(const char* title, const char** items, uint8_t count, int8_t selected) {
  dispClear();
  dispSetFont();

  const uint8_t VISIBLE = 5;
  int8_t start = selected - 2;
  if (start < 0) start = 0;
  if (start > (int8_t)(count - VISIBLE)) start = count - VISIBLE;
  if (start < 0) start = 0;

#ifdef USE_ADAFRUIT_SSD1306
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  for (uint8_t i = 0; i < VISIBLE && (start + i) < count; i++) {
    uint8_t idx = start + i;
    bool sel = (idx == selected);
    display.setCursor(sel ? 6 : 0, 14 + i * 10);
    if (sel) display.print("> ");
    display.println(items[idx]);
  }
  display.display();
#else
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, 128);
  for (uint8_t i = 0; i < VISIBLE && (start + i) < count; i++) {
    uint8_t idx = start + i;
    bool sel = (idx == selected);
    uint8_t y = 24 + i * 10;
    if (sel) {
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
    }
    u8g2.drawStr(2, y, items[idx]);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
#endif
}

// Show a full-screen status message for a moment
void showMessage(const char* line1, const char* line2 = nullptr) {
  dispClear();
  dispSetFont();
#ifdef USE_ADAFRUIT_SSD1306
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2) { display.setCursor(0, 35); display.println(line2); }
  display.display();
#else
  u8g2.drawStr(0, 28, line1);
  if (line2) u8g2.drawStr(0, 42, line2);
  u8g2.sendBuffer();
#endif
  delay(1200);
}

// ─── IR helpers ──────────────────────────────────────────────────────────────
void sendIRCode(const IRCode& c) {
  irsend.send(c.protocol, c.code, c.bits);
}

void irLearnMode() {
  showMessage("IR Learn...", "Skieruj pilot");
  irrecv.enableIRIn();
  uint32_t start = millis();
  while (millis() - start < 10000) {
    if (irrecv.decode(&irResult)) {
      irLearnedCode     = irResult.value;
      irLearnedProtocol = irResult.decode_type;
      irLearnedBits     = irResult.bits;
      irLearned         = true;
      irrecv.resume();
      char buf[20];
      snprintf(buf, sizeof(buf), "0x%08X", irLearnedCode);
      showMessage("Zapisano!", buf);
      return;
    }
  }
  showMessage("Timeout!", "Brak sygnalu");
}

void irReplayLast() {
  if (!irLearned) {
    showMessage("Brak kodu!", "Najpierw Learn");
    return;
  }
  irsend.send(irLearnedProtocol, irLearnedCode, irLearnedBits);
  char buf[20];
  snprintf(buf, sizeof(buf), "0x%08X", irLearnedCode);
  showMessage("IR Wyslano!", buf);
}

// ─── RF helpers ──────────────────────────────────────────────────────────────
int8_t findFreeRFSlot() {
  for (uint8_t i = 0; i < RF_SLOT_COUNT; i++) {
    if (rfSlots[i].valid != 0xDEADBEEF) return i;
  }
  return -1;
}

void rfScanAndSave() {
  int8_t slot = findFreeRFSlot();
  if (slot < 0) {
    showMessage("Brak miejsca!", "Usun stare");
    return;
  }
  showMessage("RF Skan...", "Nacisnij pilot");
  rfSwitch.enableReceive(PIN_RF_RX);
  uint32_t start = millis();
  while (millis() - start < 15000) {
    if (rfSwitch.available()) {
      rfSlots[slot].value    = rfSwitch.getReceivedValue();
      rfSlots[slot].bitlen   = rfSwitch.getReceivedBitlength();
      rfSlots[slot].protocol = rfSwitch.getReceivedProtocol();
      rfSlots[slot].valid    = 0xDEADBEEF;
      rfSwitch.resetAvailable();
      rfSwitch.disableReceive();
      eepromSaveSlot(slot);
      char buf[22];
      snprintf(buf, sizeof(buf), "Slot %d: %lu", slot, rfSlots[slot].value);
      showMessage("RF Zapisano!", buf);
      return;
    }
  }
  rfSwitch.disableReceive();
  showMessage("Timeout!", "Brak sygnalu RF");
}

// Build array of RF slot labels for the menu
char rfSlotLabels[RF_SLOT_COUNT][20];
const char* rfReplayItems[RF_SLOT_COUNT + 1]; // +1 for Back

void buildRFReplayMenu() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < RF_SLOT_COUNT; i++) {
    if (rfSlots[i].valid == 0xDEADBEEF) {
      snprintf(rfSlotLabels[n], 20, "Slot%d: %lu", i, rfSlots[i].value);
    } else {
      snprintf(rfSlotLabels[n], 20, "Slot%d: pusty", i);
    }
    rfReplayItems[n] = rfSlotLabels[n];
    n++;
  }
  rfReplayItems[n] = "< Powrot";
}

void rfReplay(uint8_t slot) {
  if (rfSlots[slot].valid != 0xDEADBEEF) {
    showMessage("Pusty slot!", nullptr);
    return;
  }
  rfSwitch.enableTransmit(PIN_RF_TX);
  rfSwitch.setProtocol(rfSlots[slot].protocol);
  rfSwitch.send(rfSlots[slot].value, rfSlots[slot].bitlen);
  rfSwitch.disableTransmit();
  char buf[22];
  snprintf(buf, sizeof(buf), "%lu", rfSlots[slot].value);
  showMessage("RF Wyslano!", buf);
}

void rfDeleteSlot(uint8_t slot) {
  rfSlots[slot].valid = 0;
  eepromSaveSlot(slot);
  char buf[22];
  snprintf(buf, sizeof(buf), "Slot %d usunieto", slot);
  showMessage("RF Usunieto!", buf);
}

// ─── Button reading ───────────────────────────────────────────────────────────
// Returns true if button just pressed (falling edge with debounce)
bool btnPressed(uint8_t pin, bool& prevState, uint32_t& lastTime) {
  bool cur = digitalRead(pin);
  if (cur == LOW && prevState == HIGH) {
    if (millis() - lastTime > BTN_DEBOUNCE_MS) {
      prevState = LOW;
      lastTime  = millis();
      return true;
    }
  }
  if (cur == HIGH) prevState = HIGH;
  return false;
}

// ─── Sub-menu arrays for TV/LED/AC ────────────────────────────────────────────
// We build const char* arrays from IRCode arrays
const char* tvItems[TV_CODE_COUNT + 1];
const char* ledItems[LED_CODE_COUNT + 1];
const char* acItems[AC_CODE_COUNT + 1];

void buildCodeMenus() {
  for (uint8_t i = 0; i < TV_CODE_COUNT;  i++) tvItems[i]  = TV_CODES[i].label;
  for (uint8_t i = 0; i < LED_CODE_COUNT; i++) ledItems[i] = LED_CODES[i].label;
  for (uint8_t i = 0; i < AC_CODE_COUNT;  i++) acItems[i]  = AC_CODES[i].label;
  tvItems[TV_CODE_COUNT]   = "< Powrot";
  ledItems[LED_CODE_COUNT] = "< Powrot";
  acItems[AC_CODE_COUNT]   = "< Powrot";
}

// ─── Navigation helper ────────────────────────────────────────────────────────
void navigate(int8_t& idx, int8_t count, bool up) {
  if (up) { idx--; if (idx < 0) idx = count - 1; }
  else    { idx++; if (idx >= count) idx = 0; }
}

// ─── setup & loop ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Buttons
  pinMode(PIN_BTN_UP,  INPUT_PULLUP);
  pinMode(PIN_BTN_DN,  INPUT_PULLUP);
  pinMode(PIN_BTN_SEL, INPUT_PULLUP);

  // Display init
#ifdef USE_ADAFRUIT_SSD1306
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 nie znaleziono!");
    while (true) delay(100);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
#else
  u8g2.begin();
#endif

  // Splash
  dispClear();
  dispSetFont();
#ifdef USE_ADAFRUIT_SSD1306
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("ESP IR+RF Pilot");
  display.setCursor(20, 35);
  display.println("v1.0  by ESP32");
  display.display();
#else
  u8g2.drawStr(8, 28, "ESP IR+RF Pilot");
  u8g2.drawStr(16, 42, "v1.0  by ESP32");
  u8g2.sendBuffer();
#endif
  delay(1800);

  // IR
  irsend.begin();

  // RF
  rfSwitch.enableTransmit(PIN_RF_TX);
  rfSwitch.disableTransmit();

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  eepromLoadSlots();

  // Build menus
  buildCodeMenus();
  buildRFReplayMenu();

  // Draw initial menu
  drawMenu("MENU GLOWNE", MAIN_ITEMS, MAIN_COUNT, menuIndex);
}

void loop() {
  bool up  = btnPressed(PIN_BTN_UP,  btnUpPrev,  btnUpLast);
  bool dn  = btnPressed(PIN_BTN_DN,  btnDnPrev,  btnDnLast);
  bool sel = btnPressed(PIN_BTN_SEL, btnSelPrev, btnSelLast);

  bool redraw = false;

  switch (currentMenu) {

    // ── Main menu ────────────────────────────────────────────────────────────
    case MENU_MAIN:
      if (up || dn) { navigate(menuIndex, MAIN_COUNT, up); redraw = true; }
      if (sel) {
        if (menuIndex == 0) { currentMenu = MENU_IR;  menuIndex = 0; redraw = true; }
        if (menuIndex == 1) { currentMenu = MENU_RF;  menuIndex = 0; buildRFReplayMenu(); redraw = true; }
      }
      if (redraw) drawMenu("MENU GLOWNE", MAIN_ITEMS, MAIN_COUNT, menuIndex);
      break;

    // ── IR sub-menu ──────────────────────────────────────────────────────────
    case MENU_IR:
      if (up || dn) { navigate(menuIndex, IR_COUNT, up); redraw = true; }
      if (sel) {
        switch (menuIndex) {
          case 0: currentMenu = MENU_IR_TV;     subIndex = 0; redraw = true; break;
          case 1: currentMenu = MENU_IR_LED;    subIndex = 0; redraw = true; break;
          case 2: currentMenu = MENU_IR_AC;     subIndex = 0; redraw = true; break;
          case 3: irLearnMode(); redraw = true; break;
          case 4: irReplayLast(); redraw = true; break;
          case 5: currentMenu = MENU_MAIN; menuIndex = 0; redraw = true; break;
        }
      }
      if (redraw) drawMenu("IR Piloty", IR_ITEMS, IR_COUNT, menuIndex);
      break;

    // ── TV codes ─────────────────────────────────────────────────────────────
    case MENU_IR_TV:
      if (up || dn) { navigate(subIndex, TV_CODE_COUNT + 1, up); redraw = true; }
      if (sel) {
        if (subIndex < TV_CODE_COUNT) {
          sendIRCode(TV_CODES[subIndex]);
          char buf[20];
          snprintf(buf, sizeof(buf), "TV: %s", TV_CODES[subIndex].label);
          showMessage("IR Wyslano!", buf);
          redraw = true;
        } else {
          currentMenu = MENU_IR; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenu("TV Kody", tvItems, TV_CODE_COUNT + 1, subIndex);
      break;

    // ── LED codes ────────────────────────────────────────────────────────────
    case MENU_IR_LED:
      if (up || dn) { navigate(subIndex, LED_CODE_COUNT + 1, up); redraw = true; }
      if (sel) {
        if (subIndex < LED_CODE_COUNT) {
          sendIRCode(LED_CODES[subIndex]);
          char buf[20];
          snprintf(buf, sizeof(buf), "LED: %s", LED_CODES[subIndex].label);
          showMessage("IR Wyslano!", buf);
          redraw = true;
        } else {
          currentMenu = MENU_IR; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenu("LED Kody", ledItems, LED_CODE_COUNT + 1, subIndex);
      break;

    // ── AC codes ─────────────────────────────────────────────────────────────
    case MENU_IR_AC:
      if (up || dn) { navigate(subIndex, AC_CODE_COUNT + 1, up); redraw = true; }
      if (sel) {
        if (subIndex < AC_CODE_COUNT) {
          sendIRCode(AC_CODES[subIndex]);
          char buf[20];
          snprintf(buf, sizeof(buf), "AC: %s", AC_CODES[subIndex].label);
          showMessage("IR Wyslano!", buf);
          redraw = true;
        } else {
          currentMenu = MENU_IR; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenu("Klimatyzacja", acItems, AC_CODE_COUNT + 1, subIndex);
      break;

    // ── RF sub-menu ──────────────────────────────────────────────────────────
    case MENU_RF:
      if (up || dn) { navigate(menuIndex, RF_COUNT, up); redraw = true; }
      if (sel) {
        switch (menuIndex) {
          case 0: rfScanAndSave(); buildRFReplayMenu(); redraw = true; break;
          case 1: currentMenu = MENU_RF_REPLAY; subIndex = 0; buildRFReplayMenu(); redraw = true; break;
          case 2: currentMenu = MENU_RF_DELETE; subIndex = 0; buildRFReplayMenu(); redraw = true; break;
          case 3: currentMenu = MENU_MAIN; menuIndex = 0; redraw = true; break;
        }
      }
      if (redraw) drawMenu("RF 433 MHz", RF_ITEMS, RF_COUNT, menuIndex);
      break;

    // ── RF replay slot picker ─────────────────────────────────────────────────
    case MENU_RF_REPLAY:
      if (up || dn) { navigate(subIndex, RF_SLOT_COUNT + 1, up); redraw = true; }
      if (sel) {
        if (subIndex < RF_SLOT_COUNT) {
          rfReplay(subIndex);
          redraw = true;
        } else {
          currentMenu = MENU_RF; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenu("RF Odtwarzaj", rfReplayItems, RF_SLOT_COUNT + 1, subIndex);
      break;

    // ── RF delete slot picker ─────────────────────────────────────────────────
    case MENU_RF_DELETE:
      if (up || dn) { navigate(subIndex, RF_SLOT_COUNT + 1, up); redraw = true; }
      if (sel) {
        if (subIndex < RF_SLOT_COUNT) {
          rfDeleteSlot(subIndex);
          buildRFReplayMenu();
          redraw = true;
        } else {
          currentMenu = MENU_RF; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenu("RF Usun slot", rfReplayItems, RF_SLOT_COUNT + 1, subIndex);
      break;

    default:
      currentMenu = MENU_MAIN;
      menuIndex   = 0;
      redraw      = true;
      if (redraw) drawMenu("MENU GLOWNE", MAIN_ITEMS, MAIN_COUNT, menuIndex);
      break;
  }
}
