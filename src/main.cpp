/*
 * ============================================================
 *  ESP32 All-in-One Multi-Tool
 * ============================================================
 *  Display  : ILI9488 3.5" 480x320 TFT (SPI)
 *  Touch    : XPT2046
 *  IR       : GPIO 33 TX / GPIO 35 RX  (IRremoteESP8266)
 *  RF 433   : GPIO 25 TX / GPIO 26 RX  (RCSwitch)
 *  GPS      : RX2=GPIO 16 / TX2=GPIO 17 (TinyGPS++)
 *  Backlight: GPIO 32 (PWM)
 *
 *  SPI (shared TFT + Touch):
 *    CLK = GPIO 18  MOSI = GPIO 23  MISO = GPIO 19
 *
 *  Libraries (platformio.ini):
 *    adafruit/Adafruit GFX Library
 *    adafruit/Adafruit ILI9488
 *    paulstoffregen/XPT2046_Touchscreen
 *    crankyoldgit/IRremoteESP8266 >= 2.8.6
 *    sui77/rc-switch >= 2.6.4
 *    mikalhart/TinyGPSPlus >= 1.0.3
 *    ESP32 built-in: BLE + WiFi
 * ============================================================
 */

#include <Arduino.h>
#include <SPI.h>

// ── Display + Touch ───────────────────────────────────────────
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9488.h>
#include <XPT2046_Touchscreen.h>

// ── IR (IRremoteESP8266) ─────────────────────────────────────
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <IRac.h>

// ── 433 MHz RF ───────────────────────────────────────────────
#include <RCSwitch.h>

// ── BLE ──────────────────────────────────────────────────────
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

// ── WiFi ─────────────────────────────────────────────────────
#include <WiFi.h>

// ── GPS ──────────────────────────────────────────────────────
#include <TinyGPS++.h>

// ============================================================
//  PIN DEFINITIONS
// ============================================================
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define TFT_BL    32

#define TOUCH_CS  14
#define TOUCH_IRQ 27

#define IR_TX_PIN 33
#define IR_RX_PIN 35

#define RF433_TX  25
#define RF433_RX  26

#define GPS_RX    16
#define GPS_TX    17

// ============================================================
//  DISPLAY CONSTANTS
// ============================================================
#define SCR_W  480
#define SCR_H  320

#define TS_MINX  300
#define TS_MAXX  3800
#define TS_MINY  300
#define TS_MAXY  3800

// ============================================================
//  COLORS
// ============================================================
#define C_BG     0x0000u
#define C_HDR    0x0318u
#define C_BTN    0x2104u
#define C_OK     0x07E0u
#define C_ERR    0xF800u
#define C_WARN   0xFD20u
#define C_WHITE  0xFFFFu
#define C_GRAY   0x8410u
#define C_CYAN   0x07FFu
#define C_YELLOW 0xFFE0u
#define C_PURPLE 0xF81Fu
#define C_LTBLUE 0x7BEFu
#define C_ORANGE 0xFC00u
#define C_TEAL   0x0438u

// Tile colors
#define TC_IR    0x000Bu
#define TC_AC    0x0808u
#define TC_RF    0x6000u
#define TC_BLE   0x000Fu
#define TC_BCN   0x4008u
#define TC_WIFI  0x0600u
#define TC_GPS   0x2808u
#define TC_SET   0x4208u

// ============================================================
//  OBJECTS
// ============================================================
Adafruit_ILI9488    tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
RCSwitch            rc;
IRrecv              irRecv(IR_RX_PIN, 1024, 15, true);
IRsend              irSend(IR_TX_PIN);
TinyGPSPlus         gps;
HardwareSerial      gpsSerial(2);

// ============================================================
//  APPLICATION STATE
// ============================================================
enum State {
    S_MAIN,
    S_IR,
    S_IR_CAPTURE,
    S_IR_AC,
    S_IR_AC_MANUAL,
    S_RF,
    S_RF_CAPTURE,
    S_BLE,
    S_BLE_SCAN,
    S_BLE_ADV,
    S_WIFI_SCAN,
    S_GPS,
    S_SETTINGS
};
State state = S_MAIN;

// ---- Captured IR signal ----
static uint16_t ir_raw[1024];
static uint16_t ir_raw_len = 0;
static bool     ir_captured = false;
static decode_results ir_results;

// ---- Captured RF signal ----
static unsigned long rf_value    = 0;
static unsigned int  rf_bits     = 0;
static unsigned int  rf_protocol = 0;
static bool          rf_captured = false;
static int16_t       rf_logY     = 95;
static int           rf_count    = 0;

// ---- AC settings ----
static const char* AC_PROTO_NAMES[] = {
    "Daikin", "Mitsubishi", "LG", "Samsung AC", "Panasonic AC"
};
static const decode_type_t AC_PROTOS[] = {
    DAIKIN, MITSUBISHI_AC, LG, SAMSUNG_AC, PANASONIC_AC
};
static int ac_proto_idx = 0;

