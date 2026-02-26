#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// net_manager.h – Non-blocking WiFi + MQTT state machine
//
// EFFICIENCY NOTES
// ─────────────────
// • All reconnect logic is governed by millis() timestamps; there are NO
//   blocking delay() / while(WiFi.status() != WL_CONNECTED) loops.
// • WiFi association is started with WiFi.begin() and polled each loop()
//   tick so the ESP8266 background stack keeps running normally.
// • MQTT reconnect uses a minimum-interval guard (MQTT_RECONNECT_MS) to
//   avoid hammering the broker with rapid successive attempts.
// • Incoming MQTT messages are dispatched through a callback registered
//   once at startup; PubSubClient handles the socket non-blockingly via
//   client.loop() called every tick.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Signature for the application-level MQTT message handler.
using MqttCallback = void (*)(const char *topic, const uint8_t *payload,
                               uint16_t length);

class NetManager {
public:
    explicit NetManager(MqttCallback cb);

    // Call once from setup().
    void begin();

    // Call every iteration of loop().
    // Drives the WiFi and MQTT state machines; never blocks.
    void update();

    // Publish |payload| to |topic|. Returns false if not connected.
    bool publish(const char *topic, const char *payload, bool retain = false);

    bool isConnected() const;

private:
    WiFiClient    _wifiClient;
    PubSubClient  _mqtt;
    MqttCallback  _userCb;

    uint32_t _lastWifiAttempt  = 0;
    uint32_t _lastMqttAttempt  = 0;

    void _connectWifi();
    void _connectMqtt();

    // Thin trampoline that PubSubClient calls on incoming messages.
    // (PubSubClient uses a plain function pointer, not std::function.)
    static NetManager *_instance;
    static void _mqttDispatch(char *topic, uint8_t *payload,
                               unsigned int length);
};
