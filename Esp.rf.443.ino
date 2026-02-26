// ─────────────────────────────────────────────────────────────────────────────
// Esp.rf.443.ino – RF 433 MHz + IR + WiFi controller (ESP8266)
//
// EFFICIENCY OVERVIEW
// ────────────────────
// The single most important rule for ESP8266 firmware is: NEVER block loop().
// Blocking starves the TCP/IP stack and causes WiFi disconnections.
//
// This sketch enforces that rule throughout:
//   • RF receive  → interrupt-driven (RCSwitch ISR) + circular buffer
//   • IR receive  → interrupt + hardware timer (IRrecv ISR)
//   • WiFi/MQTT   → millis()-based state machine; no delay() / while-loops
//   • MQTT rx     → dispatched through a callback; no polling
//   • All periodic actions use the millis()-gate pattern instead of delay()
//
// char[] buffers (not Arduino String) are used for all message formatting
// to avoid heap fragmentation from repeated String allocations.
// ─────────────────────────────────────────────────────────────────────────────

#include "config.h"
#include "rf_handler.h"
#include "ir_handler.h"
#include "net_manager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
static void onMqttMessage(const char *topic, const uint8_t *payload,
                           uint16_t length);

// ─────────────────────────────────────────────────────────────────────────────
// Module instances
// ─────────────────────────────────────────────────────────────────────────────
static RFHandler  rf;
static IRHandler  ir;
static NetManager net(onMqttMessage);

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println(F("\nEsp.rf.443 starting…"));

    rf.begin();
    ir.begin();
    net.begin();

    Serial.println(F("Ready."));
}

// ─────────────────────────────────────────────────────────────────────────────
// loop() – fully non-blocking; runs as fast as possible
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // 1. Drive network state machine (WiFi + MQTT keep-alive).
    net.update();

    // 2. Drain decoded RF codes from the circular receive buffer.
    rf.update();
    {
        RFCode code;
        while (rf.receive(code)) {
            // Format into a fixed-size stack buffer – no heap allocation.
            // Format: "<value>,<bits>,<protocol>"
            char msg[32];
            snprintf(msg, sizeof(msg), "%lu,%u,%u",
                     static_cast<unsigned long>(code.value),
                     code.bits,
                     code.protocol);
            net.publish(MQTT_TOPIC_RF_RX, msg);
            Serial.print(F("RF rx: "));
            Serial.println(msg);
        }
    }

    // 3. Check for a fully decoded IR frame.
    {
        decode_results irResult;
        if (ir.update(irResult)) {
            // typeToString() returns a flash-string; copy to stack buffer.
            char protoName[24];
            strncpy(protoName, typeToString(irResult.decode_type, false).c_str(),
                    sizeof(protoName) - 1);
            protoName[sizeof(protoName) - 1] = '\0';

            char msg[48];
            snprintf(msg, sizeof(msg), "%s,%llX,%u",
                     protoName,
                     static_cast<unsigned long long>(irResult.value),
                     irResult.bits);
            net.publish(MQTT_TOPIC_IR_RX, msg);
            Serial.print(F("IR rx: "));
            Serial.println(msg);
        }
    }

    // 4. Yield to the ESP8266 background tasks (TCP stack, OTA, watchdog).
    //    This is equivalent to delay(0) but makes intent explicit.
    yield();
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT message handler
// Called by NetManager when a message arrives on a subscribed topic.
// ─────────────────────────────────────────────────────────────────────────────
static void onMqttMessage(const char *topic, const uint8_t *payload,
                           uint16_t length) {
    // Guard against oversized payloads before copying to the stack.
    static const uint16_t MAX_PAYLOAD = 64;
    if (length >= MAX_PAYLOAD) {
        Serial.println(F("MQTT: payload too large, ignoring"));
        return;
    }

    // Copy to a null-terminated stack buffer for safe parsing.
    char buf[MAX_PAYLOAD];
    memcpy(buf, payload, length);
    buf[length] = '\0';

    Serial.print(F("MQTT rx ["));
    Serial.print(topic);
    Serial.print(F("]: "));
    Serial.println(buf);

    // ── RF transmit command ───────────────────────────────────────────────────
    // Expected format: "<value>,<bits>,<protocol>[,<pulse_us>]"
    // bits, protocol and pulse_us are optional; named defaults from config.h
    // are substituted for any field that is absent or parses as zero.
    if (strcmp(topic, MQTT_TOPIC_RF_TX) == 0) {
        unsigned long rawValue    = 0;
        uint8_t       bits        = RF_DEFAULT_BITS;
        uint8_t       protocol    = RF_DEFAULT_PROTOCOL;
        uint16_t      pulseUs     = RF_DEFAULT_PULSE_US;

        int parsed = sscanf(buf, "%lu,%hhu,%hhu,%hu",
                            &rawValue, &bits, &protocol, &pulseUs);
        if (parsed >= 1) {
            // Apply named defaults for any field not present in the payload.
            RFCode code;
            code.value    = static_cast<uint32_t>(rawValue);
            code.bits     = (bits     != 0) ? bits     : RF_DEFAULT_BITS;
            code.protocol = (protocol != 0) ? protocol : RF_DEFAULT_PROTOCOL;
            code.delay    = (pulseUs  != 0) ? pulseUs  : RF_DEFAULT_PULSE_US;
            rf.send(code);
        }
        return;
    }

    // ── IR transmit command ───────────────────────────────────────────────────
    // Expected format: "<protocol_name>,<hex_value>[,<bits>]"
    // Example: "NEC,20DF10EF,32"
    // bits is optional and defaults to IR_DEFAULT_BITS (32) when omitted.
    if (strcmp(topic, MQTT_TOPIC_IR_TX) == 0) {
        char     protoName[24] = {};
        char     hexValue[20]  = {};
        uint16_t bits          = IR_DEFAULT_BITS;

        int parsed = sscanf(buf, "%23[^,],%19[^,],%hu",
                            protoName, hexValue, &bits);
        if (parsed >= 2) {
            unsigned long long value =
                strtoull(hexValue, nullptr, 16);
            decode_type_t protocol = strToDecodeType(protoName);
            ir.send(protocol, static_cast<uint64_t>(value), bits);
        }
        return;
    }
}
