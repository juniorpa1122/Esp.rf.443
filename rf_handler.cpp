// ─────────────────────────────────────────────────────────────────────────────
// rf_handler.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "rf_handler.h"

// RF_QUEUE_SIZE must be a power of 2 so we can use fast bitwise masking
// instead of modulo in the ISR-side enqueue path.
static_assert((RF_QUEUE_SIZE & (RF_QUEUE_SIZE - 1)) == 0,
              "RF_QUEUE_SIZE must be a power of 2");

// Bitmask used in place of `% RF_QUEUE_SIZE` – valid only for power-of-2 sizes.
static const uint8_t RF_QUEUE_MASK = RF_QUEUE_SIZE - 1;

void RFHandler::begin() {
    _rc.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
    _rc.enableTransmit(RF_TX_PIN);
    _rc.setRepeatTransmit(RF_SEND_REPEAT);
}

void RFHandler::update() {
    // Called from loop() – drain all newly available codes into the ring buffer.
    while (_rc.available()) {
        RFCode code;
        code.value    = static_cast<uint32_t>(_rc.getReceivedValue());
        code.bits     = static_cast<uint8_t>(_rc.getReceivedBitlength());
        code.delay    = static_cast<uint16_t>(_rc.getReceivedDelay());
        code.protocol = static_cast<uint8_t>(_rc.getReceivedProtocol());
        _enqueue(code);
        _rc.resetAvailable();
    }
}

bool RFHandler::receive(RFCode &out) {
    // Consumer side – safe to call only from loop() (not from an ISR).
    uint8_t head = _head;   // single volatile read
    if (head == _tail) {
        return false;       // buffer empty
    }
    out   = _buf[_tail];
    _tail = (_tail + 1) & RF_QUEUE_MASK;   // bitwise mask instead of modulo
    return true;
}

void RFHandler::send(const RFCode &code) {
    // RCSwitch::send() is synchronous but intentionally blocking only during
    // the brief transmission window (~100 ms for 3 repeats of a 24-bit code).
    _rc.setProtocol(code.protocol);
    _rc.setPulseLength(code.delay);
    _rc.send(code.value, code.bits);
}

// ─── private ─────────────────────────────────────────────────────────────────

void RFHandler::_enqueue(const RFCode &code) {
    uint8_t next = (_head + 1) & RF_QUEUE_MASK;   // bitwise mask instead of modulo
    if (next == _tail) {
        // Buffer full – drop the oldest entry to make room for the latest.
        _tail = (_tail + 1) & RF_QUEUE_MASK;
    }
    _buf[_head] = code;
    _head = next;           // publish to consumer with a single volatile write
}
