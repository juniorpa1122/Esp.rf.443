# Esp.rf.443
RF 433MHz + IR + WiFi Controller with ST7789 Display

## Menu System dla ESP32 z wyświetlaczem ST7789

Projekt wykorzystuje bibliotekę **Adafruit ST7789** do wyświetlania menu na kolorowym wyświetlaczu TFT.

### Wymagane biblioteki

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library  
- Adafruit BusIO

### Schemat podłączenia (ESP32)

| Wyświetlacz ST7789 | ESP32 Pin |
|-------------------|-----------|
| TFT_CS            | GPIO 5    |
| TFT_DC            | GPIO 16   |
| TFT_RST           | GPIO 17   |
| TFT_MOSI          | GPIO 23   |
| TFT_SCLK          | GPIO 18   |
| TFT_BL            | GPIO 4    |

### Przyciski nawigacji

| Przycisk | ESP32 Pin |
|----------|-----------|
| UP       | GPIO 25   |
| DOWN     | GPIO 26   |
| OK       | GPIO 27   |

### Instalacja

#### Opcja 1: Arduino IDE
1. Otwórz Arduino IDE
2. Zainstaluj biblioteki przez Library Manager:
   - `Adafruit GFX Library`
   - `Adafruit ST7735 and ST7789 Library`
3. Otwórz plik `menu_st7789.ino`
4. Wybierz płytkę ESP32
5. Wgraj na ESP32

#### Opcja 2: PlatformIO
1. Zainstaluj PlatformIO
2. Otwórz folder projektu
3. Biblioteki zostaną automatycznie pobrane
4. Kliknij "Upload"

### Funkcje menu

- **RF 433 Nadaj** - Wysyłanie sygnału RF 433MHz
- **RF 433 Odbierz** - Odbieranie sygnału RF 433MHz
- **IR Nadaj** - Wysyłanie sygnału podczerwieni
- **IR Odbierz** - Odbieranie sygnału podczerwieni
- **WiFi Skanuj** - Skanowanie sieci WiFi
- **Ustawienia** - Konfiguracja urządzenia
- **O programie** - Informacje o programie

### Obsługa

- Przycisk **UP** - Nawigacja w górę
- Przycisk **DOWN** - Nawigacja w dół
- Przycisk **OK** - Wybór opcji
