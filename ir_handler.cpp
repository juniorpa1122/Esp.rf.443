// ─────────────────────────────────────────────────────────────────────────────
// ir_handler.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "ir_handler.h"

void IRHandler::begin() {
    _recv.enableIRIn();
    _send.begin();
}

bool IRHandler::update(decode_results &result) {
    if (!_recv.decode(&result)) {
        return false;
    }
    // Resume listening immediately so we never block the receiver.
    _recv.resume();
    return true;
}

void IRHandler::sendRaw(const uint16_t *data, uint16_t len, uint16_t hz) {
    _send.sendRaw(data, len, hz);
}

void IRHandler::send(decode_type_t protocol, uint64_t data, uint16_t bits,
                     uint16_t repeat) {
    _send.send(protocol, data, bits, repeat);
}
