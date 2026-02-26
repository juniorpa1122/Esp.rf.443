// ============================================================
//  TFT_eSPI User Setup – 3.5" ILI9488 SPI display on ESP32
//  Place this file in the TFT_eSPI library folder
//  (Arduino/libraries/TFT_eSPI/User_Setup.h)
//  or use the PlatformIO build_flags approach.
// ============================================================

// ── Display driver ──────────────────────────────────────────
// Uncomment exactly ONE driver that matches your panel.
// ILI9488 is the most common 3.5" 320x480 module.
#define ILI9488_DRIVER        // 3.5" 320x480 (most common)
// #define ST7796_DRIVER      // alternative 3.5" 320x480

// ── Screen dimensions ───────────────────────────────────────
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// ── SPI pins (ESP32 defaults) ───────────────────────────────
// Change to match your actual wiring.
#define TFT_MOSI 23   // SPI MOSI
#define TFT_SCLK 18   // SPI clock
#define TFT_CS    5   // Chip select
#define TFT_DC    2   // Data/Command
#define TFT_RST   4   // Reset pin. Set to -1 if RST is tied to 3.3 V via a 10 kΩ resistor.

// No MISO needed (display is write-only).
// Touch controller is NOT connected / NOT used.

// ── SPI frequency ───────────────────────────────────────────
#define SPI_FREQUENCY        27000000   // SPI write clock – ILI9488 supports up to ~27 MHz write
#define SPI_READ_FREQUENCY    5000000
// #define SPI_TOUCH_FREQUENCY  2500000  // not used

// ── Font / colour configuration ─────────────────────────────
#define LOAD_GLCD    // Font  1. Original Adafruit 8 pixel font
#define LOAD_FONT2   // Font  2. Small  16 pixel font
#define LOAD_FONT4   // Font  4. Medium 26 pixel font
#define LOAD_FONT6   // Font  6. Large  48 pixel font (digits only)
#define LOAD_FONT7   // Font  7. 7-segment 48 pixel font (digits only)
#define LOAD_FONT8   // Font  8. Large  75 pixel font (digits only)
#define LOAD_GFXFF   // FreeFonts
#define SMOOTH_FONT

// ── Miscellaneous ────────────────────────────────────────────
// Uncomment if colours appear inverted on your panel
// #define TFT_INVERSION_ON
