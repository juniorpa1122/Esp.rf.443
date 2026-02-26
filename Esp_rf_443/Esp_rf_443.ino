/*
 * Esp_rf_443 – ESP32 RF 433 MHz + IR + WiFi Controller
 *
 * Performance improvements over the original sketch:
 *
 * 1. pressedEdge(): replaced pin-indexed static arrays [50] with a compact
 *    struct array sized to the actual number of buttons. The original arrays
 *    wasted ~200 bytes of RAM and could silently corrupt memory for pins >= 50.
 *
 * 2. ccInitOnce(): added a ccInitialized guard so radio.begin() truly executes
 *    only once, regardless of how many times TX/RX operations are started.
 *
 * 3. doWifiScan(): replaced the blocking WiFi.scanNetworks() call (which stalls
 *    the CPU for several seconds) with an async scan started by doWifiScanStart()
 *    and polled each loop() iteration by doWifiScanPoll().
 *    The blocking delay(200) before scanning is also removed.
 *
 * 4. irLearnStep(): replaced the 8-second blocking while-loop (with delay(5)
 *    inside) with a non-blocking state machine. irLearnBegin() kicks off the
 *    learn window; irLearnPoll() is called from loop() and returns immediately
 *    when no IR signal is present, keeping buttons and the display responsive.
 *
 * 5. protoName(): changed return type from String to const char* to eliminate
 *    a heap allocation on every call.
 *
 * 6. logLine(): changed parameter from const String& to const char* so callers
 *    can pass stack-allocated snprintf buffers directly without constructing
 *    temporary String objects. All String concatenation in the original code is
 *    replaced with snprintf() calls, reducing heap fragmentation on ESP32.
 *
 * 7. irSendLast(): replaced the if-else-if chain with a switch statement.
 *
 * 8. ccSetFlag(): marked IRAM_ATTR so the ISR executes from IRAM, which is
 *    required on ESP32 when the flash cache may be disabled during an interrupt.
 */

#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <RadioLib.h>

#define IRREMOTE_DISABLE_RECEIVE_COMPLETE_CALLBACK
#include <IRremote.hpp>

// ========== SPI PINS ==========
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// ========== TFT ==========
#define TFT_CS   2
#define TFT_DC   5
#define TFT_RST  4
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ========== CC1101 ==========
#define CC1101_CS    26
#define CC1101_GDO0  14
CC1101 radio = new Module(CC1101_CS, CC1101_GDO0, RADIOLIB_NC, RADIOLIB_NC);

// ========== IR ==========
#define IR_RX_PIN 27
#define IR_TX_PIN 25

// ========== Buttons ==========
#define BTN_WIFI    32
#define BTN_RX      33
#define BTN_TX      15
#define BTN_IRTX    13
#define BTN_IRLEARN 12

// ====== UI state ======
enum Screen { SCR_HOME, SCR_WIFI, SCR_CC_RX, SCR_CC_TX, SCR_IR };
Screen scr = SCR_HOME;

// ====== CC1101 RX ======
volatile bool ccRxFlag = false;
bool ccRxOn = false;

// IMPROVEMENT 2: guard so radio.begin() truly runs only once
bool ccInitialized = false;

// IMPROVEMENT 8: IRAM_ATTR ensures ISR executes from IRAM on ESP32
void IRAM_ATTR ccSetFlag() { ccRxFlag = true; }

// ========== Constants ==========
#define BTN_DEBOUNCE_MS       180   // button debounce window in milliseconds
#define MAX_WIFI_DISPLAY      14    // maximum number of WiFi networks shown
#define IR_LEARN_TIMEOUT_MS   8000  // IR learn window duration in milliseconds

// ========== Button debounce ==========
// IMPROVEMENT 1: compact struct array sized to actual button count instead of
// static arrays of 50 elements indexed by GPIO pin number, which wasted ~200
// bytes of RAM and would corrupt memory for any pin number >= 50.
struct BtnState {
  int      pin;
  uint8_t  last;
  uint32_t lastMs;
};

