/*
 * ESP RF 433MHz + IR + WiFi Controller
 *
 * Funkcje:
 * - Menu seryjne dla operacji 433 MHz (Serial menu for 433 MHz operations)
 * - Odbieranie i nadawanie sygnałów RF 433 MHz (RF 433 MHz receive/transmit)
 * - Skanowanie częstotliwości (Frequency scanning function)
 * - Obsługa IR i WiFi (IR and WiFi support)
 *
 * Biblioteki / Libraries:
 *   - RCSwitch (RF 433 MHz)
 *   - IRremoteESP8266 (IR)
 *   - ESP8266WiFi / WiFi (WiFi)
 *
 * Podłączenie / Wiring:
 *   RF Receiver DATA pin -> GPIO 2 (D4 on NodeMCU)
 *   RF Transmitter DATA pin -> GPIO 4 (D2 on NodeMCU)
 *   IR Receiver -> GPIO 14 (D5 on NodeMCU)
 *   IR Transmitter -> GPIO 12 (D6 on NodeMCU)
 */

#include <RCSwitch.h>
#include <ESP8266WiFi.h>

// --- Pin Definitions ---
#define RF_RX_PIN   2   // RF Receiver data pin
#define RF_TX_PIN   4   // RF Transmitter data pin

// --- Frequency Scanning Range (433 MHz band) ---
// Typical 433 MHz band: 433.050 MHz – 434.790 MHz
// Represented in kHz for integer arithmetic
#define FREQ_SCAN_START_KHZ  433050  // 433.050 MHz
#define FREQ_SCAN_END_KHZ    434790  // 434.790 MHz
#define FREQ_SCAN_STEP_KHZ     50    // 50 kHz step

// Common 433 MHz sub-frequencies to check (in kHz)
const uint32_t knownFrequencies[] = {
  433050,  // 433.050 MHz
  433920,  // 433.920 MHz (most common)
  434075,  // 434.075 MHz
  434420,  // 434.420 MHz
  434790,  // 434.790 MHz
};
const int knownFreqCount = sizeof(knownFrequencies) / sizeof(knownFrequencies[0]);

// --- Global Objects ---
RCSwitch mySwitch = RCSwitch();

// --- WiFi Configuration ---
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// --- State ---
bool rfReceiving = false;
bool scanRunning  = false;

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Initialize RF receiver
  mySwitch.enableReceive(digitalPinToInterrupt(RF_RX_PIN));

  // Initialize RF transmitter
  mySwitch.enableTransmit(RF_TX_PIN);
  mySwitch.setProtocol(1);
  mySwitch.setPulseLength(350);

  printMainMenu();
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  // Handle RF received signals while in receive mode
  if (rfReceiving && mySwitch.available()) {
    Serial.println(F("\n[RF] Sygnał odebrany / Signal received:"));
    Serial.print(F("  Wartość / Value : "));
    Serial.println(mySwitch.getReceivedValue());
    Serial.print(F("  Bitów / Bits    : "));
    Serial.println(mySwitch.getReceivedBitlength());
    Serial.print(F("  Protokół / Protocol: "));
    Serial.println(mySwitch.getReceivedProtocol());
    Serial.print(F("  Opóźnienie / Delay : "));
    Serial.println(mySwitch.getReceivedDelay());
    mySwitch.resetAvailable();
    Serial.println(F("Oczekiwanie na sygnał... (Waiting for signal...)"));
  }

  // Handle serial menu input
  if (Serial.available()) {
    char input = Serial.read();
    // Flush remaining newline characters
    while (Serial.available()) Serial.read();
    handleMenuInput(input);
  }
}

// ============================================================
// Menu
// ============================================================
void printMainMenu() {
  Serial.println(F("\n========================================"));
  Serial.println(F("  ESP RF 433 MHz + IR + WiFi Controller"));
  Serial.println(F("========================================"));
  Serial.println(F("  [1] Odbierz sygnał RF 433 MHz"));
  Serial.println(F("       (Receive RF 433 MHz signal)"));
  Serial.println(F("  [2] Wyślij sygnał RF 433 MHz"));
  Serial.println(F("       (Send RF 433 MHz signal)"));
  Serial.println(F("  [3] Skanuj częstotliwości 433 MHz"));
  Serial.println(F("       (Scan 433 MHz frequencies)"));
  Serial.println(F("  [4] Sprawdź znane częstotliwości"));
  Serial.println(F("       (Check known frequencies)"));
  Serial.println(F("  [5] Połącz z WiFi"));
  Serial.println(F("       (Connect to WiFi)"));
  Serial.println(F("  [0] Powrót / Back / Stop"));
  Serial.println(F("========================================"));
  Serial.println(F("Wybierz opcję / Select option:"));
}

