# SmokelessRuntimeEFIPatcher - Complete User Guide

## Overview

SmokelessRuntimeEFIPatcher (SREP) v0.3.1 is an advanced UEFI/BIOS configuration tool that provides a BIOS-like interface for viewing and modifying system settings. It dynamically extracts real configuration forms from your BIOS and organizes them into an intuitive tabbed interface.

## Key Features

### 🎯 What Makes SREP Special

1. **Real BIOS Data Extraction**: Unlike other tools, SREP doesn't show placeholder data—it extracts actual form titles, options, and values from your BIOS's HII (Human Interface Infrastructure) database.

2. **BIOS-Style Interface**: Navigate using LEFT/RIGHT arrows through tabs (Main, Advanced, Power, Boot, Security, Save & Exit) just like in a real BIOS setup screen.

3. **Direct NVRAM Access**: All changes are saved directly to your system's NVRAM, persisting across reboots.

4. **Hidden Menu Unlocking**: Automatically detects and displays previously hidden or suppressed BIOS options.

5. **Multi-BIOS Support**: Works with AMI, Insyde H2O, Phoenix, and HP custom BIOS implementations.

## Quick Start

### Prerequisites
- UEFI-capable system (not Legacy BIOS)
- USB drive formatted as FAT32
- Basic knowledge of BIOS settings

