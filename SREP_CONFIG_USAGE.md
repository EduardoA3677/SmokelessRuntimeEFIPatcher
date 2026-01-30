# SREP_CONFIG.cfg Usage Guide

## Quick Start

### Prerequisites
1. **SREP Tool**: SmokelessRuntimeEFIPatcher EFI application (compiled from this repo)
2. **Target BIOS**: o.bin from https://github.com/EduardoA3677/UEFITool/releases/download/A73/o.bin
3. **USB Drive**: FAT32 formatted USB drive for booting
4. **UEFI System**: Computer capable of booting UEFI applications

### Installation Steps

#### Step 1: Build SREP (if needed)
```bash
# Using EDK2 build environment
build -p SmokelessRuntimeEFIPatcher.dsc -a X64 -t GCC5
```

Or download pre-built EFI from releases.

#### Step 2: Prepare USB Drive
1. Format USB drive as FAT32
2. Create directory structure:
   ```
   USB:\
   ├── EFI\
   │   └── BOOT\
   │       └── BOOTX64.EFI  (SREP renamed)
   └── SREP_CONFIG.cfg
   ```

#### Step 3: Copy Files
1. Copy `SREP_CONFIG.cfg` to root of USB drive
2. Copy SREP EFI file to `\EFI\BOOT\BOOTX64.EFI`
   - Or keep as `SmokelessRuntimeEFIPatcher.efi` and boot manually

#### Step 4: Boot from USB
1. Restart computer
2. Enter UEFI boot menu (usually F9, F12, or ESC)
3. Select USB drive as boot device
4. SREP will automatically load and execute

### Alternative: Manual Execution

If you don't want to replace BOOTX64.EFI:

1. Copy files to USB:
   ```
   USB:\
   ├── SmokelessRuntimeEFIPatcher.efi
   └── SREP_CONFIG.cfg
   ```

2. Boot to UEFI Shell:
   - Access BIOS setup
   - Select "UEFI Shell" or download UEFI Shell bootloader

3. From UEFI Shell:
   ```
   fs0:
   SmokelessRuntimeEFIPatcher.efi
   ```

## What to Expect

### During Execution
1. **Initial Message**: "Welcome to SREP (Smokeless Runtime EFI Patcher)"
2. **Config Loading**: Reads SREP_CONFIG.cfg
3. **Module Patching**: 
   - Loads Setup module
   - Loads HPSetupData module
   - Loads AMITSESetup module
   - Loads HPAmiTse module
   - Applies patches to each
4. **Setup Launch**: Loads and executes SetupUtilityApp
5. **Result**: BIOS setup appears with unlocked menus

### In BIOS Setup
After successful execution, navigate through setup to find:

#### Newly Visible Menus
- **Advanced** - CPU, Chipset, Memory settings
- **Diagnostics** - Hardware testing utilities
- **Power** - Advanced power management
- **Security** - Extended security options
- **Boot** - Advanced boot configuration

#### Newly Visible Options
Look for options that were previously hidden:
- CPU microcode control
- Memory timings
- PCIe configuration
- SATA/NVMe settings
- USB configuration
- TPM settings
- Secure Boot management

## Logging and Debugging

### Log File Location
SREP creates `SREP.log` on the USB drive with detailed execution information.

### Log Contents
```
Welcome to SREP (Smokeless Runtime EFI Patcher) 0.1.4c
Opened SREP_Config
Config Size: 0x[size]
Parsing Config
[... patching operations ...]
Executing Loaded OP
Loaded Image [status] -> [address]
Executing Patch
Finding Offset
Found at [offset]
Patched
```

### Troubleshooting

#### Config Not Found
**Error**: "Failed on Opening SREP_Config"
**Solution**: 
- Ensure `SREP_CONFIG.cfg` is in root of USB drive
- Check filename is exact (case-sensitive on some systems)

#### Module Not Loaded
**Error**: Module name not found in loaded images
**Solution**:
- Check BIOS has that module (HP AMI BIOS specific)
- Verify spelling in config file
- Some modules may load with different names

#### Pattern Not Found
**Log**: "No Pattern Found"
**Solution**:
- BIOS version mismatch
- Extract patterns from your specific BIOS using UEFITool
- Update patterns in SREP_CONFIG.cfg

#### Setup Doesn't Launch
**Error**: Can't locate SetupUtilityApp
**Solutions**:
- Try alternative names: "Setup", "SetupApp", "SetupUtility"
- Check if setup is in firmware volume
- May need to extract exact name from BIOS

## Configuration Customization

### Finding Patterns for Different BIOS

#### Method 1: Using UEFITool
1. Download UEFITool: https://github.com/LongSoft/UEFITool
2. Open your BIOS file in UEFITool
3. Search for "Setup" (Text, case-insensitive)
4. Extract module as binary
5. Open in hex editor
6. Find pattern to modify
7. Update SREP_CONFIG.cfg with your patterns

#### Method 2: Using Runtime Dump
1. Boot with basic SREP config
2. Add debug output to log
3. Dump module memory to file
4. Analyze with hex editor
5. Identify patterns

### Pattern Format
Patterns must be:
- **Hex strings** without spaces or separators
- **Even length** (pairs of hex digits)
- **Exact match** (no wildcards)

Example:
```
# Good
5365747570000001000101

# Bad
53 65 74 75 70  (has spaces)
Setup\x00\x00   (not hex)
```

### Testing Custom Patterns

1. Start with one patch at a time
2. Comment out other patches with `#`
3. Test if pattern is found (check log)
4. Verify effect in setup
5. Add more patches incrementally

## Safety and Best Practices

