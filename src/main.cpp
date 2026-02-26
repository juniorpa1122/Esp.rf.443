/*
 * ESP32 Multi-Tool – ILI9488 Touchscreen Menu
 * ============================================
 * Features:
 *   • Touch-driven menu on a 3.5" ILI9488 SPI display
 *   • IR remote: record, replay, send TV brand codes
 *   • 433 MHz: scan / store / replay up to 16 codes
 *   • General signal scanner (passive – no jamming / hacking)
 *
 * Libraries required (see platformio.ini):
 *   Adafruit GFX + ILI9341 (same SPI driver, re-used for ILI9488)
 *   XPT2046_Touchscreen
 *   IRremoteESP8266
 *   rc-switch
 *
 * ─── Wiring summary ─────────────────────────────────────────────────────────
 * Display ILI9488 (SPI):
 *   VCC  → 3.3 V        GND → GND
 *   CS   → GPIO 15      DC  → GPIO 2
 *   RST  → GPIO 4       SDI → GPIO 23 (MOSI)
 *   SCK  → GPIO 18      LED → 3.3 V (or PWM)
 *
 * Touch XPT2046 (same SPI bus):
 *   T_CS → GPIO 5       T_IRQ → GPIO 27
 *   T_DIN → GPIO 23     T_DO  → GPIO 19 (MISO)
 *   T_CLK → GPIO 18
 *
 * 433 MHz RX → GPIO 34   TX → GPIO 12
 * IR RX      → GPIO 35   TX → GPIO 14
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>   // ILI9488 uses a compatible initialisation
#include <XPT2046_Touchscreen.h>

#include "ir_module.h"
#include "rf433_module.h"
#include "scanner_module.h"

// ─── Display pins ─────────────────────────────────────────────────────────────
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

// ─── Touch pins ───────────────────────────────────────────────────────────────
#define TOUCH_CS  5
#define TOUCH_IRQ 27

// ─── Display size (ILI9488 = 480×320, driver reports 320×240 – use 480×320) ──
#define DISP_W  480
#define DISP_H  320

// ─── Colours ──────────────────────────────────────────────────────────────────
#define COL_BG       0x0000   // Black background
#define COL_HDR      0x001F   // Blue header bar
#define COL_HDR_TXT  0xFFFF   // White header text
#define COL_BTN      0x2945   // Dark blue-grey button
#define COL_BTN_SEL  0x07FF   // Cyan – highlighted / active
#define COL_BTN_TXT  0xFFFF   // White button label
#define COL_GOOD     0x07E0   // Green – signal detected
#define COL_IDLE     0x7BEF   // Light-grey – idle
#define COL_WARN     0xFD20   // Orange – warning / attention

// ─── Object instances ─────────────────────────────────────────────────────────
Adafruit_ILI9341    tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// ─── Touch calibration (adjust for your panel) ────────────────────────────────
// Raw ADC range → pixel mapping:  px = (raw - minRaw) * dispSize / rawRange
#define TOUCH_X_MIN  200
#define TOUCH_X_MAX  3700
#define TOUCH_Y_MIN  200
#define TOUCH_Y_MAX  3700
#define TOUCH_PRESSURE_MIN 400  // Minimum Z pressure to accept a touch event
// ─── Menu states ──────────────────────────────────────────────────────────────
enum Screen {
    SCR_MAIN = 0,
    SCR_IR,
    SCR_IR_TV,
    SCR_IR_RECORD,
    SCR_IR_REPLAY,
    SCR_RF,
    SCR_RF_SCAN,
    SCR_RF_REPLAY,
    SCR_SCANNER,
};

static Screen currentScreen = SCR_MAIN;

// ─── Simple button descriptor ─────────────────────────────────────────────────
struct Button {
    int16_t  x, y, w, h;
    uint16_t color;
    const char *label;
};

// ─── Helper: draw a rounded button ────────────────────────────────────────────
static void drawButton(const Button &b, bool selected = false) {
    uint16_t bg = selected ? COL_BTN_SEL : b.color;
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, bg);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COL_BTN_TXT);
    tft.setTextColor(COL_BTN_TXT, bg);
    tft.setTextSize(2);
    // Centre text (6px char width × 2 size = 12px; 8px char height × 2 = 16px)
    int16_t tx = b.x + (b.w - (int16_t)strlen(b.label) * 12) / 2;
    int16_t ty = b.y + (b.h - 16) / 2;
    tft.setCursor(tx, ty);
    tft.print(b.label);
}

// ─── Helper: draw page header ─────────────────────────────────────────────────
static void drawHeader(const char *title) {
    tft.fillRect(0, 0, DISP_W, 36, COL_HDR);
    tft.setTextColor(COL_HDR_TXT, COL_HDR);
    tft.setTextSize(2);
    tft.setCursor(8, 10);
    tft.print(title);
    // Back button (top-right corner)
    if (currentScreen != SCR_MAIN) {
        tft.fillRoundRect(DISP_W - 70, 4, 64, 28, 6, COL_WARN);
        tft.setTextColor(COL_BTN_TXT, COL_WARN);
        tft.setCursor(DISP_W - 62, 12);
        tft.print("BACK");
    }
}

// ─── Helper: map raw touch to pixels ──────────────────────────────────────────
static void mapTouch(uint16_t rx, uint16_t ry, int16_t &px, int16_t &py) {
    px = map((int32_t)rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, DISP_W - 1);
    py = map((int32_t)ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, DISP_H - 1);
    px = constrain(px, 0, DISP_W - 1);
    py = constrain(py, 0, DISP_H - 1);
}

// ─── Helper: check if a point is inside a button ─────────────────────────────
static bool buttonHit(const Button &b, int16_t px, int16_t py) {
    return px >= b.x && px < b.x + b.w &&
           py >= b.y && py < b.y + b.h;
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCREEN DRAW FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

// ── Main menu ─────────────────────────────────────────────────────────────────
static const Button MAIN_BTNS[] = {
    {  10,  50, 220, 60, COL_BTN, "IR Remote"   },
    { 250,  50, 220, 60, COL_BTN, "RF 433 MHz"  },
    {  10, 130, 220, 60, COL_BTN, "Scanner"     },
    { 250, 130, 220, 60, COL_BTN, "About"       },
};
static const uint8_t MAIN_BTN_COUNT = 4;

static void drawMainMenu() {
    tft.fillScreen(COL_BG);
    drawHeader("ESP32 Multi-Tool v1.0");
    for (uint8_t i = 0; i < MAIN_BTN_COUNT; i++)
        drawButton(MAIN_BTNS[i]);
    tft.setTextColor(0x7BEF, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(8, DISP_H - 16);
    tft.print("IR:GPIO35/14  RF:GPIO34/12");
}

static void handleMainMenu(int16_t px, int16_t py) {
    if (buttonHit(MAIN_BTNS[0], px, py)) { currentScreen = SCR_IR;      return; }
    if (buttonHit(MAIN_BTNS[1], px, py)) { currentScreen = SCR_RF;      return; }
    if (buttonHit(MAIN_BTNS[2], px, py)) { currentScreen = SCR_SCANNER; return; }
    if (buttonHit(MAIN_BTNS[3], px, py)) {
        // "About" – show info inline
        tft.fillRect(0, 210, DISP_W, 110, COL_BG);
        tft.setTextColor(0x07FF, COL_BG);
        tft.setTextSize(1);
        tft.setCursor(8, 215);
        tft.println("ESP32 Multi-Tool");
        tft.println("IR + RF 433 + Scanner");
        tft.println("Libraries: IRremoteESP8266, RCSwitch");
        tft.println("Display: ILI9488 3.5\" SPI Touch");
    }
}

// ── IR menu ───────────────────────────────────────────────────────────────────
static const Button IR_BTNS[] = {
    {  10,  50, 220, 60, COL_BTN, "TV Codes"  },
    { 250,  50, 220, 60, COL_BTN, "Record IR" },
    { 130, 130, 220, 60, COL_BTN, "Replay IR" },
};
static const uint8_t IR_BTN_COUNT = 3;

static void drawIRMenu() {
    tft.fillScreen(COL_BG);
    drawHeader("IR Remote");
    for (uint8_t i = 0; i < IR_BTN_COUNT; i++)
        drawButton(IR_BTNS[i]);
    // Show last received signal
    tft.setTextColor(COL_IDLE, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(8, DISP_H - 28);
    tft.print("Last RX: ");
    tft.print(ir_last_signal_str());
}

static void handleIRMenu(int16_t px, int16_t py) {
    if (buttonHit(IR_BTNS[0], px, py)) { currentScreen = SCR_IR_TV;     return; }
    if (buttonHit(IR_BTNS[1], px, py)) { currentScreen = SCR_IR_RECORD;  return; }
    if (buttonHit(IR_BTNS[2], px, py)) { currentScreen = SCR_IR_REPLAY;  return; }
}

// ── IR – TV brand codes ───────────────────────────────────────────────────────
static void drawIRTVMenu() {
    tft.fillScreen(COL_BG);
    drawHeader("Send TV Code (POWER)");
    uint8_t cols = 2, rows = (TV_CODES_COUNT + 1) / 2;
    int16_t bw = (DISP_W - 30) / cols;
    int16_t bh = min((int16_t)48, (int16_t)((DISP_H - 50) / rows));
    for (uint8_t i = 0; i < TV_CODES_COUNT; i++) {
        int16_t col = i % cols, row = i / cols;
        Button b = { (int16_t)(10 + col * (bw + 10)),
                     (int16_t)(44 + row * (bh + 6)),
                     (int16_t)bw, (int16_t)bh,
                     COL_BTN, TV_CODES[i].label };
        drawButton(b);
    }
}

static void handleIRTVMenu(int16_t px, int16_t py) {
    uint8_t cols = 2, rows = (TV_CODES_COUNT + 1) / 2;
    int16_t bw = (DISP_W - 30) / cols;
    int16_t bh = min((int16_t)48, (int16_t)((DISP_H - 50) / rows));
    for (uint8_t i = 0; i < TV_CODES_COUNT; i++) {
        int16_t col = i % cols, row = i / cols;
        Button b = { (int16_t)(10 + col * (bw + 10)),
                     (int16_t)(44 + row * (bh + 6)),
                     (int16_t)bw, (int16_t)bh,
                     COL_BTN, TV_CODES[i].label };
        if (buttonHit(b, px, py)) {
            ir_send_tv_code(i);
            drawButton(b, true);
            delay(300);
            drawButton(b, false);
            return;
        }
    }
}

// ── IR – Record ───────────────────────────────────────────────────────────────
static bool irRecording = false;

static void drawIRRecord() {
    tft.fillScreen(COL_BG);
    drawHeader("Record IR Signal");
    // Status area
    tft.fillRoundRect(10, 44, DISP_W - 20, 100, 8,
                      irRecording ? COL_GOOD : COL_BTN);
    tft.setTextColor(COL_BTN_TXT, irRecording ? COL_GOOD : COL_BTN);
    tft.setTextSize(2);
    tft.setCursor(20, 70);
    tft.print(irRecording ? "LISTENING..." : "Tap START");
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.print(ir_last_signal_str());

    // START / STOP button
    Button startBtn = { 130, 160, 220, 60, irRecording ? COL_WARN : COL_GOOD,
                        irRecording ? "STOP" : "START" };
    drawButton(startBtn);
}

static void handleIRRecord(int16_t px, int16_t py) {
    Button startBtn = { 130, 160, 220, 60, COL_GOOD, "START" };
    if (buttonHit(startBtn, px, py)) {
        irRecording = !irRecording;
        drawIRRecord();
    }
}

// ── IR – Replay ───────────────────────────────────────────────────────────────
static void drawIRReplay() {
    tft.fillScreen(COL_BG);
    drawHeader("Replay IR Signal");
    tft.setTextColor(COL_IDLE, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 48);
    tft.print("Stored: ");
    tft.print(ir_last_signal_str());
    Button replayBtn = { 130, 120, 220, 60, COL_BTN_SEL, "SEND NOW" };
    drawButton(replayBtn);
}

static void handleIRReplay(int16_t px, int16_t py) {
    Button replayBtn = { 130, 120, 220, 60, COL_BTN_SEL, "SEND NOW" };
    if (buttonHit(replayBtn, px, py)) {
        ir_replay();
        drawButton(replayBtn, true);
        delay(300);
        drawButton(replayBtn, false);
    }
}

// ── RF 433 menu ───────────────────────────────────────────────────────────────
static const Button RF_BTNS[] = {
    {  10,  50, 220, 60, COL_BTN, "Scan 433"    },
    { 250,  50, 220, 60, COL_BTN, "Replay Last" },
};
static const uint8_t RF_BTN_COUNT = 2;

static void drawRFMenu() {
    tft.fillScreen(COL_BG);
    drawHeader("RF 433 MHz");
    for (uint8_t i = 0; i < RF_BTN_COUNT; i++)
        drawButton(RF_BTNS[i]);
    // List stored codes
    tft.setTextColor(COL_IDLE, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 130);
    tft.printf("Stored codes: %u / %u", rf433Count, RF_MAX_STORED);
    for (uint8_t i = 0; i < min((uint8_t)8, rf433Count); i++) {
        tft.setCursor(8, 148 + i * 18);
        tft.printf("#%u: %s", i + 1, rf433_code_str(i).c_str());
    }
}

static void handleRFMenu(int16_t px, int16_t py) {
    if (buttonHit(RF_BTNS[0], px, py)) { currentScreen = SCR_RF_SCAN;   return; }
    if (buttonHit(RF_BTNS[1], px, py)) { currentScreen = SCR_RF_REPLAY; return; }
}

// ── RF – Scan ─────────────────────────────────────────────────────────────────
static bool rfScanning = false;

static void drawRFScan() {
    tft.fillScreen(COL_BG);
    drawHeader("Scan 433 MHz");
    tft.fillRoundRect(10, 44, DISP_W - 20, 100, 8,
                      rfScanning ? COL_GOOD : COL_BTN);
    tft.setTextColor(COL_BTN_TXT, rfScanning ? COL_GOOD : COL_BTN);
    tft.setTextSize(2);
    tft.setCursor(20, 68);
    tft.print(rfScanning ? "Scanning..." : "Tap START");
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.printf("Captured: %u codes", rf433Count);
    if (rf433Count > 0) {
        tft.setCursor(20, 116);
        tft.print("Last: ");
        tft.print(rf433_code_str(rf433Count - 1));
    }
    Button startBtn = { 130, 160, 220, 60,
                        rfScanning ? COL_WARN : COL_GOOD,
                        rfScanning ? "STOP" : "START" };
    drawButton(startBtn);
}

static void handleRFScan(int16_t px, int16_t py) {
    Button startBtn = { 130, 160, 220, 60, COL_GOOD, "START" };
    if (buttonHit(startBtn, px, py)) {
        rfScanning = !rfScanning;
        drawRFScan();
    }
}

// ── RF – Replay ───────────────────────────────────────────────────────────────
static void drawRFReplay() {
    tft.fillScreen(COL_BG);
    drawHeader("Replay 433 MHz");
    tft.setTextColor(COL_IDLE, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 48);
    tft.printf("Codes stored: %u", rf433Count);
    for (uint8_t i = 0; i < min((uint8_t)4, rf433Count); i++) {
        // Each stored code gets its own send button
        Button b = { 10, (int16_t)(70 + i * 56), DISP_W - 20, 50,
                     COL_BTN, rf433_code_str(i).c_str() };
        drawButton(b);
    }
    if (rf433Count == 0) {
        tft.setCursor(8, 90);
        tft.print("No codes stored yet. Use Scan first.");
    }
}

static void handleRFReplay(int16_t px, int16_t py) {
    for (uint8_t i = 0; i < min((uint8_t)4, rf433Count); i++) {
        Button b = { 10, (int16_t)(70 + i * 56), DISP_W - 20, 50,
                     COL_BTN, rf433_code_str(i).c_str() };
        if (buttonHit(b, px, py)) {
            rf433_replay(i);
            drawButton(b, true);
            delay(400);
            drawButton(b, false);
            return;
        }
    }
}

// ── General Scanner ────────────────────────────────────────────────────────────
static uint32_t scannerLastDraw = 0;

static void drawScannerScreen(bool fullRedraw) {
    if (fullRedraw) {
        tft.fillScreen(COL_BG);
        drawHeader("Signal Scanner (passive)");
        tft.setTextColor(COL_IDLE, COL_BG);
        tft.setTextSize(1);
        tft.setCursor(8, 42);
        tft.print("Monitoring RF 433 & IR  – no signals sent");
    }

    // RF status box
    uint16_t rfCol = scanner_rf_active() ? COL_GOOD : COL_BTN;
    tft.fillRoundRect(10, 60, 220, 100, 10, rfCol);
    tft.setTextColor(COL_BTN_TXT, rfCol);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.print("RF 433");
    tft.setTextSize(1);
    tft.setCursor(20, 106);
    tft.print(scanner_rf_active() ? ">>> SIGNAL <<<" : "   idle");
    tft.setCursor(20, 122);
    tft.printf("Events: %lu", scanner_rf_event_count());

    // IR status box
    uint16_t irCol = scanner_ir_active() ? COL_GOOD : COL_BTN;
    tft.fillRoundRect(250, 60, 220, 100, 10, irCol);
    tft.setTextColor(COL_BTN_TXT, irCol);
    tft.setTextSize(2);
    tft.setCursor(260, 80);
    tft.print("IR");
    tft.setTextSize(1);
    tft.setCursor(260, 106);
    tft.print(scanner_ir_active() ? ">>> SIGNAL <<<" : "   idle");
    tft.setCursor(260, 122);
    tft.printf("Events: %lu", scanner_ir_event_count());

    // Reset button
    Button resetBtn = { 130, 180, 220, 50, COL_WARN, "RESET CNT" };
    if (fullRedraw) drawButton(resetBtn);
}

static void handleScanner(int16_t px, int16_t py) {
    Button resetBtn = { 130, 180, 220, 50, COL_WARN, "RESET CNT" };
    if (buttonHit(resetBtn, px, py)) {
        scanner_reset();
        drawScannerScreen(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCREEN ROUTER
// ─────────────────────────────────────────────────────────────────────────────

static Screen previousScreen = SCR_MAIN;

static void enterScreen(Screen scr) {
    previousScreen = currentScreen;
    currentScreen  = scr;
    irRecording    = false;
    rfScanning     = false;

    switch (scr) {
        case SCR_MAIN:      drawMainMenu();         break;
        case SCR_IR:        drawIRMenu();           break;
        case SCR_IR_TV:     drawIRTVMenu();         break;
        case SCR_IR_RECORD: drawIRRecord();         break;
        case SCR_IR_REPLAY: drawIRReplay();         break;
        case SCR_RF:        drawRFMenu();           break;
        case SCR_RF_SCAN:   drawRFScan();           break;
        case SCR_RF_REPLAY: drawRFReplay();         break;
        case SCR_SCANNER:
            scanner_reset();
            drawScannerScreen(true);
            break;
        default:            drawMainMenu();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println(F("\n[BOOT] ESP32 Multi-Tool starting..."));

    // Display
    tft.begin();
    tft.setRotation(1);  // Landscape
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_HDR_TXT, COL_BG);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("Initialising...");

    // Touch
    touch.begin();
    touch.setRotation(1);

    // Modules
    ir_init();
    rf433_init();
    scanner_init();

    Serial.println(F("[BOOT] All modules ready. Drawing main menu."));
    enterScreen(SCR_MAIN);
}

// ─────────────────────────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    // ── Background tasks ─────────────────────────────────────────────────────
    // IR recording (when active)
    if (irRecording) {
        if (ir_record()) {
            // Refresh status area without full redraw
            tft.fillRoundRect(10, 44, DISP_W - 20, 100, 8, COL_GOOD);
            tft.setTextColor(COL_BTN_TXT, COL_GOOD);
            tft.setTextSize(2);
            tft.setCursor(20, 70);
            tft.print("CAPTURED!    ");
            tft.setTextSize(1);
            tft.setCursor(20, 100);
            tft.print(ir_last_signal_str());
            irRecording = false;
            // Re-draw STOP→START button
            Button startBtn = { 130, 160, 220, 60, COL_GOOD, "START" };
            drawButton(startBtn);
        }
    }

    // RF scanning
    if (rfScanning) {
        if (rf433_scan()) {
            // Refresh capture count
            tft.fillRoundRect(10, 44, DISP_W - 20, 100, 8, COL_GOOD);
            tft.setTextColor(COL_BTN_TXT, COL_GOOD);
            tft.setTextSize(2);
            tft.setCursor(20, 68);
            tft.print("GOT ONE!    ");
            tft.setTextSize(1);
            tft.setCursor(20, 100);
            tft.printf("Captured: %u codes", rf433Count);
            tft.setCursor(20, 116);
            tft.print(rf433_code_str(rf433Count - 1));
            delay(600);   // brief visual feedback
            drawRFScan(); // redraw with updated count
        }
    }

    // Signal scanner
    if (currentScreen == SCR_SCANNER) {
        bool changed = scanner_poll();
        uint32_t now = millis();
        if (changed || now - scannerLastDraw > 500) {
            scannerLastDraw = now;
            drawScannerScreen(false);
        }
    }

    // ── Touch input ──────────────────────────────────────────────────────────
    if (!touch.tirqTouched() || !touch.touched()) return;

    TS_Point p = touch.getPoint();
    if (p.z < TOUCH_PRESSURE_MIN) return;   // Noise filter

    int16_t px, py;
    mapTouch((uint16_t)p.x, (uint16_t)p.y, px, py);

    Serial.printf("[TOUCH] raw(%d,%d) -> px(%d,%d)\n", p.x, p.y, px, py);

    // Global BACK button (top-right, any non-main screen)
    if (currentScreen != SCR_MAIN && px > DISP_W - 70 && py < 36) {
        // Navigate back
        Screen parent = SCR_MAIN;
        if (currentScreen == SCR_IR_TV    ||
            currentScreen == SCR_IR_RECORD ||
            currentScreen == SCR_IR_REPLAY) parent = SCR_IR;
        if (currentScreen == SCR_RF_SCAN   ||
            currentScreen == SCR_RF_REPLAY) parent = SCR_RF;
        enterScreen(parent);
        delay(200);
        return;
    }

    // Per-screen handlers
    switch (currentScreen) {
        case SCR_MAIN:
            handleMainMenu(px, py);
            if (currentScreen != SCR_MAIN) enterScreen(currentScreen);
            break;
        case SCR_IR:
            handleIRMenu(px, py);
            if (currentScreen != SCR_IR) enterScreen(currentScreen);
            break;
        case SCR_IR_TV:     handleIRTVMenu(px, py);  break;
        case SCR_IR_RECORD: handleIRRecord(px, py);  break;
        case SCR_IR_REPLAY: handleIRReplay(px, py);  break;
        case SCR_RF:
            handleRFMenu(px, py);
            if (currentScreen != SCR_RF) enterScreen(currentScreen);
            break;
        case SCR_RF_SCAN:   handleRFScan(px, py);    break;
        case SCR_RF_REPLAY: handleRFReplay(px, py);  break;
        case SCR_SCANNER:   handleScanner(px, py);   break;
        default: break;
    }

    delay(150);  // simple debounce
}
