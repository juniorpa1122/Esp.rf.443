/**
 * ESP32 Multi-Tool
 * ================
 * Features:
 *  1. BLE Scan
 *  2. WiFi Scan
 *  3. 433 MHz RF Receive (RCSwitch)
 *  4. 433 MHz RF Transmit (RCSwitch)
 *  5. IR Receive (IRremoteESP8266)
 *  6. IR Transmit (IRremoteESP8266)
 *  7. GPS (TinyGPS++ over UART2)
 *
 * Wiring:
 *  RF433 Receiver DATA  -> GPIO 19
 *  RF433 Transmitter DATA -> GPIO 18
 *  IR Receiver OUT      -> GPIO 34
 *  IR LED (via transistor/driver) -> GPIO 4
 *  GPS TX (module)      -> GPIO 16 (ESP32 RX2)
 *  GPS RX (module)      -> GPIO 17 (ESP32 TX2)
 *
 * Usage:
 *  Open Serial Monitor at 115200 baud.
 *  Enter a number (1-7) followed by Enter to select a mode.
 *  Send '0' + Enter while in a scanning mode to return to the menu.
 */

#include <Arduino.h>

// ── BLE ───────────────────────────────────────────────────────────────────────
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
#include <WiFi.h>

// ── 433 MHz RF ────────────────────────────────────────────────────────────────
#include <RCSwitch.h>

// ── IR ────────────────────────────────────────────────────────────────────────
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

// ── GPS ───────────────────────────────────────────────────────────────────────
#include <TinyGPS++.h>

// ─────────────────────────────────────────────────────────────────────────────
// Pin definitions
// ─────────────────────────────────────────────────────────────────────────────
static const uint8_t RF433_RX_PIN = 19;
static const uint8_t RF433_TX_PIN = 18;
static const uint8_t IR_RX_PIN    = 34;
static const uint8_t IR_TX_PIN    = 4;
static const uint8_t GPS_RX_PIN   = 16;  // connect module TX here
static const uint8_t GPS_TX_PIN   = 17;  // connect module RX here
static const uint32_t GPS_BAUD    = 9600;

// IR capture buffer size (number of pulses)
static const uint16_t IR_BUF_SIZE = 1024;

// BLE scan duration in seconds
static const int BLE_SCAN_SECONDS = 5;

// ─────────────────────────────────────────────────────────────────────────────
// Global objects
// ─────────────────────────────────────────────────────────────────────────────
BLEScan*      pBLEScan = nullptr;
RCSwitch      rcSwitch;
IRrecv        irRecv(IR_RX_PIN, IR_BUF_SIZE, 15, true);
IRsend        irSend(IR_TX_PIN);
TinyGPSPlus   gps;
HardwareSerial gpsSerial(2);

// ─────────────────────────────────────────────────────────────────────────────
// Helper: wait for a complete line from Serial, return trimmed string
// ─────────────────────────────────────────────────────────────────────────────
static String serialReadLine() {
    while (!Serial.available()) {
        delay(10);
    }
    String s = Serial.readStringUntil('\n');
    s.trim();
    return s;
}

