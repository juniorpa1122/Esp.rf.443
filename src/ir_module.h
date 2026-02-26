#pragma once
/*
 * ir_module.h
 * -----------
 * IR send / receive module for ESP32.
 *
 * Library: IRremoteESP8266 (crankyoldgit/IRremoteESP8266)
 *
 * Pin assignments (change to match your wiring):
 *   IR_RX_PIN  – GPIO connected to the TSOP IR receiver data pin
 *   IR_TX_PIN  – GPIO connected to the IR LED (via NPN transistor / driver)
 */

#ifndef IR_MODULE_H
#define IR_MODULE_H

#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <IRac.h>          // optional: A/C helpers
#include <IRtext.h>

// ─── Pin definitions ─────────────────────────────────────────────────────────
#define IR_RX_PIN  35   // Receiver data (TSOP38238 or similar)
#define IR_TX_PIN  14   // Transmitter LED driver

// ─── Receiver buffer size ─────────────────────────────────────────────────────
#define IR_BUF_SIZE 1024

// ─── Internal objects ─────────────────────────────────────────────────────────
static IRrecv irRecv(IR_RX_PIN, IR_BUF_SIZE, 15, true);
static IRsend irSend(IR_TX_PIN);

// Last decoded result (populated by ir_record())
static decode_results irLastResult;
static bool           irHasSignal = false;

// ─── Known TV brand codes (NEC protocol examples) ────────────────────────────
// Format: { label, protocol, address, command, bits }
struct TVCode {
    const char *label;
    decode_type_t protocol;
    uint64_t      data;
    uint16_t      bits;
    uint16_t      repeats;
};

// A small selection of common power-on codes (extend as needed)
static const TVCode TV_CODES[] = {
    { "Samsung POWER", SAMSUNG,   0xE0E040BF, 32, 0 },
    { "LG POWER",      LG,        0x20DF10EF, 32, 0 },
    { "Sony POWER",    SONY,      0xA90,      12, 2 },
    { "Philips POWER", RC5,       0x100C,     12, 0 },
    { "Panasonic PWR", PANASONIC, 0x400401100101, 48, 0 },
    { "Sharp POWER",   SHARP,     0xABCDE,    15, 0 },
    { "Pioneer POWER", NEC,       0xA55A48B7, 32, 0 },
    { "Denon POWER",   DENON,     0x2278,     15, 0 },
};
static const uint8_t TV_CODES_COUNT = sizeof(TV_CODES) / sizeof(TV_CODES[0]);

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * @brief  Initialise IR transmitter and receiver.
 *         Call once from setup().
 */
inline void ir_init() {
    irSend.begin();
    irRecv.setUnknownThreshold(12);
    irRecv.enableIRIn();
    irHasSignal = false;
    Serial.println(F("[IR] Initialised."));
}

/**
 * @brief  Poll the receiver for an incoming IR signal.
 *         Returns true when a new signal has been captured.
 *         The decoded result is stored in irLastResult.
 */
inline bool ir_record() {
    if (irRecv.decode(&irLastResult)) {
        irHasSignal = true;
        Serial.printf("[IR] Captured protocol=%s  bits=%u  value=0x%llX\n",
                      typeToString(irLastResult.decode_type, false).c_str(),
                      irLastResult.bits,
                      irLastResult.value);
        irRecv.resume();
        return true;
    }
    return false;
}

/**
 * @brief  Retransmit the last captured signal.
 */
inline void ir_replay() {
    if (!irHasSignal) {
        Serial.println(F("[IR] No signal recorded yet."));
        return;
    }
    Serial.printf("[IR] Replaying protocol=%s  bits=%u  value=0x%llX\n",
                  typeToString(irLastResult.decode_type, false).c_str(),
                  irLastResult.bits,
                  irLastResult.value);
    irSend.send(irLastResult.decode_type,
                irLastResult.value,
                irLastResult.bits,
                0);
}

/**
 * @brief  Send a pre-defined TV brand code by index.
 * @param  idx  Index into TV_CODES[] array.
 */
inline void ir_send_tv_code(uint8_t idx) {
    if (idx >= TV_CODES_COUNT) return;
    const TVCode &c = TV_CODES[idx];
    Serial.printf("[IR] Sending TV code: %s\n", c.label);
    irSend.send(c.protocol, c.data, c.bits, c.repeats);
}

/**
 * @brief  Return display string for the last received signal (for the UI).
 */
inline String ir_last_signal_str() {
    if (!irHasSignal) return F("No signal");
    return typeToString(irLastResult.decode_type, false)
         + " 0x" + uint64ToString(irLastResult.value, 16)
         + " (" + String(irLastResult.bits) + "b)";
}

#endif // IR_MODULE_H
