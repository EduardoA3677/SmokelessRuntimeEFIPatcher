#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/HiiConfigAccess.h>

// NVRAM Variable information
typedef struct {
    CHAR16 *Name;
    EFI_GUID Guid;
    UINT32 Attributes;
    UINTN DataSize;
    VOID *Data;
    VOID *OriginalData;  // For rollback
    BOOLEAN Modified;
} NVRAM_VARIABLE;

// NVRAM Manager context
typedef struct {
    NVRAM_VARIABLE *Variables;
    UINTN VariableCount;
    UINTN VariableCapacity;  // Maximum capacity before reallocation
    UINTN ModifiedCount;
} NVRAM_MANAGER;

/**
 * Initialize NVRAM manager
 */
EFI_STATUS NvramInitialize(NVRAM_MANAGER *Manager);

/**
 * Read a variable from NVRAM
 */
EFI_STATUS NvramReadVariable(
    NVRAM_MANAGER *Manager,
    CHAR16 *Name,
    EFI_GUID *Guid,
    VOID **Data,
    UINTN *DataSize
);

/**
 * Write a variable to NVRAM
 */
EFI_STATUS NvramWriteVariable(
    NVRAM_MANAGER *Manager,
    CHAR16 *Name,
    EFI_GUID *Guid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
);

/**
 * Load all Setup-related variables
 */
EFI_STATUS NvramLoadSetupVariables(NVRAM_MANAGER *Manager);

/**
 * Mark a variable as modified (staged for save)
 */
EFI_STATUS NvramStageVariable(
    NVRAM_MANAGER *Manager,
    CHAR16 *Name,
    EFI_GUID *Guid,
    VOID *NewData,
    UINTN DataSize
);

/**
 * Save all modified variables to NVRAM
 */
EFI_STATUS NvramCommitChanges(NVRAM_MANAGER *Manager);

/**
 * Discard all staged changes
 */
EFI_STATUS NvramRollback(NVRAM_MANAGER *Manager);

/**
 * Get modified variable count
 */
UINTN NvramGetModifiedCount(NVRAM_MANAGER *Manager);

/**
 * List all variables (for debugging)
 */
EFI_STATUS NvramListVariables(NVRAM_MANAGER *Manager);

/**
 * Clean up NVRAM manager
 */
VOID NvramCleanup(NVRAM_MANAGER *Manager);

// Common BIOS variable GUIDs
extern EFI_GUID gSetupVariableGuid;
extern EFI_GUID gAmiSetupGuid;
extern EFI_GUID gIntelSetupGuid;
