#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/FirmwareVolume2.h>

// BIOS Type enumeration
typedef enum {
    BIOS_TYPE_UNKNOWN = 0,
    BIOS_TYPE_AMI,
    BIOS_TYPE_AMI_HP_CUSTOM,
    BIOS_TYPE_INSYDE,
    BIOS_TYPE_PHOENIX,
    BIOS_TYPE_AWARD
} BIOS_TYPE;

// BIOS Information structure
typedef struct {
    BIOS_TYPE Type;
    CHAR16 VendorName[64];
    CHAR16 Version[64];
    CHAR16 SetupModuleName[64];
    CHAR16 FormBrowserName[64];
    BOOLEAN IsCustomOEM;
} BIOS_INFO;

/**
 * Detect BIOS type and populate BIOS information
 * 
 * @param BiosInfo  Pointer to BIOS_INFO structure to populate
 * @return EFI_SUCCESS if detection successful, error otherwise
 */
EFI_STATUS DetectBiosType(BIOS_INFO *BiosInfo);

/**
 * Find Setup-related modules in firmware volumes
 * 
 * @param BiosInfo  Pointer to BIOS_INFO structure to update
 * @return EFI_SUCCESS if modules found, error otherwise
 */
EFI_STATUS FindSetupModules(BIOS_INFO *BiosInfo);

/**
 * Get BIOS type as string
 * 
 * @param Type  BIOS type enum value
 * @return String representation of BIOS type
 */
CHAR16 *GetBiosTypeString(BIOS_TYPE Type);
