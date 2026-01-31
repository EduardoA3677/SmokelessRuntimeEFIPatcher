# Changelog

All notable changes to SmokelessRuntimeEFIPatcher will be documented in this file.

## [0.3.2] - 2026-01-31

### Added - Navigation Improvements
- **Home/End key support**: Jump directly to first/last enabled menu item
- **Page Up/Down support**: Scroll through menus 10 items at a time for efficient navigation
- **Enhanced help system**: F1 now shows comprehensive navigation guide with all available keys
- **FindLastEnabledItem helper**: New function to support End key navigation

### Fixed - Critical Issues
- **Bounds checking**: SelectedIndex now properly validated before array access (line 776)
  - Previously could cause out-of-bounds access if index became invalid
  - Now resets to first enabled item on invalid index
- **Tab wrapping behavior**: Tabs now stop at boundaries instead of circular wrapping
  - Left arrow on first tab no longer wraps to last tab
  - Right arrow on last tab no longer wraps to first tab
  - Matches standard BIOS behavior
- **Error handling**: Added proper status checking for protocol calls in main entry point
  - HandleProtocol calls now check return status
  - OpenVolume call now validates success
  - Better error messages for failed operations

### Improved - Code Quality
- **Multi-line message support**: MenuShowMessage now handles line breaks (\r\n) properly
  - Displays up to 5 lines of text in message boxes
  - Truncates long lines to fit within box (max 46 chars per line)
  - Prevents buffer overflows with safe string functions
- **Better user feedback**: Replaced "(Not implemented)" placeholders with informative messages
  - F9: Now explains feature is planned for future versions
  - F10: Provides context-aware save messages
- **Protocol error handling**: Fixed uninitialized HandleProtocol variable issue
  - Changed from function pointer to direct gBS->HandleProtocol calls
  - Proper error checking and user feedback on failures
- **Code readability**: Improved comments and removed redundant code
  - Cleaner function structure
  - Better separation of concerns

### Technical Details
- All navigation keys now properly implemented:
  - ↑↓: Navigate items
  - ←→: Switch tabs (stops at boundaries)
  - Home/End: Jump to first/last item
  - Page Up/Down: Scroll by 10 items
  - Enter: Select/activate item
  - ESC: Back/exit
  - F1: Help
  - F9: Load defaults (planned)
  - F10: Save & exit

## [0.3.1] - 2026-01-30

### Fixed
- Critical bug: ItemCount initialization in CreateMainMenu (was 0, should be 11)
  - This caused all menu items to fail adding silently
  - Menu would appear empty or malfunction
- Memory leak: Root file handle never closed
  - Added proper cleanup in all exit paths
- Resource management: Improved error path cleanup
  - All error exits now properly close LogFile and Root handles

### Changed
- Simplified ConfigManager: Removed all file I/O operations
  - Deleted ConfigSaveToFile, ConfigLoadFromFile, ConfigExportToText, ConfigImportFromText
  - Focus on direct NVRAM operations only
  - Reduced code complexity by ~1,100 lines

### Added
- Comprehensive README.md with build instructions and usage guide
- .gitignore file for proper version control
- CHANGELOG.md for tracking changes

### Improved
- Workflow improvements:
  - Explicit GCC-11 installation
  - Python3 and build dependencies properly installed
  - Parallel build support with -j$(nproc)
  - Better artifact naming with commit SHA
  - Automated release notes generation
- Code quality:
  - Fixed all C89/C90 compatibility issues
  - Proper variable declarations at function scope
  - Consistent error handling patterns
  - Better resource management

## [0.3.0] - Previous Version

### Added
- HP AMI BIOS support with specialized patching
- Direct BIOS data structure patching (PatchBiosData)
- Hidden form unlocking (UnlockHiddenForms)
- HP-specific module detection (HPSetupData, AMITSESetup)
- IFR opcode patching (SUPPRESSIF, GRAYOUTIF, DISABLEIF)

### Fixed
- Compilation errors with gBS undeclared
- Operator precedence warnings
- Loop variable redeclarations

## [0.2.0] - Earlier Version

### Added
- Interactive menu system
- BIOS auto-detection
- NVRAM direct access
- Multi-BIOS support (AMI, Insyde, Phoenix)
