# ESP32 433MHz Scanner & Transmitter

Kompletny projekt do skanowania i nadawania sygnalow 433MHz na ESP32.

## Wymagania

### Biblioteka RCSwitch
Zainstaluj biblioteke RCSwitch przez Arduino IDE:
1. Szkic -> Include Library -> Manage Libraries
2. Wyszukaj "RCSwitch" i zainstaluj

Lub recznie:
```bash
cd ~/Arduino/libraries
git clone https://github.com/sui77/rc-switch.git RCSwitch
```

## Podlaczenie

### Modul nadawczy (FS1000A / MX-FS-5V)
| ESP32 | Modul 433MHz |
|-------|--------------|
| GPIO 2 (D2)  | DATA |
| 5V          | VCC  |
| GND         | GND  |

### Modul odbiorczy (XY-MK-5V / SRX882)
| ESP32 | Modul 433MHz |
|-------|--------------|
| GPIO 4 (D4)  | DATA |
| 5V          | VCC  |
| GND         | GND  |

## Uzycie

### Komendy dostepne przez Serial Monitor (115200 baud):

1. **Skanowanie sygnalow:**
   - Wlacz/wylacz skanowanie: `scan`
   - Automatycznie skanuje i wyswietla odebrane kody

2. **Wysylanie sygnalow:**
   ```
   send <kod> <dlugosc_bitow>
   ```
   Przyklady:
   - `send 5393 24`
   - `send 43794 16`
   - `send 12345 24`

3. **Powtorzenie ostatniego kodu:**
   - `repeat`

4. **Pomoc i przyklady:**
   - `examples` lub `help`

## Uwagi

- Polacz antene do modulow (opcjonalnie ale zalecane)
- Zasil ESP32 z stabilnego zrodla 5V
- Skaner dziala na przerwaniach - nie blokuje glownego loop
- Jezeli nie odbierasz sygnalow, sprawdz polaczenie i zasilanie
