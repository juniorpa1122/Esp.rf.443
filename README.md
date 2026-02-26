# Esp.rf.443

RF 433 MHz + IR + WiFi Controller firmware for ESP8266 (NodeMCU / Wemos D1 mini).

Receives and transmits RF 433 MHz codes and IR frames, then bridges them to your
home-automation system via MQTT over WiFi.

---

## Hardware

| Signal               | Default pin |
|----------------------|-------------|
| RF 433 MHz receive   | D1 (GPIO5)  |
| RF 433 MHz transmit  | D2 (GPIO4)  |
| IR receive (TSOP)    | D5 (GPIO14) |
| IR transmit (LED)    | D6 (GPIO12) |

Override any pin in `config.h`.

---

## Dependencies (Arduino Library Manager)

| Library              | Tested version |
|----------------------|---------------|
| RCSwitch             | 2.6.4         |
| IRremoteESP8266      | 2.8.6         |
| PubSubClient         | 2.8           |

---

## Building

1. Install the **ESP8266 Arduino core** via Boards Manager
   (`http://arduino.esp8266.com/stable/package_esp8266com_index.json`).
2. Install the three libraries above via **Sketch → Include Library →
   Manage Libraries**.
3. Copy **`secrets.h.example`** → **`secrets.h`** and fill in your SSID,
   password, and MQTT broker IP.  `secrets.h` is listed in `.gitignore` and
   will never be committed.
4. Edit any other settings in `config.h` (pins, intervals, topic names).
5. Select **NodeMCU 1.0 (ESP-12E Module)** (or your board) and upload.

---

## MQTT API

### Received codes (published by the device)

| Topic        | Payload format                         | Example              |
|--------------|----------------------------------------|----------------------|
| `home/rf/rx` | `<value>,<bits>,<protocol>`            | `5592405,24,1`       |
| `home/ir/rx` | `<protocol>,<hex_value>,<bits>`        | `NEC,20DF10EF,32`    |

### Transmit commands (subscribed by the device)

| Topic        | Payload format                                | Example                    |
|--------------|-----------------------------------------------|----------------------------|
| `home/rf/tx` | `<value>,<bits>,<protocol>[,<pulse_us>]`      | `5592405,24,1,350`         |
| `home/ir/tx` | `<protocol>,<hex_value>,<bits>`               | `NEC,20DF10EF,32`          |

---

## Performance / Efficiency design notes

The single most important rule for ESP8266 firmware is **never block `loop()`**.
Blocking starves the TCP/IP background stack and causes WiFi disconnections.
This firmware enforces that rule throughout:

| Concern | Naive approach | This firmware |
|---|---|---|
| RF receive | Poll DATA pin in a tight loop | Interrupt-driven (RCSwitch ISR) + lock-free circular buffer |
| IR receive | Busy-wait for edges | Interrupt + hardware timer (IRrecv ISR) |
| WiFi reconnect | `while (WiFi.status() != WL_CONNECTED) delay(500)` | `millis()`-gated state machine; never blocks |
| MQTT reconnect | `while (!client.connect(…)) delay(1000)` | Minimum-interval guard (`MQTT_RECONNECT_MS`); never blocks |
| String building | Arduino `String` heap allocations | Fixed-size `char[]` stack buffers + `snprintf` |
| Periodic yield | `delay(x)` between operations | Explicit `yield()` at end of `loop()` |
