# SmokelessRuntimeEFIPatcher - Interactive Mode Architecture

## Menu System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    SREP Entry Point                         │
│                 (SmokelessRuntimeEFIPatcher.c)              │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │   Mode Detection              │
         │   1. SREP_Config.cfg exists?  │──Yes──▶ MANUAL Mode
         │   2. SREP_Auto.flag exists?   │──Yes──▶ AUTO Mode  
         │   3. Default                  │──Yes──▶ INTERACTIVE Mode
         └───────────────┬───────────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │    Initialize Menu System     │
         │    - MenuInitialize()         │
         │    - Setup colors & screen    │
         │    - Get input/output handles │
         └───────────────┬───────────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │     Create Main Menu          │
         │    - CreateMainMenu()         │
         │    - Add menu items           │
         │    - Setup callbacks          │
         └───────────────┬───────────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │       Menu Loop               │
         │    - MenuRun()                │
         │    - Wait for key press       │
         │    - Handle input             │
         │    - Redraw display           │
         └───────────────┬───────────────┘
                         │
                         ├─────────────────────────────────┐
                         │                                 │
                         ▼                                 ▼
         ┌───────────────────────────┐   ┌─────────────────────────────┐
         │   Menu Action Callbacks   │   │   Navigation Actions        │
         │   - MenuCallback_AutoPatch│   │   - Arrow Up/Down           │
         │   - MenuCallback_Browse   │   │   - Enter (select)          │
         │   - MenuCallback_Launch   │   │   - ESC (back)              │
         │   - MenuCallback_About    │   │   - Page stack management   │
         │   - MenuCallback_Exit     │   │                             │
         └───────────────┬───────────┘   └─────────────────────────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │   HII Browser Integration     │
         │   - HiiBrowserInitialize()    │
         │   - Enumerate forms           │
         │   - Display BIOS pages        │
         │   - Show hidden options       │
         └───────────────┬───────────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │   Auto-Patch Integration      │
         │   - DetectBiosType()          │
         │   - AutoPatchBios()           │
         │   - Launch Setup Browser      │
         └───────────────────────────────┘
```

## Menu Item Types

```
┌──────────────────┬─────────────────────────────────────────┐
│ Type             │ Description                             │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_ACTION │ Executes callback when selected         │
│                  │ Example: "Auto-Detect and Patch BIOS"   │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_SUBMENU│ Navigates to child menu                 │
│                  │ Example: "Browse BIOS Settings >"       │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_TOGGLE │ Toggles between values (reserved)       │
│                  │ Example: "Enabled / Disabled"           │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_NUMERIC│ Numeric input field (reserved)          │
│                  │ Example: "Boot Delay: [3] seconds"      │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_STRING │ Text input field (reserved)             │
│                  │ Example: "Computer Name: [...]"         │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_       │ Visual divider (not selectable)         │
│ SEPARATOR        │ Example: "─────────────────"            │
├──────────────────┼─────────────────────────────────────────┤
│ MENU_ITEM_INFO   │ Information display (not selectable)    │
│                  │ Example: "SREP v0.3.0"                  │
└──────────────────┴─────────────────────────────────────────┘
```

## Color Scheme

```
┌─────────────────┬──────────────┬──────────────────────┐
│ Item State      │ Color        │ Use Case             │
├─────────────────┼──────────────┼──────────────────────┤
│ Title           │ Yellow/Blue  │ Menu title bar       │
│ Normal          │ Light Gray   │ Regular menu items   │
│ Selected        │ Black/White  │ Highlighted item     │
│ Disabled        │ Dark Gray    │ Unavailable items    │
│ Hidden/Unlocked │ Light Green  │ Previously hidden    │
│ Description     │ Cyan         │ Help text at bottom  │
│ Background      │ Black        │ Screen background    │
└─────────────────┴──────────────┴──────────────────────┘
```

## Menu Navigation Flow

```
                Main Menu
                    │
    ┌───────────────┼───────────────┐
    │               │               │
    ▼               ▼               ▼
Auto-Patch    Browse BIOS      Launch Setup
    │         Settings              │
    │            │                  │
    ▼            │                  ▼
Detect BIOS      │            FormBrowser2
    │            │            Protocol
    ▼            │                  │
Apply Patches    ▼                  ▼
    │        Forms Menu        Setup UI
    │            │                  │
    ▼            ▼                  ▼
Launch Setup  Questions Menu   [User Config]
    │            │                  │
    ▼            ▼                  ▼
[Infinite   [Read Values]      [Return or
 Loop]      [Show Info]         Loop]
