/**
 * menu.cpp
 * MenuEngine implementation – rendering and navigation logic.
 *
 * Requires:
 *   - Adafruit_ILI9341 / ILI9488 or TFT_eSPI (adjust include below)
 *   - config.h for colour / layout constants
 */

#include "menu.h"
#include "config.h"
#include <TFT_eSPI.h>  // Replace with your display library if different

// ─────────────────────────────────────────────
//  Global instances
// ─────────────────────────────────────────────
extern TFT_eSPI tft;   // defined in main .ino
MenuEngine menuEngine;

// ─────────────────────────────────────────────
//  MenuEngine::begin
// ─────────────────────────────────────────────
void MenuEngine::begin(Menu* root) {
    _depth = 0;
    _stack[0]        = root;
    _selIdx[0]       = 0;
    _scrollOffset[0] = 0;
    _dirty           = true;
}

// ─────────────────────────────────────────────
//  MenuEngine::update
// ─────────────────────────────────────────────
void MenuEngine::update() {
    if (_dirty) {
        draw();
        _dirty = false;
    }
}

// ─────────────────────────────────────────────
//  MenuEngine::requestRedraw
// ─────────────────────────────────────────────
void MenuEngine::requestRedraw() {
    _dirty = true;
}

// ─────────────────────────────────────────────
//  Navigation
// ─────────────────────────────────────────────
void MenuEngine::navigateUp() {
    Menu*   m   = currentMenu();
    uint8_t sel = currentSel();
    if (sel == 0) return;
    _selIdx[_depth]--;

    // Scroll up if the selected item is above the visible window
    if (_selIdx[_depth] < _scrollOffset[_depth]) {
        _scrollOffset[_depth]--;
    }
    _dirty = true;
}

void MenuEngine::navigateDown() {
    Menu*   m   = currentMenu();
    uint8_t sel = currentSel();
    if (sel >= m->itemCount - 1) return;
    _selIdx[_depth]++;

    // Scroll down if the selected item is below the visible window
    if (_selIdx[_depth] >= _scrollOffset[_depth] + MAX_VISIBLE) {
        _scrollOffset[_depth]++;
    }
    _dirty = true;
}

void MenuEngine::selectCurrent() {
    activateCurrentItem();
}

void MenuEngine::navigateBack() {
    popMenu();
}

// ─────────────────────────────────────────────
//  Stack operations
// ─────────────────────────────────────────────
void MenuEngine::pushMenu(Menu* m) {
    if (_depth >= MAX_DEPTH - 1) return;
    _depth++;
    _stack[_depth]        = m;
    _selIdx[_depth]       = 0;
    _scrollOffset[_depth] = 0;
    _dirty = true;
}

void MenuEngine::popMenu() {
    if (_depth == 0) return;
    _depth--;
    _dirty = true;
}

// ─────────────────────────────────────────────
//  Item activation
// ─────────────────────────────────────────────
void MenuEngine::activateCurrentItem() {
    Menu*     m    = currentMenu();
    MenuItem& item = m->items[currentSel()];

    switch (item.type) {
        case MenuItemType::ACTION:
            if (item.action) item.action();
            break;

        case MenuItemType::TOGGLE:
            if (item.toggleValue) *item.toggleValue = !(*item.toggleValue);
            _dirty = true;
            break;

        case MenuItemType::VALUE:
            incrementValue(item);
            break;

        case MenuItemType::SUBMENU:
            if (item.submenu) pushMenu(item.submenu);
            break;

        case MenuItemType::BACK:
            popMenu();
            break;
    }
}

void MenuEngine::incrementValue(MenuItem& item) {
    if (!item.intValue) return;
    *item.intValue += item.step;
    if (*item.intValue > item.maxVal) *item.intValue = item.minVal;
    _dirty = true;
}

void MenuEngine::decrementValue(MenuItem& item) {
    if (!item.intValue) return;
    *item.intValue -= item.step;
    if (*item.intValue < item.minVal) *item.intValue = item.maxVal;
    _dirty = true;
}

// ─────────────────────────────────────────────
//  Rendering
// ─────────────────────────────────────────────
void MenuEngine::draw() {
    Menu* m = currentMenu();

    tft.fillScreen(COLOR_BG);
    drawHeader(m->title);

    uint8_t scroll  = currentScroll();
    uint8_t sel     = currentSel();
    uint8_t visible = min((uint8_t)(m->itemCount - scroll), (uint8_t)MAX_VISIBLE);

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t itemIdx = scroll + i;
        drawItem(i, m->items[itemIdx], itemIdx == sel);
    }

    if (m->itemCount > MAX_VISIBLE) {
        drawScrollBar(m->itemCount, scroll, MAX_VISIBLE);
    }

    drawFooter();
}

