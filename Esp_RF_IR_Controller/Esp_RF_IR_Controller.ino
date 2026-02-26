/*
 * Esp_RF_IR_Controller.ino
 * 
 * ESP32 Controller with:
 *   - ST7789 240x240 TFT display
 *   - RCSwitch 433 MHz RF transceiver
 *   - IRremote infrared transceiver
 * 
 * Menu (3 functions):
 *   1. IR TV Baza    – learn, store and retransmit IR remote codes
 *   2. Skaner        – sweep RF protocols and IR for any nearby signal
 *   3. Nadajnik      – blast test RF/IR signals; watch for a response
 * 
 * Required libraries (install via Arduino Library Manager):
 *   - Adafruit GFX Library
 *   - Adafruit ST7789
 *   - RCSwitch
 *   - IRremote  (v4.x)
 * 
 * Wiring (ESP32 DevKit):
 *   ST7789  CS  -> GPIO 5
 *   ST7789  DC  -> GPIO 2
 *   ST7789  RST -> GPIO 4
 *   ST7789  SDA -> GPIO 23  (MOSI)
 *   ST7789  SCL -> GPIO 18  (SCK)
 *   ST7789  VCC -> 3.3 V
 *   ST7789  GND -> GND
 *
 *   RF TX   DATA -> GPIO 17
 *   RF RX   DATA -> GPIO 16
 *
 *   IR LED  (+) -> GPIO 14  (via 33Ω resistor)
 *   IR recv OUT -> GPIO 15
 *
 *   BTN UP     -> GPIO 32  (other leg to GND, internal pull-up)
 *   BTN DOWN   -> GPIO 33
 *   BTN SELECT -> GPIO 25
 *   BTN BACK   -> GPIO 26
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <RCSwitch.h>
#include <IRremote.hpp>

// ── Pin definitions ────────────────────────────────────────────────────────────
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4
// MOSI = 23, SCK = 18 are the default ESP32 SPI pins

#define RF_TX_PIN  17
#define RF_RX_PIN  16

#define IR_TX_PIN  14
#define IR_RX_PIN  15

#define BTN_UP     32
#define BTN_DOWN   33
#define BTN_SELECT 25
#define BTN_BACK   26

// ── Display colours ───────────────────────────────────────────────────────────
#define COL_BG      ST77XX_BLACK
#define COL_TITLE   ST77XX_CYAN
#define COL_ITEM    ST77XX_WHITE
#define COL_SEL     ST77XX_YELLOW
#define COL_OK      ST77XX_GREEN
#define COL_WARN    ST77XX_RED
#define COL_INFO    0x07FF   // light-cyan

// ── IR TV database ────────────────────────────────────────────────────────────
#define IR_DB_SIZE 20

struct IREntry {
  uint32_t code;
  uint8_t  bits;
  decode_type_t protocol;
  bool     used;
};

IREntry irDB[IR_DB_SIZE];
uint8_t irDBCount = 0;

// ── Objects ───────────────────────────────────────────────────────────────────
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
RCSwitch        rf  = RCSwitch();

// ── App state ─────────────────────────────────────────────────────────────────
enum AppState {
  STATE_MENU,
  STATE_IR_TV,
  STATE_IR_TV_LEARN,
  STATE_IR_TV_SEND,
  STATE_SCANNER,
  STATE_TRANSMITTER
};
AppState appState = STATE_MENU;

const uint8_t  MENU_COUNT   = 3;
const char    *menuLabels[]  = { "1. IR TV Baza", "2. Skaner sygnałów", "3. Nadajnik" };
int8_t         menuCursor   = 0;

// Sub-menu for IR TV
const uint8_t  IR_MENU_COUNT  = 3;
const char    *irMenuLabels[] = { "Ucz sie (learn)", "Wyslij (send)", "Powrot" };
int8_t         irMenuCursor   = 0;

// ── Button helpers ────────────────────────────────────────────────────────────
bool btnPressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(40);
    return digitalRead(pin) == LOW;
  }
  return false;
}

void waitRelease(uint8_t pin) {
  while (digitalRead(pin) == LOW) delay(10);
}

// ── Display helpers ───────────────────────────────────────────────────────────
void tftClear() { tft.fillScreen(COL_BG); }

void tftTitle(const char *title) {
  tft.setTextSize(2);
  tft.setTextColor(COL_TITLE);
  tft.setCursor(4, 4);
  tft.print(title);
  tft.drawFastHLine(0, 22, 240, COL_TITLE);
}

void tftMsg(int16_t x, int16_t y, uint16_t color, uint8_t size, const char *msg) {
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(msg);
}

// ── Draw main menu ────────────────────────────────────────────────────────────
void drawMenu() {
  tftClear();
  tftTitle("ESP RF+IR");
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    uint16_t col = (i == menuCursor) ? COL_SEL : COL_ITEM;
    tft.setTextColor(col);
    tft.setTextSize(2);
    tft.setCursor(10, 40 + i * 30);
    if (i == menuCursor) tft.print("> ");
    else                 tft.print("  ");
    tft.print(menuLabels[i]);
  }
  tft.setTextColor(COL_INFO);
  tft.setTextSize(1);
  tft.setCursor(4, 220);
  tft.print("UP/DOWN  SELECT  BACK");
}

// ── Draw IR TV sub-menu ───────────────────────────────────────────────────────
void drawIRMenu() {
  tftClear();
  tftTitle("IR TV Baza");
  for (uint8_t i = 0; i < IR_MENU_COUNT; i++) {
    uint16_t col = (i == irMenuCursor) ? COL_SEL : COL_ITEM;
    tft.setTextColor(col);
    tft.setTextSize(2);
    tft.setCursor(10, 40 + i * 30);
    if (i == irMenuCursor) tft.print("> ");
    else                   tft.print("  ");
    tft.print(irMenuLabels[i]);
  }
  tft.setTextColor(COL_INFO);
  tft.setTextSize(1);
  tft.setCursor(4, 150);
  tft.printf("Kody w bazie: %d/%d", irDBCount, IR_DB_SIZE);
}

// ══════════════════════════════════════════════════════════════════════════════
// FUNCTION 1 – IR TV BASE
//   • Learn mode: receive IR signals, decode and store in irDB[]
//   • Send mode:  show stored codes on screen and retransmit selected one
// ══════════════════════════════════════════════════════════════════════════════

void runIRLearn() {
  tftClear();
  tftTitle("IR Nauka (Learn)");
  tftMsg(4, 30, COL_ITEM, 1, "Skieruj pilot w strone");
  tftMsg(4, 42, COL_ITEM, 1, "odbiornika i nacisnij");
  tftMsg(4, 54, COL_ITEM, 1, "przycisk na pilocie.");
  tftMsg(4, 70, COL_WARN, 1, "[BACK] aby wyjsc");

  IrReceiver.begin(IR_RX_PIN, DISABLE_LED_FEEDBACK);

  uint32_t timeout = millis() + 30000UL;   // 30-second window

  while (millis() < timeout) {
    if (btnPressed(BTN_BACK)) {
      waitRelease(BTN_BACK);
      IrReceiver.stop();
      return;
    }

    if (IrReceiver.decode()) {
      auto *res = &IrReceiver.decodedIRData;

      tft.fillRect(0, 90, 240, 100, COL_BG);
      tft.setTextColor(COL_OK);
      tft.setTextSize(1);
      tft.setCursor(4, 92);
      tft.printf("Protokol : %s", getProtocolString(res->protocol));
      tft.setCursor(4, 104);
      tft.printf("Kod (HEX): 0x%08X", res->decodedRawData);
      tft.setCursor(4, 116);
      tft.printf("Bity     : %d", res->numberOfBits);

      // Store if space available and not a repeat
      if (!(res->flags & IRDATA_FLAGS_IS_REPEAT) && irDBCount < IR_DB_SIZE) {
        bool duplicate = false;
        for (uint8_t i = 0; i < irDBCount; i++) {
          if (irDB[i].code == res->decodedRawData &&
              irDB[i].protocol == res->protocol) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          irDB[irDBCount].code     = res->decodedRawData;
          irDB[irDBCount].bits     = res->numberOfBits;
          irDB[irDBCount].protocol = res->protocol;
          irDB[irDBCount].used     = true;
          irDBCount++;
          tft.setTextColor(COL_OK);
          tft.setCursor(4, 132);
          tft.printf("Zapisano! (%d/%d)", irDBCount, IR_DB_SIZE);
        } else {
          tft.setTextColor(COL_WARN);
          tft.setCursor(4, 132);
          tft.print("Duplikat - pominiety");
        }
      } else if (irDBCount >= IR_DB_SIZE) {
        tft.setTextColor(COL_WARN);
        tft.setCursor(4, 132);
        tft.print("Baza pelna!");
      }

      IrReceiver.resume();
      timeout = millis() + 30000UL;   // reset timeout after each receive
    }
  }

  IrReceiver.stop();
  tft.fillRect(0, 160, 240, 20, COL_BG);
  tftMsg(4, 162, COL_WARN, 1, "Timeout. Nacisnij BACK.");
  delay(2000);
}

// ── IR send sub-function ──────────────────────────────────────────────────────
void runIRSend() {
  if (irDBCount == 0) {
    tftClear();
    tftTitle("IR Wyslij");
    tftMsg(4, 40, COL_WARN, 1, "Baza jest pusta!");
    tftMsg(4, 56, COL_ITEM, 1, "Najpierw ucz sie kodow.");
    tftMsg(4, 72, COL_WARN, 1, "Nacisnij BACK.");
    while (!btnPressed(BTN_BACK)) delay(10);
    waitRelease(BTN_BACK);
    return;
  }

  int8_t  sel = 0;
  bool    redraw = true;

  IrSender.begin(IR_TX_PIN);

  while (true) {
    if (redraw) {
      tftClear();
      tftTitle("IR Wyslij");
      tft.setTextSize(1);
      for (uint8_t i = 0; i < irDBCount && i < 8; i++) {
        uint16_t col = (i == sel) ? COL_SEL : COL_ITEM;
        tft.setTextColor(col);
        tft.setCursor(4, 30 + i * 18);
        tft.printf("%s%d: %s 0x%08X",
                   (i == sel) ? ">" : " ",
                   i + 1,
                   getProtocolString(irDB[i].protocol),
                   irDB[i].code);
      }
      tft.setTextColor(COL_INFO);
      tft.setCursor(4, 220);
      tft.print("SELECT=wyslij  BACK=powrot");
      redraw = false;
    }

    if (btnPressed(BTN_UP)) {
      waitRelease(BTN_UP);
      if (sel > 0) { sel--; redraw = true; }
    }
    if (btnPressed(BTN_DOWN)) {
      waitRelease(BTN_DOWN);
      if (sel < (int8_t)(irDBCount - 1)) { sel++; redraw = true; }
    }
    if (btnPressed(BTN_SELECT)) {
      waitRelease(BTN_SELECT);
      // Send the selected code 3 times for reliability using its stored protocol
      for (uint8_t r = 0; r < 3; r++) {
        IrSender.send(irDB[sel].protocol, irDB[sel].code, irDB[sel].bits);
        delay(50);
      }
      tft.fillRect(0, 200, 240, 18, COL_BG);
      tft.setTextColor(COL_OK);
      tft.setTextSize(1);
      tft.setCursor(4, 202);
      tft.printf("Wyslano: 0x%08X", irDB[sel].code);
      delay(1500);
      tft.fillRect(0, 200, 240, 18, COL_BG);
    }
    if (btnPressed(BTN_BACK)) {
      waitRelease(BTN_BACK);
      return;
    }

    delay(20);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// FUNCTION 2 – SIGNAL SCANNER
//   Sweeps all RCSwitch RF protocols (1-12) + passive IR listening.
//   Displays any detected signal on the TFT.
// ══════════════════════════════════════════════════════════════════════════════

void runScanner() {
  tftClear();
  tftTitle("Skaner sygnałów");
  tftMsg(4, 30, COL_ITEM, 1, "Szukam sygnalow RF+IR...");
  tftMsg(4, 42, COL_WARN, 1, "[BACK] aby wyjsc");

  rf.enableReceive(RF_RX_PIN);
  IrReceiver.begin(IR_RX_PIN, DISABLE_LED_FEEDBACK);

  uint8_t  rfProtocol     = 1;
  bool     foundSomething = false;
  uint8_t  lineY          = 60;
  uint32_t lastSweepMsg   = 0;
  uint8_t  sweepDot       = 0;

  // Arrays to record found signals
  uint32_t foundRF[10];    uint8_t  foundRFCnt  = 0;
  uint32_t foundIR[10];    uint8_t  foundIRCnt  = 0;

  while (true) {
    if (btnPressed(BTN_BACK)) {
      waitRelease(BTN_BACK);
      rf.disableReceive();
      IrReceiver.stop();
      return;
    }

    // ── Animated sweep indicator ──────────────────────────────────────────────
    if (millis() - lastSweepMsg > 600) {
      lastSweepMsg = millis();
      sweepDot = (sweepDot + 1) % 4;
      tft.fillRect(0, 54, 240, 10, COL_BG);
      tft.setTextColor(COL_INFO);
      tft.setTextSize(1);
      tft.setCursor(4, 54);
      tft.printf("Protokol RF %2d/12", rfProtocol);
      for (uint8_t d = 0; d < sweepDot; d++) tft.print('.');

      // Cycle to next RF protocol
      rfProtocol++;
      if (rfProtocol > 12) rfProtocol = 1;
    }

    // ── RF receive check ──────────────────────────────────────────────────────
    if (rf.available()) {
      uint32_t val   = (uint32_t)rf.getReceivedValue();
      uint8_t  proto = rf.getReceivedProtocol();

      rf.resetAvailable();

      if (val != 0 && foundRFCnt < 10) {
        foundRF[foundRFCnt++] = val;
        foundSomething = true;

        if (lineY < 210) {
          tft.setTextColor(COL_OK);
          tft.setTextSize(1);
          tft.setCursor(4, lineY);
          tft.printf("RF P%d: 0x%08X", proto, val);
          lineY += 12;
        }
      }
    }

    // ── IR receive check ──────────────────────────────────────────────────────
    if (IrReceiver.decode()) {
      auto *res = &IrReceiver.decodedIRData;
      if (!(res->flags & IRDATA_FLAGS_IS_REPEAT) && foundIRCnt < 10) {
        foundIR[foundIRCnt++] = res->decodedRawData;
        foundSomething = true;

        if (lineY < 210) {
          tft.setTextColor(COL_OK);
          tft.setTextSize(1);
          tft.setCursor(4, lineY);
          tft.printf("IR %s: 0x%08X",
                     getProtocolString(res->protocol),
                     res->decodedRawData);
          lineY += 12;
        }
      }
      IrReceiver.resume();
    }

    delay(10);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// FUNCTION 3 – SIGNAL TRANSMITTER + ECHO DETECTOR
//   Sends a set of test signals on both RF and IR, then listens for any
//   response or reflection.  Useful when the passive scan (Function 2)
//   found nothing.
// ══════════════════════════════════════════════════════════════════════════════

// Common NEC TV power codes (Samsung, LG, Sony NEC-mapped, Philips, Panasonic)
static const uint32_t IR_CODES[] = {
  0xE0E040BF,  // Samsung Power (NEC 32-bit)
  0x20DF10EF,  // LG Power      (NEC 32-bit)
  0x010090EF,  // Sony Power    (NEC 32-bit mapped)
  0x10EF08F7,  // Philips Power (NEC 32-bit)
  0x400401FD,  // Panasonic Power (NEC 32-bit)
  0xFFFFFF00,  // Generic broadcast probe
};
static const uint8_t IR_CODE_CNT = sizeof(IR_CODES) / sizeof(IR_CODES[0]);

// Common RF codes (used by cheap remote-controlled sockets)
static const uint32_t RF_CODES[] = {
  0x1361,   // Common socket ON
  0x1360,   // Common socket OFF
  0x5544AA,
  0xA55A00,
  0xFFFFFF,
};
static const uint8_t RF_CODE_CNT = sizeof(RF_CODES) / sizeof(RF_CODES[0]);

void runTransmitter() {
  tftClear();
  tftTitle("Nadajnik sygnałów");
  tftMsg(4, 30, COL_ITEM, 1, "Wysylam sygnaly testowe...");
  tftMsg(4, 42, COL_WARN, 1, "Nasluchuję na odpowiedzi.");
  tftMsg(4, 54, COL_WARN, 1, "[BACK] aby wyjsc");

  rf.enableTransmit(RF_TX_PIN);
  rf.enableReceive(RF_RX_PIN);
  IrSender.begin(IR_TX_PIN);
  IrReceiver.begin(IR_RX_PIN, DISABLE_LED_FEEDBACK);

  uint8_t  lineY      = 75;
  uint32_t nextSend   = millis();
  uint8_t  rfIdx      = 0;
  uint8_t  irIdx      = 0;
  uint8_t  rfProto    = 1;
  bool     echoFound  = false;
  uint32_t txCount    = 0;

  while (true) {
    if (btnPressed(BTN_BACK)) {
      waitRelease(BTN_BACK);
      rf.disableReceive();
      IrReceiver.stop();
      return;
    }

    // ── Transmit burst every 1.2 s ───────────────────────────────────────────
    if (millis() >= nextSend) {
      nextSend = millis() + 1200;

      // Send RF code
      rf.setProtocol(rfProto);
      rf.send(RF_CODES[rfIdx], 24);
      rfIdx   = (rfIdx   + 1) % RF_CODE_CNT;
      rfProto = (rfProto + 1);
      if (rfProto > 12) rfProto = 1;

      // Send IR code (NEC)
      IrSender.sendNEC(IR_CODES[irIdx], 32);
      irIdx = (irIdx + 1) % IR_CODE_CNT;

      txCount++;
      tft.fillRect(0, 66, 240, 8, COL_BG);
      tft.setTextColor(COL_INFO);
      tft.setTextSize(1);
      tft.setCursor(4, 66);
      tft.printf("Tx#%lu RF 0x%06X  IR 0x%08X",
                 txCount,
                 RF_CODES[(rfIdx == 0 ? RF_CODE_CNT : rfIdx) - 1],
                 IR_CODES[(irIdx == 0 ? IR_CODE_CNT : irIdx) - 1]);
    }

    // ── Listen for any response ───────────────────────────────────────────────
    if (rf.available()) {
      uint32_t val   = (uint32_t)rf.getReceivedValue();
      uint8_t  proto = rf.getReceivedProtocol();
      rf.resetAvailable();

      if (val != 0) {
        echoFound = true;
        if (lineY < 210) {
          tft.setTextColor(COL_OK);
          tft.setTextSize(1);
          tft.setCursor(4, lineY);
          tft.printf("ECHO RF P%d: 0x%08X", proto, val);
          lineY += 12;
        }
      }
    }

    if (IrReceiver.decode()) {
      auto *res = &IrReceiver.decodedIRData;
      if (!(res->flags & IRDATA_FLAGS_IS_REPEAT)) {
        echoFound = true;
        if (lineY < 210) {
          tft.setTextColor(COL_OK);
          tft.setTextSize(1);
          tft.setCursor(4, lineY);
          tft.printf("ECHO IR %s: 0x%X",
                     getProtocolString(res->protocol),
                     res->decodedRawData);
          lineY += 12;
        }
      }
      IrReceiver.resume();
    }

    // ── Status banner ─────────────────────────────────────────────────────────
    if (!echoFound && (txCount % 5 == 0) && txCount > 0) {
      tft.fillRect(0, 210, 240, 14, COL_BG);
      tft.setTextColor(COL_WARN);
      tft.setTextSize(1);
      tft.setCursor(4, 212);
      tft.print("Brak odpowiedzi...");
    }

    delay(10);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// setup() & loop()
// ══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  // Buttons with internal pull-up
  pinMode(BTN_UP,     INPUT_PULLUP);
  pinMode(BTN_DOWN,   INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK,   INPUT_PULLUP);

  // TFT init (240x240 ST7789)
  tft.init(240, 240);
  tft.setRotation(2);
  tftClear();
  tftMsg(30, 100, COL_TITLE, 2, "ESP RF+IR");
  tftMsg(20, 125, COL_ITEM,  1, "Inicjalizacja...");
  delay(1500);

  // Clear IR database
  memset(irDB, 0, sizeof(irDB));

  drawMenu();
}

void loop() {
  switch (appState) {
    // ── Main menu ─────────────────────────────────────────────────────────────
    case STATE_MENU:
      if (btnPressed(BTN_UP)) {
        waitRelease(BTN_UP);
        if (menuCursor > 0) { menuCursor--; drawMenu(); }
      }
      if (btnPressed(BTN_DOWN)) {
        waitRelease(BTN_DOWN);
        if (menuCursor < MENU_COUNT - 1) { menuCursor++; drawMenu(); }
      }
      if (btnPressed(BTN_SELECT)) {
        waitRelease(BTN_SELECT);
        switch (menuCursor) {
          case 0: appState = STATE_IR_TV;      irMenuCursor = 0; drawIRMenu(); break;
          case 1: appState = STATE_SCANNER;    runScanner();  appState = STATE_MENU; drawMenu(); break;
          case 2: appState = STATE_TRANSMITTER; runTransmitter(); appState = STATE_MENU; drawMenu(); break;
        }
      }
      break;

    // ── IR TV sub-menu ────────────────────────────────────────────────────────
    case STATE_IR_TV:
      if (btnPressed(BTN_UP)) {
        waitRelease(BTN_UP);
        if (irMenuCursor > 0) { irMenuCursor--; drawIRMenu(); }
      }
      if (btnPressed(BTN_DOWN)) {
        waitRelease(BTN_DOWN);
        if (irMenuCursor < IR_MENU_COUNT - 1) { irMenuCursor++; drawIRMenu(); }
      }
      if (btnPressed(BTN_SELECT)) {
        waitRelease(BTN_SELECT);
        switch (irMenuCursor) {
          case 0: runIRLearn(); drawIRMenu(); break;
          case 1: runIRSend();  drawIRMenu(); break;
          case 2:
            appState = STATE_MENU;
            menuCursor = 0;
            drawMenu();
            break;
        }
      }
      if (btnPressed(BTN_BACK)) {
        waitRelease(BTN_BACK);
        appState = STATE_MENU;
        menuCursor = 0;
        drawMenu();
      }
      break;

    default:
      appState = STATE_MENU;
      drawMenu();
      break;
  }

  delay(20);
}
