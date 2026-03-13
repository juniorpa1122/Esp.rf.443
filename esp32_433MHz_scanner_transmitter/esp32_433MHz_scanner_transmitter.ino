#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

const int RECEIVER_PIN = 4;
const int TRANSMITTER_PIN = 2;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RECEIVER_PIN, INPUT);
  pinMode(TRANSMITTER_PIN, OUTPUT);

  mySwitch.enableReceive(digitalPinToInterrupt(RECEIVER_PIN));
  mySwitch.enableTransmit(TRANSMITTER_PIN);

  Serial.println("=== ESP32 433MHz Scanner & Transmitter ===");
  Serial.println("Opcje:");
  Serial.println("  send <code> <bitLength> - wyslij kod");
  Serial.println("  scan                     - wlacz/wylacz skanowanie");
  Serial.println("  examples                 - pokaz przyklady");
}

bool scanEnabled = true;
unsigned long lastCode = 0;
unsigned int lastBitLength = 0;

void loop() {
  if (scanEnabled && mySwitch.available()) {
    unsigned long receivedCode = mySwitch.getReceivedValue();
    unsigned int bitLength = mySwitch.getReceivedBitlength();
    unsigned int protocol = mySwitch.getReceivedProtocol();

    if (receivedCode != 0) {
      Serial.println("");
      Serial.println("=== ODBIERAM DANE ===");
      Serial.print("Kod (decimal): ");
      Serial.println(receivedCode);
      Serial.print("Kod (hex): 0x");
      Serial.println(receivedCode, HEX);
      Serial.print("Dlugosc bitow: ");
      Serial.println(bitLength);
      Serial.print("Protokol: ");
      Serial.println(protocol);
      Serial.print("Sygnal: ");
      Serial.println(mySwitch.getReceivedDelay());

      Serial.print("Komenda do wyslania: send ");
      Serial.print(receivedCode);
      Serial.print(" ");
      Serial.println(bitLength);

      lastCode = receivedCode;
      lastBitLength = bitLength;
    }
    mySwitch.resetAvailable();
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command.startsWith("send ")) {
      String params = command.substring(5);
      int spaceIdx = params.indexOf(' ');
      
      if (spaceIdx > 0) {
        String codeStr = params.substring(0, spaceIdx);
        String bitStr = params.substring(spaceIdx + 1);
        
        unsigned long code = codeStr.toInt();
        int bitLength = bitStr.toInt();
        
        if (code > 0 && bitLength > 0) {
          mySwitch.send(code, bitLength);
          Serial.print("Wyslano kod: ");
          Serial.print(code);
          Serial.print(" (");
          Serial.print(bitLength);
          Serial.println(" bit)");
        } else {
          Serial.println("Blad: nieprawidlowe parametry");
        }
      } else {
        Serial.println("Blad: uzyj formatu send <kod> <dlugosc_bitow>");
      }
    }
    else if (command == "scan") {
      scanEnabled = !scanEnabled;
      Serial.print("Skanowanie: ");
      Serial.println(scanEnabled ? "Włączone" : "Wyłączone");
    }
    else if (command == "examples" || command == "help") {
      Serial.println("");
      Serial.println("=== PRZYKLADY UZYCIA ===");
      Serial.println("");
      Serial.println("1. Wyslanie prostego kodu (np. 5393, 24 bity):");
      Serial.println("   send 5393 24");
      Serial.println("");
      Serial.println("2. Wyslanie kodu hex (np. 0xAB12):");
      Serial.println("   send 43794 16");
      Serial.println("");
      Serial.println("3. Wyslanie z innym protokolem:");
      Serial.println("   send 5393 24");
      Serial.println("");
      Serial.println("4. Popularne kody urzadzen (testowe):");
      Serial.println("   send 12345 24");
      Serial.println("   send 98765 24");
      Serial.println("   send 0 24");
      Serial.println("");
      Serial.println("5. Wylaczenie/wlaczenie skanowania:");
      Serial.println("   scan");
    }
    else if (command == "repeat") {
      if (lastCode > 0) {
        mySwitch.send(lastCode, lastBitLength);
        Serial.print("Powtorzono ostatni kod: ");
        Serial.println(lastCode);
      } else {
        Serial.println("Brak zapisanego kodu do powtorzenia");
      }
    }
    else if (command.length() > 0) {
      Serial.print("Nieznana komenda: ");
      Serial.println(command);
      Serial.println("Dostepne komendy: send, scan, examples, repeat");
    }
  }

  delay(10);
}
