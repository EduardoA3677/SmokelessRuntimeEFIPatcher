#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/HiiConfigAccess.h>

// Database entry structure for configuration storage
typedef struct {
    UINT16 QuestionId;      // IFR Question ID
    CHAR16 *VariableName;   // NVRAM variable name
    EFI_GUID VariableGuid;  // NVRAM variable GUID
    UINTN Offset;           // Offset within variable
    UINTN Size;             // Size of value (1, 2, 4, or 8 bytes)
    UINT8 Type;             // Question type (checkbox, numeric, etc.)
    UINT64 Value;           // Current value
    BOOLEAN Modified;       // Has been modified
} DATABASE_ENTRY;

// Database context for managing configuration storage
typedef struct {
    DATABASE_ENTRY *Entries;
    UINTN EntryCount;
    UINTN EntryCapacity;
} DATABASE_CONTEXT;

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

/**
 * Initialize database context for configuration storage
 */
EFI_STATUS DatabaseInitialize(DATABASE_CONTEXT *DbContext);

/**
 * Add a configuration entry to the database
 */
EFI_STATUS DatabaseAddEntry(
    DATABASE_CONTEXT *DbContext,
    UINT16 QuestionId,
    CHAR16 *VariableName,
    EFI_GUID *VariableGuid,
    UINTN Offset,
    UINTN Size,
    UINT8 Type,
    UINT64 Value
);

/**
 * Update a configuration value in the database
 */
EFI_STATUS DatabaseUpdateValue(
    DATABASE_CONTEXT *DbContext,
    UINT16 QuestionId,
    UINT64 NewValue
);

/**
 * Commit database changes to NVRAM variables
 */
EFI_STATUS DatabaseCommitToNvram(
    DATABASE_CONTEXT *DbContext,
    NVRAM_MANAGER *NvramManager
);

/**
 * Load configuration values from NVRAM into database
 */
EFI_STATUS DatabaseLoadFromNvram(
    DATABASE_CONTEXT *DbContext,
    NVRAM_MANAGER *NvramManager
);

/**
 * Get a configuration value from the database
 */
EFI_STATUS DatabaseGetValue(
    DATABASE_CONTEXT *DbContext,
    UINT16 QuestionId,
    UINT64 *Value
);

/**
 * Clean up database context
 */
VOID DatabaseCleanup(DATABASE_CONTEXT *DbContext);

// Common BIOS variable GUIDs
extern EFI_GUID gSetupVariableGuid;
extern EFI_GUID gAmiSetupGuid;
extern EFI_GUID gIntelSetupGuid;

// Vendor-specific GUIDs
extern EFI_GUID gHpSetupGuid;
extern EFI_GUID gAmdCbsGuid;
extern EFI_GUID gAmdPbsGuid;
extern EFI_GUID gIntelMeGuid;
extern EFI_GUID gIntelSaGuid;
