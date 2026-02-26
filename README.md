# Esp.rf.443 — RF 433 MHz Signal Scanner & Tester

Urządzenie na ESP8266/ESP32, które:
- **Skanuje** (odbiera) sygnały RF 433 MHz i wyświetla je w przeglądarce.
- **Nadaje** różne kody testowe w wielu protokołach RF 433 MHz.
- Udostępnia **panel webowy** przez własną sieć WiFi (Access Point).

A device based on ESP8266/ESP32 that:
- **Scans** (receives) RF 433 MHz signals and shows them in a browser.
- **Transmits** various test codes across multiple RF 433 MHz protocols.
- Exposes a **web dashboard** via its own WiFi Access Point.

---

## Wymagane biblioteki / Required libraries

| Biblioteka | Link |
|---|---|
| **RCSwitch** | https://github.com/sui77/rc-switch |
| ESP8266 Arduino core **or** ESP32 Arduino core | https://github.com/esp8266/Arduino / https://github.com/espressif/arduino-esp32 |

Zainstaluj przez Arduino IDE → *Sketch → Include Library → Manage Libraries* → szukaj **RCSwitch**.

Install via Arduino IDE → *Sketch → Include Library → Manage Libraries* → search **RCSwitch**.

---

## Podłączenie / Wiring

```
ESP8266 (NodeMCU)        Moduł RF / RF module
─────────────────        ─────────────────────
GPIO 4  (D2)  ────────── Receiver  DATA
GPIO 2  (D4)  ────────── Transmitter DATA
5V            ────────── VCC  (oba moduły / both modules)
GND           ────────── GND  (oba moduły / both modules)
```

> Polecane moduły / Recommended modules:
> - Odbiornik / Receiver: **XY-MK-5V** or **RXB6**
> - Nadajnik / Transmitter: **FS1000A**

Piny można zmienić w kodzie (`RF_RX_PIN`, `RF_TX_PIN`).  
Pins can be changed in the sketch (`RF_RX_PIN`, `RF_TX_PIN`).

---

## Użytkowanie / Usage

1. Wgraj szkic `rf_scanner/rf_scanner.ino` do ESP8266/ESP32.  
   Upload the sketch `rf_scanner/rf_scanner.ino` to your ESP8266/ESP32.

2. Połącz się z siecią WiFi **RF-Scanner** (hasło: **12345678**).  
   Connect to the WiFi network **RF-Scanner** (password: **12345678**).

3. Otwórz w przeglądarce: **http://192.168.4.1**  
   Open in browser: **http://192.168.4.1**

### Panel webowy / Web dashboard

| Sekcja | Opis |
|---|---|
| **Scan mode (RX)** | Odbiera sygnały RF i wyświetla w tabeli / Receives RF signals and shows them in a table |
| **Test-TX mode** | Wysyła wybrane sygnały testowe / Sends selected test signals |
| **Received signals log** | Historia odebranych sygnałów (wartość, hex, bity, protokół) / History of received signals |
| **Send a test signal** | Wysyła jeden z 10 presetów lub własny kod / Sends one of 10 presets or a custom code |
| **Custom signal** | Własny kod DEC, liczba bitów, protokół / Custom code DEC, bit-length, protocol |

### Jak testować czy urządzenie nadaje / How to test if a device is transmitting

1. Przełącz panel na **Scan mode (RX)**.
2. Naciśnij przycisk pilota lub uruchom urządzenie RF.
3. Odebrane sygnały pojawią się w tabeli z wartością, protokołem i długością impulsu.
4. Naciśnij **Refresh** aby odświeżyć tabelę, lub użyj endpointu `/json` do odpytywania.

1. Switch the dashboard to **Scan mode (RX)**.
2. Press your remote-control button or activate the RF device under test.
3. Captured signals appear in the table with value, protocol, and pulse length.
4. Press **Refresh** to update, or poll the `/json` endpoint programmatically.

---

## Protokoły / Protocols

RCSwitch obsługuje protokoły 1–12, obejmujące większość pilotów gniazdek, rolet, bram i alarmów 433 MHz.

RCSwitch supports protocols 1–12, covering most 433 MHz socket, shutter, gate and alarm remotes.

---

## Schemat działania / How it works

```
┌─────────────────────────────────────────┐
│  ESP8266 / ESP32                        │
│                                         │
│  RF Receiver ──► RCSwitch (RX)          │
│                      │                  │
│                      ▼                  │
│               Ring-buffer log           │
│                      │                  │
│                      ▼                  │
│            HTTP web dashboard           │
│                      │                  │
│                      ▼                  │
│  RF Transmitter ◄── RCSwitch (TX)       │
│  (presets / custom codes)               │
└─────────────────────────────────────────┘
```

## Licencja / License

MIT