static BtnState btns[] = {
  { BTN_WIFI,    HIGH, 0 },
  { BTN_RX,      HIGH, 0 },
  { BTN_TX,      HIGH, 0 },
  { BTN_IRTX,    HIGH, 0 },
  { BTN_IRLEARN, HIGH, 0 },
};
static const int BTN_COUNT = sizeof(btns) / sizeof(btns[0]);

bool pressedEdge(int pin) {
  for (int i = 0; i < BTN_COUNT; i++) {
    if (btns[i].pin != pin) continue;
    uint32_t now = millis();
    uint8_t  cur = digitalRead(pin);
    bool edge = (btns[i].last == HIGH && cur == LOW &&
                 (now - btns[i].lastMs) > BTN_DEBOUNCE_MS);
    if (edge) btns[i].lastMs = now;
    btns[i].last = cur;
    return edge;
  }
  return false;
}

// ========== Display helpers ==========
void header(const char* title) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(6, 6);
  tft.println(title);
  tft.drawFastHLine(0, 28, 240, ST77XX_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(6, 34);
  tft.println("WIFI:BTN1  RX:BTN2  TX:BTN3  IR:BTN4/5");
}

// IMPROVEMENT 6: accept const char* to avoid constructing temporary String
// objects; callers use snprintf() into stack buffers instead of concatenation.
void logLine(int &y, const char* s, uint16_t color = ST77XX_WHITE) {
  if (y > 270) {
    header("LOG");
    y = 50;
  }
  tft.setTextColor(color, ST77XX_BLACK);
  tft.setCursor(6, y);
  tft.print(s);
  y += 10;
}

void showHome() {
  scr = SCR_HOME;
  header("HOME");
  int y = 50;
  logLine(y, "BTN1 WiFi scan");
  logLine(y, "BTN2 CC1101 RX on/off");
  logLine(y, "BTN3 CC1101 TX test");
  logLine(y, "BTN5 IR learn (receive)");
  logLine(y, "BTN4 IR send last");
}

// ========== WiFi ==========
// IMPROVEMENT 3: async WiFi scan so the CPU is never blocked for seconds.
// doWifiScanStart() initiates the scan; doWifiScanPoll() is called every
// loop() iteration and renders results only when the scan completes.
static bool wifiScanPending = false;
static int  wifiLogY        = 50;

void doWifiScanStart() {
  scr = SCR_WIFI;
  header("WiFi scan");
  wifiLogY = 50;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  // delay(200) removed – the async scan handles the settling period internally

  logLine(wifiLogY, "Scanning...");
  WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
  wifiScanPending = true;
}

void doWifiScanPoll() {
  if (!wifiScanPending) return;

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return; // still in progress

  wifiScanPending = false;

  if (n <= 0) {
    logLine(wifiLogY, "No networks", ST77XX_RED);
    WiFi.scanDelete();
    return;
  }

  char buf[48];
  snprintf(buf, sizeof(buf), "Found: %d", n);
  logLine(wifiLogY, buf, ST77XX_GREEN);

  int show = (n < MAX_WIFI_DISPLAY) ? n : MAX_WIFI_DISPLAY;
  for (int i = 0; i < show; i++) {
    snprintf(buf, sizeof(buf), "%d) %.28s (%d)", i + 1,
             WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    logLine(wifiLogY, buf);
  }
  WiFi.scanDelete();
}

// ========== CC1101 ==========
// IMPROVEMENT 2: ccInitialized flag ensures radio.begin() is called only once.
int ccInitOnce() {
  if (ccInitialized) return RADIOLIB_ERR_NONE;

  // 433.92 MHz, bitrate 4.8 kbps, freq dev 5.0 kHz, rxBW 58.0 kHz,
  // power 10 dBm, preamble 16 bytes
  int state = radio.begin(433.92, 4.8, 5.0, 58.0, 10, 16);
  if (state == RADIOLIB_ERR_NONE) {
    ccInitialized = true;
  } else {
    Serial.printf("CC1101 begin err=%d\n", state);
  }
  return state;
}

// CC1101 RX packet log cursor; reset to 50 each time RX is activated
static int ccRxY = 50;

void ccRxStartStop() {
  char buf[28];
  if (!ccRxOn) {
    scr = SCR_CC_RX;
    header("CC1101 RX");
    int y = 50;

    int state = ccInitOnce();
    if (state != RADIOLIB_ERR_NONE) {
      snprintf(buf, sizeof(buf), "begin ERR=%d", state);
      logLine(y, buf, ST77XX_RED);
      return;
    }

    radio.setPacketReceivedAction(ccSetFlag);
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
      snprintf(buf, sizeof(buf), "startReceive ERR=%d", state);
      logLine(y, buf, ST77XX_RED);
      return;
    }

    ccRxOn   = true;
    ccRxFlag = false;
    ccRxY    = y; // reset packet log cursor to current position
    logLine(y, "RX ON @433.92", ST77XX_GREEN);
    logLine(y, "Waiting...");
    Serial.println("CC1101 RX ON");
  } else {
    radio.standby();
    ccRxOn = false;
    scr = SCR_CC_RX;
    header("CC1101 RX");
    int y = 50;
    logLine(y, "RX OFF", ST77XX_YELLOW);
    Serial.println("CC1101 RX OFF");
  }
}

