/*
 * ============================================================
 *  ESP32 RF/IR Multi-Tool
 * ============================================================
 *
 *  Display  : ILI9488 3.5" 480×320 TFT (SPI)
 *  Touch    : XPT2046
 *  IR TX    : GPIO 33
 *  IR RX    : GPIO 35
 *  433 TX   : GPIO 25
 *  433 RX   : GPIO 26
 *  Backlight: GPIO 32 (PWM)
 *
 *  SPI (shared by TFT + Touch):
 *    CLK  = GPIO 18
 *    MOSI = GPIO 23
 *    MISO = GPIO 19
 *
 *  Libraries (see platformio.ini):
 *    - Adafruit GFX Library
 *    - Adafruit ILI9488
 *    - XPT2046_Touchscreen  (Paul Stoffregen)
 *    - IRremote  >= 4.0     (shirriff / Arduino-IRremote)
 *    - rc-switch            (sui77)
 * ============================================================
 */

// ---- Enable all IR decoders ----
#define DECODE_NEC
#define DECODE_SONY
#define DECODE_RC5
#define DECODE_RC6
#define DECODE_PANASONIC
#define DECODE_JVC
#define DECODE_SAMSUNG
#define DECODE_DENON
#define DECODE_LG
#define DECODE_SHARP

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9488.h>
#include <XPT2046_Touchscreen.h>
#include <IRremote.hpp>
#include <RCSwitch.h>

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

// ============================================================
//  DISPLAY CONSTANTS
// ============================================================
#define SCR_W  480
#define SCR_H  320

// Touch calibration – tune these for your panel
#define TS_MINX  300
#define TS_MAXX  3800
#define TS_MINY  300
#define TS_MAXY  3800

// ============================================================
//  COLOR PALETTE
// ============================================================
#define C_BG     0x0000u   // Black
#define C_HDR    0x0318u   // Dark navy
#define C_BTN    0x2104u   // Dark grey-blue
#define C_BTN_A  0x051Du   // Highlight blue
#define C_OK     0x07E0u   // Green
#define C_ERR    0xF800u   // Red
#define C_WARN   0xFD20u   // Orange
#define C_WHITE  0xFFFFu
#define C_GRAY   0x8410u
#define C_CYAN   0x07FFu
#define C_YELLOW 0xFFE0u
#define C_PURPLE 0xF81Fu
#define C_LTBLUE 0x7BEFu

// ============================================================
//  OBJECTS
// ============================================================
Adafruit_ILI9488    tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
RCSwitch            rc;

// ============================================================
//  APPLICATION STATE
// ============================================================
enum State {
    S_MAIN,
    S_IR,
    S_IR_CAPTURE,
    S_RF,
    S_RF_SCAN,
    S_SCANNER,
    S_SCAN_IR,
    S_SCAN_RF,
    S_SETTINGS
};
State state = S_MAIN;

// ---- 433 MHz captured signal ----
unsigned long rf_value    = 0;
unsigned int  rf_bits     = 0;
unsigned int  rf_protocol = 0;
bool          rf_captured = false;

// ---- IR captured signal ----
uint8_t  ir_protocol = 0;
uint16_t ir_address  = 0;
uint16_t ir_command  = 0;
uint8_t  ir_bits     = 32;
bool     ir_captured = false;

// ---- Activity monitor scroll position ----
int16_t  ir_logY   = 95;
int      ir_count  = 0;
int16_t  rf_logY   = 95;
int      rf_count  = 0;

// ============================================================
//  TV POWER CODES  (address + command split for IRremote v4)
// ============================================================
struct TVCode {
    const char*   brand;
    decode_type_t proto;
    uint16_t      addr;
    uint16_t      cmd;
    uint8_t       bits;
};