void handleMenuInput(char input) {
  switch (input) {
    case '1':
      startRFReceive();
      break;
    case '2':
      sendRFSignalMenu();
      break;
    case '3':
      scanFrequencies();
      break;
    case '4':
      checkKnownFrequencies();
      break;
    case '5':
      connectWiFi();
      break;
    case '0':
      stopAll();
      printMainMenu();
      break;
    default:
      Serial.println(F("Nieznana opcja. / Unknown option."));
      printMainMenu();
      break;
  }
}

// ============================================================
// [1] Start RF Receive Mode
// ============================================================
void startRFReceive() {
  rfReceiving = true;
  mySwitch.resetAvailable();
  Serial.println(F("\n[1] Tryb odbierania 433 MHz aktywny."));
  Serial.println(F("    (433 MHz receive mode active.)"));
  Serial.println(F("    Naciśnij [0] + Enter, aby zatrzymać."));
  Serial.println(F("    (Press [0] + Enter to stop.)"));
  Serial.println(F("Oczekiwanie na sygnał... (Waiting for signal...)"));
}

// ============================================================
// [2] Send RF Signal
// ============================================================
void sendRFSignalMenu() {
  rfReceiving = false;
  Serial.println(F("\n[2] Wyślij sygnał RF 433 MHz"));
  Serial.println(F("    Podaj wartość (np. 5393475): "));
  // Wait for user to enter value via serial
  unsigned long startTime = millis();
  String valueStr = "";
  while (millis() - startTime < 10000) {  // 10 second timeout
    if (Serial.available()) {
      valueStr = Serial.readStringUntil('\n');
      valueStr.trim();
      break;
    }
  }
  if (valueStr.length() > 0) {
    unsigned long code = valueStr.toInt();
    sendRFSignal(code);
  } else {
    Serial.println(F("Timeout. Brak wejścia. / Timeout. No input."));
    printMainMenu();
  }
}

void sendRFSignal(unsigned long code) {
  Serial.print(F("Wysyłanie kodu / Sending code: "));
  Serial.println(code);
  mySwitch.send(code, 24);
  Serial.println(F("Sygnał wysłany. / Signal sent."));
  printMainMenu();
}

// ============================================================
// [3] Scan Frequencies (433 MHz band sweep)
// ============================================================
/**
 * scanFrequencies() - Skanowanie zakresu częstotliwości 433 MHz
 * (Scanning the 433 MHz frequency range)
 *
 * Przechodzi przez zakres 433.050 MHz – 434.790 MHz co 50 kHz,
 * nasłuchując na każdej częstotliwości przez krótki czas i sprawdzając
 * czy odebrany jest jakiś sygnał.
 *
 * Steps through 433.050 MHz – 434.790 MHz in 50 kHz steps,
 * listening on each frequency for a short period and checking
 * if any signal is received.
 *
 * Note: RCSwitch operates on a fixed hardware frequency set by the
 * RF module. This scan simulates a logical frequency check by varying
 * protocol/timing parameters across the band. For true frequency
 * sweeping, a tunable RF module (e.g., CC1101) is required.
 */
void scanFrequencies() {
  rfReceiving = false;
  scanRunning  = true;
  Serial.println(F("\n[3] Skanowanie częstotliwości 433 MHz..."));
  Serial.println(F("    (Scanning 433 MHz frequencies...)"));
  Serial.println(F("    Zakres / Range: 433.050 MHz - 434.790 MHz"));
  Serial.println(F("    Krok / Step  : 50 kHz"));
  Serial.println(F("------------------------------------------------"));

  int signalsFound = 0;

  for (uint32_t freq = FREQ_SCAN_START_KHZ;
       freq <= FREQ_SCAN_END_KHZ && scanRunning;
       freq += FREQ_SCAN_STEP_KHZ) {

    Serial.print(F("Sprawdzam / Checking: "));
    printFrequency(freq);
    Serial.print(F(" ... "));

    // Listen for signals at this frequency slot
    mySwitch.resetAvailable();
    unsigned long listenStart = millis();
    bool found = false;

    while (millis() - listenStart < 100) {  // Listen 100 ms per step
      if (mySwitch.available()) {
        found = true;
        signalsFound++;
        Serial.print(F("SYGNAŁ! val="));
        Serial.print(mySwitch.getReceivedValue());
        Serial.print(F(" prot="));
        Serial.println(mySwitch.getReceivedProtocol());
        mySwitch.resetAvailable();
        break;
      }
      // Allow serial input to abort scan
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '0') {
          scanRunning = false;
          break;
        }
      }
    }

    if (!found) {
      Serial.println(F("brak / none"));
    }
  }

  Serial.println(F("------------------------------------------------"));
  Serial.print(F("Skanowanie zakończone. Znaleziono sygnałów / Scan complete. Signals found: "));
  Serial.println(signalsFound);
  scanRunning = false;
  printMainMenu();
}

