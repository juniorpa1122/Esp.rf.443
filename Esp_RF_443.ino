/*
 * ESP32 Universal Remote Control – TFT 3.5" Edition
 * ==================================================
 * Features:
 *   - TFT 3.5" display via TFT_eSPI library (NO touch, buttons only)
 *   - IR: send predefined codes (TV, LED strip, Air-Conditioning)
 *   - IR: learn (receive) & replay any code
 *   - 433 MHz RF: scan / save up to 10 codes in EEPROM
 *   - 433 MHz RF: replay / delete saved codes
 *
 * Hardware (ESP32):
 *   TFT 3.5" SPI (ILI9488 / ST7796 – configure in User_Setup.h):
 *     TFT_MOSI -> GPIO 23
 *     TFT_SCLK -> GPIO 18
 *     TFT_CS   -> GPIO  5
 *     TFT_DC   -> GPIO  2
 *     TFT_RST  -> GPIO  4
 *   IR TX       -> GPIO 32
 *   IR RX       -> GPIO 35
 *   RF TX       -> GPIO 17
 *   RF RX       -> GPIO 16
 *   BTN UP      -> GPIO 12  (INPUT_PULLUP, GND when pressed)
 *   BTN DOWN    -> GPIO 13  (INPUT_PULLUP, GND when pressed)
 *   BTN OK      -> GPIO 14  (INPUT_PULLUP, GND when pressed)
 *
 * Required libraries (Arduino Library Manager):
 *   - TFT_eSPI  (by Bodmer) – configure User_Setup.h for your display
 *   - IRremoteESP8266
 *   - rc-switch
 */

// ─── Pin definitions ──────────────────────────────────────────────────────────
#define PIN_IR_TX    32
#define PIN_IR_RX    35
#define PIN_RF_TX    17
#define PIN_RF_RX    16
#define PIN_BTN_UP   12
#define PIN_BTN_DN   13
#define PIN_BTN_OK   14

// ─── Includes ─────────────────────────────────────────────────────────────────
#include <TFT_eSPI.h>   // TFT_eSPI by Bodmer – configure display in User_Setup.h

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <RCSwitch.h>
#include <EEPROM.h>

// ─── TFT display object ───────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();

// ─── Display layout constants ─────────────────────────────────────────────────
// TFT_W / TFT_H reflect post-rotation dimensions (landscape 480x320).
// They are overwritten in setup() using tft.width() / tft.height() after
// tft.setRotation() so the values always match the actual active geometry.
uint16_t TFT_W = 480;
uint16_t TFT_H = 320;

#define HEADER_H       40   // pixels for the header bar
#define FOOTER_H       30   // pixels for the footer/status bar
#define ITEM_H         36   // height per menu row
// ITEMS_VISIBLE is computed in setup() once real dimensions are known
uint8_t ITEMS_VISIBLE = 6;

// Colours (RGB565)
#define COL_BG        TFT_BLACK
#define COL_HEADER    0x2945   // dark blue-grey
#define COL_HEADER_TXT TFT_WHITE
#define COL_SEL_BG    0x04FF   // teal / cyan
#define COL_SEL_TXT   TFT_BLACK
#define COL_ITEM_TXT  TFT_WHITE
#define COL_FOOTER    0x4208   // dark grey
#define COL_FOOTER_TXT 0xAD55  // light grey
#define COL_ACCENT    TFT_YELLOW

// ─── Button debounce ──────────────────────────────────────────────────────────
#define BTN_DEBOUNCE_MS 50

struct BtnState {
  bool prev;
  unsigned long last;
};

BtnState btnUp  = {HIGH, 0};
BtnState btnDn  = {HIGH, 0};
BtnState btnOk  = {HIGH, 0};

// Returns true on a fresh press (falling edge with debounce)
bool btnPressed(uint8_t pin, BtnState &s) {
  bool cur = digitalRead(pin);
  if (cur == LOW && s.prev == HIGH) {
    unsigned long now = millis();
    if (now - s.last > BTN_DEBOUNCE_MS) {
      s.last = now;
      s.prev = cur;
      return true;
    }
  }
  s.prev = cur;
  return false;
}

