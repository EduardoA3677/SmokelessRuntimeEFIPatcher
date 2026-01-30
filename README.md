# SmokelessRuntimeEFIPatcher

> [!WARNING]
> This tool modifies BIOS binaries at runtime.
> **Use at your own risk, I am not responsible for any damage, bricking etc. that may occur due to usage of this tool**

> [!IMPORTANT]
> This tool was originaly created by SmokelessCPUv2, however, for unknown reasons they deleted their GitHub account. Their last commit was [100502283b8ebfb06232b804de33881d9c53670f](https://github.com/hboyd2003/SmokelessRuntimeEFIPatcher/commit/100502283b8ebfb06232b804de33881d9c53670f) on Dec 16, 2022.

### BIOS mod/unlock requests
This is not a request form.
**DO NOT** open any issues requesting a BIOS unlock/mod

### Bugs, features and updates
As the original creator/maintainer is gone, do not request new features. Feel free to open a pull request if you would like to submit a new feature. If you find any bugs feel free to open an issue or submit a pull request, if I have time I may fix it.

### What?
This is a tool to patch and inject EFI modules at runtime. SmokelessCPUv2 developed this as they weren't comfortable with SPI flashing, which requires opening the laptop for every small change. With secure boot, you can no longer flash unsigned BIOSes.

**Version 0.2.0 adds intelligent auto-detection and patching** - it can now automatically detect your BIOS type and patch hidden menus without requiring a config file!

### Why?
The tool was developed as a way to unlock a BIOS without the risks/issues/annoyances associated with SPI flashing. Additionally with the usage of secure boot it is no longer possible (in most cases) to get the BIOS to flash itself with an unsigned BIOS.

## Operating Modes

### AUTO Mode (New in v0.2.0) - Recommended

When no config file is present, SREP operates in **AUTO mode** and automatically:

1. **Detects BIOS Type** via SMBIOS:
   - AMI BIOS
   - AMI customized by OEMs (HP, Dell, etc.)
   - Insyde H2O
   - Phoenix BIOS
   
2. **Finds BIOS Modules**:
   - Automatically locates Setup and FormBrowser modules
   - Searches firmware volumes for the correct module names
   
3. **Intelligent Patching**:
   - Parses IFR (Internal Forms Representation) opcodes
   - Identifies and patches conditions that hide menus:
     - `suppressif` conditions
     - `grayoutif` conditions  
     - `disableif` conditions
   - Disables write protection checks
   - Patches security restrictions
   
4. **Launches Setup Browser**:
   - Automatically loads and executes the appropriate Setup application
   - Works with AMI Setup, Insyde SetupUtilityApp, and others

**Usage (AUTO mode)**:
```bash
# Simply boot the EFI file without any config file
# SREP will automatically detect and patch your BIOS
```

### MANUAL Mode - Config File (Legacy)

For advanced users or specific patches, SREP still supports manual configuration files.

### How?
When the EFI App is booted up, it looks for a file called *SREP_Config.cfg* in the root of the drive it booted from. If found, it operates in MANUAL mode. If not found, it operates in AUTO mode.

### Usage (MANUAL mode)
A file with the name ```SREP_Config.cfg``` can be located at the root of the drive the EFI boots from. You can either create your own config or use one of the many created by the community.

Some configs can be found here: [Maxinator500's SREP-Patches](https://github.com/Maxinator500/SREP-Patches)

To launch you can either get the UEFI BIOS to run it directly or launch it using any other normal means (Grub, UEFI Console, etc.)

## SREP_Config
### Structure

    Op OpName1
        Argument 1
        Argument 2
        Argument n
    Op OpName2
        Argument 1
        Argument 2
        Argument n
    End
    
    Op OpName3
        Argument 1
        Argument 2
        Argument n
    End


### Commands
|   Command  |                                                  Description                                                  |                                                        Arguments                                                       |
|:----------:|:-------------------------------------------------------------------------------------------------------------:|:----------------------------------------------------------------------------------------------------------------------:|
| LoadFromFS | <div align="left">Loads a EFI file into memory from a EFI partition and sets it as the current target</div>   | <div align="left"> <ol> <li>File name: The Filename to load</li> </ol> </div>                                          |
| LoadFromFV | <div align="left">Loads a EFI file into memory from the FV/Bios image and sets it as the current target</div> | <div align="left"> <ol> <li>Section name: The section to load</li> </ol> </div>                                        |
|   Loaded   | <div align="left">Set an already loaded module as the current target</div>                                    | <div align="left"> <ol> <li>Name: The name of the loaded module</li> </ol> </div>                                      |
|    Patch   | <div align="left">Patches the current target</div>                                                            | <div align="left"> <ol> <li>Type: Patch type to perform</li> <li>2..n: Patch type specific arguments</li> </ol> </div> |
|    Exec    | <div align="left">Executes the current target</div>                                                           |                                                                                                                        |

#### Patch types
|     Type     |                           Description                          |                                                             Arguments                                                             |
|:------------:|:--------------------------------------------------------------:|:---------------------------------------------------------------------------------------------------------------------------------:|
|    Pattern   | <div align="left">Searches for a pattern and replaces it</div> | <div align="left"> <ol> <li>Hex search: Hex bytes to search for</li> <li>Hex replace: Hex bytes to replace with</li> </ol> </div> |
| RelPosOffset | <div align="left">Replaces based on byte offset</div>          | <div align="left"> <ol> <li>Offset: Offset in bytes</li> <li>Hex replace: Hex bytes to replace with</li> </ol> </div>             |

## Examples
This is an Example of Loading a simple EFI, and executing it:

    Op LoadFromFS APP.efi
    Op Exec
    End

This is an Example of Loading a simple EFI, replacing by pattern,and executing it

Find and replace AABBCCDDEEFF with AABBCCDDEEEE,
find and replace AABBCCDDAABB with AABBCCDDAAAA:

    Op LoadFromFS
    APP.efi
    Op Patch
    Pattern
    AABBCCDDEEFF
    AABBCCDDEEEE
    Op Patch
    Pattern
    AABBCCDDAABB
    AABBCCDDAAAA
    Op Exec
    End

This is an Example of using relative pattern

Find the pattern AABBCCDDEEFF (replace with AABBCCDDEEFF, as we want it's own start address), then write AABBCCDDAAAA, at +50 from the pattern start

    Op LoadFromFS
    APP.efi
    Op Patch
    Pattern
    AABBCCDDEEFF
    AABBCCDDEEFF
    Op Patch
    RelPosOffset
    50
    AABBCCDDAAAA
    Op Exec
    End


### Lenovo-BIOS-Unlock Example
Now a real example on how to use it to patch a Lenovo Legion Bios to Unlock the Advanced menu:

The Target Insyde H2O, is very simple in the regard on which form is shown...
> [!NOTE]
> Their is a surprisingly large variance in the structure of Insyde H20 BIOSes. This form structure is unique to Lenovo.

in the H2OFormBrowserDxe there is a simple array of struct:

     struct Form
    {
        GUID FormGUID;
        uint32_t isShown;
    }

    struct Form FormList[NO_OF_FORM];

The previus cE! backdoor, was very simple, looked like this:

    if(gRS->GetVariable("cE!".....))
        for(int i=0;i<NO_OF_FORM;i++)
        {
            FormList[i].isShown=1;
        }
if the Variable didn't existed default isShow was used...

With this tool we can manually set the isShow Flag so we can replicate that behavoir...

The H2OFormBrowserDxe, is already loaded when we are able to execute SREP, so we can use the Loaded OP....

Let's say that we want to show the CBS Menu , it's guid is {B04535E3-3004-4946-9EB7-149428983053}, so it's hex representation is
 
    E33545B0043046499EB7149428983053
 
 given that is disabled, the complete Form Struct in memory will be 
 
    E33545B0043046499EB714942898305300000000

 as we appended the 4 byte uint32_t of value 0;

 we want to replace this with the 1, the little endian version of a uint32_t is
 
    01 00 00 00

so the replace string is

     E33545B0043046499EB714942898305301000000


So putting all toghether the SREP_Config.cfg file look like


    Op Loaded
    H2OFormBrowserDxe
    Op Patch
    Pattern
    E33545B0043046499EB714942898305300000000
    E33545B0043046499EB714942898305301000000
    Op End

Now we have patched the H2OFormBrowserDxe, but the Bios UI will be not loaded as we booted from a USB, but we can force it to load with

    Op LoadFromFV
    SetupUtilityApp
    Op Exec


So the Finall SREP_Config.cfg is:

    Op Loaded
    H2OFormBrowserDxe
    Op Patch
    Pattern
    E33545B0043046499EB714942898305300000000
    E33545B0043046499EB714942898305301000000
    Op End

    Op LoadFromFV
    SetupUtilityApp
    Op Exec

> [!NOTE]
> Note You can't show the AOD menu with this, as it is not even loaded on non HX, cpu, you can force to load, Patching also AoDSetupDxe, but that's a topic for another day.


You could do the same to show the PBS menu and the Advanced Menu on intel one, if you are lazy you can use the combined one AMD/Intel provided here(I might have forgot to unlock something tbh):

    Op Loaded
    H2OFormBrowserDxe
    Op Patch
    Pattern
    59B963B8C60E334099C18FD89F04022200000000
    59B963B8C60E334099C18FD89F04022201000000
    Op Patch
    Pattern
    E33545B0043046499EB714942898305300000000
    E33545B0043046499EB714942898305301000000
    Op Patch
    Pattern
    732871A65F92C64690B4A40F86A0917B00000000
    732871A65F92C64690B4A40F86A0917B01000000
    Op Patch
    Pattern
    9E76D4C6487F2A4D98E987ADCCF35CCC00000000
    9E76D4C6487F2A4D98E987ADCCF35CCC01000000
    Op End

    Op LoadFromFV
    SetupUtilityApp
    Op Exec

## Technical Details - AUTO Mode

### BIOS Detection
SREP uses SMBIOS Type 0 (BIOS Information) to identify the BIOS vendor and version:
- Reads BIOS vendor string from SMBIOS
- Detects AMI, Insyde, Phoenix, and OEM-customized variants
- Searches firmware volumes for Setup-related modules

### IFR Parsing
The IFR (Internal Forms Representation) parser analyzes the Setup module's binary data:
- Identifies IFR opcodes that control form visibility
- Detects `EFI_IFR_SUPPRESS_IF_OP` (0x0A) - hides menu items
- Detects `EFI_IFR_GRAY_OUT_IF_OP` (0x19) - grays out menu items
- Detects `EFI_IFR_DISABLE_IF_OP` (0x1E) - disables menu items

### Patching Strategy
1. **IFR Condition Patching**: Replaces condition opcodes with `EFI_IFR_FALSE_OP` to ensure hidden items are always shown
2. **AMI-Specific**: Patches `SuppressIf(TRUE)` patterns commonly used in AMI BIOS
3. **Insyde-Specific**: Patches form visibility flags (GUID + uint32_t structures)
4. **Write Protection**: Disables common security checks by patching conditional jumps

### Supported BIOS Types
- **AMI BIOS**: Full support for standard AMI and OEM-customized versions (HP, Dell, Asus, etc.)
- **Insyde H2O**: Full support including Lenovo-style form structures
- **Phoenix BIOS**: Basic support
- **Unknown**: Falls back to generic patching strategies

### Module Auto-Detection
SREP automatically searches for:
- Setup modules: `Setup`, `SetupUtilityApp`, `SetupUtility`
- FormBrowser modules: `FormBrowser`, `H2OFormBrowserDxe`, `SetupBrowser`

### Logging
All operations are logged to `SREP.log` on the boot drive for debugging and verification.

## Building from Source

Requirements:
- EDK2 (UEFI Development Kit)
- GCC5 toolchain
- NASM assembler
- Python 3

```bash
# Clone EDK2
git clone --depth 1 --branch edk2-stable202205 https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init --recursive

# Build BaseTools
make -C BaseTools

# Setup environment
source edksetup.sh

# Copy SREP files to EDK2 workspace
cp -r /path/to/SmokelessRuntimeEFIPatcher .
cp SmokelessRuntimeEFIPatcher.dsc .

# Build
build -b RELEASE -t GCC5 -p SmokelessRuntimeEFIPatcher.dsc -a X64 -s

# Output: Build/SmokelessRuntimeEFIPatcher/RELEASE_GCC5/X64/SmokelessRuntimeEFIPatcher.efi
```

## Version History

### v0.2.0 (2026)
- **Major Feature**: Auto-detection and intelligent patching mode
- **New**: BIOS type detection via SMBIOS
- **New**: IFR opcode parser for finding hidden menus
- **New**: Automatic module discovery
- **New**: Vendor-specific patching strategies
- **New**: Write protection bypass
- **Improved**: Logging and error handling
- **Maintained**: Backward compatibility with config files

### v0.1.4c (2022)
- Last release by original author SmokelessCPUv2
- Manual config file mode only

## Credits

- **Original Author**: SmokelessCPUv2 (2022)
- **Enhanced Version**: Community contributions (2026)
- **Testing**: Community testers with various BIOS types

## Disclaimer

This tool is provided for educational and research purposes only. Modifying BIOS settings can potentially damage your system. Always:
- Create a backup of your BIOS
- Know how to recover from a bad flash
- Test on non-critical systems first
- Understand the risks involved

The authors and contributors are not responsible for any damage caused by using this tool.