void ccTxTest() {
  scr = SCR_CC_TX;
  header("CC1101 TX");
  int y = 50;

  int state = ccInitOnce();
  if (state != RADIOLIB_ERR_NONE) {
    char buf[24];
    snprintf(buf, sizeof(buf), "begin ERR=%d", state);
    logLine(y, buf, ST77XX_RED);
    return;
  }

  logLine(y, "TX: TEST433");
  state = radio.transmit("TEST433");
  if (state == RADIOLIB_ERR_NONE) {
    logLine(y, "TX OK", ST77XX_GREEN);
    Serial.println("CC1101 TX OK");
  } else {
    char buf[20];
    snprintf(buf, sizeof(buf), "TX ERR=%d", state);
    logLine(y, buf, ST77XX_RED);
    Serial.printf("CC1101 TX err=%d\n", state);
  }
}

// ========== IR ==========
struct LearnedIR {
  bool     valid    = false;
  uint16_t address  = 0;
  uint16_t command  = 0;
  uint8_t  protocol = 0;
} lastIr;

// IMPROVEMENT 5: return const char* instead of String to avoid a heap
// allocation on every call.
const char* protoName(uint8_t p) {
  switch (p) {
    case NEC:  return "NEC";
    case SONY: return "SONY";
    case RC5:  return "RC5";
    case RC6:  return "RC6";
    default:   return "OTHER";
  }
}

void irShow() {
  scr = SCR_IR;
  header("IR");
  int y = 50;
  if (!lastIr.valid) {
    logLine(y, "No code learned yet", ST77XX_YELLOW);
  } else {
    char buf[32];
    logLine(y, protoName(lastIr.protocol));
    snprintf(buf, sizeof(buf), "Addr : 0x%04X", lastIr.address);
    logLine(y, buf);
    snprintf(buf, sizeof(buf), "Cmd  : 0x%04X", lastIr.command);
    logLine(y, buf);
  }
  logLine(y, "BTN5 learn / BTN4 send");
}

// IMPROVEMENT 4: non-blocking IR learn state machine.
// The original while-loop with delay(5) blocked the CPU for up to 8 seconds,
// making buttons and the display unresponsive during that time.
// irLearnBegin() kicks off the learn window; irLearnPoll() is called from
// loop() and returns immediately when no IR data is available.
static bool     irLearning   = false;
static uint32_t irLearnStart = 0;

void irLearnBegin() {
  scr = SCR_IR;
  header("IR LEARN");
  int y = 50;
  logLine(y, "Point remote at RX", ST77XX_GREEN);
  logLine(y, "Waiting...", ST77XX_GREEN);

  IrReceiver.resume();
  irLearning   = true;
  irLearnStart = millis();
}