// Helper: check if user sent '0' to quit the current mode
static bool userWantsQuit() {
    if (Serial.available()) {
        String s = Serial.readStringUntil('\n');
        s.trim();
        if (s == "0") return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BLE – Advertised device callback
// ─────────────────────────────────────────────────────────────────────────────
class BLECallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {
        Serial.print("[BLE] ");
        Serial.print(device.getAddress().toString().c_str());
        Serial.print("  RSSI: ");
        Serial.print(device.getRSSI());
        Serial.print(" dBm");
        if (device.haveName()) {
            Serial.print("  Name: ");
            Serial.print(device.getName().c_str());
        }
        if (device.haveServiceUUID()) {
            Serial.print("  UUID: ");
            Serial.print(device.getServiceUUID().toString().c_str());
        }
        Serial.println();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 1. BLE Scan
// ─────────────────────────────────────────────────────────────────────────────
static void doBLEScan() {
    Serial.printf("[BLE] Scanning for %d seconds...\n", BLE_SCAN_SECONDS);
    BLEScanResults results = pBLEScan->start(BLE_SCAN_SECONDS, false);
    Serial.printf("[BLE] Scan complete. Devices found: %d\n", results.getCount());
    pBLEScan->clearResults();
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. WiFi Scan
// ─────────────────────────────────────────────────────────────────────────────
static void doWiFiScan() {
    Serial.println("[WiFi] Scanning networks...");
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        Serial.println("[WiFi] No networks found.");
        return;
    }
    Serial.printf("[WiFi] Found %d network(s):\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  %2d. %-32s  %4d dBm  Ch%2d  %s\n",
                      i + 1,
                      WiFi.SSID(i).c_str(),
                      WiFi.RSSI(i),
                      WiFi.channel(i),
                      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
    }
    WiFi.scanDelete();
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. 433 MHz RF Receive
// ─────────────────────────────────────────────────────────────────────────────
static void doRF433Receive() {
    Serial.println("[RF433] Listening for 433 MHz signals. Send '0' to stop.");
    rcSwitch.enableReceive(RF433_RX_PIN);
    while (true) {
        if (userWantsQuit()) break;
        if (rcSwitch.available()) {
            unsigned long value    = rcSwitch.getReceivedValue();
            unsigned int  bits     = rcSwitch.getReceivedBitlength();
            unsigned int  protocol = rcSwitch.getReceivedProtocol();
            unsigned int  delay_us = rcSwitch.getReceivedDelay();
            if (value == 0) {
                Serial.println("[RF433] Unknown/unrecognized encoding.");
            } else {
                Serial.printf("[RF433] Value: %lu  Bits: %u  Protocol: %u  Delay: %u µs\n",
                              value, bits, protocol, delay_us);
            }
            rcSwitch.resetAvailable();
        }
        delay(10);
    }
    rcSwitch.disableReceive();
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. 433 MHz RF Transmit
// ─────────────────────────────────────────────────────────────────────────────
static void doRF433Send() {
    Serial.println("[RF433] Enter decimal code to send (e.g. 5592405):");
    String input = serialReadLine();
    unsigned long code = strtoul(input.c_str(), nullptr, 10);
    if (code == 0) {
        Serial.println("[RF433] Invalid code.");
        return;
    }
    Serial.print("[RF433] Enter bit length (default 24): ");
    String bitsInput = serialReadLine();
    unsigned int bits = (bitsInput.length() > 0) ? (unsigned int)bitsInput.toInt() : 24;

    rcSwitch.enableTransmit(RF433_TX_PIN);
    rcSwitch.setRepeatTransmit(10);
    rcSwitch.send(code, bits);
    rcSwitch.disableTransmit();
    Serial.printf("[RF433] Sent code %lu (%u bits).\n", code, bits);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. IR Receive
// ─────────────────────────────────────────────────────────────────────────────
static void doIRReceive() {
    Serial.println("[IR] Listening for IR signals. Send '0' to stop.");
    irRecv.enableIRIn();
    decode_results results;
    while (true) {
        if (userWantsQuit()) break;
        if (irRecv.decode(&results)) {
            Serial.printf("[IR] Protocol: %-12s  Value: 0x%08llX  Bits: %d\n",
                          typeToString(results.decode_type, results.repeat).c_str(),
                          results.value,
                          results.bits);
            // Print raw timing for unknown protocols
            if (results.decode_type == UNKNOWN) {
                Serial.print("[IR] Raw (");
                Serial.print(results.rawlen - 1);
                Serial.print("): ");
                for (uint16_t i = 1; i < results.rawlen; i++) {
                    if (i % 2) Serial.print('+');
                    else        Serial.print('-');
                    Serial.print(results.rawbuf[i] * kRawTick);
                    Serial.print(' ');
                }
                Serial.println();
            }
            irRecv.resume();
        }
        delay(10);
    }
    irRecv.disableIRIn();
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. IR Transmit
// ─────────────────────────────────────────────────────────────────────────────
static void doIRSend() {
    Serial.println("[IR] IR Send options:");
    Serial.println("  1. NEC");
    Serial.println("  2. Samsung");
    Serial.println("  3. Sony");
    Serial.println("  4. RC5");
    Serial.println("  5. Raw (example pulse train)");
    Serial.print("Select protocol (1-5): ");
    String proto = serialReadLine();

    if (proto == "5") {
        // Example raw NEC "power" burst – replace with your recorded raw values
        uint16_t rawData[] = {
            9024, 4512,
            564, 564, 564, 564, 564, 1692, 564, 564,
            564, 564, 564, 564, 564, 564,  564, 564,
            564, 1692, 564, 1692, 564, 1692, 564, 1692,
            564, 1692, 564, 1692, 564, 1692, 564, 564,
            564, 564, 564, 564, 564, 1692, 564, 564,
            564, 564, 564, 564, 564, 564,  564, 564,
            564, 1692, 564, 1692, 564, 1692, 564, 1692,
            564, 1692, 564, 1692, 564, 1692, 564, 40884
        };
        irSend.sendRaw(rawData, sizeof(rawData) / sizeof(rawData[0]), 38);
        Serial.println("[IR] Raw signal sent.");
        return;
    }

    Serial.print("[IR] Enter hex value (e.g. 20DF10EF): ");
    String hexStr = serialReadLine();
    uint64_t value = strtoull(hexStr.c_str(), nullptr, 16);

    Serial.print("[IR] Enter bit length (default 32): ");
    String bitsStr = serialReadLine();
    uint16_t bits = (bitsStr.length() > 0) ? (uint16_t)bitsStr.toInt() : 32;

    switch (proto.toInt()) {
        case 1:
            irSend.sendNEC(value, bits);
            Serial.printf("[IR] NEC sent: 0x%08llX (%u bits)\n", value, bits);
            break;
        case 2:
            irSend.sendSAMSUNG(value, bits);
            Serial.printf("[IR] Samsung sent: 0x%08llX (%u bits)\n", value, bits);
            break;
        case 3:
            irSend.sendSony(value, bits);
            Serial.printf("[IR] Sony sent: 0x%08llX (%u bits)\n", value, bits);
            break;
        case 4:
            irSend.sendRC5(value, bits);
            Serial.printf("[IR] RC5 sent: 0x%08llX (%u bits)\n", value, bits);
            break;
        default:
            Serial.println("[IR] Unknown protocol selection.");
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. GPS
// ─────────────────────────────────────────────────────────────────────────────
static void doGPS() {
    Serial.println("[GPS] Reading GPS data. Send '0' to stop.");
    unsigned long lastPrint = 0;
    while (true) {
        if (userWantsQuit()) break;
        while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
        }
        if (millis() - lastPrint >= 2000) {
            lastPrint = millis();
            if (gps.location.isValid()) {
                Serial.printf("[GPS] Lat: %.6f  Lng: %.6f  Alt: %.1f m  Sats: %u  Speed: %.1f km/h\n",
                              gps.location.lat(),
                              gps.location.lng(),
                              gps.altitude.meters(),
                              gps.satellites.value(),
                              gps.speed.kmph());
                if (gps.date.isValid() && gps.time.isValid()) {
                    Serial.printf("[GPS] Date: %04d-%02d-%02d  Time: %02d:%02d:%02d UTC\n",
                                  gps.date.year(), gps.date.month(), gps.date.day(),
                                  gps.time.hour(), gps.time.minute(), gps.time.second());
                }
            } else {
                Serial.printf("[GPS] Waiting for fix... (chars processed: %u, sentences: %u, failed: %u)\n",
                              gps.charsProcessed(), gps.sentencesWithFix(), gps.failedChecksum());
            }
        }
        delay(10);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu
// ─────────────────────────────────────────────────────────────────────────────
static void printMenu() {
    Serial.println();
    Serial.println("╔══════════════════════════════╗");
    Serial.println("║     ESP32 Multi-Tool v1.0    ║");
    Serial.println("╠══════════════════════════════╣");
    Serial.println("║  1. BLE Scan                 ║");
    Serial.println("║  2. WiFi Scan                ║");
    Serial.println("║  3. 433 MHz RF Receive       ║");
    Serial.println("║  4. 433 MHz RF Send          ║");
    Serial.println("║  5. IR Receive               ║");
    Serial.println("║  6. IR Send                  ║");
    Serial.println("║  7. GPS                      ║");
    Serial.println("╚══════════════════════════════╝");
    Serial.print("Select (1-7): ");
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    // ── BLE init ────────────────────────────────────────────────────────────
    BLEDevice::init("ESP32-MultiTool");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new BLECallback(), false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // ── WiFi init ───────────────────────────────────────────────────────────
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // ── GPS UART init ────────────────────────────────────────────────────────
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // ── IR TX init ───────────────────────────────────────────────────────────
    irSend.begin();

    Serial.println("\nESP32 Multi-Tool ready!");
    printMenu();
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop – menu dispatcher
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    if (!Serial.available()) return;

    String choice = serialReadLine();
    int sel = choice.toInt();

    switch (sel) {
        case 1: doBLEScan();      break;
        case 2: doWiFiScan();     break;
        case 3: doRF433Receive(); break;
        case 4: doRF433Send();    break;
        case 5: doIRReceive();    break;
        case 6: doIRSend();       break;
        case 7: doGPS();          break;
        default:
            Serial.println("Invalid selection.");
            break;
    }
    printMenu();
}
