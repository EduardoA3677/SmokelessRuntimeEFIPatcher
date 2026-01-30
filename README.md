# SmokelessRuntimeEFIPatcher (SREP)

Enhanced UEFI/BIOS configuration tool with auto-detection and intelligent patching capabilities.

## Features

- **Auto-Detection**: Automatically detects BIOS type (AMI, HP AMI, Insyde, Phoenix)
- **Interactive Menu**: User-friendly menu interface for BIOS configuration
- **Direct NVRAM Access**: Saves settings directly to BIOS NVRAM
- **Hidden Menu Unlocking**: Patches IFR opcodes to reveal hidden BIOS options
- **HP AMI Support**: Specialized support for HP customized AMI BIOS

## Building

### Prerequisites

- Ubuntu 22.04 or later (or compatible Linux distribution)
- GCC 11 or later
- Python 3
- NASM assembler
- EDK2 build environment

### Build Instructions

1. Install dependencies:
```bash
sudo apt-get install build-essential gcc-11 nasm uuid-dev python3 python3-distutils git acpica-tools
```

2. Clone EDK2:
```bash
git clone --recursive https://github.com/tianocore/edk2.git
cd edk2
git checkout edk2-stable202205
git submodule update --init --recursive
```

3. Clone SREP into EDK2:
```bash
git clone https://github.com/EduardoA3677/SmokelessRuntimeEFIPatcher.git SmokelessRuntimeEFIPatcher
```

4. Build EDK2 Base Tools:
```bash
make -C BaseTools
```

5. Build SREP:
```bash
source edksetup.sh
build -b RELEASE -t GCC5 -p SmokelessRuntimeEFIPatcher/SmokelessRuntimeEFIPatcher.dsc -a X64
```

The built EFI file will be located at:
`Build/SmokelessRuntimeEFIPatcher/RELEASE_GCC5/X64/SmokelessRuntimeEFIPatcher.efi`

## Usage

### Interactive Mode (Default)

1. Copy `SmokelessRuntimeEFIPatcher.efi` to a USB drive formatted as FAT32
2. Boot into UEFI Shell
3. Run the application:
```
fs0:
SmokelessRuntimeEFIPatcher.efi
```

### Auto Mode

Create a file named `SREP_Auto.flag` in the same directory as the EFI file to enable automatic mode.

## Menu Options

1. **Auto-Detect and Patch BIOS** - Automatically detects and patches BIOS
2. **Browse BIOS Settings** - View available BIOS forms (read-only)
3. **Load Modules and Edit Settings** - Modify BIOS settings and save to NVRAM
4. **Launch BIOS Setup Browser** - Launch native BIOS Setup interface
5. **About** - Application information
6. **Exit** - Exit to UEFI Shell

## Architecture

### Core Components

- **AutoPatcher**: Auto-detection and patching logic
- **BiosDetector**: BIOS type detection from SMBIOS
- **ConfigManager**: Configuration entry management
- **HiiBrowser**: HII form enumeration and browsing
- **IfrParser**: IFR data parsing and patching
- **MenuUI**: Interactive menu system
- **NvramManager**: Direct NVRAM read/write operations

### Supported BIOS Types

- AMI BIOS
- HP Customized AMI BIOS
- Insyde H2O BIOS
- Phoenix BIOS
- Award BIOS (partial)

## Technical Details

### NVRAM Storage

All configuration changes are saved directly to BIOS NVRAM using UEFI Runtime Services. Settings persist across reboots.

### Form Unlocking

SREP can unlock hidden BIOS forms by patching IFR opcodes:
- SUPPRESSIF TRUE → FALSE
- GRAYOUTIF TRUE → FALSE
- DISABLEIF TRUE → FALSE

### HP AMI Specifics

HP customized AMI BIOS uses additional modules:
- HPSetupData
- NewHPSetupData
- HPAmiTse (customized AMI TSE)

## License

This project is open source. See LICENSE file for details.

## Contributing

Contributions are welcome! Please submit pull requests or open issues for bugs and feature requests.

## Version

Current version: 0.3.1

## Acknowledgments

- TianoCore EDK2 project
- AMI BIOS documentation
- UEFI Forum specifications
