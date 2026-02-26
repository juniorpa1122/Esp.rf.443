#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define IRREMOTE_DISABLE_RECEIVE_COMPLETE_CALLBACK  // zostaw, bywa stabilniej na ESP32
#include <IRremote.hpp>

#include <RadioLib.h>

// ========== SPI PINS ==========
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// ========== TFT (Twoje piny) ==========
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

// ========== Buttons (podaj jeśli masz; jak nie masz, zostaw i steruj z Serial) ==========
#define BTN_WIFI    32
#define BTN_RX      33
#define BTN_TX      15
#define BTN_IRTX    13
#define BTN_IRLEARN 12

// ====== display layout ======
#define LOG_START_Y  50
#define MAX_LOG_Y   270

// ====== UI state ======
enum Screen { SCR_HOME, SCR_WIFI, SCR_CC_RX, SCR_CC_TX, SCR_IR };
Screen scr = SCR_HOME;

// ====== CC1101 RX ======
volatile bool ccRxFlag = false;
bool ccRxOn = false;
void ccSetFlag() { ccRxFlag = true; }

// ====== helpers ======
// ESP32 has GPIO 0-39; use 40 as the exclusive upper bound.
#define GPIO_PIN_COUNT 40

bool pressedEdge(int pin) {
  if (pin < 0 || pin >= GPIO_PIN_COUNT) return false;
  static uint8_t last[GPIO_PIN_COUNT];
  static uint32_t lastMs[GPIO_PIN_COUNT];
  uint32_t now = millis();
  uint8_t cur = digitalRead(pin);
  bool edge = (last[pin] == HIGH && cur == LOW && (now - lastMs[pin]) > 180);
  if (edge) lastMs[pin] = now;
  last[pin] = cur;
  return edge;
}

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

void logLine(int &y, const String &s, uint16_t color = ST77XX_WHITE) {
  if (y > MAX_LOG_Y) {
    header("LOG");
    y = LOG_START_Y;
  }
  tft.setTextColor(color, ST77XX_BLACK);
  tft.setCursor(6, y);
  tft.print(s);
  y += 10;
}

void showHome() {
  scr = SCR_HOME;
  header("HOME");
  int y = LOG_START_Y;
  logLine(y, "BTN1 WiFi scan");
  logLine(y, "BTN2 CC1101 RX on/off");
  logLine(y, "BTN3 CC1101 TX test");
  logLine(y, "BTN5 IR learn (receive)");
  logLine(y, "BTN4 IR send last");
}

// ========== WiFi ==========
void doWifiScan() {
  scr = SCR_WIFI;
  header("WiFi scan");
  int y = LOG_START_Y;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  logLine(y, "Scanning...");
  int n = WiFi.scanNetworks();

  if (n <= 0) {
    logLine(y, "No networks", ST77XX_RED);
    return;
  }

  logLine(y, "Found: " + String(n), ST77XX_GREEN);
  for (int i = 0; i < min(n, 14); i++) {
    String line = String(i + 1) + ") " + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + ")";
    logLine(y, line);
  }
}

// ========== CC1101 ==========
int ccInitOnce() {
  // 433.92 MHz, bitrate 4.8kbps, freq dev 5.0kHz, rxBW 58.0kHz, power 10, preamble 16
  int state = radio.begin(433.92, 4.8, 5.0, 58.0, 10, 16);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("CC1101 begin err=%d\n", state);
  }
  return state;
}

void ccRxStartStop() {
  if (!ccRxOn) {
    scr = SCR_CC_RX;
    header("CC1101 RX");
    int y = LOG_START_Y;

    int state = ccInitOnce();
    if (state != RADIOLIB_ERR_NONE) {
      logLine(y, "begin ERR=" + String(state), ST77XX_RED);
      return;
    }

    radio.setPacketReceivedAction(ccSetFlag);
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
      logLine(y, "startReceive ERR=" + String(state), ST77XX_RED);
      return;
    }

    ccRxOn = true;
    ccRxFlag = false;
    logLine(y, "RX ON @433.92", ST77XX_GREEN);
    logLine(y, "Waiting...");
    Serial.println("CC1101 RX ON");
  } else {
    radio.standby();
    ccRxOn = false;
    scr = SCR_CC_RX;
    header("CC1101 RX");
    int y = LOG_START_Y;
    logLine(y, "RX OFF", ST77XX_YELLOW);
    Serial.println("CC1101 RX OFF");
  }
}

void ccTxTest() {
  scr = SCR_CC_TX;
  header("CC1101 TX");
  int y = LOG_START_Y;

  int state = ccInitOnce();
  if (state != RADIOLIB_ERR_NONE) {
    logLine(y, "begin ERR=" + String(state), ST77XX_RED);
    return;
  }

  String msg = "TEST433";
  logLine(y, "TX: " + msg);

  state = radio.transmit(msg);
  if (state == RADIOLIB_ERR_NONE) {
    logLine(y, "TX OK", ST77XX_GREEN);
    Serial.println("CC1101 TX OK");
  } else {
    logLine(y, "TX ERR=" + String(state), ST77XX_RED);
    Serial.printf("CC1101 TX err=%d\n", state);
  }
}

// ========== IR ==========
struct LearnedIR {
  bool valid = false;
  uint16_t address = 0;
  uint16_t command = 0;
  uint8_t protocol = 0; // decode_type_t (ale trzymamy jako liczba)
} lastIr;