static const char* AC_MODE_NAMES[] = { "Chlodzenie", "Ogrzewanie", "Auto", "Wentylator", "Osuszanie" };
static const stdAc::opmode_t AC_MODES[] = {
    stdAc::opmode_t::kCool,
    stdAc::opmode_t::kHeat,
    stdAc::opmode_t::kAuto,
    stdAc::opmode_t::kFan,
    stdAc::opmode_t::kDry
};
static int ac_mode_idx = 0;

static const char* AC_FAN_NAMES[] = { "Auto", "Niski", "Sredni", "Wysoki", "Maks" };
static const stdAc::fanspeed_t AC_FANS[] = {
    stdAc::fanspeed_t::kAuto,
    stdAc::fanspeed_t::kLow,
    stdAc::fanspeed_t::kMedium,
    stdAc::fanspeed_t::kHigh,
    stdAc::fanspeed_t::kMax
};
static int ac_fan_idx = 0;
static float ac_temp    = 22.0f;
static bool  ac_power   = true;

// ---- BLE ----
struct BLEDev {
    char addr[24];
    char name[40];
    int  rssi;
};
static BLEDev  ble_devices[20];
static int     ble_dev_count = 0;
static int16_t ble_logY      = 50;
static bool    ble_adv_on    = false;
static bool    ble_scan_done = false;

BLEScan*      pBLEScan = nullptr;
BLEAdvertising* pBLEAdv = nullptr;

// ---- GPS ----
static unsigned long gps_lastPrint = 0;

// ============================================================
//  BUTTON / TILE HELPERS
// ============================================================
struct Btn {
    int16_t     x, y, w, h;
    const char* label;
    uint16_t    color;
};

static bool inBtn(const Btn& b, int x, int y) {
    return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

static void drawBtn(const Btn& b) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, b.color);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, C_GRAY);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    int16_t cx = b.x + b.w / 2 - (int16_t)(strlen(b.label) * 6);
    int16_t cy = b.y + b.h / 2 - 8;
    tft.setCursor(cx, cy);
    tft.print(b.label);
}

static void drawSmallBtn(const Btn& b) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 4, b.color);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 4, C_GRAY);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    int16_t cx = b.x + b.w / 2 - (int16_t)(strlen(b.label) * 3);
    int16_t cy = b.y + b.h / 2 - 4;
    tft.setCursor(cx, cy);
    tft.print(b.label);
}

// ============================================================
//  COMMON UI ELEMENTS
// ============================================================
static void drawHdr(const char* title) {
    tft.fillRect(0, 0, SCR_W, 36, C_HDR);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print(title);
}

static void statusBar(const char* msg, uint16_t color = C_GRAY) {
    tft.fillRect(0, SCR_H - 22, SCR_W, 22, color);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(4, SCR_H - 14);
    tft.print(msg);
}

static void clearBody() {
    tft.fillRect(0, 36, SCR_W, SCR_H - 58, C_BG);
}

static const Btn BTN_BACK = {SCR_W - 92, SCR_H - 56, 82, 34, "< Wstecz", C_BTN};
static void drawBack() { drawBtn(BTN_BACK); }
static bool isBack(int x, int y) { return inBtn(BTN_BACK, x, y); }

// ============================================================
//  MAIN MENU (8 tiles in 4×2 grid)
// ============================================================
#define TILE_W   114
#define TILE_H    90
#define TILE_PAD   4
#define TILE_X0    4
#define TILE_Y0   40

struct Tile {
    int16_t  x, y, w, h;
    uint16_t color;
    const char* line1;
    const char* line2;
};

static const Tile TILES[] = {
    { TILE_X0,                            TILE_Y0,            TILE_W, TILE_H, TC_IR,   "IR",         "Pilot"    },
    { TILE_X0 + (TILE_W+TILE_PAD),        TILE_Y0,            TILE_W, TILE_H, TC_AC,   "IR",         "Klima"    },
    { TILE_X0 + 2*(TILE_W+TILE_PAD),      TILE_Y0,            TILE_W, TILE_H, TC_RF,   "RF",         "433 MHz"  },
    { TILE_X0 + 3*(TILE_W+TILE_PAD),      TILE_Y0,            TILE_W, TILE_H, TC_BLE,  "BLE",        "Skan"     },
    { TILE_X0,                            TILE_Y0+TILE_H+TILE_PAD, TILE_W, TILE_H, TC_BCN,  "BLE",  "Beacon"   },
    { TILE_X0 + (TILE_W+TILE_PAD),        TILE_Y0+TILE_H+TILE_PAD, TILE_W, TILE_H, TC_WIFI, "WiFi", "Skan"     },
    { TILE_X0 + 2*(TILE_W+TILE_PAD),      TILE_Y0+TILE_H+TILE_PAD, TILE_W, TILE_H, TC_GPS,  "GPS",  "Lokacja"  },
    { TILE_X0 + 3*(TILE_W+TILE_PAD),      TILE_Y0+TILE_H+TILE_PAD, TILE_W, TILE_H, TC_SET,  "Ustaw","ienia"   },
};
static const int TILE_COUNT = (int)(sizeof(TILES) / sizeof(TILES[0]));

