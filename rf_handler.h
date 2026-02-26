#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// rf_handler.h – Interrupt-driven RF 433 MHz receive + transmit
//
// EFFICIENCY NOTES
// ─────────────────
// • The receive side is entirely interrupt-driven (RCSwitch attaches an ISR to
//   the DATA pin).  No CPU cycles are wasted polling the pin inside loop().
// • Decoded codes are pushed into a small circular buffer from the ISR so that
//   loop() can process them at its leisure without missing a burst.
// • All timing is done with millis() / micros(); there are NO blocking delay()
//   calls anywhere in this module.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <RCSwitch.h>
#include "config.h"

// A decoded RF code together with its metadata.
struct RFCode {
    uint32_t value;
    uint8_t  bits;
    uint16_t delay;   // µs pulse length reported by RCSwitch
    uint8_t  protocol;
};

class RFHandler {
public:
    // Call once from setup().
    void begin();

    // Call every iteration of loop() – moves newly decoded codes from the
    // RCSwitch object into the circular receive buffer.
    void update();

    // Returns true and fills |out| if a code is waiting in the buffer.
    bool receive(RFCode &out);

    // Transmit |code| using the pre-configured DATA pin.
    // Uses RF_SEND_REPEAT retransmissions for reliability.
    void send(const RFCode &code);

private:
    RCSwitch _rc;

    // Lock-free single-producer / single-consumer circular buffer.
    // The ISR (producer) runs inside RCSwitch; loop() is the consumer.
    volatile uint8_t _head = 0;   // written by producer (ISR)
    volatile uint8_t _tail = 0;   // written by consumer (loop)
    RFCode _buf[RF_QUEUE_SIZE];

    void _enqueue(const RFCode &code);
};
