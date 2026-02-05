# SREP Configuration Files Summary

This directory contains a complete SREP configuration for unlocking hidden BIOS menus on HP AMI-based systems.

## Files Overview

### 1. SREP_CONFIG.cfg
**Purpose**: The actual configuration file used by SREP  
**Size**: ~1.5 KB  
**Format**: SREP command format (Op/End blocks)  
**Usage**: Place in root of USB drive alongside SREP EFI application

**Contents**:
- 5 targeted patches for AMI BIOS modules
- Module loading sequence
- Setup application execution command

### 2. SREP_CONFIG_DOCUMENTATION.md
**Purpose**: Technical Documentation and Analysis  
**Size**: ~9.2 KB  
**Audience**: Advanced users, BIOS modders, researchers

**Contents**:
- BIOS architecture analysis
- Module identification (Setup, AMITSESetup, HPSetupData, HPAmiTse)
- Pattern locations in BIOS binary
- Detailed explanation of each patch
- Access level system explanation
- Technical implementation details

### 3. SREP_CONFIG_USAGE.md
**Purpose**: End-user guide  
**Size**: ~9.7 KB  
**Audience**: All users wanting to unlock BIOS menus

**Contents**:
- Step-by-step setup instructions
- Booting and execution guide
- Troubleshooting common issues
- Safety warnings and best practices
- FAQ section
- Advanced usage scenarios

## Quick Start

### For Users
1. Read: `SREP_CONFIG_USAGE.md`
2. Use: `SREP_CONFIG.cfg` on USB drive with SREP
3. Boot and enjoy unlocked BIOS menus

### For Developers/Researchers
1. Read: `SREP_CONFIG_DOCUMENTATION.md` for technical details
2. Use: Pattern analysis to create configs for other BIOSes
3. Contribute: Share patterns for different systems

## Target System

**BIOS Type**: HP AMI (American Megatrends Inc.)  
**Target Binary**: o.bin from A73 release  
**Source URL**: https://github.com/EduardoA3677/UEFITool/releases/download/A73/o.bin

**Modules Patched**:
- Setup (main setup configuration)
- AMITSESetup (AMITSE form visibility)
- HPSetupData (HP-specific options)
- HPAmiTse (HP AMITSE customization)

**Result**: Unlocked advanced menus, diagnostics, and hidden options

## Key Features

### Safety First
✅ **Non-Persistent**: All changes are RAM-only  
✅ **Non-Destructive**: Never writes to BIOS flash  
✅ **Reversible**: Simply reboot to return to normal  
✅ **No Brick Risk**: Cannot damage firmware

### Unlock Capabilities
🔓 Advanced CPU/Memory settings  
🔓 Chipset configuration options  
🔓 Hidden diagnostic menus  
🔓 HP-specific utilities  
🔓 Extended power management  
🔓 Security option details

### Compatibility
✅ HP systems with AMI BIOS  
✅ UEFI 2.x compliant systems  
✅ x64 architecture  
⚠️ Patterns specific to o.bin (may need adjustment)

## Pattern Analysis Methodology

The patterns in `SREP_CONFIG.cfg` were derived through:

1. **BIOS Binary Analysis**
   - Downloaded o.bin (8 MB HP AMI BIOS)
   - Identified firmware volume structure
   - Located setup-related modules

2. **Module Identification**
   - Found Setup variable at 0x3c00b2
   - Found AMITSESetup at 0x3c02d3
   - Found HPSetupData at 0x3c0db5
   - Identified HPAmiTse module

3. **Pattern Extraction**
   - Analyzed access level flags (0x00=hidden, 0x01=visible)
   - Identified form visibility structures
   - Located menu suppression patterns

4. **Patch Creation**
   - Created 5 targeted patches
   - Each patch enables specific menu sets
   - Validated hex pattern lengths and pairs

## Technical Approach

### AMI BIOS Structure
```
NVRAM Variables
├── Setup         - Main configuration storage
├── AMITSESetup   - UI behavior and form visibility
└── HPSetupData   - OEM-specific options

DXE Drivers
├── HPAmiTse      - HP's AMITSE customization
└── SetupUtilityApp - The setup UI application
```

### Unlock Strategy
1. **Access Level Elevation**: Change flags from 0x00 (hidden) to 0x01 (visible)
2. **Form Visibility**: Enable all AMITSE forms regardless of suppression
3. **OEM Options**: Unlock HP-specific diagnostic and configuration menus
4. **Verification Bypass**: Modify access level checks to always succeed

### Pattern Format
```
Original Pattern:  5365747570000001000101000100...
Modified Pattern:  5365747570000001010101000101...
                                    ^^
                   Changed hidden (0x00) to visible (0x01)
```

## Usage Workflow

