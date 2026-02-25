/*
 * RF 433 MHz Signal Scanner & Tester
 * ESP8266 / ESP32
 *
 * Urządzenie do wysyłania różnych sygnałów RF 433 MHz
 * oraz testowania czy inne urządzenie nadaje na tej częstotliwości.
 *
 * --- Podłączenie (Wiring) ---
 * RF Receiver (e.g. XY-MK-5V):
 *   DATA  -> GPIO 4  (D2 on NodeMCU)
 *   VCC   -> 5V
 *   GND   -> GND
 *
 * RF Transmitter (e.g. FS1000A):
 *   DATA  -> GPIO 2  (D4 on NodeMCU)
 *   VCC   -> 5V
 *   GND   -> GND
 *
 * Wymagane biblioteki / Required libraries:
 *   - RCSwitch  (https://github.com/sui77/rc-switch)
 *   - ESP8266WiFi (part of ESP8266 Arduino core)
 *     OR WiFi.h (ESP32 Arduino core)
 *   - ESP8266WebServer / WebServer (ESP32)
 */

#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  WebServer server(80);
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
#endif

#include <RCSwitch.h>

// ── Configuration ──────────────────────────────────────────────────────────────
#define RF_RX_PIN   4   // GPIO for RF receiver DATA pin
#define RF_TX_PIN   2   // GPIO for RF transmitter DATA pin

// WiFi Access Point credentials (connect to this AP to use the web UI)
const char* AP_SSID = "RF-Scanner";
const char* AP_PASS = "12345678";

// ── Globals ────────────────────────────────────────────────────────────────────
RCSwitch rf;

// Ring-buffer for last received signals (scan log)
#define LOG_SIZE 20
struct RFSignal {
  unsigned long value;
  unsigned int  bits;
  unsigned int  protocol;
  unsigned long pulseLength;
  unsigned long timestamp; // millis()
};
RFSignal signalLog[LOG_SIZE];
int       logHead  = 0;   // index of next write position
int       logCount = 0;   // number of entries stored

bool scanMode = true;  // true = receiving, false = transmitting test