static void drawTile(const Tile& t) {
    tft.fillRoundRect(t.x, t.y, t.w, t.h, 8, t.color);
    tft.drawRoundRect(t.x, t.y, t.w, t.h, 8, C_GRAY);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    int16_t cx = t.x + t.w / 2;
    tft.setCursor(cx - (int16_t)(strlen(t.line1) * 6), t.y + 20);
    tft.print(t.line1);
    if (t.line2[0]) {
        tft.setTextSize(1);
        tft.setCursor(cx - (int16_t)(strlen(t.line2) * 3), t.y + 52);
        tft.print(t.line2);
    }
}

static void drawMainMenu() {
    state = S_MAIN;
    tft.fillScreen(C_BG);
    drawHdr("  ESP32 Multi-Tool v2.0");
    for (int i = 0; i < TILE_COUNT; i++) drawTile(TILES[i]);
    statusBar("Dotknij kafelek aby wybrac funkcje", C_HDR);
}

static bool inTile(const Tile& t, int x, int y) {
    return x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h;
}

// ============================================================
//  IR SUBMENU
// ============================================================
static const Btn IR_BTNS[] = {
    {10,  46, 460, 60, "Przechwytuj sygnal IR",   TC_IR},
    {10, 116, 460, 60, "Nadawaj sygnal IR",        TC_IR},
};

static void drawIRMenu() {
    state = S_IR;
    tft.fillScreen(C_BG);
    drawHdr("IR Pilot");
    for (auto& b : IR_BTNS) drawBtn(b);
    drawBack();
    statusBar(ir_captured ? "Sygnal IR przechwycony  [gotowy]"
                          : "Brak sygnalu -- przechwytuj najpierw",
              ir_captured ? C_OK : C_GRAY);
}

// ============================================================
//  IR CAPTURE SCREEN
// ============================================================
static void drawIRCapture() {
    state = S_IR_CAPTURE;
    tft.fillScreen(C_BG);
    drawHdr("IR - Przechwytywanie");
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(8, 42);
    tft.print("Skieruj pilota na odbiornik IR i nacisnij przycisk.");
    tft.drawFastHLine(0, 58, SCR_W, C_GRAY);
    drawBack();
    statusBar("Czekanie na sygnal IR...", C_WARN);
    irRecv.enableIRIn();
}

static void taskIRCapture() {
    if (!irRecv.decode(&ir_results)) return;

    ir_raw_len = (ir_results.rawlen > 1024) ? 1024 : (uint16_t)ir_results.rawlen;
    for (uint16_t i = 0; i < ir_raw_len; i++) {
        ir_raw[i] = (uint16_t)(ir_results.rawbuf[i] * kRawTick);
    }
    ir_captured = true;

    clearBody();
    tft.setTextColor(C_OK);
    tft.setTextSize(2);
    tft.setCursor(8, 65);
    tft.print("Przechwycono!");

    char buf[64];
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);

    snprintf(buf, sizeof(buf), "Protokol : %s", typeToString(ir_results.decode_type).c_str());
    tft.setCursor(8, 92);  tft.print(buf);

    snprintf(buf, sizeof(buf), "Wartosc  : 0x%08llX", (unsigned long long)ir_results.value);
    tft.setCursor(8, 108); tft.print(buf);

    snprintf(buf, sizeof(buf), "Bity     : %d     Dlug.: %d impulsow",
             ir_results.bits, ir_results.rawlen);
    tft.setCursor(8, 124); tft.print(buf);

    statusBar("Przechwycono! Mozesz teraz nadawac.", C_OK);
    irRecv.resume();
}

// ============================================================
//  IR REPLAY
// ============================================================
static void doIRReplay() {
    if (!ir_captured) {
        statusBar("Brak sygnalu IR do nadania!", C_ERR);
        return;
    }
    irRecv.disableIRIn();
    irSend.sendRaw(ir_raw, ir_raw_len, 38);
    irRecv.enableIRIn();
    statusBar("Sygnal IR wyslany!", C_OK);
}

// ============================================================
//  AC (Klimatyzacja) SUBMENU
// ============================================================
static const Btn AC_BTNS[] = {
    {10,  46, 460, 60, "Przechwytuj sygnal AC",  TC_AC},
    {10, 116, 460, 60, "Powiel sygnal AC",        TC_AC},
    {10, 186, 460, 60, "Reczna kontrola AC",      0x0815},
};

static void drawACMenu() {
    state = S_IR_AC;
    tft.fillScreen(C_BG);
    drawHdr("Klimatyzacja (AC)");
    for (auto& b : AC_BTNS) drawBtn(b);
    drawBack();
    statusBar(ir_captured ? "Sygnal AC przechwycony [gotowy]"
                          : "Brak sygnalu AC -- przechwytuj najpierw",
              ir_captured ? C_OK : C_GRAY);
}