// ─── EEPROM / RF constants ────────────────────────────────────────────────────
#define EEPROM_SIZE       512
#define RF_SLOT_COUNT     10
#define RF_SLOT_SIZE      16          // 4B value + 4B bitlen + 4B protocol + 4B valid-flag
#define RF_EEPROM_BASE    0
#define RF_SLOT_VALID_MARKER 0xDEADBEEFUL  // magic value written to the valid field

// ─── Timing constants ─────────────────────────────────────────────────────────
#define IR_RX_BUF_SIZE    1024   // IRrecv capture buffer size (bytes)
#define IR_RX_TIMEOUT_MS  50     // IRrecv gap-timeout before decode (ms)
#define IR_LEARN_TIMEOUT_MS  10000UL   // max wait for an IR signal during learn
#define RF_SCAN_TIMEOUT_MS   15000UL   // max wait for an RF signal during scan
#define MESSAGE_DISPLAY_MS   2000      // how long showMessage() stays visible

// ─── IR predefined codes ──────────────────────────────────────────────────────
struct IRCode { const char *label; uint32_t code; decode_type_t protocol; uint16_t bits; };

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

const IRCode LED_CODES[] = {
  { "On/Off",  0xFF02FD, NEC, 24 },
  { "Bright+", 0xFF3AC5, NEC, 24 },
  { "Bright-", 0xFFBA45, NEC, 24 },
  { "Red",     0xFF1AE5, NEC, 24 },
  { "Green",   0xFF9A65, NEC, 24 },
  { "Blue",    0xFFA25D, NEC, 24 },
  { "White",   0xFF22DD, NEC, 24 },
  { "DIY1",    0xFF2AD5, NEC, 24 },
};
const uint8_t LED_CODE_COUNT = sizeof(LED_CODES) / sizeof(LED_CODES[0]);

