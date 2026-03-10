/**
 * config.h
 * Pin definitions and display settings for ESP32 + ILI9488 menu project.
 *
 * Hardware:
 *   - ESP32 (or ESP8266 with pin adjustments)
 *   - ILI9488 3.5" SPI TFT display (320x480)
 *   - Optional: XPT2046 touch controller
 *   - Navigation buttons: UP / DOWN / SELECT / BACK
 *
 * RF 433 MHz + IR + WiFi Controller
 */

#pragma once

// ─────────────────────────────────────────────
//  SPI / ILI9488 display pins  (ESP32 VSPI)
// ─────────────────────────────────────────────
#define TFT_CS    5    // Chip Select
#define TFT_DC    2    // Data / Command
#define TFT_RST   4    // Reset  (-1 to use MCU reset)
#define TFT_MOSI  23   // SPI MOSI
#define TFT_SCLK  18   // SPI clock
#define TFT_MISO  19   // SPI MISO  (needed for touch)
#define TFT_BL    32   // Backlight PWM  (-1 if always-on)

// ─────────────────────────────────────────────
//  XPT2046 touch controller pins
// ─────────────────────────────────────────────
#define TOUCH_CS  15
#define TOUCH_IRQ 27

// ─────────────────────────────────────────────
//  Physical navigation buttons (active LOW)
// ─────────────────────────────────────────────
#define BTN_UP      34
#define BTN_DOWN    35
#define BTN_SELECT  33
#define BTN_BACK    25

// ─────────────────────────────────────────────
//  RF 433 MHz
// ─────────────────────────────────────────────
#define RF_TX_PIN   26
#define RF_RX_PIN   36

// ─────────────────────────────────────────────
//  IR
// ─────────────────────────────────────────────
#define IR_TX_PIN   13
#define IR_RX_PIN   14

// ─────────────────────────────────────────────
//  Display dimensions
// ─────────────────────────────────────────────
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480

// ─────────────────────────────────────────────
//  Colour palette  (RGB565)
// ─────────────────────────────────────────────
#define COLOR_BG          0x0841   // Dark navy
#define COLOR_HEADER_BG   0x2945   // Dark teal
#define COLOR_HEADER_TEXT 0xFFFF   // White
#define COLOR_ITEM_BG     0x10A2   // Dark slate
#define COLOR_ITEM_SEL    0x04FF   // Bright cyan accent
#define COLOR_ITEM_TEXT   0xFFFF   // White
#define COLOR_SEL_TEXT    0x0000   // Black on highlight
#define COLOR_BORDER      0x2104   // Dim grey

// ─────────────────────────────────────────────
//  Menu layout
// ─────────────────────────────────────────────
#define HEADER_H       50    // Header strip height  (px)
#define ITEM_H         52    // Height of each menu row (px)
#define ITEM_PADDING_X 16    // Horizontal text offset (px)
#define FOOTER_H       30    // Footer strip height  (px)
#define MAX_VISIBLE    7     // Max items visible without scrolling

// ─────────────────────────────────────────────
//  Timing
// ─────────────────────────────────────────────
#define DEBOUNCE_MS     50   // Button debounce
#define LONG_PRESS_MS  600   // Long-press threshold
#define SPLASH_MS     2000   // Splash screen duration