// ============================================================
//  AC MANUAL CONTROL SCREEN
// ============================================================
// Buttons defined as indices for handleACManual:
// 0=proto-, 1=proto+, 2=mode-, 3=mode+, 4=temp-, 5=temp+,
// 6=fan-, 7=fan+, 8=power, 9=send
static const Btn AC_CTRL_BTNS[10] = {
    {  8, 168,  72, 32, "< Proto", C_BTN},
    { 88, 168,  72, 32, "Proto >", C_BTN},
    {176, 168,  72, 32, "< Tryb",  C_BTN},
    {256, 168,  72, 32, "Tryb >",  C_BTN},
    {344, 168,  60, 32, "- Temp",  C_BTN},
    {412, 168,  60, 32, "+ Temp",  C_BTN},
    {  8, 210,  72, 32, "< Fan",   C_BTN},
    { 88, 210,  72, 32, "Fan >",   C_BTN},
    {176, 210, 120, 32, "Wl./Wyl.",C_BTN},
    {304, 210, 168, 32, "WYSLIJ",  C_OK},
};

static void drawACManualValues() {
    tft.fillRect(0, 46, SCR_W, 118, C_BG);

    struct Row { int y; const char* label; const char* value; uint16_t col; };
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f C", ac_temp);
    Row rows[] = {
        { 52, "Protokol  :", AC_PROTO_NAMES[ac_proto_idx], C_CYAN},
        { 76, "Tryb      :", AC_MODE_NAMES[ac_mode_idx],   C_YELLOW},
        {100, "Temperatura:", buf,                          C_ORANGE},
        {124, "Wentylator :", AC_FAN_NAMES[ac_fan_idx],    C_LTBLUE},
        {148, "Zasilanie  :", ac_power ? "WLACZONE" : "WYLACZONE",
              ac_power ? C_OK : C_ERR},
    };
    for (auto& r : rows) {
        tft.setTextColor(C_GRAY);
        tft.setTextSize(1);
        tft.setCursor(8, r.y);
        tft.print(r.label);
        tft.setTextColor(r.col);
        tft.setCursor(160, r.y);
        tft.print(r.value);
    }
}

static void drawACManual() {
    state = S_IR_AC_MANUAL;
    tft.fillScreen(C_BG);
    drawHdr("Reczna kontrola AC");
    drawACManualValues();
    for (auto& b : AC_CTRL_BTNS) drawSmallBtn(b);
    drawBack();
    statusBar("Ustaw parametry i nacisnij WYSLIJ", C_TEAL);
}

static void handleACManual(int tx, int ty) {
    if (isBack(tx, ty)) { drawACMenu(); return; }

    bool redraw = true;
    if      (inBtn(AC_CTRL_BTNS[0], tx, ty)) { if (ac_proto_idx > 0) ac_proto_idx--; }
    else if (inBtn(AC_CTRL_BTNS[1], tx, ty)) { if (ac_proto_idx < 4) ac_proto_idx++; }
    else if (inBtn(AC_CTRL_BTNS[2], tx, ty)) { if (ac_mode_idx  > 0) ac_mode_idx--;  }
    else if (inBtn(AC_CTRL_BTNS[3], tx, ty)) { if (ac_mode_idx  < 4) ac_mode_idx++;  }
    else if (inBtn(AC_CTRL_BTNS[4], tx, ty)) { if (ac_temp > 16.0f)  ac_temp -= 1.0f; }
    else if (inBtn(AC_CTRL_BTNS[5], tx, ty)) { if (ac_temp < 30.0f)  ac_temp += 1.0f; }
    else if (inBtn(AC_CTRL_BTNS[6], tx, ty)) { if (ac_fan_idx  > 0) ac_fan_idx--;  }
    else if (inBtn(AC_CTRL_BTNS[7], tx, ty)) { if (ac_fan_idx  < 4) ac_fan_idx++;  }
    else if (inBtn(AC_CTRL_BTNS[8], tx, ty)) { ac_power = !ac_power; }
    else if (inBtn(AC_CTRL_BTNS[9], tx, ty)) {
        // Send AC command
        IRac acCtrl(IR_TX_PIN);
        acCtrl.next.protocol = AC_PROTOS[ac_proto_idx];
        acCtrl.next.mode     = AC_MODES[ac_mode_idx];
        acCtrl.next.degrees  = ac_temp;
        acCtrl.next.fanspeed = AC_FANS[ac_fan_idx];
        acCtrl.next.power    = ac_power;
        acCtrl.sendAc();
        statusBar("Komenda AC wyslana!", C_OK);
        redraw = false;
    } else {
        redraw = false;
    }
    if (redraw) drawACManualValues();
}

