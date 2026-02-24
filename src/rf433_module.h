#pragma once
/*
 * rf433_module.h
 * --------------
 * 433 MHz OOK scan and replay module for ESP32.
 *
 * Library: rc-switch (sui77/rc-switch)
 *
 * Pin assignments (change to match your wiring):
 *   RF_RX_PIN – GPIO connected to the DATA/OUT pin of a 433 MHz superhet receiver
 *   RF_TX_PIN – GPIO connected to the DATA pin of a 433 MHz transmitter module
 */

#ifndef RF433_MODULE_H
#define RF433_MODULE_H

#include <Arduino.h>
#include <RCSwitch.h>

// ─── Pin definitions ─────────────────────────────────────────────────────────
#define RF_RX_PIN  34   // 433 MHz receiver data (input-only GPIO)
#define RF_TX_PIN  12   // 433 MHz transmitter data

// ─── Max number of stored codes ───────────────────────────────────────────────
#define RF_MAX_STORED  16

// ─── Internal objects ─────────────────────────────────────────────────────────
static RCSwitch rcSwitch;

// Ring-buffer of captured codes
struct RF433Code {
    unsigned long value;
    unsigned int  bitLength;
    unsigned int  protocol;
    unsigned int  delay;      // pulse-length µs
};

static RF433Code rf433Stored[RF_MAX_STORED];
static uint8_t   rf433Count = 0;

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * @brief  Initialise RC-Switch for both receive and transmit.
 *         Call once from setup().
 */
inline void rf433_init() {
    rcSwitch.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
    rcSwitch.enableTransmit(RF_TX_PIN);
    rcSwitch.setRepeatTransmit(5);   // retransmit 5× for reliability
    rf433Count = 0;
    Serial.println(F("[RF433] Initialised."));
}

/**
 * @brief  Poll for a newly received 433 MHz packet.
 *         Returns true when a new code has been decoded.
 *         The code is automatically stored in rf433Stored[].
 */
inline bool rf433_scan() {
    if (!rcSwitch.available()) return false;

    RF433Code code;
    code.value     = rcSwitch.getReceivedValue();
    code.bitLength = rcSwitch.getReceivedBitlength();
    code.protocol  = rcSwitch.getReceivedProtocol();
    code.delay     = rcSwitch.getReceivedDelay();

    rcSwitch.resetAvailable();

    if (code.value == 0) {
        Serial.println(F("[RF433] Unknown encoding received (raw)."));
        return false;
    }

    Serial.printf("[RF433] Code: %lu  bits=%u  proto=%u  delay=%uµs\n",
                  code.value, code.bitLength, code.protocol, code.delay);

    // Store (circular: overwrite oldest when full)
    if (rf433Count < RF_MAX_STORED) {
        rf433Stored[rf433Count++] = code;
    } else {
        // Shift left and overwrite last slot
        memmove(&rf433Stored[0], &rf433Stored[1],
                (RF_MAX_STORED - 1) * sizeof(RF433Code));
        rf433Stored[RF_MAX_STORED - 1] = code;
    }
    return true;
}

/**
 * @brief  Replay the most-recently captured 433 MHz code.
 */
inline void rf433_replay_last() {
    if (rf433Count == 0) {
        Serial.println(F("[RF433] No code stored yet."));
        return;
    }
    uint8_t idx = (rf433Count < RF_MAX_STORED) ? rf433Count - 1
                                                : RF_MAX_STORED - 1;
    const RF433Code &c = rf433Stored[idx];
    Serial.printf("[RF433] Replaying code %lu (proto=%u, bits=%u, delay=%u)\n",
                  c.value, c.protocol, c.bitLength, c.delay);

    rcSwitch.disableReceive();  // avoid self-reception during TX
    rcSwitch.setProtocol(c.protocol);
    rcSwitch.setPulseLength(c.delay);
    rcSwitch.send(c.value, c.bitLength);
    rcSwitch.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
}

/**
 * @brief  Replay a stored code by index.
 * @param  idx  0-based index (0 = oldest stored).
 */
inline void rf433_replay(uint8_t idx) {
    if (idx >= rf433Count) return;
    const RF433Code &c = rf433Stored[idx];
    rcSwitch.disableReceive();
    rcSwitch.setProtocol(c.protocol);
    rcSwitch.setPulseLength(c.delay);
    rcSwitch.send(c.value, c.bitLength);
    rcSwitch.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
}

/**
 * @brief  Return a one-line summary of stored code #idx.
 */
inline String rf433_code_str(uint8_t idx) {
    if (idx >= rf433Count) return F("empty");
    const RF433Code &c = rf433Stored[idx];
    return String(c.value) + " P" + c.protocol + " " + c.bitLength + "b";
}

#endif // RF433_MODULE_H
