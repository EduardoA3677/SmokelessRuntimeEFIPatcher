#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>
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
    CHAR16 *ConfigFilePath;  // Path to config file
    BOOLEAN Modified;        // Has unsaved changes
} CONFIG_MANAGER;

/**
 * Initialize configuration manager
 */
EFI_STATUS ConfigInitialize(CONFIG_MANAGER *Manager);

/**
 * Load configuration from file
 * @param ImageHandle - Handle to the loaded image
 * @param FilePath - Path to config file (e.g., L"fs0:\\SREP_Settings.cfg")
 */
EFI_STATUS ConfigLoadFromFile(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath);

/**
 * Save configuration to file
 * @param ImageHandle - Handle to the loaded image
 * @param FilePath - Path to save config file
 */
EFI_STATUS ConfigSaveToFile(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath);

/**
 * Export configuration in human-readable format
 * @param ImageHandle - Handle to the loaded image
 * @param FilePath - Path to export file (e.g., L"fs0:\\SREP_Export.txt")
 */
EFI_STATUS ConfigExportToText(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath);

/**
 * Import configuration from text file
 * @param ImageHandle - Handle to the loaded image
 * @param FilePath - Path to import file
 */
EFI_STATUS ConfigImportFromText(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath);

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
 * Apply configuration to NVRAM manager
 */
EFI_STATUS ConfigApplyToNvram(CONFIG_MANAGER *Manager, NVRAM_MANAGER *NvramManager);

/**
 * Load configuration from NVRAM manager
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
 * Create a backup of configuration before applying changes
 * @param ImageHandle - Handle to the loaded image
 * @param BackupPath - Path to save backup file
 */
EFI_STATUS ConfigCreateBackup(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *BackupPath);

/**
 * Validate configuration entries before applying
 */
EFI_STATUS ConfigValidate(CONFIG_MANAGER *Manager);

/**
 * Clean up configuration manager
 */
VOID ConfigCleanup(CONFIG_MANAGER *Manager);