const IRCode AC_CODES[] = {
  { "Power",   0xFF58A7, NEC, 24 },
  { "Temp+",   0xFF08F7, NEC, 24 },
  { "Temp-",   0xFF8877, NEC, 24 },
  { "Cool",    0xFF48B7, NEC, 24 },
  { "Heat",    0xFFC837, NEC, 24 },
  { "Fan",     0xFF28D7, NEC, 24 },
  { "Sleep",   0xFFA857, NEC, 24 },
  { "Timer",   0xFF6897, NEC, 24 },
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

const char *MAIN_ITEMS[]  = { "IR Piloty", "RF 433 MHz" };
const char *IR_ITEMS[]    = { "Telewizor", "LED Strip", "Klimatyzacja",
                               "Naucz (Learn)", "Replay ostatni", "< Powrot" };
const char *RF_ITEMS[]    = { "Skanuj/Zapisz", "Odtwarzaj", "Usun slot", "< Powrot" };

const uint8_t MAIN_COUNT = 2;
const uint8_t IR_COUNT   = 6;
const uint8_t RF_COUNT   = 4;

// Dynamic item lists built at runtime
char tvItems [TV_CODE_COUNT  + 1][20];
char ledItems[LED_CODE_COUNT + 1][20];
char acItems [AC_CODE_COUNT  + 1][20];
char rfReplayItems[RF_SLOT_COUNT + 1][24];

// ─── Global state ─────────────────────────────────────────────────────────────
MenuLevel currentMenu = MENU_MAIN;
int       menuIndex   = 0;   // cursor for the active top-level menu
int       subIndex    = 0;   // cursor for sub-menus (IR codes, RF slots)
int       menuScroll  = 0;   // first visible item index

IRsend   irsend(PIN_IR_TX);
// IR_RX_BUF_SIZE = capture buffer, IR_RX_TIMEOUT_MS = inter-signal gap timeout
IRrecv   irrecv(PIN_IR_RX, IR_RX_BUF_SIZE, IR_RX_TIMEOUT_MS, true);
decode_results irResult;
uint64_t lastIRCode   = 0;
decode_type_t lastIRProtocol = UNKNOWN;
uint16_t lastIRBits   = 0;

RCSwitch rcswitch;

// ─── RF EEPROM helpers ────────────────────────────────────────────────────────
struct RFSlot {
  uint32_t value;
  uint32_t bitlen;
  uint32_t protocol;
  uint32_t valid;   // RF_SLOT_VALID_MARKER when slot contains data
};

void rfReadSlot(uint8_t idx, RFSlot &s) {
  int base = RF_EEPROM_BASE + idx * RF_SLOT_SIZE;
  EEPROM.get(base, s);
}

void rfWriteSlot(uint8_t idx, const RFSlot &s) {
  int base = RF_EEPROM_BASE + idx * RF_SLOT_SIZE;
  EEPROM.put(base, s);
  EEPROM.commit();
}

uint8_t rfFindFreeSlot() {
  for (uint8_t i = 0; i < RF_SLOT_COUNT; i++) {
    RFSlot s;
    rfReadSlot(i, s);
    if (s.valid != RF_SLOT_VALID_MARKER) return i;
  }
  return 0xFF;
}

void buildRFReplayMenu() {
  for (uint8_t i = 0; i < RF_SLOT_COUNT; i++) {
    RFSlot s;
    rfReadSlot(i, s);
    if (s.valid == RF_SLOT_VALID_MARKER) {
      snprintf(rfReplayItems[i], sizeof(rfReplayItems[0]),
               "Slot%u: %lu", i, (unsigned long)s.value);
    } else {
      snprintf(rfReplayItems[i], sizeof(rfReplayItems[0]), "Slot%u: pusty", i);
    }
  }
  snprintf(rfReplayItems[RF_SLOT_COUNT], sizeof(rfReplayItems[0]), "< Powrot");
}

// ─── TFT display helpers ──────────────────────────────────────────────────────

// Draw the header bar with the given title
void drawHeader(const char *title) {
  tft.fillRect(0, 0, TFT_W, HEADER_H, COL_HEADER);
  tft.setTextColor(COL_HEADER_TXT, COL_HEADER);
  tft.setTextSize(2);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(title, 10, HEADER_H / 2);
}

// Draw the footer hint
void drawFooter(const char *hint) {
  tft.fillRect(0, TFT_H - FOOTER_H, TFT_W, FOOTER_H, COL_FOOTER);
  tft.setTextColor(COL_FOOTER_TXT, COL_FOOTER);
  tft.setTextSize(1);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(hint, 8, TFT_H - FOOTER_H / 2);
}

// Draw the scrollable list area
// items[]  – array of C-string pointers
// count    – total items
// selected – currently highlighted index
// scroll   – first visible item index (updated internally via global menuScroll)
void drawList(const char *const items[], uint8_t count, int selected) {
  int listTop = HEADER_H;
  int listH   = TFT_H - HEADER_H - FOOTER_H;

  // Adjust scroll so that selected is always visible
  if (selected < menuScroll) menuScroll = selected;
  if (selected >= menuScroll + ITEMS_VISIBLE) menuScroll = selected - ITEMS_VISIBLE + 1;

  tft.fillRect(0, listTop, TFT_W, listH, COL_BG);

  for (int i = 0; i < ITEMS_VISIBLE; i++) {
    int idx = menuScroll + i;
    if (idx >= count) break;

    int y = listTop + i * ITEM_H;
    bool sel = (idx == selected);

    if (sel) {
      tft.fillRect(0, y, TFT_W, ITEM_H, COL_SEL_BG);
      // Draw selection arrow
      tft.setTextColor(COL_SEL_TXT, COL_SEL_BG);
      tft.drawString(">", 6, y + ITEM_H / 2);
    } else {
      tft.setTextColor(COL_ITEM_TXT, COL_BG);
    }
    tft.setTextSize(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(items[idx], 22, y + ITEM_H / 2);

    // Divider line
    if (!sel) {
      tft.drawFastHLine(0, y + ITEM_H - 1, TFT_W, 0x2104);
    }
  }

  // Scroll indicators
  if (menuScroll > 0) {
    tft.setTextColor(COL_ACCENT, COL_BG);
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("^", TFT_W - 4, listTop + 8);
  }
  if (menuScroll + ITEMS_VISIBLE < count) {
    tft.setTextColor(COL_ACCENT, COL_BG);
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("v", TFT_W - 4, listTop + listH - 8);
  }
}

// Full menu redraw (header + list + footer)
void drawMenu(const char *title, const char *const items[],
              uint8_t count, int selected) {
  drawHeader(title);
  drawList(items, count, selected);
  drawFooter("UP/DN: nawiguj  OK: wybierz");
}

// Helper overload for char[][N] arrays (RF / code sub-menus)
template<int N>
void drawMenuArr(const char *title, char items[][N], uint8_t count, int selected) {
  // Build pointer array on the stack
  const char *ptrs[count];
  for (int i = 0; i < count; i++) ptrs[i] = items[i];
  drawMenu(title, ptrs, count, selected);
}

// Display a temporary message (2 s)
void showMessage(const char *line1, const char *line2 = nullptr) {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(line1, TFT_W / 2, TFT_H / 2 - (line2 ? 30 : 0));
  if (line2) {
    tft.setTextColor(COL_ITEM_TXT, COL_BG);
    tft.setTextSize(2);
    tft.drawString(line2, TFT_W / 2, TFT_H / 2 + 20);
  }
  delay(MESSAGE_DISPLAY_MS);
  tft.fillScreen(COL_BG);
}

// ─── Menu navigation helper ───────────────────────────────────────────────────
void navigate(int &idx, uint8_t count, bool up) {
  if (up) { if (idx > 0) idx--; else idx = count - 1; }
  else    { if (idx < count - 1) idx++; else idx = 0; }
}

// ─── IR helpers ───────────────────────────────────────────────────────────────
void sendIRCode(const IRCode &c) {
  irsend.send(c.protocol, c.code, c.bits);
}

void irLearnMode() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Skieruj pilot i", TFT_W / 2, TFT_H / 2 - 30);
  tft.drawString("nacisnij przycisk", TFT_W / 2, TFT_H / 2);
  tft.setTextSize(1);
  tft.drawString("(OK aby anulowac)", TFT_W / 2, TFT_H / 2 + 40);

  irrecv.enableIRIn();
  unsigned long start = millis();

  while (millis() - start < IR_LEARN_TIMEOUT_MS) {
    if (btnPressed(PIN_BTN_OK, btnOk)) { irrecv.disableIRIn(); return; }
    if (irrecv.decode(&irResult)) {
      lastIRCode     = irResult.value;
      lastIRProtocol = irResult.decode_type;
      lastIRBits     = irResult.bits;
      irrecv.resume();
      irrecv.disableIRIn();
      char buf[32];
      snprintf(buf, sizeof(buf), "0x%08llX", lastIRCode);
      showMessage("Zapisano kod!", buf);
      return;
    }
    delay(10);
  }
  irrecv.disableIRIn();
  showMessage("Timeout!", "Brak sygnalu IR");
}

void irReplayLast() {
  if (lastIRProtocol == UNKNOWN) {
    showMessage("Brak kodu", "Najpierw naucz!");
    return;
  }
  irsend.send(lastIRProtocol, lastIRCode, lastIRBits);
  showMessage("IR Wyslano!", "Ostatni kod");
}

// ─── RF helpers ───────────────────────────────────────────────────────────────
void rfScanAndSave() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Nacisnij przycisk RF", TFT_W / 2, TFT_H / 2 - 20);
  tft.setTextSize(1);
  tft.drawString("(OK aby anulowac)", TFT_W / 2, TFT_H / 2 + 30);

  rcswitch.enableReceive(PIN_RF_RX);
  unsigned long start = millis();

  while (millis() - start < RF_SCAN_TIMEOUT_MS) {
    if (btnPressed(PIN_BTN_OK, btnOk)) { rcswitch.disableReceive(); return; }
    if (rcswitch.available()) {
      RFSlot s;
      s.value    = (uint32_t)rcswitch.getReceivedValue();
      s.bitlen   = (uint32_t)rcswitch.getReceivedBitlength();
      s.protocol = (uint32_t)rcswitch.getReceivedProtocol();
      s.valid    = RF_SLOT_VALID_MARKER;
      rcswitch.resetAvailable();
      rcswitch.disableReceive();

      uint8_t slot = rfFindFreeSlot();
      bool overwrite = (slot == 0xFF);
      if (overwrite) {
        slot = 0;   // all slots full – overwrite slot 0
        showMessage("Pelna pamiiec!", "Nadpisuje Slot 0");
      }
      rfWriteSlot(slot, s);

      char buf[30];
      snprintf(buf, sizeof(buf), "Slot %u: %lu", slot, (unsigned long)s.value);
      showMessage("RF Zapisano!", buf);
      return;
    }
    delay(10);
  }
  rcswitch.disableReceive();
  showMessage("Timeout!", "Brak sygnalu RF");
}