```
┌─────────────────┐
│  Prepare USB    │
│  - SREP EFI     │
│  - CONFIG.cfg   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Boot USB      │
│   (UEFI mode)   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  SREP Executes  │
│  - Load config  │
│  - Find modules │
│  - Apply patches│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Launch Setup    │
│ SetupUtilityApp │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Unlocked BIOS!  │
│ - Adv. menus    │
│ - Hidden opts   │
│ - Diagnostics   │
└─────────────────┘
```

## Validation and Testing

### Pattern Validation
✅ All patterns are even-length hex strings  
✅ 5 search/replace pairs identified  
✅ Pattern lengths validated (13-34 bytes each)  
✅ Syntax follows SREP Op/End format

### Testing Status
⚠️ **Not tested on physical hardware** (requires actual HP system)  
✅ Configuration syntax validated  
✅ Pattern format verified  
✅ Module names confirmed from BIOS analysis

**Note**: Actual effectiveness depends on:
- Exact BIOS version match
- Module loading state at runtime
- BIOS-specific implementations

## Troubleshooting Quick Reference

| Issue | Likely Cause | Solution |
|-------|-------------|----------|
| Config not found | Wrong filename/location | Check `SREP_CONFIG.cfg` in USB root |
| Module not loaded | BIOS difference | Check SREP.log for module names |
| Pattern not found | Version mismatch | Extract patterns from your BIOS |
| Setup won't launch | Wrong module name | Try "Setup", "SetupApp" variants |
| Menus still hidden | Incomplete unlock | May need IFR-level patches |

## Safety Warnings

⚠️ **Use at your own risk**  
⚠️ **May void warranty**  
⚠️ **Test in safe environment first**  
⚠️ **Some settings can cause instability**  
⚠️ **Always have recovery plan (CMOS clear)**

## Legal Notice

This configuration is provided for:
- ✅ Educational purposes
- ✅ Personal device customization
- ✅ Research and development
- ✅ Hardware diagnostics

**NOT** for:
- ❌ Bypassing security on non-owned devices
- ❌ Violating manufacturer policies for commercial gain
- ❌ Any illegal activities

Users assume all responsibility for use.

## Contributing

### Share Your Patterns
If you've created patterns for other BIOSes:
1. Fork the repository
2. Add your config with documentation
3. Submit pull request

### Report Issues
Found a problem?
1. Check SREP.log for errors
2. Verify BIOS version matches
3. Open issue with details

### Improve Documentation
Documentation improvements welcome:
- Clarity enhancements
- Additional examples
- Translation to other languages

## Resources

### Required Tools
- **SREP**: This repository (compile or use release)
- **UEFITool**: For BIOS analysis - https://github.com/LongSoft/UEFITool
- **Hex Editor**: For pattern analysis (HxD, 010 Editor, etc.)

### Learning Resources
- UEFI Specification: https://uefi.org/specifications
- AMI BIOS structure guides (various online sources)
- BIOS modding community forums

### Related Projects
- SREP-Patches: Community config collection - https://github.com/Maxinator500/SREP-Patches
- UEFITool: BIOS analysis tool - https://github.com/LongSoft/UEFITool
- IFRExtract: Form extraction - https://github.com/LongSoft/Universal-IFR-Extractor

## Credits and Acknowledgments

### Original Tool
**SmokelessCPUv2** - Creator of SmokelessRuntimeEFIPatcher  
- Original concept and implementation
- Last active: December 2022

### This Configuration
**EduardoA3677** - Configuration and documentation  
- BIOS analysis and pattern extraction
- Configuration file creation
- Comprehensive documentation

### Community
- BIOS modding community for techniques and knowledge
- HP and AMI for BIOS architecture (inadvertently)
- LongSoft for UEFITool and analysis tools

## Version History

### v1.0 (Current)
- Initial release for HP AMI BIOS (o.bin A73)
- 5 targeted patches
- Complete documentation suite
- Usage guide for end users

### Future Plans
- Test on physical hardware
- Refine patterns based on testing
- Add support for more BIOS variants
- Community pattern submissions

## Support

### Getting Help
1. **Read docs first**: Check SREP_CONFIG_USAGE.md
2. **Check logs**: SREP.log has detailed execution info
3. **Search issues**: Someone may have same problem
4. **Ask community**: Open issue with details

### Providing Help
- Share your successful configs
- Document patterns for other BIOSes
- Answer questions from other users
- Improve documentation

## File Integrity

For verification, you can generate checksums using:

```bash
sha256sum SREP_CONFIG*.* SREP_FILES_README.md
```

This ensures files have not been modified during transfer.

## License

Follows the license of the main SmokelessRuntimeEFIPatcher project.

Configuration files and documentation are provided as-is without warranty.

---

**Last Updated**: 2024-01-30  
**Configuration Version**: 1.0  
**Target BIOS**: HP AMI o.bin (A73)  
**SREP Version**: 0.1.4c compatible
