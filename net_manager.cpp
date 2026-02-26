// ─────────────────────────────────────────────────────────────────────────────
// net_manager.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "net_manager.h"

NetManager *NetManager::_instance = nullptr;

NetManager::NetManager(MqttCallback cb)
    : _mqtt(_wifiClient), _userCb(cb)
{
    _instance = this;
}

void NetManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);   // let the SDK reconnect after transient drops
    WiFi.persistent(false);        // avoid wearing out the flash with every connect
    _mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    _mqtt.setCallback(_mqttDispatch);
    _connectWifi();
}

void NetManager::update() {
    // ── WiFi ──────────────────────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        uint32_t now = millis();
        // Attempt reconnect only after the guard interval to avoid spamming.
        if (now - _lastWifiAttempt >= WIFI_RECONNECT_MS) {
            _connectWifi();
        }
        return;   // MQTT can't work without WiFi
    }

    // ── MQTT ─────────────────────────────────────────────────────────────────
    if (!_mqtt.connected()) {
        uint32_t now = millis();
        if (now - _lastMqttAttempt >= MQTT_RECONNECT_MS) {
            _connectMqtt();
        }
        return;
    }

    // Let PubSubClient drive its socket in a non-blocking fashion.
    _mqtt.loop();
}

bool NetManager::publish(const char *topic, const char *payload, bool retain) {
    if (!_mqtt.connected()) {
        return false;
    }
    return _mqtt.publish(topic, payload, retain);
}

bool NetManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED && _mqtt.connected();
}

// ─── private ─────────────────────────────────────────────────────────────────

void NetManager::_connectWifi() {
    _lastWifiAttempt = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // We do NOT wait here – update() polls WiFi.status() each loop tick.
}

void NetManager::_connectMqtt() {
    _lastMqttAttempt = millis();
    // connect() is a blocking call but completes in one TCP RTT (~few ms on LAN).
    if (_mqtt.connect(MQTT_CLIENT_ID)) {
        _mqtt.subscribe(MQTT_TOPIC_RF_TX);
        _mqtt.subscribe(MQTT_TOPIC_IR_TX);
    }
}

void NetManager::_mqttDispatch(char *topic, uint8_t *payload,
                                unsigned int length) {
    if (_instance && _instance->_userCb) {
        _instance->_userCb(topic, payload, static_cast<uint16_t>(length));
    }
}