void rfReplay(uint8_t idx) {
  RFSlot s;
  rfReadSlot(idx, s);
  if (s.valid != RF_SLOT_VALID_MARKER) {
    showMessage("Slot pusty!", nullptr);
    return;
  }
  rcswitch.enableTransmit(PIN_RF_TX);
  rcswitch.setProtocol(s.protocol);
  rcswitch.send(s.value, s.bitlen);
  rcswitch.disableTransmit();
  char buf[24];
  snprintf(buf, sizeof(buf), "Val: %lu", (unsigned long)s.value);
  showMessage("RF Wyslano!", buf);
}

void rfDeleteSlot(uint8_t idx) {
  RFSlot s;
  memset(&s, 0, sizeof(s));
  rfWriteSlot(idx, s);
  char buf[20];
  snprintf(buf, sizeof(buf), "Slot %u usunieto", idx);
  showMessage("RF Usunieto", buf);
}

// ─── Build static sub-menu item lists ────────────────────────────────────────
void buildStaticMenus() {
  for (uint8_t i = 0; i < TV_CODE_COUNT; i++)
    strncpy(tvItems[i],  TV_CODES[i].label,  sizeof(tvItems[0]) - 1);
  strncpy(tvItems[TV_CODE_COUNT], "< Powrot", sizeof(tvItems[0]) - 1);

  for (uint8_t i = 0; i < LED_CODE_COUNT; i++)
    strncpy(ledItems[i], LED_CODES[i].label, sizeof(ledItems[0]) - 1);
  strncpy(ledItems[LED_CODE_COUNT], "< Powrot", sizeof(ledItems[0]) - 1);

  for (uint8_t i = 0; i < AC_CODE_COUNT; i++)
    strncpy(acItems[i],  AC_CODES[i].label,  sizeof(acItems[0]) - 1);
  strncpy(acItems[AC_CODE_COUNT], "< Powrot", sizeof(acItems[0]) - 1);
}

