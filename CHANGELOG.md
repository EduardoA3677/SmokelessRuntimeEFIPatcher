# Changelog

All notable changes to SmokelessRuntimeEFIPatcher will be documented in this file.

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
