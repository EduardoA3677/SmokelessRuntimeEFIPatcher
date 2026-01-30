#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include "NvramManager.h"

// Configuration entry
typedef struct {
    CHAR16 *VariableName;
    EFI_GUID Guid;
    UINTN Offset;        // Offset within variable
    UINTN Size;          // Size of value
    VOID *Value;         // Current value
    CHAR8 *Description;  // Human-readable description
} CONFIG_ENTRY;

// Configuration manager context
typedef struct {
    CONFIG_ENTRY *Entries;
    UINTN EntryCount;
    BOOLEAN Modified;        // Has unsaved changes
} CONFIG_MANAGER;

/**
 * Initialize configuration manager
 */
EFI_STATUS ConfigInitialize(CONFIG_MANAGER *Manager);

/**
 * Add a configuration entry
 */
EFI_STATUS ConfigAddEntry(
    CONFIG_MANAGER *Manager,
    CHAR16 *VariableName,
    EFI_GUID *Guid,
    UINTN Offset,
    UINTN Size,
    VOID *Value,
    CHAR8 *Description
);

/**
 * Update a configuration entry value
 */
EFI_STATUS ConfigUpdateEntry(
    CONFIG_MANAGER *Manager,
    UINTN Index,
    VOID *NewValue
);

/**
 * Apply configuration to NVRAM (saves to real BIOS NVRAM)
 */
EFI_STATUS ConfigApplyToNvram(CONFIG_MANAGER *Manager, NVRAM_MANAGER *NvramManager);

/**
 * Load configuration from NVRAM (reads from real BIOS NVRAM)
 */
EFI_STATUS ConfigLoadFromNvram(CONFIG_MANAGER *Manager, NVRAM_MANAGER *NvramManager);

/**
 * Get modified status
 */
BOOLEAN ConfigIsModified(CONFIG_MANAGER *Manager);

/**
 * Mark as saved (clear modified flag)
 */
VOID ConfigMarkSaved(CONFIG_MANAGER *Manager);

/**
 * Get entry count
 */
UINTN ConfigGetEntryCount(CONFIG_MANAGER *Manager);

/**
 * Get entry by index
 */
CONFIG_ENTRY* ConfigGetEntry(CONFIG_MANAGER *Manager, UINTN Index);

/**
 * Validate configuration entries before applying to NVRAM
 */
EFI_STATUS ConfigValidate(CONFIG_MANAGER *Manager);

/**
 * Clean up configuration manager
 */
VOID ConfigCleanup(CONFIG_MANAGER *Manager);