void irLearnPoll() {
  if (!irLearning) return;

  if (IrReceiver.decode()) {
    auto &d = IrReceiver.decodedIRData;

    lastIr.valid    = true;
    lastIr.protocol = (uint8_t)d.protocol;
    lastIr.address  = d.address;
    lastIr.command  = d.command;

    Serial.printf("IR learned: %s addr=0x%04X cmd=0x%04X\n",
                  protoName(lastIr.protocol), lastIr.address, lastIr.command);

    char buf[32];
    header("IR LEARN");
    int y = 50;
    logLine(y, "Learned OK", ST77XX_GREEN);
    logLine(y, protoName(lastIr.protocol));
    snprintf(buf, sizeof(buf), "Addr : 0x%04X", lastIr.address);
    logLine(y, buf);
    snprintf(buf, sizeof(buf), "Cmd  : 0x%04X", lastIr.command);
    logLine(y, buf);

    IrReceiver.resume();
    // delay(800) removed – caller returns immediately; loop() continues normally
    irLearning = false;
    return;
  }

  if (millis() - irLearnStart >= IR_LEARN_TIMEOUT_MS) {
    irLearning = false;
    header("IR LEARN");
    int y = 50;
    logLine(y, "Timeout (no signal)", ST77XX_RED);
  }
}

// IMPROVEMENT 7: switch statement instead of if-else chain for IR send.
void irSendLast() {
  scr = SCR_IR;
  header("IR SEND");
  int y = 50;

  if (!lastIr.valid) {
    logLine(y, "No learned code!", ST77XX_RED);
    return;
  }

  char buf[32];
  logLine(y, "Sending...", ST77XX_GREEN);
  logLine(y, protoName(lastIr.protocol));
  snprintf(buf, sizeof(buf), "Addr : 0x%04X", lastIr.address);
  logLine(y, buf);
  snprintf(buf, sizeof(buf), "Cmd  : 0x%04X", lastIr.command);
  logLine(y, buf);

  bool sent = false;
  switch (lastIr.protocol) {
    case NEC:  IrSender.sendNEC(lastIr.address, lastIr.command, 0);  sent = true; break;
    case SONY: IrSender.sendSony(lastIr.command, 12, 2);             sent = true; break;
    case RC5:  IrSender.sendRC5(lastIr.address, lastIr.command, 0);  sent = true; break;
    case RC6:  IrSender.sendRC6(lastIr.address, lastIr.command, 0);  sent = true; break;
    default:   break;
  }

  if (sent) {
    logLine(y, "IR TX OK", ST77XX_GREEN);
    Serial.println("IR TX OK");
  } else {
    logLine(y, "Unsupported proto", ST77XX_YELLOW);
    Serial.println("IR TX unsupported protocol");
  }
}

// ========== setup / loop ==========
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < BTN_COUNT; i++) {
    pinMode(btns[i].pin, INPUT_PULLUP);
  }

  tft.init(240, 320);
  tft.setRotation(1);

  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_TX_PIN);

  showHome();
}

void loop() {
  // Poll async WiFi scan result (IMPROVEMENT 3)
  doWifiScanPoll();

  // Poll non-blocking IR learn state machine (IMPROVEMENT 4)
  irLearnPoll();

  // Handle CC1101 packet received via ISR flag (IMPROVEMENT 8)
  if (ccRxOn && ccRxFlag) {
    ccRxFlag = false;
    uint8_t pkt[64];
    int     pktLen = sizeof(pkt);
    int     state  = radio.readData(pkt, pktLen);
    if (state == RADIOLIB_ERR_NONE) {
      char buf[72];
      snprintf(buf, sizeof(buf), "RX %d B: %.*s", pktLen, pktLen, (char*)pkt);
      logLine(ccRxY, buf, ST77XX_GREEN);
      Serial.println(buf);
    }
    radio.startReceive();
  }

  // Button handling
  if (pressedEdge(BTN_WIFI))    doWifiScanStart();
  if (pressedEdge(BTN_RX))      ccRxStartStop();
  if (pressedEdge(BTN_TX))      ccTxTest();
  if (pressedEdge(BTN_IRTX))    irSendLast();
  if (pressedEdge(BTN_IRLEARN)) irLearnBegin();
}
