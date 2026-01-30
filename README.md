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
This is a simple tool to patch and inject EFI modules at runtime, SmokelessCPUv2 developed this as they weren't confortable with SPI flashing, and requires opening the laptop for every small change, and with secure boot you can no longer flash unsigned BIOSes.

### Why?
The tool was developed as a way to unlock a BIOS without the risks/issues/annoyances associated with SPI flashing. Additionally with the usage of secure boot it is no longer possible (in most cases) to get the BIOS to flash itself with a unsigned BIOS.

### How?
When the EFI App is booted up, it looks for a file Called *SREP_Config.cfg* in the root of the drive it booted from, containing a list of command to execute.

### Usage
A file with the name ```SREP_Config.cfg``` must be located at the root of the drive the EFI boots from. You can either create your own config or use one of the many created by the community.

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

## AMI BIOS Support

### Overview
AMI (American Megatrends Inc.) BIOS uses a different architecture compared to Insyde H2O, but SREP can still be used to unlock hidden menus and modify BIOS behavior at runtime. AMI BIOS typically uses the TSE (Text Setup Environment) module for setup menu management.

### Key Differences from Insyde H2O
- **Module Names**: AMI uses modules like `Setup`, `TSE`, or `SetupBrowser` instead of `H2OFormBrowserDxe`
- **Structure**: While both use UEFI standard form structures with GUIDs, AMI's internal implementation differs
- **Form Management**: AMI TSE handles form visibility and access differently than Insyde's H2O FormBrowser

### Finding AMI Module Names
To identify the correct module name for your AMI BIOS:

1. Boot SREP and check the log file `SREP.log` which lists all loaded modules
2. Look for modules with names containing:
   - `Setup`
   - `TSE` 
   - `SetupBrowser`
   - `FormBrowser`
3. Use tools like UEFITool to extract and analyze your BIOS image offline

### AMI BIOS Pattern Discovery
To create patches for AMI BIOS:

1. **Extract the BIOS**: Use UEFITool to open your BIOS image
2. **Locate Setup Module**: Find the Setup/TSE module in the firmware volume
3. **Extract IFR**: Use IFRExtractor or Universal IFR Extractor to get human-readable form data
4. **Identify Form GUIDs**: Look for hidden menus you want to unlock (typically marked with "Suppress If" conditions)
5. **Create Pattern**: The visibility flag is usually a byte following the GUID (00 = hidden, 01 = visible)

### AMI-BIOS-Unlock Example

For AMI BIOS, the process is similar to Insyde but targets different modules. Here's a generic example:

**Step 1**: Identify your Setup module name (e.g., `Setup` or `TSE`)

**Step 2**: Find the GUID of the menu you want to unlock using IFRExtractor

**Step 3**: Create a pattern to search and replace. For example, to unlock an "Advanced" menu with GUID `{A1B2C3D4-E5F6-7890-ABCD-EF0123456789}`:

    Op Loaded
    Setup
    Op Patch
    Pattern
    D4C3B2A1F6E5907890ABCDEF012345678900000000
    D4C3B2A1F6E5907890ABCDEF012345678901000000
    Op End
    
    Op LoadFromFV
    SetupUtilityApp
    Op Exec

> [!NOTE]
> The GUID bytes are in little-endian format. Convert your GUID accordingly:
> - GUID `{A1B2C3D4-E5F6-7890-ABCD-EF0123456789}` becomes
> - Bytes `D4C3B2A1 F6E5 9078 90AB CDEF01234567 89` then append `00000000` for the visibility flag

### Common AMI Module Names by Vendor

Different manufacturers may use different module names:

| Vendor | Common Module Names |
|--------|-------------------|
| ASUS | `Setup`, `SetupUtilityApp`, `TSE` |
| MSI | `Setup`, `ClickBiosSetup`, `TSE` |
| ASRock | `Setup`, `TSE` |
| Gigabyte | `Setup`, `TSE` |
| Generic AMI | `Setup`, `SetupBrowser`, `TSE` |

### Advanced AMI Patching Techniques

#### Multiple Form Unlocking
You can unlock multiple menus in a single config:

    Op Loaded
    Setup
    Op Patch
    Pattern
    [GUID1]00000000
    [GUID1]01000000
    Op Patch
    Pattern
    [GUID2]00000000
    [GUID2]01000000
    Op Patch
    Pattern
    [GUID3]00000000
    [GUID3]01000000
    Op End
    
    Op LoadFromFV
    SetupUtilityApp
    Op Exec

#### Conditional Patching
For BIOS-specific features, you might need to patch at specific offsets discovered through analysis:

    Op Loaded
    Setup
    Op Patch
    RelPosOffset
    1234
    01000000
    Op End

### Tips for AMI BIOS

1. **Always check SREP.log**: The log file will show you exactly which modules are loaded and any errors encountered
2. **Pattern not found?**: Your BIOS might use a different structure. Extract and analyze with UEFITool/IFRExtractor
3. **Test incrementally**: Unlock one menu at a time to identify which patterns work
4. **Backup always**: Keep a BIOS backup and know how to recover (BIOS Flashback, dual BIOS, or SPI programmer)
5. **Check module variations**: Some AMI BIOS versions might name their setup module differently

### Troubleshooting

| Issue | Solution |
|-------|----------|
| Pattern not found | Use IFRExtractor to verify the exact GUID and structure in your BIOS |
| Module not loaded | Check SREP.log for actual module name, it might differ from expected |
| Setup doesn't show changes | Try loading and executing the setup utility explicitly with LoadFromFV |
| System hangs | Pattern might be corrupted or wrong; verify GUID byte order (little-endian) |

### Security Considerations

> [!WARNING]
> - AMI BIOS with Secure Boot enabled may prevent module loading
> - Some AMI BIOS implementations have additional protections (Intel Boot Guard, AMD Platform Security Processor)
> - Runtime patching may be detected by security features
> - Always test on non-critical systems first

### Resources
- [UEFITool](https://github.com/LongSoft/UEFITool) - For BIOS analysis
- [IFRExtractor](https://github.com/LongSoft/Universal-IFR-Extractor) - For extracting human-readable setup forms
- [AMI BIOS Structure Documentation](https://www.ami.com/) - Official AMI resources
- [BIOS Modding Communities](https://winraid.level1techs.com/) - Community knowledge base