### Installation
1. Build or download `SmokelessRuntimeEFIPatcher.efi`
2. Copy to USB drive (e.g., to `E:\EFI\BOOT\`)
3. Create `SREP_BiosTab.flag` file in same directory
4. Reboot system

### Booting SREP
1. Enter UEFI Shell (usually F2, F12, or Del during boot)
2. Navigate to USB drive: `fs0:`, `fs1:`, etc.
3. Run: `SmokelessRuntimeEFIPatcher.efi`

### First Launch
```
Welcome to SREP v0.3.1
Enhanced with Auto-Detection and Intelligent Patching

=== Extracting Real BIOS Forms ===
Found 15 HII package lists
Extracted 47 real BIOS forms from HII database

Creating dynamic tabs from 47 BIOS forms...
  Main: 8 forms
  Advanced: 21 forms
  Power: 6 forms
  Boot: 7 forms
  Security: 3 forms
  Save & Exit: 2 forms

Press any key to enter BIOS-style interface...
```

## Interface Guide

### Tab Bar Layout
```
┌────────────────────────────────────────────────────┐
│    SREP - SmokelessRuntimeEFIPatcher v0.3.1        │
├────────────────────────────────────────────────────┤
│ [Main] Advanced  Power  Boot  Security  Save&Exit  │ ← Tab Bar
├────────────────────────────────────────────────────┤
│  > System Information                               │
│    Date and Time Configuration                      │
│    Memory Configuration                             │
│    Processor Information                            │
│  ---                                                │
│    8 configuration form(s) in this category         │
├────────────────────────────────────────────────────┤
│ Up/Down: Navigate | Left/Right: Switch Tabs        │
│ System BIOS information and basic settings          │
└────────────────────────────────────────────────────┘
```

### Color Coding
- **White/Blue** (`[Active]`): Currently selected tab
- **Gray** (`Inactive`): Other tabs
- **Light Gray**: Normal menu items
- **Black/White**: Selected item
- **Light Green**: Previously hidden/unlocked options
- **Cyan**: Help text at bottom

### Keyboard Controls

#### Tab Navigation
- **LEFT Arrow**: Previous tab
- **RIGHT Arrow**: Next tab
- **HOME**: Jump to Main tab
- **END**: Jump to Save & Exit tab

#### Menu Navigation
- **UP Arrow**: Previous item
- **DOWN Arrow**: Next item
- **Page Up**: Scroll up 10 items
- **Page Down**: Scroll down 10 items

#### Actions
- **ENTER**: Select item / Edit value
- **ESC**: Go back / Cancel
- **F1**: Help (if available)
- **F9**: Load defaults (if available)
- **F10**: Save and exit (in Save & Exit tab)

## Tab Organization

### Main Tab
Contains fundamental system information:
- System Information
- Date and Time
- Memory Configuration
- Processor Details
- BIOS Version Info

### Advanced Tab
Advanced configuration options:
- CPU Configuration
- Chipset Settings
- PCIe/PCI Configuration
- USB Configuration
- SATA Configuration
- Onboard Devices
- Legacy Support Options

### Power Tab
Power management settings:
- ACPI Configuration
- Suspend/Resume Options
- Wake Events
- Power Button Behavior
- Thermal Management

### Boot Tab
Boot-related settings:
- Boot Order Configuration
- Boot Mode (UEFI/Legacy)
- Fast Boot
- Secure Boot Options
- Boot Delay
- Network Boot

### Security Tab
Security features:
- Secure Boot Configuration
- Password Protection
- TPM (Trusted Platform Module)
- Boot Guard
- Administrator Password
- User Password

### Save & Exit Tab
Exit options:
- Save Changes and Exit
- Discard Changes and Exit
- Save Changes (stay in setup)
- Load Optimal Defaults
- Load Custom Defaults
- About SREP

## Editing Values

### Numeric Values
When you select a numeric option:

```
+--------------------------------------------------+
| Edit Value                                       |
|                                                  |
| Value: 2400                                      |
|                                                  |
| +/- to change | Enter to save | ESC to cancel   |
+--------------------------------------------------+

Min: 800, Max: 3200, Step: 100
Current: 2400
```

**Controls:**
- `+` or `=`: Increase value
- `-` or `_`: Decrease value
- **ENTER**: Save value
- **ESC**: Cancel

### Boolean Options (Checkbox)
- **ENTER**: Toggle Enabled/Disabled
- Changes are staged immediately

### Dropdown Options (OneOf)
When implemented, will show:
```
Select an option:
  ○ Option 1
  ● Option 2 (Current)
  ○ Option 3
  ○ Option 4
```

### String Values
Text input dialog (when fully implemented):
```
Enter text (max 32 characters):
[Current Value________________]

Type new value, Enter to save, ESC to cancel
```

## Workflow Examples

### Example 1: Enable Secure Boot
1. Boot SREP
2. Press RIGHT arrow to navigate to **Security** tab
3. Use DOWN arrow to select "Secure Boot Configuration"
4. Press ENTER
5. Select "Secure Boot" option
6. Press ENTER to toggle to "Enabled"
7. Press RIGHT arrow twice to **Save & Exit** tab
8. Select "Save Changes and Exit"
9. Press ENTER
10. System reboots with Secure Boot enabled

### Example 2: Change CPU Fan Speed
1. Navigate to **Advanced** tab
2. Find "CPU Configuration" or "Hardware Monitor"
3. Press ENTER
4. Find "CPU Fan Speed Control"
5. Press ENTER to edit
6. Use +/- to adjust value
7. Press ENTER to save
8. Go to **Save & Exit** tab
9. Save changes

### Example 3: View Hidden Options
1. SREP automatically detects hidden forms
2. Look for items marked in **light green**
3. These were previously hidden via SuppressIf
4. You can now access them normally
5. Some may still be grayed out if dependencies aren't met

## Advanced Features

### Auto Mode
Skip the menu and auto-patch immediately:
1. Delete `SREP_BiosTab.flag`
2. Create `SREP_Auto.flag`
3. Run SREP
4. System will auto-detect and patch

### Manual Configuration
For advanced users:
1. Create `SREP_Config.cfg`
2. Specify custom settings
3. SREP will apply configuration

### Logging
All operations are logged to `SREP.log` on the USB drive:
```
Welcome to SREP v0.3.1
Enhanced with Auto-Detection and Intelligent Patching
BIOS-style tab interface mode enabled

=== INTERACTIVE MODE: Starting Menu ===
Found 15 HII package lists
Extracted 47 real BIOS forms
Creating dynamic tabs...
```

## Troubleshooting

### "No HII Package Lists Found"
**Cause**: Your BIOS doesn't use HII or SREP can't access it
**Solution**: Try auto mode, or use traditional BIOS setup

### "Failed to Enumerate BIOS Forms"
**Cause**: HII database access issue
**Solution**: 
- Reboot and try again
- Update system firmware
- Check UEFI compatibility

### Tab Bar Not Showing
**Cause**: Missing `SREP_BiosTab.flag` file
**Solution**: Create the flag file in same directory as EFI

### Changes Not Persisting
**Cause**: NVRAM write protection or save not confirmed
**Solution**:
- Ensure you selected "Save Changes and Exit"
- Check if BIOS has NVRAM write protection
- Some settings require specific boot mode

### System Won't Boot After Changes
**Cause**: Invalid configuration
**Solution**:
1. Boot back to SREP
2. Navigate to Save & Exit tab
3. Select "Load Optimal Defaults"
4. Save and exit
5. Or clear CMOS (consult motherboard manual)

## Safety Precautions

### ⚠️ Important Warnings

1. **Always Read First**: Understand what a setting does before changing it
2. **Backup Current Settings**: Take photos or notes of current config
3. **One Change at a Time**: Don't change multiple critical settings at once
4. **Test Thoroughly**: Verify system stability after changes
5. **Have Recovery Plan**: Know how to reset BIOS (CMOS clear)

### ⚠️ Dangerous Settings

Be especially careful with:
- **CPU Voltage/Clock**: Can damage hardware if set too high
- **Memory Timings**: System instability possible
- **PCIe Configuration**: Can prevent boot if misconfigured
- **Secure Boot**: May prevent OS boot if not configured properly
- **RAID Settings**: Can result in data loss

### ⚠️ Do NOT Modify

Unless you know what you're doing:
- Firmware update settings
- ME (Management Engine) configuration
- TPM clearing options
- Boot Guard settings
- Critical security settings

## Technical Information

### Supported BIOS Types
- **AMI BIOS**: Full support, all features
- **HP AMI**: Custom HP modules supported
- **Insyde H2O**: Full support
- **Phoenix**: Full support
- **Award**: Partial support

### NVRAM Storage
- Uses EFI Runtime Services
- Changes persist across reboots
- Stored in BIOS flash memory
- Can be cleared via CMOS reset

### Form Detection
- Scans HII database at runtime
- Parses IFR (Internal Form Representation)
- Extracts forms, questions, and options
- Detects suppression conditions

### Value Types Supported
- **UINT8**: 1-byte unsigned integer (0-255)
- **UINT16**: 2-byte unsigned integer (0-65535)
- **UINT32**: 4-byte unsigned integer
- **UINT64**: 8-byte unsigned integer
- **BOOLEAN**: True/False (1/0)
- **STRING**: Character strings

## FAQ

**Q: Will SREP brick my system?**
A: SREP only modifies NVRAM settings, same as BIOS setup. However, incorrect settings can prevent boot. Always have a recovery method ready.

**Q: Can I undo changes?**
A: Yes, use "Discard Changes and Exit" or "Load Optimal Defaults". Individual undo not yet implemented.

**Q: Why are some options missing?**
A: Some options may be:
- Hidden by BIOS vendor
- Dependent on other settings
- Not available on your hardware
- In a different form

**Q: Is SREP safe to use?**
A: SREP is as safe as using BIOS setup. It reads/writes the same NVRAM variables. Use caution with unknown settings.

**Q: Can I use SREP on a laptop?**
A: Yes, but be extra careful with:
- Power settings
- Display configurations
- Battery options
- Laptop-specific features

**Q: Does SREP work with Secure Boot enabled?**
A: SREP needs to be signed or Secure Boot must be disabled to run.

**Q: How do I get back to normal BIOS setup?**
A: Simply reboot without running SREP. Your normal BIOS setup (F2/Del) will work as before.

## Support and Contributing

### Getting Help
- Read IMPLEMENTATION_SUMMARY.md for technical details
- Check BIOS_TAB_INTERFACE.md for interface guide
- Review ARCHITECTURE.md for system design

### Reporting Issues
Include:
- BIOS type and version
- SREP.log file content
- Steps to reproduce
- Expected vs actual behavior

### Contributing
- Submit pull requests on GitHub
- Follow existing code style
- Add tests for new features
- Update documentation

## Version History

**v0.3.1** (Current)
- Dynamic BIOS form extraction
- BIOS-style tab interface
- Real question parsing
- NVRAM integration
- Critical bug fixes
- Code refactoring

**v0.3.0**
- Initial HII browser
- Basic menu system
- NVRAM manager

**v0.2.x**
- Auto-detection
- Patching engine

**v0.1.x**
- Basic runtime patching

## Legal Disclaimer

This software is provided "as is" without warranty. Use at your own risk. The authors are not responsible for any damage caused by using this software. Always backup your data and have a recovery plan before modifying BIOS settings.

## Credits

- **TianoCore EDK2**: UEFI development framework
- **UEFI Forum**: Specifications and standards
- **Community Contributors**: Testing and feedback

---

**SmokelessRuntimeEFIPatcher v0.3.1**
© 2024 - Enhanced UEFI BIOS Configuration Tool