```

## Key Bindings

```
┌────────────┬────────────────────────────────────────┐
│ Key        │ Action                                 │
├────────────┼────────────────────────────────────────┤
│ ↑ (Up)     │ Move selection up to previous item     │
│ ↓ (Down)   │ Move selection down to next item       │
│ ← (Left)   │ Reserved for future use                │
│ → (Right)  │ Reserved for future use                │
│ Enter      │ Select current item / Execute action   │
│ ESC        │ Go back to parent menu / Exit          │
│ Y/y        │ Confirm "Yes" in dialogs               │
│ N/n        │ Confirm "No" in dialogs                │
└────────────┴────────────────────────────────────────┘
```

## Data Structures

```c
// Menu Context - Main state
typedef struct {
    MENU_PAGE *CurrentPage;      // Current displayed page
    MENU_PAGE *RootPage;         // Main menu (for exit)
    MENU_COLOR_SCHEME Colors;    // Color configuration
    BOOLEAN Running;             // Event loop flag
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *TextIn;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOut;
    UINTN ScreenWidth;
    UINTN ScreenHeight;
} MENU_CONTEXT;

// Menu Page - A screen of items
typedef struct {
    CHAR16 *Title;               // Page title
    MENU_ITEM *Items;            // Array of items
    UINTN ItemCount;             // Number of items
    UINTN SelectedIndex;         // Currently selected
    MENU_PAGE *Parent;           // For back navigation
} MENU_PAGE;

// Menu Item - An individual option
typedef struct {
    MENU_ITEM_TYPE Type;         // Action, submenu, etc.
    CHAR16 *Title;               // Display text
    CHAR16 *Description;         // Help text
    VOID *Data;                  // Custom data
    MENU_ITEM_CALLBACK Callback; // Action function
    MENU_PAGE *Submenu;          // Child page
    BOOLEAN Enabled;             // Selectable?
    BOOLEAN Hidden;              // Was it unlocked?
    UINTN Tag;                   // Custom identifier
} MENU_ITEM;
```

## HII Browser Integration

```
┌─────────────────────────────────────────────────────┐
│           HII Database Protocol                     │
│  (Contains all BIOS forms and settings)             │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────┐
│    HiiBrowserEnumerateForms()                       │
│    - Get all HII handles                            │
│    - Parse form packages                            │
│    - Build form list                                │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────┐
│    HiiBrowserCreateFormsMenu()                      │
│    - Create menu page                               │
│    - Add form items                                 │
│    - Mark hidden forms (green)                      │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────┐
│    Display Forms Menu                               │
│    > BIOS Page 1 (Main)                             │
│      BIOS Page 2 (Advanced) [Hidden]                │
│      BIOS Page 3 (Boot)                             │
│      BIOS Page 4 (Security) [Hidden]                │
│      Back to Main Menu                              │
└─────────────────────────────────────────────────────┘
```

## Callback Execution Flow

```
User presses Enter on "Auto-Detect and Patch BIOS"
            │
            ▼
MenuHandleInput() detects Enter key
            │
            ▼
Gets current selected item
            │
            ▼
Item->Callback(Item, Context)
            │
            ▼
MenuCallback_AutoPatch()
            │
            ├──▶ Show message: "Detecting BIOS..."
            │
            ├──▶ DetectBiosType()
            │
            ├──▶ AutoPatchBios()
            │
            ├──▶ Show message: "Patching complete!"
            │
            └──▶ Launches Setup Browser
                 (enters infinite loop)
```

## File Organization

```
SmokelessRuntimeEFIPatcher/
├── MenuUI.c/h                  # Menu system implementation
│   ├── Menu initialization
│   ├── Drawing functions
│   ├── Input handling
│   ├── Navigation logic
│   └── Dialog boxes
│
├── HiiBrowser.c/h              # HII database browser
│   ├── HII protocol access
│   ├── Form enumeration
│   ├── Question parsing
│   └── Menu creation
│
├── SmokelessRuntimeEFIPatcher.c # Main entry point
│   ├── Mode detection
│   ├── Menu callbacks
│   ├── Main menu creation
│   └── Auto/Manual modes
│
├── AutoPatcher.c/h             # Patching engine
│   ├── BIOS detection
│   ├── Module patching
│   └── Setup launch
│
├── IfrParser.c/h               # IFR opcode parser
│   ├── IFR data scanning
│   ├── Condition detection
│   └── Patch generation
│
└── BiosDetector.c/h            # BIOS type detection
    ├── SMBIOS parsing
    ├── Vendor identification
    └── Module discovery
```

## Future Enhancements

Possible additions to the menu system:

1. **Value Editing**:
   - Edit numeric values with +/- keys
   - Toggle boolean options
   - String input with keyboard

2. **Search Function**:
   - Search BIOS settings by name
   - Filter hidden options only
   - Quick jump to option

3. **Profiles**:
   - Save current settings
   - Load preset configurations
   - Export/import profiles

4. **Advanced Browser**:
   - Full IFR tree navigation
   - Dependency visualization
   - Conditional highlighting

5. **Help System**:
   - F1 key for detailed help
   - Context-sensitive information
   - Tips and warnings
