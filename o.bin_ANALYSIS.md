# Analysis Report for o.bin BIOS

## BIOS Information

- **Source**: https://github.com/EduardoA3677/UEFITool/releases/download/A73/o.bin
- **File Size**: 8,388,608 bytes (8 MB)
- **BIOS Type**: AMI Aptio (American Megatrends Inc.)
- **Setup Module**: AMITSESetup (Text Setup Environment)

## Analysis Results

### Detected Modules

The following setup-related modules were identified in this BIOS:

1. **Setup** - Main setup module
2. **AMITSESetup** - AMI Text Setup Environment (primary setup handler)
3. **SecureBootSetup** - Secure Boot configuration
4. **ALCSetup** - Audio (Realtek ALC) setup
5. **HPSetupData** / **NewHPSetupData** - Hardware-specific setup data

### Module Location

- **AMITSESetup** found at offset: `0x3C02D3`
- Search range analyzed: `0x3702D3` to `0x4C02D3`

### Hidden Forms Discovered

The analysis identified **144 potential hidden forms** with visibility flag set to 0 (hidden).

#### Top Candidates for Unlocking

##### Pattern 1: Near "Advanced" Menu
- **Offset**: `0x57f0b4`
- **GUID (Little-Endian)**: `040400030003040B808000030407080B`
- **Current Visibility**: 0 (Hidden)
- **Context**: Found near "Advanced Micro Devices, Inc." string
- **Search Pattern**: `040400030003040B808000030407080B00000000`
- **Replace Pattern**: `040400030003040B808000030407080B01000000`

##### Pattern 2: Near "Security" Menu
- **Offset**: `0x3c1a38`
- **GUID (Little-Endian)**: `521C0052B80082074D656D4365696C2E`
- **Current Visibility**: 0 (Hidden)
- **Context**: Found near "Security" keyword in setup data
- **Search Pattern**: `521C0052B80082074D656D4365696C2E00000000`
- **Replace Pattern**: `521C0052B80082074D656D4365696C2E01000000`

##### Pattern 3: Setup Module Area
- **Offset Range**: `0x3c00af` - `0x3c02da`
- **Multiple GUIDs**: Various form structures in this range
- **Note**: These appear to be part of the setup module's internal structures

### Verified Menu Locations

The following menus were confirmed to exist in the BIOS:

- **Main Menu**: Found at `0x3c051d` (Currently Visible)
- **Boot Menu**: Found at `0x3c04ed` (Currently Visible)
- **Advanced**: String found at `0x57f108` (Menu may be hidden)
- **Security**: String found at `0x3c1a38` (Various security options)

## Recommended Configuration

### Primary Configuration (SREP_Config.cfg)

```
Op Loaded
Setup
Op Patch
Pattern
040400030003040B808000030407080B00000000
040400030003040B808000030407080B01000000
Op End

Op Loaded
AMITSESetup
Op Patch
Pattern
521C0052B80082074D656D4365696C2E00000000
521C0052B80082074D656D4365696C2E01000000
Op End

Op LoadFromFV
SetupUtilityApp
Op Exec
```

## Usage Instructions

### Step 1: Prepare USB Drive
1. Format a USB drive as FAT32
2. Copy `SmokelessRuntimeEFIPatcher.efi` to the root
3. Copy `SREP_Config.cfg` to the root

### Step 2: Boot and Run
1. Boot your system with the USB drive
2. Enter UEFI Shell or use boot menu to run the .efi file
3. Wait for SREP to complete execution

### Step 3: Check Results
1. Check `SREP.log` file created on the USB drive
2. Look for messages like:
   - "Found pattern at offset 0x..." = Success!
   - "Pattern not found" = Pattern doesn't exist or wrong module name

### Expected Log Output

**Success:**
```
Welcome to SREP (Smokeless Runtime EFI Patcher) 0.1.4c
=== Listing All Loaded Modules ===
Found pattern at offset 0x...
Successfully patched 4 bytes at offset 0x...
```

**Failure:**
```
Error: Pattern not found in image
```

