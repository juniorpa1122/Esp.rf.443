/**
 * menu.h
 * Multi-level menu data structures and public API.
 *
 * Supports:
 *   - Unlimited depth sub-menus
 *   - Toggle (on/off) items
 *   - Value items (integer with min/max/step)
 *   - Action items (callback on select)
 *   - Back navigation
 */

#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────
//  Menu item types
// ─────────────────────────────────────────────
enum class MenuItemType : uint8_t {
    ACTION,      // Calls a void callback
    TOGGLE,      // Boolean on/off value
    VALUE,       // Integer value with bounds
    SUBMENU,     // Opens a nested menu
    BACK         // Returns to parent menu
};

// ─────────────────────────────────────────────
//  Forward declaration
// ─────────────────────────────────────────────
struct Menu;

// ─────────────────────────────────────────────
//  Single menu item
// ─────────────────────────────────────────────
struct MenuItem {
    const char*  label;        // Display text
    MenuItemType type;

    // ACTION
    void (*action)();          // Callback for ACTION type

    // TOGGLE
    bool*        toggleValue;  // Pointer to bool variable

    // VALUE
    int*         intValue;     // Pointer to int variable
    int          minVal;
    int          maxVal;
    int          step;

    // SUBMENU
    Menu*        submenu;      // Pointer to sub-menu
};

// ─────────────────────────────────────────────
//  A complete menu page
// ─────────────────────────────────────────────
struct Menu {
    const char* title;         // Header text
    MenuItem*   items;         // Array of items
    uint8_t     itemCount;     // Number of items
};

// ─────────────────────────────────────────────
//  Menu engine – public API
// ─────────────────────────────────────────────
class MenuEngine {
public:
    // Initialise with the root menu
    void begin(Menu* root);

    // Call from loop() to process input and redraw if needed
    void update();

    // Navigation (called by button / touch handlers)
    void navigateUp();
    void navigateDown();
    void selectCurrent();
    void navigateBack();

    // Force a full redraw on next update()
    void requestRedraw();

private:
    static const uint8_t MAX_DEPTH = 8;

    Menu*   _stack[MAX_DEPTH]; // Navigation stack
    uint8_t _selIdx[MAX_DEPTH];// Selected item per level
    uint8_t _scrollOffset[MAX_DEPTH]; // Scroll position per level
    uint8_t _depth;            // Current stack depth

    bool    _dirty;            // Needs redraw?

    Menu*   currentMenu()  const { return _stack[_depth]; }
    uint8_t currentSel()   const { return _selIdx[_depth]; }
    uint8_t currentScroll()const { return _scrollOffset[_depth]; }

    void    draw();
    void    drawHeader(const char* title);
    void    drawItem(uint8_t visibleIndex, const MenuItem& item, bool selected);
    void    drawFooter();
    void    drawScrollBar(uint8_t total, uint8_t offset, uint8_t visible);

    void    activateCurrentItem();
    void    pushMenu(Menu* m);
    void    popMenu();

    // Value editing helpers
    void    incrementValue(MenuItem& item);
    void    decrementValue(MenuItem& item);
};

// Single global instance
extern MenuEngine menuEngine;