static const TVCode TV_CODES[] = {
    {"Samsung",    NEC,     0xE0E0, 0x40, 32},
    {"LG",         NEC,     0x20DF, 0x10, 32},
    {"Sony 12b",   SONY,    0x01,   0x15, 12},
    {"Sony 15b",   SONY,    0x01,   0x15, 15},
    {"Sony 20b",   SONY,    0x001,  0x95, 20},
    {"Philips RC5",RC5,     0x00,   0x0C, 12},
    {"Sharp",      NEC,     0x00,   0xA0, 16},
    {"Toshiba",    NEC,     0x02FD, 0x48, 32},
    {"Hisense",    NEC,     0x4004, 0x01, 32},
    {"TCL",        NEC,     0x0000, 0xF7, 24},
    {"Haier",      NEC,     0x56AB, 0x90, 32},
    {"Hitachi",    NEC,     0x0CC3, 0x3D, 32},
    {"Vizio",      NEC,     0x0000, 0xE3, 16},
    {"Pioneer",    NEC,     0x0000, 0x9C,  8},
    {"JVC",        JVC,     0xC5,   0xE8, 16},
    {"Onkyo",      NEC,     0x0000, 0x04, 16},
};
static const int TV_COUNT = (int)(sizeof(TV_CODES) / sizeof(TV_CODES[0]));

// ============================================================
//  BUTTON HELPER
// ============================================================
struct Btn {
    int16_t     x, y, w, h;
    const char* label;
    uint16_t    color;
};

static bool inBtn(const Btn& b, int x, int y) {
    return (x >= b.x && x <= b.x + b.w &&
            y >= b.y && y <= b.y + b.h);
}

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
void drawMain();
void drawIRMenu();
void drawRFMenu();
void drawScannerMenu();
void drawSettings();

void handleMain(int tx, int ty);
void handleIR(int tx, int ty);
void handleRF(int tx, int ty);
void handleScanner(int tx, int ty);

void taskCaptureIR();
void taskScanRF433();
void taskMonitorIR();
void taskMonitorRF();

void sendAllTV();
void replayIR();
void replayRF();

void drawHdr(const char* title);
void drawBtn(const Btn& b, bool active = false);
void drawBack();
void statusBar(const char* msg, uint16_t c = C_GRAY);
void clearBody();
bool touched();
void getTouch(int& x, int& y);
const char* protoName(decode_type_t p);

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n=== ESP32 RF/IR Multi-Tool ==="));

    // --- TFT ---
    tft.begin();
    tft.setRotation(1); // landscape

    // --- Backlight (PWM on ledc channel 0) ---
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, 200);  // ~78 % brightness

    // --- Touch ---
    ts.begin();
    ts.setRotation(1);

    // --- IR ---
    IrReceiver.begin(IR_RX_PIN, DISABLE_LED_FEEDBACK);
    IrSender.begin(IR_TX_PIN, DISABLE_LED_FEEDBACK);

    // --- 433 MHz ---
    rc.enableReceive(RF433_RX);
    rc.enableTransmit(RF433_TX);

    // --- Splash ---
    tft.fillScreen(0x0010u);
    tft.setTextColor(C_WARN);
    tft.setTextSize(3);
    tft.setCursor(55, 90);
    tft.print(F("ESP32 RF/IR Tool"));
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(95, 140);
    tft.print(F("433 MHz  |  IR  |  Touch"));
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY);
    tft.setCursor(160, 175);
    tft.print(F("v1.0  by juniorpa1122"));
    delay(2000);

    drawMain();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    if (touched()) {
        int tx, ty;
        getTouch(tx, ty);
        delay(60); // debounce

        // Universal "Back" button (bottom-left corner)
        static const Btn BACK = {5, 278, 85, 35, "< Back", 0x4208u};
        if (inBtn(BACK, tx, ty)) {
            switch (state) {
                case S_IR:
                case S_IR_CAPTURE: drawMain(); break;
                case S_RF:
                case S_RF_SCAN:    drawMain(); break;
                case S_SCANNER:
                case S_SCAN_IR:
                case S_SCAN_RF:    drawMain(); break;
                case S_SETTINGS:   drawMain(); break;
                default:           break;
            }
            return;
        }

        switch (state) {
            case S_MAIN:    handleMain(tx, ty);    break;
            case S_IR:      handleIR(tx, ty);      break;
            case S_RF:      handleRF(tx, ty);      break;
            case S_SCANNER: handleScanner(tx, ty); break;
            default:        break;
        }
    }

    // Background tasks
    switch (state) {
        case S_IR_CAPTURE: taskCaptureIR();  break;
        case S_RF_SCAN:    taskScanRF433();  break;
        case S_SCAN_IR:    taskMonitorIR();  break;
        case S_SCAN_RF:    taskMonitorRF();  break;
        default:           break;
    }
}

