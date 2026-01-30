# SREP_CONFIG.cfg Documentation

## Overview
This configuration file is designed to unlock hidden advanced menus and options in HP AMI-based BIOS firmware. It specifically targets the BIOS binary `o.bin` from the A73 release.

## Target BIOS Information
- **Type**: HP AMI BIOS
- **Source**: https://github.com/EduardoA3677/UEFITool/releases/download/A73/o.bin
- **Size**: ~8 MB
- **Format**: UEFI Firmware Volume (FV)

## BIOS Architecture
This is an AMI (American Megatrends Inc.) BIOS customized for HP hardware. Key components:
- **AMITSE** (AMI Text Setup Environment): The setup UI engine
- **Setup Variables**: NVRAM variables containing BIOS settings
- **HPSetupData**: HP-specific setup configuration
- **HPAmiTse**: HP's customization layer for AMITSE

## Modules Identified

### 1. Setup Module
- **Location**: NVRAM variable `Setup`
- **Function**: Main BIOS setup configuration storage
- **Structure**: Contains access level flags and option visibility settings
- **Pattern Location**: 0x3c00b2 in o.bin

### 2. AMITSESetup Module
- **Location**: NVRAM variable `AMITSESetup`
- **Function**: AMITSE configuration and behavior control
- **Pattern Location**: 0x3c02d3 in o.bin
- **Purpose**: Controls which forms and options are visible in setup UI

### 3. HPSetupData Module
- **Location**: NVRAM variable `HPSetupData`
- **Function**: HP-specific BIOS options
- **Pattern Location**: 0x3c0db5 in o.bin
- **Contains**: HP diagnostic menus, hardware configuration options

### 4. HPAmiTse Module
- **Type**: DXE Driver (loaded at runtime)
- **Function**: HP's customization of AMITSE
- **Purpose**: Implements HP-specific UI behavior and menu restrictions

### 5. SetupUtilityApp
- **Type**: UEFI Application
- **Function**: The actual BIOS setup UI application
- **Location**: Firmware Volume (FV)
- **Purpose**: Provides the user interface for BIOS configuration

## Unlock Strategy

### Access Level System
AMI BIOS uses a hierarchical access level system:
- **0x00**: Hidden/Disabled (completely hidden from UI)
- **0x01**: User Level (visible to all users)
- **0x02**: Advanced Level (requires elevated access)
- **0x03**: OEM/Manufacturing Level (typically locked)

### Patch Operations

#### Patch 1: Setup Variable Access Levels
**Pattern Matched**:
```
5365747570000001000101000100000000000000020101010002010000000000
```

**Replacement**:
```
5365747570000001010101000101010101010101020101010102010101010101
```

**Effect**: 
- Changes access level flags from 0x00 (hidden) to 0x01 (visible)
- Enables visibility of restricted setup options
- Affects: Main setup menu structure

#### Patch 2: HPSetupData Unlock
**Pattern Matched**:
```
485053657475704461746100000101000000000001000200030004000500
```

**Replacement**:
```
485053657475704461746100000101010101010101010202020304040505
```

**Effect**:
- Enables HP-specific diagnostic menus
- Unlocks hardware configuration options
- Reveals hidden HP utilities

#### Patch 3: AMITSESetup Form Visibility
**Pattern Matched**:
```
414D4954534553657475700000000000000000000000000000000000000000000000
```

**Replacement**:
```
414D4954534553657475700101010101010101010101010101010101010101010101
```

**Effect**:
- Forces all AMITSE forms to visible state
- Bypasses form suppression checks
- Enables hidden setup pages

#### Patch 4: Setup Access Verification
**Pattern Matched**:
```
00000000000002010101000201000000000001020000
```

**Replacement**:
```
01010101010102010101010201010101010101020101
```

**Effect**:
- Modifies access level verification logic
- Allows access to advanced configuration options
- Bypasses permission checks

#### Patch 5: HPAmiTse Extended Controls
**Pattern Matched**:
```
00010000000100000001000000
```

**Replacement**:
```
01010101010101010101010101
```

**Effect**:
- Enables HP-specific advanced features
- Unlocks diagnostic tools
- Reveals hidden system information

## Execution Flow

1. **Initialization**: SREP loads and parses SREP_CONFIG.cfg
2. **Module Loading**: Identifies already-loaded DXE drivers (Setup, AMITSESetup, HPAmiTse, HPSetupData)
3. **Pattern Matching**: Searches each module's memory space for specified hex patterns
4. **Patching**: Replaces matched patterns with unlock patterns in RAM
5. **Setup Launch**: Loads SetupUtilityApp from firmware volume
6. **Execution**: Runs setup application with all patches active in memory
7. **Result**: User sees setup UI with previously hidden menus enabled