### Backup First
Before modifying any BIOS settings:
1. Save current BIOS settings to file
2. Note default values
3. Take photos of critical settings

### Test in Stages
1. Run SREP to verify it patches correctly
2. Launch setup to see unlocked options
3. Don't modify settings immediately
4. Understand what each option does

### Safe Options to Modify
Generally safe:
- Boot order
- Date/time
- Display settings
- USB settings (non-critical)

### Dangerous Options
Modify with caution:
- CPU voltage/frequency
- Memory timings
- PCIe speed
- SATA mode (can break boot)
- Secure Boot (can prevent OS boot)

### Recovery
If system won't boot after changing settings:
1. **Clear CMOS**: Remove battery or use jumper
2. **Safe Mode Boot**: Try booting with defaults
3. **Recovery Mode**: Some BIOSes have built-in recovery
4. **BIOS Reflash**: Last resort if settings persisted

## Advanced Usage

### Combining with Other Tools

#### UEFI Shell Scripts
Create `startup.nsh` on USB:
```
@echo -off
SmokelessRuntimeEFIPatcher.efi
```

Auto-executes when UEFI Shell loads.

#### Multiple Configurations
Create multiple configs for different purposes:
- `SREP_CONFIG_FULL.cfg` - All patches
- `SREP_CONFIG_SAFE.cfg` - Conservative patches
- `SREP_CONFIG_DEBUG.cfg` - Minimal for testing

Rename active config to `SREP_CONFIG.cfg`.

### Integration with Boot Manager

#### rEFInd
Add to `refind.conf`:
```
menuentry "SREP BIOS Unlock" {
    loader /SmokelessRuntimeEFIPatcher.efi
}
```

#### GRUB2
Add to `grub.cfg`:
```
menuentry "SREP BIOS Unlock" {
    chainloader /EFI/SREP/SmokelessRuntimeEFIPatcher.efi
}
```

## Platform-Specific Notes

### HP Systems
- Uses AMI BIOS with HP customizations
- Modules: HPSetupData, HPAmiTse
- Often has hidden diagnostic menus
- May require BIOS password cleared first

### Secure Boot Considerations
- SREP must be signed or Secure Boot disabled
- Some patches may conflict with Secure Boot
- Unlocked options may allow disabling Secure Boot

### BIOS Passwords
If BIOS has supervisor password:
1. Password may block some options
2. SREP can still patch menus
3. But changing settings may require password
4. Some systems lock patching when password set

## Performance and Limitations

### Execution Time
- Config parsing: < 1 second
- Pattern matching: 1-5 seconds per module
- Setup launch: 2-3 seconds
- **Total**: ~10-15 seconds typical

### Memory Usage
- SREP: ~1 MB
- Loaded modules: 5-20 MB depending on BIOS
- Safe for all systems

### Compatibility
**Compatible**:
- Most AMI BIOS (2010+)
- HP, Dell, Lenovo AMI-based systems
- UEFI 2.x compliant firmware

**Not Compatible**:
- Award/Phoenix BIOS
- Pure Insyde BIOS (different structure)
- Legacy/CSM-only systems
- Some locked OEM implementations

## Legal and Warranty

### Disclaimer
This tool modifies BIOS behavior at runtime:
- **No permanent changes** to flash
- **Temporary** until reboot
- **Use at your own risk**
- **No warranty** provided

### Warranty Impact
Using this tool may:
- Void manufacturer warranty
- Violate terms of service
- Be detectable by OEM
- Affect support eligibility

### Legal Use Cases
Legitimate uses include:
- Personal device customization
- Research and education
- Hardware diagnostics
- Performance optimization
- Rescue/recovery operations

## Additional Resources

### Documentation
- [SmokelessRuntimeEFIPatcher README](README.md)
- [SREP_CONFIG_DOCUMENTATION.md](SREP_CONFIG_DOCUMENTATION.md)
- [Community Configs](https://github.com/Maxinator500/SREP-Patches)

### Tools
- **UEFITool**: BIOS analysis - https://github.com/LongSoft/UEFITool
- **IFRExtract**: Form extraction - https://github.com/LongSoft/Universal-IFR-Extractor
- **HxD**: Hex editor - https://mh-nexus.de/en/hxd/

### Community
- GitHub Issues: Report bugs or ask questions
- SREP Community Patches: Share your configs
- BIOS Modding Forums: Additional support

## FAQ

**Q: Is this permanent?**  
A: No, all changes are in RAM only and lost on reboot.

**Q: Can I brick my system?**  
A: Very unlikely. SREP doesn't write to flash. Settings you change might cause boot issues but can be cleared via CMOS reset.

**Q: Will this work on my BIOS?**  
A: If it's HP AMI BIOS similar to o.bin, likely yes. Other BIOSes need custom patterns.

**Q: How do I create patterns for my BIOS?**  
A: Extract modules with UEFITool, analyze with hex editor, identify changes needed.

**Q: Can I unlock all options?**  
A: Not always. Some options are hardware-dependent or require deeper modifications.

**Q: Does this work with Secure Boot enabled?**  
A: SREP must be signed or Secure Boot disabled to execute.

**Q: Can I modify Intel ME settings?**  
A: No, ME is separate from BIOS and not accessible via this method.

## Version Information

- **Config Version**: 1.0
- **Target BIOS**: HP AMI (o.bin A73)
- **SREP Version**: 0.1.4c+
- **Last Updated**: 2024

## Credits

- **SmokelessCPUv2**: Original SREP tool creator
- **EduardoA3677**: Configuration and documentation
- **LongSoft**: UEFITool development
- **Community**: Testing and pattern discovery