void MenuEngine::drawHeader(const char* title) {
    tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COLOR_HEADER_BG);

    // Decorative bottom line
    tft.drawFastHLine(0, HEADER_H - 2, SCREEN_WIDTH, COLOR_ITEM_SEL);

    tft.setTextSize(2);
    tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER_BG);
    tft.setCursor(ITEM_PADDING_X, (HEADER_H - 16) / 2);
    tft.print(title);

    // Depth indicator dots
    uint8_t dotX = SCREEN_WIDTH - 10;
    for (uint8_t d = 0; d <= _depth; d++) {
        tft.fillCircle(dotX - d * 12, HEADER_H / 2, 4,
                       d == _depth ? COLOR_ITEM_SEL : COLOR_BORDER);
    }
}

void MenuEngine::drawItem(uint8_t visibleIndex, const MenuItem& item, bool selected) {
    int16_t y = HEADER_H + visibleIndex * ITEM_H;

    uint16_t bgColor   = selected ? COLOR_ITEM_SEL : COLOR_ITEM_BG;
    uint16_t textColor = selected ? COLOR_SEL_TEXT : COLOR_ITEM_TEXT;

    tft.fillRect(0, y, SCREEN_WIDTH - 12, ITEM_H - 1, bgColor);

    // Separator line
    tft.drawFastHLine(0, y + ITEM_H - 1, SCREEN_WIDTH - 12, COLOR_BORDER);

    // Selection indicator bar
    if (selected) {
        tft.fillRect(0, y, 4, ITEM_H - 1, COLOR_ITEM_SEL);
    }

    tft.setTextSize(2);
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(ITEM_PADDING_X + (selected ? 4 : 0), y + (ITEM_H - 16) / 2);
    tft.print(item.label);

    // Right-side type indicator
    int16_t rx = SCREEN_WIDTH - 70;
    int16_t ry = y + (ITEM_H - 16) / 2;

    switch (item.type) {
        case MenuItemType::TOGGLE:
            if (item.toggleValue) {
                bool on = *item.toggleValue;
                tft.fillRoundRect(rx, ry, 44, 18, 9,
                                  on ? 0x07E0 : COLOR_BORDER);
                tft.setTextColor(on ? 0x0000 : COLOR_ITEM_TEXT,
                                 on ? 0x07E0 : COLOR_BORDER);
                tft.setCursor(rx + 6, ry + 2);
                tft.setTextSize(1);
                tft.print(on ? " ON " : " OFF");
            }
            break;

        case MenuItemType::VALUE:
            if (item.intValue) {
                tft.setTextColor(COLOR_ITEM_SEL, bgColor);
                tft.setTextSize(2);
                tft.setCursor(rx, ry);
                tft.print(*item.intValue);
            }
            break;

        case MenuItemType::SUBMENU:
            tft.setTextColor(COLOR_BORDER, bgColor);
            tft.setTextSize(2);
            tft.setCursor(SCREEN_WIDTH - 24, ry);
            tft.print(">");
            break;

        case MenuItemType::BACK:
            tft.setTextColor(COLOR_BORDER, bgColor);
            tft.setTextSize(2);
            tft.setCursor(SCREEN_WIDTH - 30, ry);
            tft.print("<");
            break;

        default:
            break;
    }
}

void MenuEngine::drawFooter() {
    int16_t y = SCREEN_HEIGHT - FOOTER_H;
    tft.fillRect(0, y, SCREEN_WIDTH, FOOTER_H, COLOR_HEADER_BG);
    tft.drawFastHLine(0, y, SCREEN_WIDTH, COLOR_ITEM_SEL);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_BORDER, COLOR_HEADER_BG);
    tft.setCursor(4, y + 10);
    tft.print("UP/DN:Navigate  OK:Select  BACK:Return");
}

void MenuEngine::drawScrollBar(uint8_t total, uint8_t offset, uint8_t visible) {
    const int16_t barX  = SCREEN_WIDTH - 10;
    const int16_t barY  = HEADER_H;
    const int16_t barH  = (int16_t)MAX_VISIBLE * ITEM_H;

    // Track
    tft.fillRect(barX, barY, 10, barH, COLOR_BORDER);

    // Thumb
    int16_t thumbH = max((int16_t)10, (int16_t)((float)visible / total * barH));
    int16_t thumbY = barY + (int16_t)((float)offset / total * barH);
    tft.fillRect(barX + 2, thumbY, 6, thumbH, COLOR_ITEM_SEL);
}