// ============================================================
//  RF 433 MHz SUBMENU
// ============================================================
static const Btn RF_BTNS[] = {
    {10,  46, 460, 60, "Przechwytuj sygnal 433 MHz",  TC_RF},
    {10, 116, 460, 60, "Nadawaj sygnal 433 MHz",       TC_RF},
    {10, 186, 460, 60, "Monitor 433 MHz",              C_BTN},
};

static void drawRFMenu() {
    state = S_RF;
    tft.fillScreen(C_BG);
    drawHdr("RF 433 MHz");
    for (auto& b : RF_BTNS) drawBtn(b);
    drawBack();
    if (rf_captured) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Przechwycony: 0x%lX  Bity:%u  Proto:%u",
                 rf_value, rf_bits, rf_protocol);
        statusBar(buf, C_OK);
    } else {
        statusBar("Brak sygnalu RF -- przechwytuj najpierw", C_GRAY);
    }
}

// ============================================================
//  RF CAPTURE SCREEN
// ============================================================
static void drawRFCapture() {
    state = S_RF_CAPTURE;
    tft.fillScreen(C_BG);
    drawHdr("RF 433 MHz - Przechwytywanie");
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(8, 42);
    tft.print("Nacisnij przycisk na pilocie 433 MHz...");
    tft.drawFastHLine(0, 58, SCR_W, C_GRAY);
    rf_logY  = 68;
    rf_count = 0;
    rc.enableReceive(RF433_RX);
    drawBack();
    statusBar("Nasłuchiwanie na 433 MHz...", C_WARN);
}

static void taskRFCapture() {
    if (!rc.available()) return;

    rf_value    = rc.getReceivedValue();
    rf_bits     = rc.getReceivedBitlength();
    rf_protocol = rc.getReceivedProtocol();
    rc.resetAvailable();
    rf_captured = true;
    rf_count++;

    if (rf_logY > 250) { tft.fillRect(0, 58, SCR_W, 200, C_BG); rf_logY = 68; }

    char buf[72];
    snprintf(buf, sizeof(buf), "[%d] 0x%08lX  Bity:%u  Proto:%u",
             rf_count, rf_value, rf_bits, rf_protocol);
    tft.setTextColor((rf_count & 1) ? C_CYAN : C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, rf_logY);
    tft.print(buf);
    rf_logY += 14;

    char st[56];
    snprintf(st, sizeof(st), "Przechwycono %d sygnalow | Ostatni: 0x%lX", rf_count, rf_value);
    statusBar(st, C_OK);
}

static void doRFReplay() {
    if (!rf_captured) {
        statusBar("Brak sygnalu RF do nadania!", C_ERR);
        return;
    }
    rc.disableReceive();
    rc.enableTransmit(RF433_TX);
    rc.setProtocol(rf_protocol);
    rc.send(rf_value, rf_bits);
    rc.disableTransmit();
    rc.enableReceive(RF433_RX);
    statusBar("Sygnal 433 MHz wyslany!", C_OK);
}

// ============================================================
//  BLE SUBMENU
// ============================================================
static const Btn BLE_BTNS[] = {
    {10, 46, 460, 60, "Skanuj urzadzenia BLE",    TC_BLE},
    {10, 116, 460, 60, "Nadawaj jako BLE Beacon",  TC_BCN},
};

static void drawBLEMenu() {
    state = S_BLE;
    tft.fillScreen(C_BG);
    drawHdr("BLE Bluetooth");
    for (auto& b : BLE_BTNS) drawBtn(b);
    drawBack();
    statusBar("Wybierz tryb BLE", C_TEAL);
}

// ============================================================
//  BLE SCAN SCREEN
// ============================================================
class BLECallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        if (ble_dev_count >= 20) return;
        BLEDev& d = ble_devices[ble_dev_count++];
        strncpy(d.addr, dev.getAddress().toString().c_str(), sizeof(d.addr) - 1);
        d.addr[sizeof(d.addr) - 1] = '\0';
        strncpy(d.name, dev.haveName() ? dev.getName().c_str() : "???", sizeof(d.name) - 1);
        d.name[sizeof(d.name) - 1] = '\0';
        d.rssi = dev.getRSSI();

        // Draw on screen immediately
        if (ble_logY > 265) {
            tft.fillRect(0, 50, SCR_W, 218, C_BG);
            ble_logY = 50;
        }
        char buf[72];
        snprintf(buf, sizeof(buf), "%-17s  %3d dBm  %s", d.addr, d.rssi, d.name);
        tft.setTextColor((ble_dev_count & 1) ? C_CYAN : C_LTBLUE);
        tft.setTextSize(1);
        tft.setCursor(4, ble_logY);
        tft.print(buf);
        ble_logY += 13;

        char st[48];
        snprintf(st, sizeof(st), "Znalezione urzadzenia: %d", ble_dev_count);
        tft.fillRect(0, SCR_H - 22, SCR_W, 22, C_TEAL);
        tft.setTextColor(C_WHITE);
        tft.setTextSize(1);
        tft.setCursor(4, SCR_H - 14);
        tft.print(st);
    }
};