String protoName(uint8_t p) {
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
  int y = LOG_START_Y;
  if (!lastIr.valid) {
    logLine(y, "No code learned yet", ST77XX_YELLOW);
  } else {
    logLine(y, "Proto: " + protoName(lastIr.protocol));
    logLine(y, "Addr : 0x" + String(lastIr.address, HEX));
    logLine(y, "Cmd  : 0x" + String(lastIr.command, HEX));
  }
  logLine(y, "BTN5 learn / BTN4 send");
}

void irLearnStep() {
  scr = SCR_IR;
  header("IR LEARN");
  int y = LOG_START_Y;
  logLine(y, "Point remote at RX", ST77XX_GREEN);
  logLine(y, "Waiting...", ST77XX_GREEN);

  uint32_t t0 = millis();
  while (millis() - t0 < 8000) { // 8s okno uczenia
    if (IrReceiver.decode()) {
      auto &d = IrReceiver.decodedIRData;

      lastIr.valid    = true;
      lastIr.protocol = d.protocol;
      lastIr.address  = d.address;
      lastIr.command  = d.command;

      Serial.print("IR learned: ");
      Serial.print(protoName(lastIr.protocol));
      Serial.print(" addr=0x"); Serial.print(lastIr.address, HEX);
      Serial.print(" cmd=0x");  Serial.println(lastIr.command, HEX);

      header("IR LEARN");
      y = LOG_START_Y;
      logLine(y, "Learned OK", ST77XX_GREEN);
      logLine(y, "Proto: " + protoName(lastIr.protocol));
      logLine(y, "Addr : 0x" + String(lastIr.address, HEX));
      logLine(y, "Cmd  : 0x" + String(lastIr.command, HEX));

      IrReceiver.resume();
      delay(800);
      return;
    }
    delay(5);
  }

  header("IR LEARN");
  y = LOG_START_Y;
  logLine(y, "Timeout (no signal)", ST77XX_RED);
}

void irSendLast() {
  scr = SCR_IR;
  header("IR SEND");
  int y = LOG_START_Y;

  if (!lastIr.valid) {
    logLine(y, "No learned code!", ST77XX_RED);
    return;
  }

  logLine(y, "Sending...", ST77XX_GREEN);
  logLine(y, "Proto: " + protoName(lastIr.protocol));
  logLine(y, "Addr : 0x" + String(lastIr.address, HEX));
  logLine(y, "Cmd  : 0x" + String(lastIr.command, HEX));

  bool sent = false;

  // Najczęstsze:
  if (lastIr.protocol == NEC) {
    IrSender.sendNEC(lastIr.address, lastIr.command, 0);
    sent = true;
  } else if (lastIr.protocol == SONY) {
    IrSender.sendSony(lastIr.command, 12, 2); // bywa różnie; traktuj jako test
    sent = true;
  } else if (lastIr.protocol == RC5) {
    IrSender.sendRC5(lastIr.address, lastIr.command, 0);
    sent = true;
  } else if (lastIr.protocol == RC6) {
    IrSender.sendRC6(lastIr.address, lastIr.command, 0);
    sent = true;
  }

  if (sent) {
    logLine(y, "IR TX OK", ST77XX_GREEN);
    Serial.println("IR TX OK");
  } else {
    logLine(y, "Unsupported proto", ST77XX_YELLOW);
    Serial.println("IR TX unsupported protocol");
  }
}

// ========== setup/loop ==========
void setup() {
  Serial.begin(115200);
  delay(200);

  // Buttons
  pinMode(BTN_WIFI,    INPUT_PULLUP);
  pinMode(BTN_RX,      INPUT_PULLUP);
  pinMode(BTN_TX,      INPUT_PULLUP);
  pinMode(BTN_IRTX,    INPUT_PULLUP);
  pinMode(BTN_IRLEARN, INPUT_PULLUP);

  // SPI
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // TFT
  tft.init(240, 280);
  tft.setRotation(1);

  // IR
  IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_TX_PIN, ENABLE_LED_FEEDBACK);

  showHome();
  Serial.println("BOOT OK");
}

void loop() {
  // Buttons -> actions
  if (pressedEdge(BTN_WIFI))    doWifiScan();
  if (pressedEdge(BTN_RX))      ccRxStartStop();
  if (pressedEdge(BTN_TX))      ccTxTest();
  if (pressedEdge(BTN_IRLEARN)) irLearnStep();
  if (pressedEdge(BTN_IRTX))    irSendLast();

  // CC1101 RX handler
  if (ccRxOn && ccRxFlag) {
    ccRxFlag = false;
    String str;

    scr = SCR_CC_RX;
    header("CC1101 RX");
    int y = LOG_START_Y;

    int state = radio.readData(str);
    if (state == RADIOLIB_ERR_NONE) {
      logLine(y, "RX: " + str, ST77XX_GREEN);
      logLine(y, "RSSI: " + String(radio.getRSSI(), 1));
      logLine(y, "LQI : " + String(radio.getLQI(), 0));
      Serial.println("CC1101 RX: " + str);
    } else {
      logLine(y, "read ERR=" + String(state), ST77XX_RED);
      Serial.printf("CC1101 read err=%d\n", state);
    }
    radio.startReceive();
  }

  // IR live decode (opcjonalny podgląd bez uczenia)
  if (IrReceiver.decode()) {
    auto &d = IrReceiver.decodedIRData;
    Serial.print("IR: ");
    Serial.print(protoName(d.protocol));
    Serial.print(" addr=0x"); Serial.print(d.address, HEX);
    Serial.print(" cmd=0x");  Serial.println(d.command, HEX);
    IrReceiver.resume();
  }

  delay(10);
}