// ─── Arduino entry points ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Buttons
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DN, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);

  // TFT display – landscape 480x320
  tft.init();
  tft.setRotation(1);           // 1 = landscape; adjust if needed (0-3)
  tft.fillScreen(COL_BG);

  // Read actual dimensions after rotation so layout constants are correct
  TFT_W = tft.width();
  TFT_H = tft.height();
  ITEMS_VISIBLE = (TFT_H - HEADER_H - FOOTER_H) / ITEM_H;

  // IR
  irsend.begin();

  // RF
  rcswitch.enableTransmit(PIN_RF_TX);

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Build item arrays
  buildStaticMenus();
  buildRFReplayMenu();

  // Draw initial menu
  menuScroll = 0;
  drawMenu("MENU GLOWNE", MAIN_ITEMS, MAIN_COUNT, menuIndex);
}

void loop() {
  bool up  = btnPressed(PIN_BTN_UP, btnUp);
  bool dn  = btnPressed(PIN_BTN_DN, btnDn);
  bool ok  = btnPressed(PIN_BTN_OK, btnOk);

  bool redraw = false;

  switch (currentMenu) {

    // ── Main menu ─────────────────────────────────────────────────────────────
    case MENU_MAIN:
      if (up || dn) { navigate(menuIndex, MAIN_COUNT, up); redraw = true; }
      if (ok) {
        menuScroll = 0;
        if (menuIndex == 0) { currentMenu = MENU_IR; menuIndex = 0; redraw = true; }
        if (menuIndex == 1) { currentMenu = MENU_RF; menuIndex = 0; buildRFReplayMenu(); redraw = true; }
      }
      if (redraw) drawMenu("MENU GLOWNE", MAIN_ITEMS, MAIN_COUNT, menuIndex);
      break;

    // ── IR sub-menu ───────────────────────────────────────────────────────────
    case MENU_IR:
      if (up || dn) { navigate(menuIndex, IR_COUNT, up); redraw = true; }
      if (ok) {
        menuScroll = 0;
        switch (menuIndex) {
          case 0: currentMenu = MENU_IR_TV;  subIndex = 0; redraw = true; break;
          case 1: currentMenu = MENU_IR_LED; subIndex = 0; redraw = true; break;
          case 2: currentMenu = MENU_IR_AC;  subIndex = 0; redraw = true; break;
          case 3: irLearnMode(); redraw = true; break;
          case 4: irReplayLast(); redraw = true; break;
          case 5: currentMenu = MENU_MAIN; menuIndex = 0; redraw = true; break;
        }
      }
      if (redraw) drawMenu("IR Piloty", IR_ITEMS, IR_COUNT, menuIndex);
      break;

    // ── TV codes ──────────────────────────────────────────────────────────────
    case MENU_IR_TV:
      if (up || dn) { navigate(subIndex, TV_CODE_COUNT + 1, up); redraw = true; }
      if (ok) {
        if (subIndex < TV_CODE_COUNT) {
          sendIRCode(TV_CODES[subIndex]);
          char buf[24];
          snprintf(buf, sizeof(buf), "TV: %s", TV_CODES[subIndex].label);
          showMessage("IR Wyslano!", buf);
          redraw = true;
        } else {
          menuScroll = 0;
          currentMenu = MENU_IR; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenuArr("TV Kody", tvItems, TV_CODE_COUNT + 1, subIndex);
      break;

    // ── LED codes ─────────────────────────────────────────────────────────────
    case MENU_IR_LED:
      if (up || dn) { navigate(subIndex, LED_CODE_COUNT + 1, up); redraw = true; }
      if (ok) {
        if (subIndex < LED_CODE_COUNT) {
          sendIRCode(LED_CODES[subIndex]);
          char buf[24];
          snprintf(buf, sizeof(buf), "LED: %s", LED_CODES[subIndex].label);
          showMessage("IR Wyslano!", buf);
          redraw = true;
        } else {
          menuScroll = 0;
          currentMenu = MENU_IR; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenuArr("LED Kody", ledItems, LED_CODE_COUNT + 1, subIndex);
      break;

    // ── AC codes ──────────────────────────────────────────────────────────────
    case MENU_IR_AC:
      if (up || dn) { navigate(subIndex, AC_CODE_COUNT + 1, up); redraw = true; }
      if (ok) {
        if (subIndex < AC_CODE_COUNT) {
          sendIRCode(AC_CODES[subIndex]);
          char buf[24];
          snprintf(buf, sizeof(buf), "AC: %s", AC_CODES[subIndex].label);
          showMessage("IR Wyslano!", buf);
          redraw = true;
        } else {
          menuScroll = 0;
          currentMenu = MENU_IR; menuIndex = 0; redraw = true;
        }
      }
      if (redraw) drawMenuArr("Klimatyzacja", acItems, AC_CODE_COUNT + 1, subIndex);
      break;

    // ── RF sub-menu ───────────────────────────────────────────────────────────
    case MENU_RF:
      if (up || dn) { navigate(menuIndex, RF_COUNT, up); redraw = true; }
      if (ok) {
        menuScroll = 0;
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
      if (ok) {
        if (subIndex < RF_SLOT_COUNT) {
          rfReplay(subIndex);
          redraw = true;
        } else {
          menuScroll = 0;
          currentMenu = MENU_RF; menuIndex = 1; redraw = true;
        }
      }
      if (redraw) drawMenuArr("RF Odtwarzaj", rfReplayItems, RF_SLOT_COUNT + 1, subIndex);
      break;

    // ── RF delete slot picker ─────────────────────────────────────────────────
    case MENU_RF_DELETE:
      if (up || dn) { navigate(subIndex, RF_SLOT_COUNT + 1, up); redraw = true; }
      if (ok) {
        if (subIndex < RF_SLOT_COUNT) {
          rfDeleteSlot(subIndex);
          buildRFReplayMenu();
          redraw = true;
        } else {
          menuScroll = 0;
          currentMenu = MENU_RF; menuIndex = 2; redraw = true;
        }
      }
      if (redraw) drawMenuArr("RF Usun slot", rfReplayItems, RF_SLOT_COUNT + 1, subIndex);
      break;

    default:
      currentMenu = MENU_MAIN;
      menuIndex   = 0;
      menuScroll  = 0;
      drawMenu("MENU GLOWNE", MAIN_ITEMS, MAIN_COUNT, menuIndex);
      break;
  }
}
