# BIOS-Style Tabbed Interface

## Overview

SmokelessRuntimeEFIPatcher now includes a BIOS-style tabbed interface that mimics traditional BIOS/UEFI setup screens. This interface provides an intuitive way to navigate between different configuration categories.

## Features

### Tab Navigation
- **6 Tabs**: Main, Advanced, Power, Boot, Security, Save & Exit
- **Horizontal Navigation**: Use LEFT/RIGHT arrow keys to switch between tabs
- **Visual Indicators**: Active tab is highlighted in white/blue, inactive tabs in gray
- **Context Preservation**: Each tab maintains its own menu state

### Tab Descriptions

#### 1. Main
- BIOS Information
- Auto-Detect BIOS Type
- Browse BIOS Settings
- System Date and Time
- System Memory Info
- Processor Info

#### 2. Advanced
- Advanced BIOS Features
- Load and Edit BIOS Settings
- Launch Native Setup Browser
- CPU Configuration
- Chipset Configuration

#### 3. Power
- Power Management Options
- ACPI Settings
- Wake Events
- Power Button Configuration

#### 4. Boot
- Boot Options
- Boot Order Configuration
- Boot Mode Selection
- Fast Boot Settings

#### 5. Security
- Security Settings
- Secure Boot Configuration
- Password Protection
- TPM Configuration
- Secure Boot Status

#### 6. Save & Exit
- Save Changes and Exit
- Discard Changes and Exit
- About SREP
- Version Information

## How to Enable

### Method 1: Flag File
Create a file named `SREP_BiosTab.flag` in the same directory as the EFI executable:

```
fs0:
cd \
echo. > SREP_BiosTab.flag
SmokelessRuntimeEFIPatcher.efi
```

### Method 2: Default Behavior
The BIOS-style tab interface can be set as the default by modifying the code or configuration.

## Keyboard Controls

| Key | Action |
|-----|--------|
| ↑ (Up Arrow) | Move selection up within current tab |
| ↓ (Down Arrow) | Move selection down within current tab |
| ← (Left Arrow) | Switch to previous tab |
| → (Right Arrow) | Switch to next tab |
| Enter | Select/Execute current item |
| ESC | Go back or exit |

## Visual Layout

```
┌────────────────────────────────────────────────────────────────┐
│         SREP - SmokelessRuntimeEFIPatcher v0.3.1              │ Title Bar
├────────────────────────────────────────────────────────────────┤
│ [Main] Advanced  Power  Boot  Security  Save & Exit           │ Tab Bar
├────────────────────────────────────────────────────────────────┤
│                                                                │
│   > BIOS Information                                           │
│   ---                                                          │
│     Auto-Detect BIOS Type                                      │
│     Browse BIOS Settings                                       │
│   ---                                                          │
│     System Date and Time                                       │
│     System Memory: (Auto-detected)                             │
│     Processor: (Auto-detected)                                 │
│                                                                │
│                                                                │
├────────────────────────────────────────────────────────────────┤
│ Up/Down: Navigate | Left/Right: Switch Tabs | Enter: Select   │ Help Bar
│ Automatically detect and display BIOS vendor information      │ Description
└────────────────────────────────────────────────────────────────┘
```

## Color Scheme

- **Active Tab**: White text on Blue background
- **Inactive Tab**: Light Gray text on Black background
- **Title Bar**: Yellow text on Blue background
- **Selected Item**: Black text on Light Gray background
- **Normal Item**: Light Gray text on Black background
- **Disabled/Info**: Dark Gray text on Black background
- **Hidden/Unlocked**: Light Green text on Black background

## Technical Implementation

### Tab Structure

Each tab contains:
- Tab Name (displayed in tab bar)
- Associated Menu Page (content)
- Enabled status
- Tag identifier

### Context Management

- Tab state is preserved when switching between tabs
- Each tab's selected item position is remembered
- Current tab index is tracked globally
- Tab rendering is integrated into MenuDraw()

### Navigation Flow

1. User presses LEFT/RIGHT arrow key
2. MenuHandleInput() detects tab navigation
3. MenuSwitchTab() updates CurrentTabIndex and CurrentPage
4. MenuDraw() redraws screen with new tab active
5. User can now navigate items in the new tab

## Compatibility

- Works with existing SREP functionality
- Compatible with all supported BIOS types (AMI, Insyde, Phoenix)
- Can be disabled to use traditional single-menu interface
- No changes to underlying NVRAM operations

## Future Enhancements

Planned improvements for the tab interface:
- Dynamic tab content based on detected BIOS
- Save/restore tab preferences
- Customizable tab layouts
- Search across all tabs
- Tab-specific hotkeys (F1-F6)

## Troubleshooting

### Tab Bar Not Visible
- Ensure SREP_BiosTab.flag file exists
- Check screen dimensions (minimum 80x25)
- Verify UseTabMode flag is set

### Navigation Not Working
- Verify LEFT/RIGHT keys are functional
- Check that tabs have valid pages assigned
- Ensure Context->UseTabMode is TRUE

### Items Not Displaying
- Check ItemCount in each tab's page
- Verify MenuAddXXX functions completed successfully
- Ensure screen height accommodates all items

## Example Usage

```c
// Initialize tab mode
EFI_STATUS Status = MenuInitializeTabs(&MenuCtx, 6);

// Create pages for each tab
MENU_PAGE *MainPage = MenuCreatePage(L"Main Configuration", 8);
// ... populate MainPage items ...

// Add tabs
MenuAddTab(&MenuCtx, 0, L"Main", MainPage);
MenuAddTab(&MenuCtx, 1, L"Advanced", AdvancedPage);
// ... add other tabs ...

// Start with first tab
MenuSwitchTab(&MenuCtx, 0);

// Run menu
MenuRun(&MenuCtx, NULL);  // NULL for tab mode
```

## Credits

BIOS-style tab interface designed to replicate traditional UEFI BIOS setup screens for familiarity and ease of use.