// ============================================================
//  TOUCH HELPERS
// ============================================================
bool touched() {
    return ts.touched();
}

void getTouch(int& x, int& y) {
    TS_Point p = ts.getPoint();
    x = (int)map(constrain(p.x, TS_MINX, TS_MAXX), TS_MINX, TS_MAXX, 0, SCR_W);
    y = (int)map(constrain(p.y, TS_MINY, TS_MAXY), TS_MINY, TS_MAXY, 0, SCR_H);
}

// ============================================================
//  UI PRIMITIVES
// ============================================================
void drawHdr(const char* title) {
    tft.fillRect(0, 0, SCR_W, 42, C_HDR);
    tft.drawFastHLine(0, 42, SCR_W, C_WARN);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 12);
    tft.print(title);
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY);
    tft.setCursor(400, 16);
    tft.print(F("ESP32"));
}

void drawBtn(const Btn& b, bool active) {
    uint16_t fc = active ? C_BTN_A : b.color;
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 7, fc);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 7, C_WARN);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    // Centre text inside button
    int16_t tx = b.x + (b.w - (int16_t)(strlen(b.label) * 12)) / 2;
    int16_t ty = b.y + (b.h - 16) / 2;
    if (tx < b.x + 4) tx = b.x + 4;
    tft.setCursor(tx, ty);
    tft.print(b.label);
}

void drawBack() {
    static const Btn b = {5, 278, 85, 35, "< Back", 0x4208u};
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, b.color);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, C_WARN);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(b.x + 6, b.y + 9);
    tft.print(b.label);
}

void statusBar(const char* msg, uint16_t c) {
    tft.fillRect(0, 302, SCR_W, 18, C_BG);
    tft.setTextColor(c);
    tft.setTextSize(1);
    tft.setCursor(5, 305);
    tft.print(msg);
}

void clearBody() {
    tft.fillRect(0, 43, SCR_W, SCR_H - 43, C_BG);
}

// ============================================================
//  PROTOCOL NAME
// ============================================================
const char* protoName(decode_type_t p) {
    switch (p) {
        case NEC:      return "NEC";
        case SONY:     return "SONY";
        case RC5:      return "RC5";
        case RC6:      return "RC6";
        case PANASONIC:return "PANASONIC";
        case JVC:      return "JVC";
        case SAMSUNG:  return "SAMSUNG";
        case DENON:    return "DENON";
        case LG:       return "LG";
        case SHARP:    return "SHARP";
        default:       return "UNKNOWN";
    }
}

// ============================================================
//  MAIN MENU
// ============================================================
static const Btn MAIN_BTNS[4] = {
    { 20,  58, 210, 70, "IR Remote",      C_BTN},
    {250,  58, 210, 70, "433 MHz RF",     C_BTN},
    { 20, 148, 210, 70, "Signal Scanner", C_BTN},
    {250, 148, 210, 70, "Settings",       C_BTN},
};

void drawMain() {
    state = S_MAIN;
    tft.fillScreen(C_BG);
    drawHdr("ESP32 RF/IR Multi-Tool");

    for (int i = 0; i < 4; i++) drawBtn(MAIN_BTNS[i]);

    tft.setTextSize(1);
    tft.setTextColor(C_GRAY);
    tft.setCursor(10, 232);
    tft.print(F("IR RX:GPIO35 IR TX:GPIO33 | 433 RX:GPIO26 433 TX:GPIO25"));
    tft.setCursor(10, 248);
    tft.print(F("Touch a button to begin"));
}

void handleMain(int tx, int ty) {
    if (inBtn(MAIN_BTNS[0], tx, ty)) drawIRMenu();
    else if (inBtn(MAIN_BTNS[1], tx, ty)) drawRFMenu();
    else if (inBtn(MAIN_BTNS[2], tx, ty)) drawScannerMenu();
    else if (inBtn(MAIN_BTNS[3], tx, ty)) drawSettings();
}

// ============================================================
//  IR MENU
// ============================================================
static const Btn IR_BTNS[4] = {
    { 20,  55, 440, 52, "Send POWER to ALL TV Brands", C_ERR },
    { 20, 118, 440, 52, "TV Volume UP (common brands)", C_BTN},
    { 20, 181, 440, 52, "Capture IR Signal",            C_BTN},
    { 20, 244, 440, 52, "Replay Captured IR",           C_BTN},
};

