#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// config.h – Compile-time configuration for Esp.rf.443
//
// WiFi credentials and MQTT broker address live in secrets.h (not committed).
// Copy secrets.h.example → secrets.h and fill in your own values.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER

// ── Hardware pins (NodeMCU / Wemos D1 mini layout) ───────────────────────────
static const uint8_t RF_RX_PIN  = D1;   // 433 MHz receiver DATA
static const uint8_t RF_TX_PIN  = D2;   // 433 MHz transmitter DATA
static const uint8_t IR_RX_PIN  = D5;   // TSOP38238 or compatible
static const uint8_t IR_TX_PIN  = D6;   // IR LED (through transistor/resistor)

// ── WiFi ─────────────────────────────────────────────────────────────────────
// How often (ms) to retry a lost WiFi connection
static const uint32_t WIFI_RECONNECT_MS = 30000UL;

// ── MQTT ─────────────────────────────────────────────────────────────────────
static const uint16_t MQTT_PORT         = 1883;
#define MQTT_CLIENT_ID       "esp-rf-443"
#define MQTT_TOPIC_RF_RX     "home/rf/rx"
#define MQTT_TOPIC_RF_TX     "home/rf/tx"
#define MQTT_TOPIC_IR_RX     "home/ir/rx"
#define MQTT_TOPIC_IR_TX     "home/ir/tx"
// Minimum gap (ms) between MQTT reconnect attempts
static const uint32_t MQTT_RECONNECT_MS = 5000UL;

// ── RF 433 MHz ────────────────────────────────────────────────────────────────
// Size of the lock-free circular buffer shared between the ISR and loop().
// MUST be a power of 2 (see static_assert in rf_handler.cpp).
static const uint8_t  RF_QUEUE_SIZE     = 8;
// Number of times each RF code is re-transmitted to improve reliability.
static const uint8_t  RF_SEND_REPEAT    = 3;
// Defaults used when a TX command omits optional fields.
static const uint8_t  RF_DEFAULT_BITS     = 24;
static const uint8_t  RF_DEFAULT_PROTOCOL = 1;
static const uint16_t RF_DEFAULT_PULSE_US = 350;

// ── IR ────────────────────────────────────────────────────────────────────────
// Raw capture buffer (uint16_t entries, each = µs mark/space duration)
static const uint16_t IR_BUF_SIZE      = 1024;
// Default bit-length used when an IR TX command omits the bits field.
static const uint16_t IR_DEFAULT_BITS  = 32;