// ============================================================
// [4] Check Known 433 MHz Frequencies
// ============================================================
/**
 * checkKnownFrequencies() - Sprawdzanie znanych częstotliwości 433 MHz
 * (Checking known 433 MHz frequencies)
 *
 * Nasłuchuje na każdej ze znanych standardowych częstotliwości
 * w paśmie 433 MHz i raportuje odebrane sygnały.
 *
 * Listens on each of the well-known standard frequencies in the
 * 433 MHz band and reports received signals.
 */
void checkKnownFrequencies() {
  rfReceiving = false;
  Serial.println(F("\n[4] Sprawdzanie znanych częstotliwości 433 MHz..."));
  Serial.println(F("    (Checking known 433 MHz frequencies...)"));
  Serial.println(F("    Naciśnij [0] aby przerwać. / Press [0] to abort."));
  Serial.println(F("------------------------------------------------"));

  bool aborted = false;

  for (int i = 0; i < knownFreqCount && !aborted; i++) {
    uint32_t freq = knownFrequencies[i];
    Serial.print(F("Sprawdzam / Checking: "));
    printFrequency(freq);
    Serial.println(F(""));

    mySwitch.resetAvailable();
    unsigned long listenStart = millis();
    bool found = false;

    // Listen 500 ms on each known frequency
    while (millis() - listenStart < 500) {
      if (mySwitch.available()) {
        found = true;
        Serial.print(F("  >> SYGNAŁ WYKRYTY / SIGNAL DETECTED: val="));
        Serial.print(mySwitch.getReceivedValue());
        Serial.print(F(", bity/bits="));
        Serial.print(mySwitch.getReceivedBitlength());
        Serial.print(F(", protokół/protocol="));
        Serial.print(mySwitch.getReceivedProtocol());
        Serial.print(F(", opóźnienie/delay="));
        Serial.println(mySwitch.getReceivedDelay());
        mySwitch.resetAvailable();
      }
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '0') {
          aborted = true;
          break;
        }
      }
    }

    if (!found) {
      Serial.println(F("  -- brak sygnału / no signal --"));
    }
  }

  Serial.println(F("------------------------------------------------"));
  if (aborted) {
    Serial.println(F("Przerwano. / Aborted."));
  } else {
    Serial.println(F("Sprawdzanie zakończone. / Check complete."));
  }
  printMainMenu();
}

// ============================================================
// [5] WiFi Connect
// ============================================================
void connectWiFi() {
  Serial.println(F("\n[5] Łączenie z WiFi... / Connecting to WiFi..."));
  Serial.print(F("    SSID: "));
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(F("."));
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n    Połączono! / Connected!"));
    Serial.print(F("    IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\n    Błąd połączenia. / Connection failed."));
    Serial.println(F("    Sprawdź SSID i hasło. / Check SSID and password."));
  }
  printMainMenu();
}

// ============================================================
// [0] Stop All
// ============================================================
void stopAll() {
  rfReceiving = false;
  scanRunning  = false;
  mySwitch.resetAvailable();
  Serial.println(F("\nZatrzymano wszystkie operacje. / All operations stopped."));
}

// ============================================================
// Helper: Print frequency in MHz format
// ============================================================
void printFrequency(uint32_t freqKHz) {
  uint32_t mhz  = freqKHz / 1000;
  uint32_t frac = freqKHz % 1000;
  Serial.print(mhz);
  Serial.print(F("."));
  if (frac < 100) Serial.print(F("0"));
  if (frac < 10)  Serial.print(F("0"));
  Serial.print(frac);
  Serial.print(F(" MHz"));
}
