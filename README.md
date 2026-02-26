# Esp.rf.443
RF 433MHz + IR + WiFi Controller with ESP8266/ESP32

---

## 🇵🇱 Jak pobrać kod z GitHub i wgrać na Arduino / ESP

### 1. Pobieranie kodu z GitHub

Masz dwie opcje:

**Opcja A – Pobierz jako ZIP (bez instalacji Gita)**

1. Wejdź na stronę repozytorium na GitHub.
2. Kliknij zielony przycisk **Code** → **Download ZIP**.
3. Wypakuj pobrany plik ZIP na dysku.

**Opcja B – Sklonuj repozytorium za pomocą Git lub GitHub Desktop**

- **GitHub Desktop** (zalecane dla początkujących, Windows/macOS):
  1. Pobierz i zainstaluj [GitHub Desktop](https://desktop.github.com/).
  2. Kliknij **File → Clone repository…**, wklej URL repozytorium i wybierz lokalizację.
- **Git (wiersz poleceń)**:
  ```bash
  git clone https://github.com/juniorpa1122/Esp.rf.443.git
  ```

### 2. Instalacja Arduino IDE

1. Pobierz i zainstaluj [Arduino IDE](https://www.arduino.cc/en/software) (wersja 2.x lub 1.8.x).
2. Jeśli używasz płytki **ESP8266** lub **ESP32**, dodaj obsługę tych płytek:
   - Otwórz Arduino IDE → **File → Preferences**.
   - W polu *Additional boards manager URLs* wklej:
     - ESP8266: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
     - ESP32: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Otwórz **Tools → Board → Boards Manager**, wyszukaj *esp8266* lub *esp32* i zainstaluj.

### 3. Instalacja wymaganych bibliotek

W Arduino IDE otwórz **Sketch → Include Library → Manage Libraries…** i zainstaluj potrzebne biblioteki (np. `RCSwitch`, `IRremote`, `ESP8266WiFi`).

### 4. Wgranie kodu na Arduino / ESP

1. Otwórz plik `.ino` z pobranego repozytorium w Arduino IDE.
2. Podłącz płytkę Arduino lub ESP do komputera kablem USB.
3. Wybierz odpowiednią płytkę i port: **Tools → Board** oraz **Tools → Port**.
4. Kliknij przycisk **Upload** (strzałka →) lub naciśnij `Ctrl+U`.

---

## 🇬🇧 How to Download Code from GitHub and Upload to Arduino / ESP

### 1. Download the code

**Option A – Download as ZIP (no Git required)**

1. Go to the repository page on GitHub.
2. Click the green **Code** button → **Download ZIP**.
3. Extract the ZIP archive on your computer.

**Option B – Clone with Git or GitHub Desktop**

- **GitHub Desktop** (recommended for beginners, Windows/macOS):
  1. Download and install [GitHub Desktop](https://desktop.github.com/).
  2. Click **File → Clone repository…**, paste the repository URL, and choose a local folder.
- **Git (command line)**:
  ```bash
  git clone https://github.com/juniorpa1122/Esp.rf.443.git
  ```

### 2. Install Arduino IDE

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software) (version 2.x or 1.8.x).
2. For **ESP8266** / **ESP32** boards, add board support:
   - Open **File → Preferences** and add the following to *Additional boards manager URLs*:
     - ESP8266: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
     - ESP32: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Open **Tools → Board → Boards Manager**, search for *esp8266* or *esp32*, and install.

### 3. Install required libraries

In Arduino IDE open **Sketch → Include Library → Manage Libraries…** and install any required libraries (e.g. `RCSwitch`, `IRremote`, `ESP8266WiFi`).

### 4. Upload the sketch

1. Open the `.ino` file from the downloaded repository in Arduino IDE.
2. Connect your Arduino or ESP board to the computer via USB.
3. Select the correct board and port under **Tools → Board** and **Tools → Port**.
4. Click the **Upload** button (→ arrow) or press `Ctrl+U`.