static BLECallback bleCb;

static void drawBLEScan() {
    state = S_BLE_SCAN;
    ble_dev_count = 0;
    ble_logY      = 50;
    ble_scan_done = false;
    tft.fillScreen(C_BG);
    drawHdr("BLE - Skanowanie reklam");
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(4, 38);
    tft.print("Szukanie urzadzen BLE w poblizu...");
    drawBack();
    statusBar("Skanowanie BLE... (10 sek.)", C_TEAL);
    pBLEScan->clearResults();
    pBLEScan->start(10, false);
    ble_scan_done = true;
}

// ============================================================
//  BLE BEACON ADVERTISE SCREEN
// ============================================================
static void drawBLEAdv() {
    state = S_BLE_ADV;
    tft.fillScreen(C_BG);
    drawHdr("BLE Beacon - Nadawanie");

    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(8, 44);
    tft.print("Nazwa    : ESP32-MultiTool");
    tft.setCursor(8, 60);
    tft.print("UUID     : 12345678-ESP32-TOOL");
    tft.setCursor(8, 76);
    tft.print("Moc nadaw: 0 dBm");
    tft.setCursor(8, 92);
    tft.print("Interval : 100 ms");

    static const Btn BTN_TOGGLE = {80, 130, 320, 60, "WLACZ / WYLACZ", C_OK};
    drawBtn(BTN_TOGGLE);

    tft.setTextColor(ble_adv_on ? C_OK : C_ERR);
    tft.setTextSize(2);
    tft.setCursor(160, 210);
    tft.print(ble_adv_on ? "NADAJE..." : "  STOP   ");

    drawBack();

    if (ble_adv_on) {
        statusBar("BLE Beacon aktywny - telefony widza 'ESP32-MultiTool'", C_OK);
    } else {
        statusBar("Nacisnij przycisk aby wlacz/wylaczyc nadawanie", C_GRAY);
    }
}

static void handleBLEAdv(int tx, int ty) {
    if (isBack(tx, ty)) {
        if (ble_adv_on && pBLEAdv) { pBLEAdv->stop(); ble_adv_on = false; }
        drawBLEMenu();
        return;
    }
    static const Btn BTN_TOGGLE = {80, 130, 320, 60, "WLACZ / WYLACZ", C_OK};
    if (inBtn(BTN_TOGGLE, tx, ty)) {
        if (!ble_adv_on) {
            pBLEAdv->start();
            ble_adv_on = true;
        } else {
            pBLEAdv->stop();
            ble_adv_on = false;
        }
        // Redraw status indicator
        tft.fillRect(0, 200, SCR_W, 30, C_BG);
        tft.setTextColor(ble_adv_on ? C_OK : C_ERR);
        tft.setTextSize(2);
        tft.setCursor(160, 210);
        tft.print(ble_adv_on ? "NADAJE..." : "  STOP   ");
        if (ble_adv_on) {
            statusBar("BLE Beacon aktywny - telefony widza 'ESP32-MultiTool'", C_OK);
        } else {
            statusBar("Nacisnij przycisk aby wlacz/wylaczyc nadawanie", C_GRAY);
        }
    }
}

