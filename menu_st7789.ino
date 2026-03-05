/*
 * Menu 1:1 dla ESP32/ESP8266 z wyświetlaczem ST7789
 * Używa biblioteki Adafruit ST7789
 * 
 * Podłączenie wyświetlacza ST7789 do ESP32:
 * TFT_CS   -> GPIO 5
 * TFT_DC   -> GPIO 16
 * TFT_RST  -> GPIO 17
 * TFT_MOSI -> GPIO 23 (SPI MOSI)
 * TFT_SCLK -> GPIO 18 (SPI CLK)
 * TFT_BL   -> GPIO 4 (Podświetlenie, opcjonalne)
 * 
 * Przyciski:
 * BTN_UP   -> GPIO 25
 * BTN_DOWN -> GPIO 26
 * BTN_OK   -> GPIO 27
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Definicje pinów wyświetlacza
#define TFT_CS    5
#define TFT_DC    16
#define TFT_RST   17
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_BL    4

// Definicje pinów przycisków
#define BTN_UP    25
#define BTN_DOWN  26
#define BTN_OK    27

// Kolory
#define COLOR_BG        ST77XX_BLACK
#define COLOR_TEXT      ST77XX_WHITE
#define COLOR_SELECTED  ST77XX_YELLOW
#define COLOR_TITLE     ST77XX_CYAN
#define COLOR_BORDER    ST77XX_BLUE

// Rozmiar wyświetlacza
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 135

// Maksymalna liczba pozycji menu
#define MAX_MENU_ITEMS 10

// Utworzenie obiektu wyświetlacza
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Struktura pozycji menu
struct MenuItem {
  const char* name;
  void (*action)();
};

// Zmienne menu
MenuItem menuItems[MAX_MENU_ITEMS];
int menuItemCount = 0;
int currentSelection = 0;
int menuOffset = 0;
const int visibleItems = 5;

// Zmienne dla debounce przycisków
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// ===========================================
// Funkcje akcji menu
// ===========================================

void actionRF433Send() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.println("Wysylanie RF...");
  delay(2000);
  drawMenu();
}

void actionRF433Receive() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.println("Odbieranie RF...");
  delay(2000);
  drawMenu();
}

void actionIRSend() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.println("Wysylanie IR...");
  delay(2000);
  drawMenu();
}

void actionIRReceive() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.println("Odbieranie IR...");
  delay(2000);
  drawMenu();
}

void actionWiFiScan() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.println("Skanowanie WiFi...");
  delay(2000);
  drawMenu();
}

void actionSettings() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.println("Ustawienia...");
  delay(2000);
  drawMenu();
}

void actionAbout() {
  tft.fillScreen(COLOR_BG);
  tft.setCursor(10, 20);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(2);
  tft.println("ESP RF 433");
  tft.setCursor(10, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.println("Wersja: 1.0");
  tft.println("RF 433MHz + IR + WiFi");
  tft.println("");
  tft.println("ST7789 Menu System");
  delay(3000);
  drawMenu();
}

// ===========================================
// Funkcje menu
// ===========================================

void addMenuItem(const char* name, void (*action)()) {
  if (menuItemCount < MAX_MENU_ITEMS) {
    menuItems[menuItemCount].name = name;
    menuItems[menuItemCount].action = action;
    menuItemCount++;
  }
}

void initMenu() {
  // Dodaj pozycje menu
  addMenuItem("RF 433 Nadaj", actionRF433Send);
  addMenuItem("RF 433 Odbierz", actionRF433Receive);
  addMenuItem("IR Nadaj", actionIRSend);
  addMenuItem("IR Odbierz", actionIRReceive);
  addMenuItem("WiFi Skanuj", actionWiFiScan);
  addMenuItem("Ustawienia", actionSettings);
  addMenuItem("O programie", actionAbout);
}

void drawMenu() {
  // Wyczyść ekran
  tft.fillScreen(COLOR_BG);
  
  // Rysuj tytuł
  tft.fillRect(0, 0, SCREEN_WIDTH, 25, COLOR_BORDER);
  tft.setCursor(10, 5);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(2);
  tft.println("ESP RF MENU");
  
  // Rysuj ramkę
  tft.drawRect(0, 25, SCREEN_WIDTH, SCREEN_HEIGHT - 25, COLOR_BORDER);
  
  // Oblicz widoczne elementy
  if (currentSelection >= menuOffset + visibleItems) {
    menuOffset = currentSelection - visibleItems + 1;
  } else if (currentSelection < menuOffset) {
    menuOffset = currentSelection;
  }
  
  // Rysuj pozycje menu
  int yPos = 30;
  int itemHeight = 20;
  
  for (int i = menuOffset; i < min(menuOffset + visibleItems, menuItemCount); i++) {
    if (i == currentSelection) {
      // Zaznaczony element
      tft.fillRect(5, yPos, SCREEN_WIDTH - 10, itemHeight, COLOR_BORDER);
      tft.setCursor(10, yPos + 2);
      tft.setTextColor(COLOR_SELECTED);
      tft.setTextSize(2);
      tft.print("> ");
      tft.println(menuItems[i].name);
    } else {
      // Normalny element
      tft.setCursor(10, yPos + 2);
      tft.setTextColor(COLOR_TEXT);
      tft.setTextSize(2);
      tft.print("  ");
      tft.println(menuItems[i].name);
    }
    yPos += itemHeight;
  }
  
  // Rysuj wskaźnik pozycji
  int scrollbarHeight = SCREEN_HEIGHT - 30;
  int indicatorHeight = scrollbarHeight / menuItemCount;
  int indicatorPos = 28 + (currentSelection * indicatorHeight);
  
  tft.fillRect(SCREEN_WIDTH - 5, 28, 3, scrollbarHeight, ST77XX_MAGENTA);
  tft.fillRect(SCREEN_WIDTH - 5, indicatorPos, 3, indicatorHeight, COLOR_SELECTED);
}

void menuUp() {
  if (currentSelection > 0) {
    currentSelection--;
    drawMenu();
  }
}

void menuDown() {
  if (currentSelection < menuItemCount - 1) {
    currentSelection++;
    drawMenu();
  }
}

void menuSelect() {
  if (menuItems[currentSelection].action != NULL) {
    menuItems[currentSelection].action();
  }
}

// ===========================================
// Obsługa przycisków
// ===========================================

void handleButtons() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastButtonPress < debounceDelay) {
    return;
  }
  
  if (digitalRead(BTN_UP) == LOW) {
    lastButtonPress = currentTime;
    menuUp();
  }
  
  if (digitalRead(BTN_DOWN) == LOW) {
    lastButtonPress = currentTime;
    menuDown();
  }
  
  if (digitalRead(BTN_OK) == LOW) {
    lastButtonPress = currentTime;
    menuSelect();
  }
}

// ===========================================
// Setup i Loop
// ===========================================

void setup() {
  Serial.begin(115200);
  Serial.println("ESP RF 433 - Menu ST7789");
  
  // Konfiguracja pinów przycisków
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  
  // Konfiguracja podświetlenia
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Inicjalizacja wyświetlacza ST7789
  // Dla wyświetlacza 240x135:
  tft.init(135, 240);
  tft.setRotation(1);
  
  // Ekran startowy
  tft.fillScreen(COLOR_BG);
  tft.setCursor(30, 50);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(3);
  tft.println("ESP RF");
  tft.setCursor(50, 90);
  tft.setTextSize(2);
  tft.println("433 MHz");
  
  delay(2000);
  
  // Inicjalizacja menu
  initMenu();
  drawMenu();
}

void loop() {
  handleButtons();
  delay(10);
}
