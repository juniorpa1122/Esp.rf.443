#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ir_handler.h – Non-blocking IR receive / transmit
//
// EFFICIENCY NOTES
// ─────────────────
// • IRrecv uses an ISR + hardware timer so loop() never spins waiting for
//   signal edges.
// • Decoded results are processed only when IRrecv::decode() returns true,
//   keeping CPU overhead proportional to actual traffic.
// • The transmit path encodes and replays raw timing arrays; no busy-wait
//   is introduced beyond the short ~40–150 ms transmission itself.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "config.h"

class IRHandler {
public:
    // Call once from setup().
    void begin();

    // Call every iteration of loop().
    // Returns true if a complete IR frame was just decoded; fills |result|.
    bool update(decode_results &result);

    // Re-transmit a previously captured raw frame.
    void sendRaw(const uint16_t *data, uint16_t len, uint16_t hz = 38);

    // Transmit a known-protocol code (e.g. decoded from a previous result).
    void send(decode_type_t protocol, uint64_t data, uint16_t bits,
              uint16_t repeat = 0);

private:
    IRrecv _recv{IR_RX_PIN, IR_BUF_SIZE, 15, true};
    IRsend _send{IR_TX_PIN};
};
