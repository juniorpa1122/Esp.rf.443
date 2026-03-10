/**
 * esp_menu_ili9488.ino
 *
 * RF 433 MHz + IR + WiFi Controller
 * ESP32 + ILI9488 3.5" SPI TFT (320 × 480)
 *
 * Features:
 *   - Multi-level, scrollable menu skeleton
 *   - Toggle, value, sub-menu, action, and back item types
 *   - Physical button navigation  (UP / DOWN / SELECT / BACK)
 *   - Optional touch-screen navigation via XPT2046
 *   - Backlight brightness control via PWM
 *
 * Libraries required (install via Arduino Library Manager):
 *   - TFT_eSPI        (configure User_Setup.h for ILI9488 + ESP32)
 *   - XPT2046_Touchscreen  (optional – for touch input)
 *
 * TFT_eSPI User_Setup.h key settings:
 *   #define ILI9488_DRIVER
 *   #define TFT_CS    5
 *   #define TFT_DC    2
 *   #define TFT_RST   4
 *   #define TFT_MOSI  23
 *   #define TFT_SCLK  18
 *   #define TFT_MISO  19
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "menu.h"

// ─────────────────────────────────────────────
//  Optional touch support
// ─────────────────────────────────────────────
#ifdef TOUCH_CS
  #include <XPT2046_Touchscreen.h>
  SPIClass touchSPI(HSPI);
  XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
  bool touchEnabled = true;
#else
  bool touchEnabled = false;
#endif

// ─────────────────────────────────────────────
//  Display
// ─────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();

// ─────────────────────────────────────────────
//  Persistent user settings (adjust as needed)
// ─────────────────────────────────────────────
static bool  rfEnabled    = true;
static bool  irEnabled    = true;
static bool  wifiEnabled  = false;
static bool  soundEnabled = true;
static int   brightness   = 80;    // 0–100 %
static int   rfChannel    = 1;     // 1–8
static int   irProtocol   = 0;     // 0 = NEC, 1 = RC5, 2 = Sony
static int   wifiChannel  = 6;     // 1–13

// ─────────────────────────────────────────────
//  Action callbacks (stubs – add real code here)
// ─────────────────────────────────────────────
void actionRfSend()      { Serial.println("[ACTION] RF Send"); }
void actionRfLearn()     { Serial.println("[ACTION] RF Learn new code"); }
void actionIrSend()      { Serial.println("[ACTION] IR Send"); }
void actionIrLearn()     { Serial.println("[ACTION] IR Learn new code"); }
void actionWifiScan()    { Serial.println("[ACTION] WiFi Scan networks"); }
void actionWifiConnect() { Serial.println("[ACTION] WiFi Connect"); }
void actionReboot()      { Serial.println("[ACTION] Rebooting..."); delay(500); ESP.restart(); }
void actionAbout()       { Serial.println("[ACTION] About screen"); }

// ─────────────────────────────────────────────
//  Menu definitions  (bottom-up: leaves first)
// ─────────────────────────────────────────────

// ── RF sub-menu ────────────────────────────
static MenuItem rfItems[] = {
    { "RF Enable",      MenuItemType::TOGGLE,  nullptr, &rfEnabled,  nullptr, 0, 0,   0, nullptr },
    { "RF Channel",     MenuItemType::VALUE,   nullptr, nullptr, &rfChannel,  1, 8,   1, nullptr },
    { "Send Signal",    MenuItemType::ACTION,  actionRfSend,   nullptr, nullptr, 0, 0, 0, nullptr },
    { "Learn Code",     MenuItemType::ACTION,  actionRfLearn,  nullptr, nullptr, 0, 0, 0, nullptr },
    { "< Back",         MenuItemType::BACK,    nullptr, nullptr, nullptr, 0, 0, 0, nullptr },
};
static Menu rfMenu = { "RF 433 MHz", rfItems, 5 };

// ── IR sub-menu ────────────────────────────
static MenuItem irItems[] = {
    { "IR Enable",      MenuItemType::TOGGLE,  nullptr, &irEnabled,  nullptr, 0, 0,   0, nullptr },
    { "IR Protocol",    MenuItemType::VALUE,   nullptr, nullptr, &irProtocol, 0, 2, 1, nullptr },
    { "Send Signal",    MenuItemType::ACTION,  actionIrSend,   nullptr, nullptr, 0, 0, 0, nullptr },
    { "Learn Code",     MenuItemType::ACTION,  actionIrLearn,  nullptr, nullptr, 0, 0, 0, nullptr },
    { "< Back",         MenuItemType::BACK,    nullptr, nullptr, nullptr, 0, 0, 0, nullptr },
};
static Menu irMenu = { "Infrared (IR)", irItems, 5 };

// ── WiFi sub-menu ──────────────────────────
static MenuItem wifiItems[] = {
    { "WiFi Enable",    MenuItemType::TOGGLE,  nullptr, &wifiEnabled, nullptr, 0, 0,  0, nullptr },
    { "WiFi Channel",   MenuItemType::VALUE,   nullptr, nullptr, &wifiChannel, 1, 13, 1, nullptr },
    { "Scan Networks",  MenuItemType::ACTION,  actionWifiScan,    nullptr, nullptr, 0, 0, 0, nullptr },
    { "Connect",        MenuItemType::ACTION,  actionWifiConnect, nullptr, nullptr, 0, 0, 0, nullptr },
    { "< Back",         MenuItemType::BACK,    nullptr, nullptr, nullptr, 0, 0, 0, nullptr },
};
static Menu wifiMenu = { "WiFi", wifiItems, 5 };

// ── Settings sub-menu ──────────────────────
static MenuItem settingsItems[] = {
    { "Brightness",     MenuItemType::VALUE,   nullptr, nullptr, &brightness, 10, 100, 10, nullptr },
    { "Sound",          MenuItemType::TOGGLE,  nullptr, &soundEnabled, nullptr, 0, 0, 0, nullptr },
    { "Reboot",         MenuItemType::ACTION,  actionReboot, nullptr, nullptr, 0, 0, 0, nullptr },
    { "About",          MenuItemType::ACTION,  actionAbout,  nullptr, nullptr, 0, 0, 0, nullptr },
    { "< Back",         MenuItemType::BACK,    nullptr, nullptr, nullptr, 0, 0, 0, nullptr },
};
static Menu settingsMenu = { "Settings", settingsItems, 5 };

// ── Root / Main menu ───────────────────────
static MenuItem rootItems[] = {
    { "RF 433 MHz",     MenuItemType::SUBMENU, nullptr, nullptr, nullptr, 0, 0, 0, &rfMenu       },
    { "Infrared (IR)",  MenuItemType::SUBMENU, nullptr, nullptr, nullptr, 0, 0, 0, &irMenu       },
    { "WiFi",           MenuItemType::SUBMENU, nullptr, nullptr, nullptr, 0, 0, 0, &wifiMenu     },
    { "Settings",       MenuItemType::SUBMENU, nullptr, nullptr, nullptr, 0, 0, 0, &settingsMenu },
};
static Menu rootMenu = { "ESP Controller", rootItems, 4 };

// ─────────────────────────────────────────────
//  Button state tracking
// ─────────────────────────────────────────────
struct Button {
    uint8_t  pin;
    bool     lastState;
    uint32_t lastChange;
};

static Button btnUp     = { BTN_UP,     HIGH, 0 };
static Button btnDown   = { BTN_DOWN,   HIGH, 0 };
static Button btnSel    = { BTN_SELECT, HIGH, 0 };
static Button btnBack   = { BTN_BACK,   HIGH, 0 };

// Returns true on falling-edge (press) after debounce
bool buttonPressed(Button& b) {
    bool state = digitalRead(b.pin);
    uint32_t now = millis();
    if (state != b.lastState && (now - b.lastChange) > DEBOUNCE_MS) {
        b.lastState  = state;
        b.lastChange = now;
        if (state == LOW) return true;   // Active-LOW press
    }
    return false;
}

// ─────────────────────────────────────────────
//  Splash screen
// ─────────────────────────────────────────────
void showSplash() {
    tft.fillScreen(COLOR_BG);

    // Logo / title
    tft.setTextSize(3);
    tft.setTextColor(COLOR_ITEM_SEL, COLOR_BG);
    tft.setCursor((SCREEN_WIDTH - 9 * 18) / 2, 140);
    tft.print("ESP");

    tft.setTextSize(2);
    tft.setTextColor(COLOR_HEADER_TEXT, COLOR_BG);
    tft.setCursor((SCREEN_WIDTH - 13 * 12) / 2, 190);
    tft.print("RF + IR + WiFi");

    tft.setTextSize(1);
    tft.setTextColor(COLOR_BORDER, COLOR_BG);
    tft.setCursor((SCREEN_WIDTH - 19 * 6) / 2, 240);
    tft.print("433 MHz Controller");

    // Progress bar outline
    tft.drawRect(60, 300, SCREEN_WIDTH - 120, 16, COLOR_BORDER);
    for (int i = 0; i <= 100; i += 2) {
        tft.fillRect(62, 302, (int16_t)((SCREEN_WIDTH - 124) * i / 100), 12, COLOR_ITEM_SEL);
        delay(SPLASH_MS / 50);
    }
}

// ─────────────────────────────────────────────
//  Backlight PWM
// ─────────────────────────────────────────────
void applyBrightness(int pct) {
#if TFT_BL >= 0
    analogWrite(TFT_BL, map(pct, 0, 100, 0, 255));
#endif
}

// ─────────────────────────────────────────────
//  Touch input  (optional)
// ─────────────────────────────────────────────
#ifdef TOUCH_CS
void handleTouch() {
    if (!touch.tirqTouched() || !touch.touched()) return;

    TS_Point p = touch.getPoint();
    // Map raw ADC values to screen coordinates (calibrate for your panel)
    int16_t tx = map(p.x, 300, 3800, 0, SCREEN_WIDTH);
    int16_t ty = map(p.y, 300, 3800, 0, SCREEN_HEIGHT);

    // Determine which item was tapped
    if (ty < HEADER_H) return;  // Tapped header – ignore

    int16_t itemAreaY = ty - HEADER_H;
    uint8_t tappedIdx = itemAreaY / ITEM_H;

    // Move selection to tapped row then activate
    // (Simple approach: tap = navigate to that row + select)
    // A more refined approach would require comparing with scroll offset.
    (void)tx;
    (void)tappedIdx;
    // TODO: implement precise touch-to-item mapping
}
#endif

// ─────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP Controller booting ===");

    // Backlight
#if TFT_BL >= 0
    pinMode(TFT_BL, OUTPUT);
    applyBrightness(brightness);
#endif

    // Navigation buttons (internal pull-ups)
    pinMode(BTN_UP,     INPUT_PULLUP);
    pinMode(BTN_DOWN,   INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK,   INPUT_PULLUP);

    // Display
    tft.init();
    tft.setRotation(0);   // Portrait

    showSplash();

    // Touch
#ifdef TOUCH_CS
    touchSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
    touch.begin(touchSPI);
    touch.setRotation(0);
#endif

    // Menu engine
    menuEngine.begin(&rootMenu);

    Serial.println("=== Ready ===");
}

// ─────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────
void loop() {
    // ── Physical buttons ──────────────────────
    if (buttonPressed(btnUp))   menuEngine.navigateUp();
    if (buttonPressed(btnDown)) menuEngine.navigateDown();
    if (buttonPressed(btnSel))  menuEngine.selectCurrent();
    if (buttonPressed(btnBack)) menuEngine.navigateBack();

    // ── Touch input ───────────────────────────
#ifdef TOUCH_CS
    if (touchEnabled) handleTouch();
#endif

    // ── Redraw if needed ──────────────────────
    menuEngine.update();

    // ── Apply brightness setting ──────────────
    static int lastBrightness = -1;
    if (brightness != lastBrightness) {
        applyBrightness(brightness);
        lastBrightness = brightness;
    }

    delay(10);
}
