# AMI BIOS Configuration Guide for SREP

This guide provides step-by-step instructions for creating SREP configuration files for AMI BIOS systems.

## Prerequisites

1. **SREP** (SmokelessRuntimeEFIPatcher) - This tool
2. **UEFITool** - For extracting BIOS modules ([Download](https://github.com/LongSoft/UEFITool))
3. **IFRExtractor** - For extracting readable setup forms ([Download](https://github.com/LongSoft/Universal-IFR-Extractor))
4. **Your BIOS Image** - Backup of your system's BIOS/UEFI firmware
5. **Hex Editor** (optional) - For manual pattern analysis

## Step 1: Extract Your BIOS Image

### Option A: From Official Source
1. Download the latest BIOS from your motherboard manufacturer's website
2. Extract the archive - the BIOS image is usually a `.bin`, `.rom`, or `.cap` file

### Option B: From Your System
```bash
# Linux (requires root)
sudo flashrom -r bios_backup.bin

# Windows (use AFUWIN or similar tools from manufacturer)
afuwin64.exe bios_backup.bin /O
```

> **⚠️ IMPORTANT**: Always keep a backup of your original BIOS!

## Step 2: Identify Setup Module

1. Open your BIOS image in UEFITool
2. Use Search (Ctrl+F) → "Text" tab → Search for "Setup" or "TSE"
3. Common module names to look for:
   - **Setup** (most common)
   - **TSE** (Text Setup Environment)
   - **SetupBrowser**
   - **SetupUtility**
   - **ClickBiosSetup** (MSI boards)

4. Right-click the module → "Extract body..."
5. Save as `Setup.pe32` or similar

## Step 3: Extract IFR (Internal Form Representation)

1. Run IFRExtractor on the extracted module:
   ```bash
   # Universal IFR Extractor
   ifrextract Setup.pe32 Setup.txt
   
   # Or use the GUI version
   ```

2. Open the resulting `Setup.txt` file

## Step 4: Find Form GUIDs

The IFR file contains form definitions. Look for hidden menus:

```
Form: Advanced, FormId: 0x1234 {01 86 34 12 56 78}
  Guid: {12345678-1234-1234-1234-123456789ABC}, Optional data: [29, 8F]
  
  SuppressIf {0A 82}
    // This form is suppressed (hidden)!
  End {29 02}
  
  // Form items...
End {29 02}
```

**Key indicators of hidden menus:**
- `SuppressIf` - Form is conditionally hidden
- `GrayOutIf` - Form is visible but disabled
- Access level restrictions

**Extract the GUID**: `{12345678-1234-1234-1234-123456789ABC}`

## Step 5: Convert GUID to Pattern

AMI BIOS stores form visibility as a byte following the GUID. Convert the GUID to little-endian hex:

### GUID Format
```
{12345678-1234-1234-1234-123456789ABC}
 ^^^^^^^^ ^^^^ ^^^^ ^^^^ ^^^^^^^^^^^^
 4 bytes  2B   2B   2B   6 bytes
```

### Conversion (Little-endian)
```
GUID:     12345678-1234-1234-1234-123456789ABC
Bytes:    78 56 34 12 - 34 12 - 34 12 - 12 34 - 12 34 56 78 9A BC
Pattern:  78563412341234121234123456789ABC
```

### Add Visibility Flag
- `00000000` = Hidden (0x00 in 4 bytes, little-endian)
- `01000000` = Visible (0x01 in 4 bytes, little-endian)

**Search Pattern**: `78563412341234121234123456789ABC00000000`
**Replace Pattern**: `78563412341234121234123456789ABC01000000`

## Step 6: Run SREP to List Modules

1. Copy `SmokelessRuntimeEFIPatcher.efi` to a USB drive (FAT32)
2. Boot from the USB in UEFI mode
3. Run SREP (it will fail without config, but creates log)
4. Check `SREP.log` for loaded module names
5. Or use the helper script:
   ```bash
   ./ami_helper.sh
   ```

Look for your Setup module name in the output.

## Step 7: Create SREP Configuration

Create `SREP_Config.cfg` on your USB drive:

```
Op Loaded
Setup
Op Patch
Pattern
78563412341234121234123456789ABC00000000
78563412341234121234123456789ABC01000000
Op End

Op LoadFromFV
SetupUtilityApp
Op Exec
```

**Replace:**
- `Setup` - with your actual module name from Step 6
- `78563412...00000000` - with your search pattern from Step 5
- `78563412...01000000` - with your replace pattern from Step 5

## Step 8: Test Your Configuration

1. Reboot with the USB drive containing:
   - `SmokelessRuntimeEFIPatcher.efi`
   - `SREP_Config.cfg` (at root of drive)

2. Boot into UEFI shell or use UEFI boot manager to run SREP

3. Check `SREP.log` for results:
   - "Found pattern at offset 0x..." = Success!
   - "Pattern not found" = Wrong GUID or module name

## Common Patterns to Unlock

### Example: Advanced Menu
```
Op Loaded
Setup
Op Patch
Pattern
[YOUR_ADVANCED_MENU_GUID_HERE]00000000
[YOUR_ADVANCED_MENU_GUID_HERE]01000000
Op End
```

### Example: Multiple Menus
```
Op Loaded
Setup
Op Patch
Pattern
[ADVANCED_GUID]00000000
[ADVANCED_GUID]01000000
Op Patch
Pattern
[CHIPSET_GUID]00000000
[CHIPSET_GUID]01000000
Op Patch
Pattern
[POWER_GUID]00000000
[POWER_GUID]01000000
Op End

Op LoadFromFV
SetupUtilityApp
Op Exec
```

## Troubleshooting

### Pattern Not Found
**Problem**: Log shows "Pattern not found in image"

**Solutions**:
1. Verify GUID byte order (must be little-endian)
2. Check if visibility flag is different (try other patterns)
3. Module might use different structure - analyze with hex editor
4. Try searching without the visibility flag first

### Wrong Module Name
**Problem**: Log shows "Module not found" or fails to load

**Solutions**:
1. Check `SREP.log` for actual module name
2. Run `ami_helper.sh` to list all modules
3. Try alternative names: `Setup`, `TSE`, `SetupBrowser`

### No Setup UI Appears
**Problem**: Patch succeeds but no setup menu shows

**Solutions**:
1. Add the `LoadFromFV` and `Exec` commands to launch setup
2. Try different setup app names: `SetupUtilityApp`, `Setup`, `BiosSetup`
3. Check if your platform requires specific execution method

### System Hangs or Crashes
**Problem**: System freezes after running SREP

**Solutions**:
1. **Immediately check your patterns!** Wrong patterns can corrupt memory
2. Verify GUID format and byte order
3. Test with one pattern at a time
4. Check BIOS protections (Secure Boot, Boot Guard)

## Advanced Techniques

### Finding Offset Manually
If pattern matching fails, use offset-based patching:

1. Open Setup module in hex editor
2. Search for your GUID bytes
3. Note the offset (e.g., `0x1234`)
4. Create config:
   ```
   Op Loaded
   Setup
   Op Patch
   Offset
   1234
   01000000
   Op End
   ```

### Multiple BIOS Versions
For different BIOS versions, use relative offsets:

```
Op Loaded
Setup
Op Patch
Pattern
[KNOWN_CONSTANT_PATTERN]
[KNOWN_CONSTANT_PATTERN]
Op Patch
RelPosOffset
10
01000000
Op End
```

This finds a known pattern, then patches 0x10 bytes ahead.

## Safety Guidelines

1. **Always backup your BIOS** before experimenting
2. **Test one menu at a time** to identify issues
3. **Know your recovery method**: BIOS Flashback, dual BIOS, CH341A programmer
4. **Check compatibility**: Not all AMI BIOS versions work the same way
5. **Respect security**: Don't bypass security features maliciously

## Vendor-Specific Notes

### ASUS
- Module name: Usually `Setup` or `SetupUtility`
- Often requires `SetupUtilityApp` to launch UI

### MSI
- Module name: `ClickBiosSetup` or `Setup`
- May have additional menu protections

### ASRock
- Module name: `Setup`
- Generally straightforward implementation

### Gigabyte
- Module name: `Setup` or `TSE`
- Some versions use custom form structures

## Resources

- [UEFITool](https://github.com/LongSoft/UEFITool)
- [Universal IFR Extractor](https://github.com/LongSoft/Universal-IFR-Extractor)
- [BIOS Modding Wiki](https://winraid.level1techs.com/)
- [SREP Community Patches](https://github.com/Maxinator500/SREP-Patches)

## Getting Help

If you're stuck:
1. Check `SREP.log` for detailed error messages
2. Verify your GUID extraction with IFRExtractor output
3. Compare with existing configs in community repositories
4. Share your IFR output (sanitized) in modding forums

## Example Success Log

When everything works, your `SREP.log` should show:

```
Welcome to SREP (Smokeless Runtime EFI Patcher) 0.1.4c
=== Listing All Loaded Modules ===
Found 124 loaded modules:
  [42] Setup (Base: 0x7B2E4000, Size: 0x2A000)
  ...
=== End of Module List ===

Parsing Config
Executing Loaded OP
Found pattern at offset 0x1A234
Successfully patched 4 bytes at offset 0x1A234
```

Good luck, and happy BIOS modding! 🔧
