#pragma once
/*
 * scanner_module.h
 * ----------------
 * General-purpose passive signal-presence detector for ESP32.
 *
 * What it does (passive / non-intrusive – NO hacking / jamming):
 *   • Monitors the 433 MHz receiver for ANY carrier activity
 *     (even if the packet cannot be decoded by rc-switch).
 *   • Monitors the IR receiver for any burst of IR light.
 *   • Reports signal presence on the serial port and provides
 *     helper functions for the UI.
 *
 * No signals are transmitted in this mode.
 * No decoding beyond "signal present / not present" is performed
 * unless the library decodes it voluntarily.
 */

#ifndef SCANNER_MODULE_H
#define SCANNER_MODULE_H

#include <Arduino.h>

// ─── Pin definitions ─────────────────────────────────────────────────────────
// These are the same DATA/OUT pins used by the RF and IR modules.
// The scanner reads them as plain digital inputs.
#define SCAN_RF_PIN   34    // 433 MHz receiver DATA pin
#define SCAN_IR_PIN   35    // IR receiver DATA pin (active-low)

// ─── Detection hysteresis / timing ───────────────────────────────────────────
#define SCAN_SAMPLE_MS       10    // Polling interval in ms
#define SCAN_ACTIVITY_HOLD   500   // Keep "active" flag for at least this long (ms)

// ─── State ────────────────────────────────────────────────────────────────────
static bool     scanRfActive      = false;
static bool     scanIrActive      = false;
static uint32_t scanRfLastSeen    = 0;
static uint32_t scanIrLastSeen    = 0;
static uint32_t scanNextPollMs    = 0;

// Counters since last reset
static uint32_t scanRfEventCount  = 0;
static uint32_t scanIrEventCount  = 0;

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * @brief  Initialise GPIO inputs for the scanner.
 *         The RC-Switch and IRremote libraries may still own the same
 *         physical GPIO via their internal interrupt handlers; here we only
 *         configure pull-ups so idle state is known.
 *         Call once from setup() AFTER ir_init() and rf433_init().
 */
inline void scanner_init() {
    // Both receivers have active-low DATA outputs and idle HIGH.
    // No additional pinMode needed if the libraries already claimed them;
    // we use digitalRead() which works regardless.
    scanRfActive     = false;
    scanIrActive     = false;
    scanRfLastSeen   = 0;
    scanIrLastSeen   = 0;
    scanRfEventCount = 0;
    scanIrEventCount = 0;
    scanNextPollMs   = 0;
    Serial.println(F("[SCAN] General scanner initialised."));
}

/**
 * @brief  Reset the event counters (call when entering the scanner screen).
 */
inline void scanner_reset() {
    scanRfEventCount = 0;
    scanIrEventCount = 0;
    scanRfActive     = false;
    scanIrActive     = false;
}

/**
 * @brief  Poll sensors – call repeatedly from loop().
 *         Returns true if any state changed (useful for redrawing the UI).
 */
inline bool scanner_poll() {
    uint32_t now = millis();
    if (now < scanNextPollMs) return false;
    scanNextPollMs = now + SCAN_SAMPLE_MS;

    bool changed = false;

    // ── 433 MHz ──────────────────────────────────────────────────────────────
    // The SRX882 / RXB6 and similar receivers output a logic HIGH when they
    // detect carrier energy, even for signals they cannot decode.
    // (RC-Switch uses an interrupt; reading the pin directly still works.)
    int rfLevel = digitalRead(SCAN_RF_PIN);
    if (rfLevel == HIGH) {
        scanRfLastSeen = now;
        if (!scanRfActive) {
            scanRfActive = true;
            scanRfEventCount++;
            changed = true;
            Serial.printf("[SCAN] RF 433 activity detected  (event #%lu)\n",
                          scanRfEventCount);
        }
    } else {
        if (scanRfActive && (now - scanRfLastSeen > SCAN_ACTIVITY_HOLD)) {
            scanRfActive = false;
            changed = true;
        }
    }

    // ── IR ───────────────────────────────────────────────────────────────────
    // TSOP receivers output LOW during an IR burst (active-low).
    int irLevel = digitalRead(SCAN_IR_PIN);
    if (irLevel == LOW) {
        scanIrLastSeen = now;
        if (!scanIrActive) {
            scanIrActive = true;
            scanIrEventCount++;
            changed = true;
            Serial.printf("[SCAN] IR activity detected  (event #%lu)\n",
                          scanIrEventCount);
        }
    } else {
        if (scanIrActive && (now - scanIrLastSeen > SCAN_ACTIVITY_HOLD)) {
            scanIrActive = false;
            changed = true;
        }
    }

    return changed;
}

// ── Accessor helpers for the UI ───────────────────────────────────────────────

inline bool     scanner_rf_active()      { return scanRfActive; }
inline bool     scanner_ir_active()      { return scanIrActive; }
inline uint32_t scanner_rf_event_count() { return scanRfEventCount; }
inline uint32_t scanner_ir_event_count() { return scanIrEventCount; }

#endif // SCANNER_MODULE_H