// ============================================================
//  WIFI SCAN SCREEN
// ============================================================
static void drawWiFiScan() {
    state = S_WIFI_SCAN;
    tft.fillScreen(C_BG);
    drawHdr("WiFi - Skanowanie sieci");
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(4, 38);
    tft.print("Skanowanie...");
    drawBack();
    statusBar("Skanowanie WiFi...", C_WARN);

    int n = WiFi.scanNetworks();
    tft.fillRect(0, 36, SCR_W, SCR_H - 58, C_BG);

    if (n <= 0) {
        tft.setTextColor(C_ERR);
        tft.setTextSize(1);
        tft.setCursor(8, 48);
        tft.print("Nie znaleziono sieci WiFi.");
        statusBar("Brak sieci WiFi", C_ERR);
        return;
    }

    int16_t y = 40;
    tft.setTextSize(1);
    for (int i = 0; i < n && y < SCR_H - 60; i++) {
        char buf[72];
        snprintf(buf, sizeof(buf), "%-28s  %4d dBm  Ch:%2d  %s",
                 WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                 (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Lock");
        uint16_t col = (WiFi.RSSI(i) > -60) ? C_OK :
                       (WiFi.RSSI(i) > -75) ? C_YELLOW : C_ERR;
        tft.setTextColor(col);
        tft.setCursor(4, y);
        tft.print(buf);
        y += 13;
    }

    char st[48];
    snprintf(st, sizeof(st), "Znaleziono %d sieci WiFi", n);
    statusBar(st, C_OK);
    WiFi.scanDelete();
}

// ============================================================
//  GPS SCREEN
// ============================================================
static void drawGPS() {
    state = S_GPS;
    tft.fillScreen(C_BG);
    drawHdr("GPS - Lokacja");
    tft.setTextColor(C_GRAY);
    tft.setTextSize(1);
    tft.setCursor(8, 40);
    tft.print("Czekanie na dane GPS...");
    drawBack();
    statusBar("Polaczenie z satelitami GPS...", C_WARN);
    gps_lastPrint = 0;
}

static void taskGPS() {
    while (gpsSerial.available()) gps.encode(gpsSerial.read());

    if (millis() - gps_lastPrint < 2000) return;
    gps_lastPrint = millis();

    tft.fillRect(0, 36, SCR_W, SCR_H - 78, C_BG);
    tft.setTextSize(1);

    if (gps.location.isValid()) {
        char buf[64];
        tft.setTextColor(C_OK);
        snprintf(buf, sizeof(buf), "Szerokosc : %.6f", gps.location.lat());
        tft.setCursor(8, 44); tft.print(buf);
        snprintf(buf, sizeof(buf), "Dlugosc   : %.6f", gps.location.lng());
        tft.setCursor(8, 60); tft.print(buf);
        snprintf(buf, sizeof(buf), "Wysokosc  : %.1f m", gps.altitude.meters());
        tft.setCursor(8, 76); tft.print(buf);
        snprintf(buf, sizeof(buf), "Satelity  : %u", gps.satellites.value());
        tft.setCursor(8, 92); tft.print(buf);
        snprintf(buf, sizeof(buf), "Predkosc  : %.1f km/h", gps.speed.kmph());
        tft.setCursor(8, 108); tft.print(buf);

        if (gps.date.isValid() && gps.time.isValid()) {
            tft.setTextColor(C_YELLOW);
            snprintf(buf, sizeof(buf), "Data : %04d-%02d-%02d",
                     gps.date.year(), gps.date.month(), gps.date.day());
            tft.setCursor(8, 128); tft.print(buf);
            snprintf(buf, sizeof(buf), "Czas : %02d:%02d:%02d UTC",
                     gps.time.hour(), gps.time.minute(), gps.time.second());
            tft.setCursor(8, 144); tft.print(buf);
        }
        statusBar("GPS: sygnał OK", C_OK);
    } else {
        tft.setTextColor(C_WARN);
        char buf[64];
        snprintf(buf, sizeof(buf), "Brak fixa GPS... Znaki: %u  Fix: %u  Bledy: %u",
                 gps.charsProcessed(), gps.sentencesWithFix(), gps.failedChecksum());
        tft.setCursor(8, 44); tft.print(buf);
        statusBar("Czekanie na satelity GPS...", C_WARN);
    }
}

// ============================================================
//  SETTINGS SCREEN
// ============================================================
static void drawSettings() {
    state = S_SETTINGS;
    tft.fillScreen(C_BG);
    drawHdr("Ustawienia i Piny");

    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(8, 38);
    tft.print("Przypisanie pinow:");

    const char* lines[] = {
        "TFT  CS=15  DC=2   RST=4   BL=32(PWM)",
        "Touch CS=14  IRQ=27",
        "SPI  CLK=18  MOSI=23  MISO=19",
        "IR   TX=33   RX=35   (IRremoteESP8266)",
        "RF   TX=25   RX=26   (RCSwitch 433 MHz)",
        "GPS  RX2=16  TX2=17  (TinyGPS++)",
        "",
        "Biblioteki:",
        "  Adafruit GFX + ILI9488",
        "  XPT2046_Touchscreen",
        "  crankyoldgit/IRremoteESP8266 v2.8",
        "  sui77/rc-switch v2.6",
        "  mikalhart/TinyGPSPlus",
        "  ESP32 BLE + WiFi (wbudowane)",
        "",
        "MCU: ESP32 240 MHz  Wyswietlacz: ILI9488 3.5\"",
    };
    int y = 54;
    tft.setTextColor(C_WHITE);
    for (auto& l : lines) {
        tft.setCursor(8, y);
        tft.print(l);
        y += 14;
    }
    drawBack();
}

// ============================================================
//  TOUCH DISPATCH
// ============================================================
static void handleTouch(int tx, int ty) {
    switch (state) {
        // ---- Main menu ----
        case S_MAIN:
            if      (inTile(TILES[0], tx, ty)) drawIRMenu();
            else if (inTile(TILES[1], tx, ty)) drawACMenu();
            else if (inTile(TILES[2], tx, ty)) drawRFMenu();
            else if (inTile(TILES[3], tx, ty)) drawBLEScan();
            else if (inTile(TILES[4], tx, ty)) drawBLEAdv();
            else if (inTile(TILES[5], tx, ty)) drawWiFiScan();
            else if (inTile(TILES[6], tx, ty)) drawGPS();
            else if (inTile(TILES[7], tx, ty)) drawSettings();
            break;

        // ---- IR submenu ----
        case S_IR:
            if      (isBack(tx, ty))             drawMainMenu();
            else if (inBtn(IR_BTNS[0], tx, ty))  drawIRCapture();
            else if (inBtn(IR_BTNS[1], tx, ty))  { doIRReplay(); }
            break;

        // ---- IR Capture ----
        case S_IR_CAPTURE:
            if (isBack(tx, ty)) { irRecv.disableIRIn(); drawIRMenu(); }
            break;

        // ---- AC submenu ----
        case S_IR_AC:
            if      (isBack(tx, ty))              drawMainMenu();
            else if (inBtn(AC_BTNS[0], tx, ty))  drawIRCapture();
            else if (inBtn(AC_BTNS[1], tx, ty))  { doIRReplay(); }
            else if (inBtn(AC_BTNS[2], tx, ty))  drawACManual();
            break;

        // ---- AC Manual Control ----
        case S_IR_AC_MANUAL:
            handleACManual(tx, ty);
            break;

        // ---- RF submenu ----
        case S_RF:
            if      (isBack(tx, ty))              drawMainMenu();
            else if (inBtn(RF_BTNS[0], tx, ty))  drawRFCapture();
            else if (inBtn(RF_BTNS[1], tx, ty))  { doRFReplay(); }
            else if (inBtn(RF_BTNS[2], tx, ty))  drawRFCapture();
            break;

        // ---- RF Capture ----
        case S_RF_CAPTURE:
            if (isBack(tx, ty)) { rc.disableReceive(); drawRFMenu(); }
            break;

        // ---- BLE submenu ----
        case S_BLE:
            if      (isBack(tx, ty))               drawMainMenu();
            else if (inBtn(BLE_BTNS[0], tx, ty))   drawBLEScan();
            else if (inBtn(BLE_BTNS[1], tx, ty))   drawBLEAdv();
            break;

        // ---- BLE Scan ----
        case S_BLE_SCAN:
            if (isBack(tx, ty)) { pBLEScan->stop(); drawBLEMenu(); }
            break;

        // ---- BLE Advertise ----
        case S_BLE_ADV:
            handleBLEAdv(tx, ty);
            break;

        // ---- WiFi Scan ----
        case S_WIFI_SCAN:
            if (isBack(tx, ty)) drawMainMenu();
            break;

        // ---- GPS ----
        case S_GPS:
            if (isBack(tx, ty)) drawMainMenu();
            break;

        // ---- Settings ----
        case S_SETTINGS:
            if (isBack(tx, ty)) drawMainMenu();
            break;

        default:
            drawMainMenu();
            break;
    }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);

    // ── Backlight ────────────────────────────────────────────
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 200);

    // ── Display init ─────────────────────────────────────────
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(C_BG);

    // ── Touch init ───────────────────────────────────────────
    ts.begin();
    ts.setRotation(1);

    // ── Boot splash ──────────────────────────────────────────
    tft.setTextColor(C_WHITE);
    tft.setTextSize(3);
    tft.setCursor(60, 80);
    tft.print("ESP32 Multi-Tool");
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY);
    tft.setCursor(120, 130);
    tft.print("IR + AC + RF 433 + BLE + WiFi + GPS");
    tft.setCursor(180, 148);
    tft.print("Inicjalizacja...");

    // ── RCSwitch ─────────────────────────────────────────────
    rc.enableTransmit(RF433_TX);

    // ── IRremoteESP8266 TX ───────────────────────────────────
    irSend.begin();

    // ── GPS UART ─────────────────────────────────────────────
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    // ── WiFi (scan mode only) ────────────────────────────────
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // ── BLE init ─────────────────────────────────────────────
    BLEDevice::init("ESP32-MultiTool");

    // BLE scan
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(&bleCb, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // BLE advertise
    pBLEAdv = BLEDevice::getAdvertising();
    BLEAdvertisementData advData;
    advData.setName("ESP32-MultiTool");
    advData.setFlags(0x06);  // LE General Discoverable
    pBLEAdv->setAdvertisementData(advData);
    BLEAdvertisementData scanResp;
    scanResp.setName("ESP32-MultiTool");
    pBLEAdv->setScanResponseData(scanResp);
    pBLEAdv->setMinInterval(160);
    pBLEAdv->setMaxInterval(160);

    delay(1000);

    // ── Main menu ────────────────────────────────────────────
    drawMainMenu();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ── Touch handling ───────────────────────────────────────
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint();
        int tx = map(p.x, TS_MINX, TS_MAXX, 0, SCR_W);
        int ty = map(p.y, TS_MINY, TS_MAXY, 0, SCR_H);
        handleTouch(tx, ty);
        delay(180);  // debounce
    }

    // ── Background tasks per state ───────────────────────────
    switch (state) {
        case S_IR_CAPTURE:
            taskIRCapture();
            break;
        case S_RF_CAPTURE:
            taskRFCapture();
            break;
        case S_GPS:
            taskGPS();
            break;
        default:
            break;
    }
}