void drawIRMenu() {
    state = S_IR;
    tft.fillScreen(C_BG);
    drawHdr("IR Remote Control");
    for (int i = 0; i < 4; i++) drawBtn(IR_BTNS[i]);
    drawBack();
}

void handleIR(int tx, int ty) {
    if (inBtn(IR_BTNS[0], tx, ty)) {
        // Send power to all TV brands
        tft.fillScreen(C_BG);
        drawHdr("Sending Power to All TVs");
        tft.setTextColor(C_WHITE);
        tft.setTextSize(2);
        tft.setCursor(10, 55);
        tft.print(F("Broadcasting to "));
        tft.print(TV_COUNT);
        tft.print(F(" brands..."));
        sendAllTV();
        delay(800);
        drawIRMenu();

    } else if (inBtn(IR_BTNS[1], tx, ty)) {
        statusBar("Sending Volume UP...", C_OK);
        IrReceiver.stop();
        IrSender.sendNEC(0xE0E0, 0xE0, 0);   // Samsung vol+
        delay(200);
        IrSender.sendNEC(0x20DF, 0x40, 0);   // LG vol+
        delay(200);
        IrSender.sendSony(0x01, 0x12, 2);    // Sony vol+
        delay(200);
        IrSender.sendRC5(0x00, 0x10, 0);     // Philips vol+
        delay(200);
        IrReceiver.start();
        statusBar("Volume UP sent to 4 brands.", C_OK);

    } else if (inBtn(IR_BTNS[2], tx, ty)) {
        state = S_IR_CAPTURE;
        ir_captured = false;
        clearBody();
        drawHdr("Capture IR Signal");
        drawBack();
        tft.setTextColor(C_WHITE);
        tft.setTextSize(2);
        tft.setCursor(10, 55);
        tft.print(F("Point remote at IR sensor"));
        tft.setCursor(10, 80);
        tft.print(F("and press any button..."));
        tft.setTextSize(1);
        tft.setTextColor(C_GRAY);
        tft.setCursor(10, 115);
        tft.print(F("IR RX pin: GPIO 35"));
        statusBar("Waiting for IR signal...", C_WARN);

    } else if (inBtn(IR_BTNS[3], tx, ty)) {
        replayIR();
    }
}

// ---- Send power to all TVs ----
void sendAllTV() {
    IrReceiver.stop();
    for (int i = 0; i < TV_COUNT; i++) {
        // Update progress on screen
        tft.fillRect(0, 90, SCR_W, 30, C_BG);
        tft.setTextColor(C_OK);
        tft.setTextSize(2);
        tft.setCursor(10, 95);
        char buf[50];
        snprintf(buf, sizeof(buf), "[%d/%d] %s", i + 1, TV_COUNT, TV_CODES[i].brand);
        tft.print(buf);

        switch (TV_CODES[i].proto) {
            case SONY:
                IrSender.sendSony((uint8_t)TV_CODES[i].addr,
                                  (uint8_t)TV_CODES[i].cmd,
                                  2,
                                  TV_CODES[i].bits);
                break;
            case RC5:
                IrSender.sendRC5((uint8_t)TV_CODES[i].addr,
                                 (uint8_t)TV_CODES[i].cmd, 0);
                break;
            case JVC:
                IrSender.sendJVC((uint8_t)TV_CODES[i].addr,
                                 (uint8_t)TV_CODES[i].cmd, 0);
                break;
            case DENON:
                IrSender.sendDenon(TV_CODES[i].addr,
                                   TV_CODES[i].cmd, 0);
                break;
            default: // NEC + SAMSUNG + LG etc.
                IrSender.sendNEC(TV_CODES[i].addr,
                                 (uint8_t)TV_CODES[i].cmd, 0);
                break;
        }
        delay(250);
    }
    tft.fillRect(0, 90, SCR_W, 30, C_BG);
    tft.setTextColor(C_OK);
    tft.setTextSize(2);
    tft.setCursor(10, 95);
    tft.print(F("Done! All brands sent."));
    IrReceiver.start();
}

