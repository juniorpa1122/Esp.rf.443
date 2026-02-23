# ESP32 Multi-Tool

Kompletny firmware dla **ESP32** obsługujący:

| #  | Funkcja                  | Biblioteka               |
|----|--------------------------|--------------------------|
| 1  | BLE Scan                 | ESP32 BLE Arduino (wbudowana) |
| 2  | WiFi Scan                | WiFi (wbudowana)         |
| 3  | 433 MHz RF Odbieranie    | [RCSwitch](https://github.com/sui77/rc-switch) |
| 4  | 433 MHz RF Nadawanie     | [RCSwitch](https://github.com/sui77/rc-switch) |
| 5  | IR Odbieranie            | [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) |
| 6  | IR Nadawanie             | [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) |
| 7  | GPS                      | [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus) |

---

## Schemat podłączenia (Wiring)

```
ESP32 GPIO  │  Moduł/Urządzenie
────────────┼──────────────────────────────────────
GPIO 19     │  RF 433 MHz – Receiver DATA
GPIO 18     │  RF 433 MHz – Transmitter DATA
GPIO 34     │  IR Receiver OUT (np. TSOP4838)
GPIO  4     │  IR LED (przez tranzystor/driver)
GPIO 16     │  GPS – Module TX → ESP32 RX2
GPIO 17     │  GPS – Module RX → ESP32 TX2
3.3 V / 5 V │  Zasilanie modułów (wg specyfikacji)
GND         │  Masa wspólna
```

> **Uwaga:** GPIO 34 jest tylko wejściem (input-only) — idealny dla odbiornika IR.
> Moduł RF 433 MHz Receiver zasilaj z 5 V; DATA podłącz bezpośrednio do GPIO 19 (toleruje 3.3 V logic).
> GPS (np. NEO-6M) zasilaj z 3.3 V lub 5 V; logika 3.3 V — można podłączyć bezpośrednio.

---

## Wymagania

- [PlatformIO](https://platformio.org/) (VS Code extension lub CLI)
- Board: **ESP32 DevKit** (lub kompatybilny)
- Framework: Arduino

Biblioteki są instalowane automatycznie przez PlatformIO na podstawie `platformio.ini`.

---

## Instalacja i uruchomienie

```bash
# Klonowanie repozytorium
git clone https://github.com/juniorpa1122/Esp.rf.443.git
cd Esp.rf.443

# Budowanie i wgrywanie przez PlatformIO CLI
pio run --target upload

# Monitor szeregowy (115200 baud)
pio device monitor
```

Lub użyj **VS Code** z rozszerzeniem PlatformIO — kliknij ▶ Upload, a następnie 🔌 Monitor.

---

## Obsługa (Serial Monitor – 115200 baud)

Po uruchomieniu wyświetla się menu:

```
╔══════════════════════════════╗
║     ESP32 Multi-Tool v1.0   ║
╠══════════════════════════════╣
║  1. BLE Scan                ║
║  2. WiFi Scan               ║
║  3. 433 MHz RF Receive      ║
║  4. 433 MHz RF Send         ║
║  5. IR Receive              ║
║  6. IR Send                 ║
║  7. GPS                     ║
╚══════════════════════════════╝
Select (1-7):
```

Wpisz numer i naciśnij **Enter**.  
W trybach ciągłego odbioru (3, 5, 7) wpisz **`0`** + Enter, aby wrócić do menu.

### Tryb 4 – Wysyłanie RF 433 MHz

```
Enter decimal code to send (e.g. 5592405): 5592405
Enter bit length (default 24): 24
[RF433] Sent code 5592405 (24 bits).
```

Kody możesz poznać używając trybu 3 (Receive).

### Tryb 6 – Wysyłanie IR

Wybierz protokół (NEC, Samsung, Sony, RC5) lub wyślij surowe impulsy (Raw).

```
Select protocol (1-5): 1
Enter hex value (e.g. 20DF10EF): 20DF10EF
Enter bit length (default 32): 32
[IR] NEC sent: 0x20DF10EF (32 bits)
```

---

## Struktura projektu

```
Esp.rf.443/
├── platformio.ini   # Konfiguracja PlatformIO + zależności
├── src/
│   └── main.cpp     # Główny kod ESP32
└── README.md
```

---

## Licencja

MIT