// ── Helpers ────────────────────────────────────────────────────────────────────
void logSignal(unsigned long value, unsigned int bits,
               unsigned int proto, unsigned long pulse)
{
  signalLog[logHead] = {value, bits, proto, pulse, millis()};
  logHead = (logHead + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;
}

// Build the HTML log table rows (newest first)
String buildLogRows() {
  if (logCount == 0) return "<tr><td colspan='5'><em>No signals received yet.</em></td></tr>";
  String rows = "";
  for (int i = 0; i < logCount; i++) {
    int idx = ((logHead - 1 - i) + LOG_SIZE) % LOG_SIZE;
    RFSignal& s = signalLog[idx];
    rows += "<tr><td>" + String(s.timestamp / 1000.0, 1) + "s</td>"
          + "<td>" + String(s.value) + "</td>"
          + "<td>0x" + String(s.value, HEX) + "</td>"
          + "<td>" + String(s.bits) + "</td>"
          + "<td>" + String(s.protocol) + " (" + String(s.pulseLength) + " µs)</td></tr>";
  }
  return rows;
}

// ── Built-in test signal catalogue ────────────────────────────────────────────
struct TestSignal {
  const char*   label;
  unsigned long code;
  unsigned int  bits;
  unsigned int  protocol;
};

const TestSignal TEST_SIGNALS[] = {
  {"Generic ON  (code 1)",        1,         24, 1},
  {"Generic OFF (code 0)",        0,         24, 1},
  {"Common remote 0x1A2B3C",  0x1A2B3C,     24, 1},
  {"Protocol 2 – 0xABCD",     0xABCD,       16, 2},
  {"Protocol 3 – 0xFF0000",   0xFF0000,     24, 3},
  {"Protocol 4 – 0x00FFFF",   0x00FFFF,     24, 4},
  {"Protocol 5 – 0x123456",   0x123456,     24, 5},
  {"Tristate ON  (1FFF0)",    0x1FFF0,      20, 1},
  {"Tristate OFF (1FFF1)",    0x1FFF1,      20, 1},
  {"24-bit 0xFFFFFF",         0xFFFFFF,     24, 1},
};
const int TEST_SIGNAL_COUNT = sizeof(TEST_SIGNALS) / sizeof(TEST_SIGNALS[0]);

// ── HTML page ──────────────────────────────────────────────────────────────────
String buildPage() {
  String page = R"rawHTML(<!DOCTYPE html>
<html lang='pl'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>RF 433 MHz Scanner &amp; Tester</title>
<style>
  body{font-family:Arial,sans-serif;max-width:860px;margin:auto;padding:12px;background:#f5f5f5}
  h1{color:#2c3e50}
  .card{background:#fff;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,.1)}
  table{border-collapse:collapse;width:100%}
  th,td{padding:6px 10px;border:1px solid #ddd;text-align:left;font-size:.9em}
  th{background:#2c3e50;color:#fff}
  tr:nth-child(even){background:#f9f9f9}
  button,input[type=submit]{padding:8px 16px;border:none;border-radius:4px;cursor:pointer;color:#fff}
  .btn-green{background:#27ae60}.btn-red{background:#c0392b}.btn-blue{background:#2980b9}
  .status{font-weight:bold;color:)rawHTML";

  page += scanMode ? "#27ae60'>SCAN (RX) mode" : "#c0392b'>TEST-TX mode";

  page += R"rawHTML(</span>
  .mode-info{margin-bottom:8px}
  input[type=number],input[type=text]{padding:6px;width:140px;border:1px solid #ccc;border-radius:4px}
</style>
</head>
<body>
<h1>📡 RF 433 MHz Scanner &amp; Tester</h1>

<div class='card'>
  <div class='mode-info'>Tryb / Mode: <span class='status'>)rawHTML";

  page += scanMode ? "🟢 SCAN (odbiór / receive)" : "🔴 TEST-TX (nadawanie / transmit)";

  page += R"rawHTML(</span></div>
  <form method='POST' action='/mode' style='display:inline'>
    <button class='btn-green' name='m' value='scan'>📥 Scan mode (RX)</button>
  </form>
  &nbsp;
  <form method='POST' action='/mode' style='display:inline'>
    <button class='btn-red' name='m' value='tx'>📤 Test-TX mode</button>
  </form>
</div>

<div class='card'>
  <h2>📥 Received signals log</h2>
  <form method='POST' action='/clear' style='display:inline'>
    <button class='btn-red' style='font-size:.8em'>🗑 Clear log</button>
  </form>
  &nbsp;
  <a href='/'><button class='btn-blue' style='font-size:.8em'>🔄 Refresh</button></a>
  <br><br>
  <table>
    <tr><th>Time</th><th>Value (DEC)</th><th>Value (HEX)</th><th>Bits</th><th>Protocol (pulse)</th></tr>
)rawHTML";

  page += buildLogRows();

  page += R"rawHTML(
  </table>
</div>

<div class='card'>
  <h2>📤 Send a test signal</h2>
  <h3>Presets</h3>
  <table>
    <tr><th>#</th><th>Label</th><th>Code (DEC)</th><th>Bits</th><th>Protocol</th><th>Action</th></tr>
)rawHTML";

  for (int i = 0; i < TEST_SIGNAL_COUNT; i++) {
    page += "<tr><td>" + String(i + 1) + "</td>"
          + "<td>" + String(TEST_SIGNALS[i].label) + "</td>"
          + "<td>" + String(TEST_SIGNALS[i].code) + "</td>"
          + "<td>" + String(TEST_SIGNALS[i].bits) + "</td>"
          + "<td>" + String(TEST_SIGNALS[i].protocol) + "</td>"
          + "<td><form method='POST' action='/send'>"
          + "<input type='hidden' name='code' value='" + String(TEST_SIGNALS[i].code) + "'>"
          + "<input type='hidden' name='bits' value='" + String(TEST_SIGNALS[i].bits) + "'>"
          + "<input type='hidden' name='proto' value='" + String(TEST_SIGNALS[i].protocol) + "'>"
          + "<button class='btn-green'>▶ Send</button></form></td></tr>";
  }

  page += R"rawHTML(
  </table>
  <h3>Custom signal</h3>
  <form method='POST' action='/send'>
    Code (DEC):&nbsp;<input type='number' name='code' value='0' min='0'>&nbsp;
    Bits:&nbsp;<input type='number' name='bits' value='24' min='1' max='64'>&nbsp;
    Protocol:&nbsp;<input type='number' name='proto' value='1' min='1' max='12'>&nbsp;
    <input type='submit' class='btn-blue' value='▶ Send custom'>
  </form>
</div>

<div class='card'>
  <h2>ℹ️ Info</h2>
  <p>Podłącz się do sieci WiFi <strong>RF-Scanner</strong> (hasło: <strong>12345678</strong>) i otwórz <strong>http://192.168.4.1</strong>.</p>
  <p>Connect to WiFi <strong>RF-Scanner</strong> (password: <strong>12345678</strong>) and open <strong>http://192.168.4.1</strong>.</p>
  <p>Uptime: )rawHTML";

  page += String(millis() / 1000) + " s</p></div></body></html>";
  return page;
}

// ── HTTP handlers ──────────────────────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleMode() {
  if (server.method() == HTTP_POST && server.hasArg("m")) {
    String m = server.arg("m");
    if (m == "scan") {
      scanMode = true;
      rf.enableReceive(RF_RX_PIN);
      rf.disableTransmit();
    } else if (m == "tx") {
      scanMode = false;
      rf.disableReceive();
      rf.enableTransmit(RF_TX_PIN);
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSend() {
  if (server.method() == HTTP_POST) {
    unsigned long code  = server.hasArg("code")  ? server.arg("code").toInt()  : 0;
    unsigned int  bits  = server.hasArg("bits")  ? server.arg("bits").toInt()  : 24;
    unsigned int  proto = server.hasArg("proto") ? server.arg("proto").toInt() : 1;

    // Temporarily switch to TX
    rf.disableReceive();
    rf.enableTransmit(RF_TX_PIN);
    rf.setProtocol(proto);
    rf.setRepeatTransmit(5);
    rf.send(code, bits);

    // If we were in scan mode, go back to RX
    if (scanMode) {
      rf.disableTransmit();
      rf.enableReceive(RF_RX_PIN);
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClear() {
  logHead  = 0;
  logCount = 0;
  server.sendHeader("Location", "/");
  server.send(303);
}

// JSON endpoint — lightweight polling alternative to full page refresh
void handleJson() {
  String json = "{\"count\":" + String(logCount) + ",\"signals\":[";
  for (int i = 0; i < logCount; i++) {
    int idx = ((logHead - 1 - i) + LOG_SIZE) % LOG_SIZE;
    RFSignal& s = signalLog[idx];
    if (i > 0) json += ",";
    json += "{\"t\":" + String(s.timestamp)
          + ",\"v\":" + String(s.value)
          + ",\"b\":" + String(s.bits)
          + ",\"p\":" + String(s.protocol)
          + ",\"pl\":" + String(s.pulseLength) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ── setup & loop ───────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== RF 433 MHz Scanner & Tester ===");

  // Start RF receiver
  rf.enableReceive(RF_RX_PIN);
  Serial.println("RF receiver enabled on GPIO " + String(RF_RX_PIN));

  // Start WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Register HTTP routes
  server.on("/",      HTTP_GET,  handleRoot);
  server.on("/mode",  HTTP_POST, handleMode);
  server.on("/send",  HTTP_POST, handleSend);
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/json",  HTTP_GET,  handleJson);
  server.begin();
  Serial.println("HTTP server started — open http://192.168.4.1 in a browser");
}

void loop() {
  server.handleClient();

  // In scan mode: check for received RF signal
  if (scanMode && rf.available()) {
    unsigned long value  = rf.getReceivedValue();
    unsigned int  bits   = rf.getReceivedBitlength();
    unsigned int  proto  = rf.getReceivedProtocol();
    unsigned long pulse  = rf.getReceivedDelay();

    if (value != 0) {
      logSignal(value, bits, proto, pulse);

      Serial.print("RF received: value=");
      Serial.print(value);
      Serial.print(" (0x"); Serial.print(value, HEX); Serial.print(")");
      Serial.print("  bits=");   Serial.print(bits);
      Serial.print("  proto=");  Serial.print(proto);
      Serial.print("  pulse=");  Serial.print(pulse);
      Serial.println(" µs");
    }
    rf.resetAvailable();
  }
}