// ---- IR capture (called from loop) ----
void taskCaptureIR() {
    if (!IrReceiver.decode()) return;

    ir_protocol = (uint8_t)IrReceiver.decodedIRData.protocol;
    ir_address  = IrReceiver.decodedIRData.address;
    ir_command  = IrReceiver.decodedIRData.command;
    ir_bits     = IrReceiver.decodedIRData.numberOfBits;
    ir_captured = true;

    clearBody();
    drawHdr("IR Signal Captured!");
    drawBack();

    tft.setTextColor(C_OK);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.print(F("Signal Received!"));

    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    char buf[64];
    snprintf(buf, sizeof(buf), "Protocol : %s",
             protoName((decode_type_t)ir_protocol));
    tft.setCursor(10, 80); tft.print(buf);

    snprintf(buf, sizeof(buf), "Address  : 0x%04X", ir_address);
    tft.setCursor(10, 95); tft.print(buf);

    snprintf(buf, sizeof(buf), "Command  : 0x%04X", ir_command);
    tft.setCursor(10, 110); tft.print(buf);

    snprintf(buf, sizeof(buf), "Bits     : %d", ir_bits);
    tft.setCursor(10, 125); tft.print(buf);

    snprintf(buf, sizeof(buf), "Raw value: 0x%08lX",
             (unsigned long)IrReceiver.decodedIRData.decodedRawData);
    tft.setCursor(10, 140); tft.print(buf);

    // First raw timings
    tft.setTextColor(C_GRAY);
    tft.setCursor(10, 162);
    tft.print(F("Raw timings (us, first 24 values):"));
    int rawLen = min((int)IrReceiver.decodedIRData.rawDataPtr->rawlen - 1, 24);
    int ry = 175;
    for (int i = 1; i <= rawLen; i++) {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%5d",
                 (int)(IrReceiver.decodedIRData.rawDataPtr->rawbuf[i] * MICROS_PER_TICK));
        tft.print(tmp);
        if (i % 8 == 0) { ry += 12; tft.setCursor(10, ry); }
    }

    statusBar("Captured! Press 'Replay IR' to resend. Back = menu.", C_OK);
    IrReceiver.resume();
    // Stay in S_IR_CAPTURE so user sees result; Back button returns to main
}

// ---- IR Replay ----
void replayIR() {
    if (!ir_captured) {
        statusBar("No IR signal captured yet!", C_ERR);
        return;
    }
    IrReceiver.stop();
    switch ((decode_type_t)ir_protocol) {
        case SONY:
            IrSender.sendSony((uint8_t)ir_address, (uint8_t)ir_command, 2, ir_bits);
            break;
        case RC5:
            IrSender.sendRC5((uint8_t)ir_address, (uint8_t)ir_command, 0);
            break;
        case RC6:
            IrSender.sendRC6((uint8_t)ir_address, (uint8_t)ir_command, 0);
            break;
        case JVC:
            IrSender.sendJVC((uint8_t)ir_address, (uint8_t)ir_command, 0);
            break;
        case DENON:
            IrSender.sendDenon(ir_address, ir_command, 0);
            break;
        case SAMSUNG:
            IrSender.sendSamsung(ir_address, (uint8_t)ir_command, 0);
            break;
        default: // NEC / LG / Sharp etc.
            IrSender.sendNEC(ir_address, (uint8_t)ir_command, 0);
            break;
    }
    IrReceiver.start();
    statusBar("IR signal replayed!", C_OK);
}

// ============================================================
//  433 MHz MENU
// ============================================================
static const Btn RF_BTNS[2] = {
    {20,  65, 440, 70, "Scan 433 MHz Signals",  C_BTN},
    {20, 155, 440, 70, "Replay Last Signal",    C_BTN},
};

void drawRFMenu() {
    state = S_RF;
    tft.fillScreen(C_BG);
    drawHdr("433 MHz RF Control");
    for (int i = 0; i < 2; i++) drawBtn(RF_BTNS[i]);
    drawBack();
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY);
    tft.setCursor(10, 240);
    tft.print(F("RX: GPIO 26 | TX: GPIO 25  |  Supports OOK/ASK protocols 1-12"));
}