## Troubleshooting

### If Pattern Not Found

1. **Wrong Module Name**
   - Try replacing `Setup` with `AMITSESetup` or vice versa
   - Check SREP.log for actual loaded module names
   
2. **BIOS Version Mismatch**
   - These patterns are specific to this BIOS dump
   - If you have a different version, you'll need to re-analyze

3. **Pattern Changed**
   - The BIOS may have been updated
   - Extract your current BIOS and re-analyze

### If System Won't Boot After Changes

**IMPORTANT**: SREP only modifies memory, not the actual BIOS chip!
- Simply restart your computer - changes are lost
- SREP does NOT permanently flash your BIOS

### Alternative Testing Methods

If the above patterns don't work, you can:

1. **Try TSE Module Directly**
   ```
   Op Loaded
   TSE
   Op Patch
   Pattern
   [YOUR_PATTERN]
   [REPLACEMENT]
   Op End
   ```

2. **Use Offset-Based Patching**
   If you know the exact offset from analysis:
   ```
   Op Loaded
   Setup
   Op Patch
   Offset
   57F0B4
   01000000
   Op End
   ```

## Additional Patterns to Try

If the main patterns don't work, here are additional candidates found in the analysis:

### Pattern Set A (Setup Module Internal Structures)
```
Op Loaded
Setup
Op Patch
Pattern
FF83005365747570000001000101000100000000
FF83005365747570000001000101000101000000
Op End
```

### Pattern Set B (Near Boot Configuration)
```
Op Loaded
AMITSESetup
Op Patch
Pattern
00010000000000000002010101000201000000 00
00010000000000000002010101000201010000 00
Op End
```

## Technical Notes

### GUID Format Conversion

The patterns use little-endian format. To convert a standard GUID:

**Standard GUID**: `{12345678-1234-5678-90AB-CDEF01234567}`

**Little-Endian**: 
- Reverse first 3 groups: `78563412-3412-7856-90AB-CDEF01234567`
- Keep last 2 groups as-is: `90ABCDEF01234567`
- Result: `78563412341278569 0ABCDEF01234567`

### Visibility Flag

The 4-byte value after the GUID:
- `00000000` = Hidden (not shown in menu)
- `01000000` = Visible (shown in menu)

This is a little-endian `uint32_t`, so value 1 = `01 00 00 00`

## Safety Information

### What SREP Does
- Modifies BIOS modules **in RAM only**
- Changes are **temporary** - lost on reboot
- **Does NOT** write to BIOS flash chip
- **Does NOT** permanently modify your system

### What SREP Does NOT Do
- Does NOT create permanent changes
- Does NOT bypass Secure Boot (if enabled)
- Does NOT unlock hardware-locked features
- Does NOT modify the BIOS file on disk

### Risks
- **Minimal**: Changes are temporary and lost on reboot
- **System hang**: Wrong patterns might cause hang - just restart
- **No bricking risk**: Your BIOS chip is never modified

## Tools Used for Analysis

1. **strings** - Extract readable strings from binary
2. **Python** - Custom scripts for pattern analysis
3. **hexdump** - Binary structure examination
4. **GUID pattern matching** - Form structure identification

## Credits

- **Original SREP Tool**: SmokelessCPUv2
- **BIOS Analysis**: Automated Python analysis scripts
- **Pattern Discovery**: Binary structure analysis and GUID extraction

## Further Reading

- [AMI BIOS Guide](AMI_BIOS_GUIDE.md) - Comprehensive AMI BIOS modding guide
- [SREP README](README.md) - Full SREP documentation
- [UEFITool](https://github.com/LongSoft/UEFITool) - For manual BIOS analysis

## Support

If these patterns don't work for your system:
1. Extract your actual BIOS using flashrom or manufacturer tools
2. Analyze it using the methods described in AMI_BIOS_GUIDE.md
3. Create custom patterns for your specific BIOS version

---

**Last Updated**: 2026-01-30
**Analysis Version**: 1.0
**BIOS MD5**: (compute with `md5sum o.bin`)