## Expected Results

### Unlocked Menus
After successful execution, you should see:
- **Advanced Menu**: CPU, chipset, and memory configuration options
- **Diagnostic Menu**: Hardware diagnostics and testing utilities
- **Power Management**: Extended power and thermal controls
- **Security**: Advanced security configuration options
- **Boot Options**: Extended boot configuration
- **HP Utilities**: OEM diagnostic and configuration tools

### Newly Visible Options
- CPU microcode control
- Memory timing adjustments
- PCIe configuration
- SATA/NVMe advanced settings
- USB advanced configuration
- Network boot options
- TPM advanced settings
- Secure boot key management
- System information (normally hidden)

## Technical Notes

### Pattern Search Algorithm
SREP uses linear search through loaded module memory:
- Searches byte-by-byte for exact pattern match
- First match only is replaced
- No wildcard or fuzzy matching

### Memory Safety
- All patches are applied in RAM only
- Original BIOS flash is never modified
- Changes are lost on reboot (non-persistent)
- Safe to experiment without bricking risk

### Module Loading
The `Loaded` directive targets already-loaded DXE drivers:
- SREP searches the DXE database for module by name
- Only works for modules loaded before SREP execution
- Cannot patch modules loaded later in boot process

### Firmware Volume Loading
The `LoadFromFV` directive:
- Searches BIOS FV for named module
- Extracts PE32 section
- Loads into memory for execution
- Used for SetupUtilityApp which isn't normally loaded at boot

## Limitations

### Pattern Specificity
Patterns are BIOS-specific:
- These patterns work for this specific HP AMI BIOS build
- Different BIOS versions will have different patterns
- May need adjustment for different HP models

### Incomplete Unlock
Some options may remain hidden:
- Options suppressed by IFR conditionals in forms
- Hardware-dependent options (may not appear if HW not detected)
- OEM-locked options requiring signed BIOS modifications

### No Persistence
Changes are runtime-only:
- Settings made in unlocked menus may not persist
- Some options may be reset by SMM code
- NVRAM variables might be validated and rejected

## Troubleshooting

### Pattern Not Found
If a pattern isn't found:
- Module may not be loaded yet
- BIOS version mismatch
- Pattern offset changed in BIOS update
- Module name changed

**Solution**: Use UEFITool/UEFIExtract to analyze your specific BIOS

### Setup Doesn't Launch
If SetupUtilityApp doesn't execute:
- Wrong module name (might be "Setup" or "SetupApp")
- Module compressed differently
- Module signature check failing

**Solution**: Check log file (SREP.log) for error messages

### Menus Still Hidden
If menus remain hidden after patching:
- Patterns matched but logic is different
- Additional suppression layers active
- IFR-level suppression not addressed

**Solution**: Need deeper analysis with IFR extraction

## Advanced Techniques

### IFR Analysis
For complete unlock, analyze IFR forms:
1. Extract Setup module with UEFITool
2. Extract IFR section with ifrextract
3. Identify suppress-if opcodes (0x5F 0x0A)
4. Patch opcodes or conditions

### Variable Monitoring
Monitor which variables suppress options:
1. Enable all suppressions in SREP config
2. Use UEFI variable dump tool
3. Identify which variables control visibility
4. Add targeted patches for those variables

### Form GUID Discovery
Find hidden form GUIDs:
1. Extract all forms from BIOS
2. Compare visible vs. total forms
3. Target hidden form GUIDs directly
4. Use Loaded + Patch to enable specific forms

## Safety Warnings

### Non-Persistent Nature
- All changes are RAM-only and temporary
- No risk of bricking the system
- Reboot returns to normal operation

### Setting Validation
- Unlocked options may not be validated
- Invalid settings can cause instability
- Always test in safe environment first

### Warranty Implications
- Using this tool may void warranty
- HP may detect modifications
- Use at your own risk

## References

### AMI BIOS Structure
- AMITSE: AMI's setup UI framework
- IFR: Internal Forms Representation (setup form language)
- HII: Human Interface Infrastructure (UEFI UI system)

### UEFI Specifications
- UEFI 2.x specification for HII protocols
- PI specification for DXE driver loading
- Variable services for NVRAM access

### Tools Used
- **UEFITool**: For BIOS analysis and module extraction
- **IFRExtract**: For extracting and analyzing setup forms
- **SREP**: Runtime patching tool (this tool)

## Version History
- **v1.0**: Initial configuration for HP AMI BIOS (o.bin A73)
  - Basic Setup variable unlock
  - AMITSESetup form visibility
  - HPSetupData options enabled
  - HPAmiTse controls unlocked

## Credits
- SmokelessCPUv2: Original SREP tool author
- HP/AMI: BIOS architecture
- Community: Pattern discovery and analysis techniques