void handleRF(int tx, int ty) {
    if (inBtn(RF_BTNS[0], tx, ty)) {
        state = S_RF_SCAN;
        rf_captured = false;
        clearBody();
        drawHdr("433 MHz Scanner");
        drawBack();
        tft.setTextColor(C_WHITE);
        tft.setTextSize(2);
        tft.setCursor(10, 55);
        tft.print(F("Scanning 433 MHz..."));
        tft.setTextSize(1);
        tft.setTextColor(C_GRAY);
        tft.setCursor(10, 82);
        tft.print(F("Press a 433 MHz remote / sensor near the RX antenna"));
        statusBar("Listening on 433 MHz band...", C_WARN);
    } else if (inBtn(RF_BTNS[1], tx, ty)) {
        replayRF();
    }
}

// ---- 433 scanner (called from loop) ----
void taskScanRF433() {
    if (!rc.available()) return;

    rf_value    = rc.getReceivedValue();
    rf_bits     = rc.getReceivedBitlength();
    rf_protocol = rc.getReceivedProtocol();
    rf_captured = true;
    rc.resetAvailable();

    tft.fillRect(0, 100, SCR_W, 180, C_BG);
    tft.setTextColor(C_OK);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.print(F("Signal Received!"));

    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    char buf[64];
    snprintf(buf, sizeof(buf), "Value (DEC) : %lu", rf_value);
    tft.setCursor(10, 128); tft.print(buf);

    snprintf(buf, sizeof(buf), "Value (HEX) : 0x%08lX", rf_value);
    tft.setCursor(10, 143); tft.print(buf);

    snprintf(buf, sizeof(buf), "Value (BIN) : ");
    tft.setCursor(10, 158); tft.print(buf);
    for (int b2 = rf_bits - 1; b2 >= 0; b2--) {
        tft.print((rf_value >> b2) & 1);
    }

    snprintf(buf, sizeof(buf), "Bit length  : %d", rf_bits);
    tft.setCursor(10, 173); tft.print(buf);

    snprintf(buf, sizeof(buf), "Protocol    : %d", rf_protocol);
    tft.setCursor(10, 188); tft.print(buf);

    statusBar("Signal captured! Scanning for more...", C_OK);
}

// ---- 433 replay ----
void replayRF() {
    if (!rf_captured) {
        statusBar("No RF signal captured yet!", C_ERR);
        return;
    }
    rc.disableReceive();
    rc.setProtocol(rf_protocol);
    rc.send(rf_value, rf_bits);
    rc.enableReceive(RF433_RX);
    statusBar("433 MHz signal replayed!", C_OK);
}

// ============================================================
//  SIGNAL SCANNER MENU
// ============================================================
static const Btn SCAN_BTNS[2] = {
    {20,  65, 440, 70, "IR Activity Monitor",      C_BTN},
    {20, 155, 440, 70, "433 MHz Activity Monitor", C_BTN},
};

void drawScannerMenu() {
    state = S_SCANNER;
    tft.fillScreen(C_BG);
    drawHdr("Signal Activity Scanner");
    for (int i = 0; i < 2; i++) drawBtn(SCAN_BTNS[i]);
    drawBack();
    tft.setTextSize(1);
    tft.setTextColor(C_GRAY);
    tft.setCursor(10, 238);
    tft.print(F("Passive monitoring only — detection, no transmission"));
}

void handleScanner(int tx, int ty) {
    if (inBtn(SCAN_BTNS[0], tx, ty)) {
        state = S_SCAN_IR;
        ir_logY  = 95;
        ir_count = 0;
        clearBody();
        drawHdr("IR Activity Monitor");
        drawBack();
        tft.setTextColor(C_CYAN);
        tft.setTextSize(1);
        tft.setCursor(10, 50);
        tft.print(F("Monitoring all IR signals passively..."));
        tft.setTextColor(C_GRAY);
        tft.setCursor(10, 65);
        tft.print(F("Any IR remote or device transmitting will appear below:"));
        tft.drawFastHLine(0, 80, SCR_W, C_GRAY);
        statusBar("IR Monitor active. Touch 'Back' to return.", C_WARN);

    } else if (inBtn(SCAN_BTNS[1], tx, ty)) {
        state = S_SCAN_RF;
        rf_logY  = 95;
        rf_count = 0;
        clearBody();
        drawHdr("433 MHz Activity Monitor");
        drawBack();
        tft.setTextColor(C_CYAN);
        tft.setTextSize(1);
        tft.setCursor(10, 50);
        tft.print(F("Monitoring 433 MHz band passively..."));
        tft.setTextColor(C_GRAY);
        tft.setCursor(10, 65);
        tft.print(F("Any 433 MHz device (remote, sensor) will appear below:"));
        tft.drawFastHLine(0, 80, SCR_W, C_GRAY);
        statusBar("433 MHz Monitor active. Touch 'Back' to return.", C_WARN);
    }
}

