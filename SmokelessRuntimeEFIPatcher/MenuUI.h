#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>

// Menu item types
typedef enum {
    MENU_ITEM_ACTION,      // Execute an action when selected
    MENU_ITEM_SUBMENU,     // Navigate to a submenu
    MENU_ITEM_TOGGLE,      // Toggle between values
    MENU_ITEM_NUMERIC,     // Numeric input
    MENU_ITEM_STRING,      // String input
    MENU_ITEM_SEPARATOR,   // Visual separator (not selectable)
    MENU_ITEM_INFO         // Information display (not selectable)
} MENU_ITEM_TYPE;

// Forward declaration
typedef struct _MENU_ITEM MENU_ITEM;
typedef struct _MENU_PAGE MENU_PAGE;

// Menu item callback function
typedef EFI_STATUS (*MENU_ITEM_CALLBACK)(MENU_ITEM *Item, VOID *Context);

// Menu item structure
struct _MENU_ITEM {
    MENU_ITEM_TYPE Type;
    CHAR16 *Title;              // Display text
    CHAR16 *Description;        // Help text shown at bottom
    VOID *Data;                 // Item-specific data
    MENU_ITEM_CALLBACK Callback; // Action to execute
    MENU_PAGE *Submenu;         // For MENU_ITEM_SUBMENU
    BOOLEAN Enabled;            // Can be selected
    BOOLEAN Hidden;             // Is this a hidden/unlocked option
    UINTN Tag;                  // Custom tag for identification
};

// Menu page structure
struct _MENU_PAGE {
    CHAR16 *Title;              // Page title
    MENU_ITEM *Items;           // Array of menu items
    UINTN ItemCount;            // Number of items
    UINTN SelectedIndex;        // Currently selected item
    MENU_PAGE *Parent;          // Parent page (for back navigation)
};

// Color scheme
typedef struct {
    UINTN TitleColor;
    UINTN NormalColor;
    UINTN SelectedColor;
    UINTN DisabledColor;
    UINTN HiddenColor;
    UINTN DescriptionColor;
    UINTN BackgroundColor;
} MENU_COLOR_SCHEME;

// Menu context
typedef struct {
    MENU_PAGE *CurrentPage;
    MENU_PAGE *RootPage;
    MENU_COLOR_SCHEME Colors;
    BOOLEAN Running;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *TextIn;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOut;
    UINTN ScreenWidth;
    UINTN ScreenHeight;
} MENU_CONTEXT;

/**
 * Initialize menu system
 */
EFI_STATUS MenuInitialize(MENU_CONTEXT *Context);

/**
 * Create a new menu page
 */
MENU_PAGE *MenuCreatePage(CHAR16 *Title, UINTN ItemCount);

/**
 * Add an action item to a menu page
 */
EFI_STATUS MenuAddActionItem(
    MENU_PAGE *Page,
    UINTN Index,
    CHAR16 *Title,
    CHAR16 *Description,
    MENU_ITEM_CALLBACK Callback,
    VOID *Data
);

/**
 * Add a submenu item to a menu page
 */
EFI_STATUS MenuAddSubmenuItem(
    MENU_PAGE *Page,
    UINTN Index,
    CHAR16 *Title,
    CHAR16 *Description,
    MENU_PAGE *Submenu
);

/**
 * Add a separator item
 */
EFI_STATUS MenuAddSeparator(MENU_PAGE *Page, UINTN Index, CHAR16 *Title);

/**
 * Add an info item
 */
EFI_STATUS MenuAddInfoItem(MENU_PAGE *Page, UINTN Index, CHAR16 *Title);

/**
 * Run the menu system (main loop)
 */
EFI_STATUS MenuRun(MENU_CONTEXT *Context, MENU_PAGE *StartPage);

/**
 * Navigate to a page
 */
EFI_STATUS MenuNavigateTo(MENU_CONTEXT *Context, MENU_PAGE *Page);

/**
 * Go back to parent page
 */
EFI_STATUS MenuGoBack(MENU_CONTEXT *Context);

/**
 * Draw the current menu page
 */
VOID MenuDraw(MENU_CONTEXT *Context);

/**
 * Handle keyboard input
 */
EFI_STATUS MenuHandleInput(MENU_CONTEXT *Context, EFI_INPUT_KEY *Key);

/**
 * Clean up menu resources
 */
VOID MenuCleanup(MENU_CONTEXT *Context);

/**
 * Show a message box with OK button
 */
EFI_STATUS MenuShowMessage(MENU_CONTEXT *Context, CHAR16 *Title, CHAR16 *Message);

/**
 * Show a yes/no confirmation dialog
 */
EFI_STATUS MenuShowConfirm(MENU_CONTEXT *Context, CHAR16 *Title, CHAR16 *Message, BOOLEAN *Result);

/**
 * Get a string input from user
 */
EFI_STATUS MenuGetStringInput(MENU_CONTEXT *Context, CHAR16 *Prompt, CHAR16 *Buffer, UINTN BufferSize);

/**
 * Free a menu page and its items
 */
VOID MenuFreePage(MENU_PAGE *Page);