// ---- IR activity monitor (called from loop) ----
void taskMonitorIR() {
    if (!IrReceiver.decode()) return;

    ir_count++;
    if (ir_logY > 260) {
        // Scroll: clear log area and restart
        tft.fillRect(0, 83, SCR_W, 180, C_BG);
        ir_logY = 95;
    }

    char buf[72];
    snprintf(buf, sizeof(buf), "[%3d] %-9s  Addr:0x%04X  Cmd:0x%04X  Bits:%2d",
             ir_count,
             protoName(IrReceiver.decodedIRData.protocol),
             IrReceiver.decodedIRData.address,
             IrReceiver.decodedIRData.command,
             IrReceiver.decodedIRData.numberOfBits);

    uint16_t tc;
    switch (IrReceiver.decodedIRData.protocol) {
        case NEC:     tc = C_CYAN;   break;
        case SONY:    tc = C_YELLOW; break;
        case RC5:
        case RC6:     tc = C_PURPLE; break;
        case SAMSUNG: tc = C_LTBLUE; break;
        default:      tc = C_WHITE;  break;
    }
    tft.setTextColor(tc);
    tft.setTextSize(1);
    tft.setCursor(5, ir_logY);
    tft.print(buf);
    ir_logY += 13;

    char st[48];
    snprintf(st, sizeof(st), "Signals: %d | Last: %s",
             ir_count,
             protoName(IrReceiver.decodedIRData.protocol));
    statusBar(st, C_OK);

    IrReceiver.resume();
}

// ---- 433 MHz activity monitor (called from loop) ----
void taskMonitorRF() {
    if (!rc.available()) return;

    rf_count++;
    unsigned long val   = rc.getReceivedValue();
    int           bits  = (int)rc.getReceivedBitlength();
    int           proto = (int)rc.getReceivedProtocol();
    rc.resetAvailable();

    if (rf_logY > 260) {
        tft.fillRect(0, 83, SCR_W, 180, C_BG);
        rf_logY = 95;
    }

    char buf[72];
    snprintf(buf, sizeof(buf), "[%3d] Val:%10lu  Hex:0x%08lX  Bits:%2d  Proto:%d",
             rf_count, val, val, bits, proto);

    tft.setTextColor((rf_count & 1) ? C_CYAN : C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, rf_logY);
    tft.print(buf);
    rf_logY += 13;

    char st[48];
    snprintf(st, sizeof(st), "433 MHz signals: %d | Last val: %lu", rf_count, val);
    statusBar(st, C_OK);
}

// ============================================================
//  SETTINGS SCREEN
// ============================================================
void drawSettings() {
    state = S_SETTINGS;
    tft.fillScreen(C_BG);
    drawHdr("Settings & Pin Map");

    tft.setTextColor(C_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(10, 52);
    tft.print(F("Pin Assignments"));

    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    const char* lines[] = {
        "TFT CS=15  DC=2   RST=4   BL=32(PWM)",
        "Touch CS=14  IRQ=27",
        "SPI  CLK=18  MOSI=23  MISO=19",
        "IR TX=33   IR RX=35",
        "433 TX=25  433 RX=26",
        "",
        "Libraries:",
        "  Adafruit GFX + ILI9488",
        "  XPT2046_Touchscreen (P. Stoffregen)",
        "  IRremote >= 4.0 (Arduino-IRremote)",
        "  RCSwitch (sui77)",
        "",
        "Display: ILI9488 3.5\" 480x320",
        "MCU    : ESP32 240 MHz dual-core",
    };
    int y = 75;
    for (auto& l : lines) {
        tft.setCursor(10, y);
        tft.print(l);
        y += 14;
    }

    drawBack();
}
