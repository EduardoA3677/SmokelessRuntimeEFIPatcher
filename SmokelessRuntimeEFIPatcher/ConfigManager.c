#include "ConfigManager.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>

/**
 * Initialize configuration manager
 */
EFI_STATUS 
ConfigInitialize(CONFIG_MANAGER *Manager)
{
    if (Manager == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    ZeroMem(Manager, sizeof(CONFIG_MANAGER));
    Manager->Entries = NULL;
    Manager->EntryCount = 0;
    Manager->ConfigFilePath = NULL;
    Manager->Modified = FALSE;

    return EFI_SUCCESS;
}

/**
 * Add a configuration entry
 */
EFI_STATUS 
ConfigAddEntry(
    CONFIG_MANAGER *Manager,
    CHAR16 *VariableName,
    EFI_GUID *Guid,
    UINTN Offset,
    UINTN Size,
    VOID *Value,
    CHAR8 *Description
)
{
    CONFIG_ENTRY *NewEntries;
    CONFIG_ENTRY *Entry;
    
    if (Manager == NULL || VariableName == NULL || Guid == NULL || Value == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Reallocate entries array
    NewEntries = AllocateZeroPool(sizeof(CONFIG_ENTRY) * (Manager->EntryCount + 1));
    if (NewEntries == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Copy existing entries
    if (Manager->Entries != NULL) {
        CopyMem(NewEntries, Manager->Entries, sizeof(CONFIG_ENTRY) * Manager->EntryCount);
        FreePool(Manager->Entries);
    }

    Manager->Entries = NewEntries;
    Entry = &Manager->Entries[Manager->EntryCount];

    // Allocate and copy variable name
    Entry->VariableName = AllocateZeroPool(StrSize(VariableName));
    if (Entry->VariableName == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    StrCpyS(Entry->VariableName, StrSize(VariableName) / sizeof(CHAR16), VariableName);

    // Copy GUID
    CopyMem(&Entry->Guid, Guid, sizeof(EFI_GUID));

    // Set other fields
    Entry->Offset = Offset;
    Entry->Size = Size;

    // Allocate and copy value
    Entry->Value = AllocateZeroPool(Size);
    if (Entry->Value == NULL) {
        FreePool(Entry->VariableName);
        return EFI_OUT_OF_RESOURCES;
    }
    CopyMem(Entry->Value, Value, Size);

    // Copy description if provided
    if (Description != NULL) {
        UINTN DescLen = AsciiStrLen(Description) + 1;
        Entry->Description = AllocateZeroPool(DescLen);
        if (Entry->Description != NULL) {
            AsciiStrCpyS(Entry->Description, DescLen, Description);
        }
    } else {
        Entry->Description = NULL;
    }

    Manager->EntryCount++;
    Manager->Modified = TRUE;

    return EFI_SUCCESS;
}

/**
 * Update a configuration entry value
 */
EFI_STATUS 
ConfigUpdateEntry(
    CONFIG_MANAGER *Manager,
    UINTN Index,
    VOID *NewValue
)
{
    CONFIG_ENTRY *Entry;

    if (Manager == NULL || NewValue == NULL || Index >= Manager->EntryCount) {
        return EFI_INVALID_PARAMETER;
    }

    Entry = &Manager->Entries[Index];
    
    // Update value
    CopyMem(Entry->Value, NewValue, Entry->Size);
    Manager->Modified = TRUE;

    return EFI_SUCCESS;
}

/**
 * Save configuration to binary file
 */
EFI_STATUS 
ConfigSaveToFile(CONFIG_MANAGER *Manager, CHAR16 *FilePath)
{
    // Simplified implementation - in production, would use EFI_FILE_PROTOCOL
    // For now, just mark as saved
    if (Manager == NULL || FilePath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // TODO: Implement actual file I/O using EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
    // This requires:
    // 1. Locate file system protocol
    // 2. Open root directory
    // 3. Create/open file
    // 4. Write configuration data
    // 5. Close file

    Print(L"ConfigSaveToFile: Saving configuration to %s\n", FilePath);
    Print(L"  Entries: %u\n", Manager->EntryCount);

    // For now, just mark as saved
    Manager->Modified = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Load configuration from file
 */
EFI_STATUS 
ConfigLoadFromFile(CONFIG_MANAGER *Manager, CHAR16 *FilePath)
{
    if (Manager == NULL || FilePath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // TODO: Implement actual file I/O
    Print(L"ConfigLoadFromFile: Loading configuration from %s\n", FilePath);

    return EFI_SUCCESS;
}

/**
 * Export configuration in human-readable format
 */
EFI_STATUS 
ConfigExportToText(CONFIG_MANAGER *Manager, CHAR16 *FilePath)
{
    UINTN i;
    CONFIG_ENTRY *Entry;

    if (Manager == NULL || FilePath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"\n=== Configuration Export ===\n");
    Print(L"Total Entries: %u\n\n", Manager->EntryCount);

    for (i = 0; i < Manager->EntryCount; i++) {
        Entry = &Manager->Entries[i];
        
        Print(L"Entry %u:\n", i);
        Print(L"  Variable: %s\n", Entry->VariableName);
        Print(L"  GUID: %g\n", &Entry->Guid);
        Print(L"  Offset: 0x%X\n", Entry->Offset);
        Print(L"  Size: %u bytes\n", Entry->Size);
        
        if (Entry->Description != NULL) {
            Print(L"  Description: %a\n", Entry->Description);
        }

        // Print value (first few bytes)
        Print(L"  Value: ");
        UINTN PrintSize = Entry->Size > 16 ? 16 : Entry->Size;
        for (UINTN j = 0; j < PrintSize; j++) {
            Print(L"%02X ", ((UINT8*)Entry->Value)[j]);
        }
        if (Entry->Size > 16) {
            Print(L"...");
        }
        Print(L"\n\n");
    }

    return EFI_SUCCESS;
}

/**
 * Apply configuration to NVRAM
 */
EFI_STATUS 
ConfigApplyToNvram(CONFIG_MANAGER *Manager, NVRAM_MANAGER *NvramManager)
{
    UINTN i;
    CONFIG_ENTRY *Entry;
    EFI_STATUS Status;
    UINTN SuccessCount = 0;

    if (Manager == NULL || NvramManager == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"\nApplying %u configuration entries to NVRAM...\n", Manager->EntryCount);

    for (i = 0; i < Manager->EntryCount; i++) {
        Entry = &Manager->Entries[i];
        
        // Stage the variable change
        Status = NvramStageVariable(
            NvramManager,
            Entry->VariableName,
            &Entry->Guid,
            Entry->Value,
            Entry->Size
        );

        if (!EFI_ERROR(Status)) {
            SuccessCount++;
        } else {
            Print(L"  Failed to stage %s: %r\n", Entry->VariableName, Status);
        }
    }

    Print(L"Staged %u/%u entries successfully\n", SuccessCount, Manager->EntryCount);

    // Commit all changes
    if (SuccessCount > 0) {
        Status = NvramCommitChanges(NvramManager);
        if (EFI_ERROR(Status)) {
            Print(L"Failed to commit changes: %r\n", Status);
            return Status;
        }
        Print(L"All changes committed to NVRAM!\n");
    }

    return EFI_SUCCESS;
}

/**
 * Load configuration from NVRAM
 */
EFI_STATUS 
ConfigLoadFromNvram(CONFIG_MANAGER *Manager, NVRAM_MANAGER *NvramManager)
{
    if (Manager == NULL || NvramManager == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Clear existing entries
    ConfigCleanup(Manager);
    ConfigInitialize(Manager);

    // Load from NVRAM manager
    // This would iterate through NVRAM variables and create config entries
    Print(L"Loading configuration from NVRAM...\n");

    return EFI_SUCCESS;
}

/**
 * Get modified status
 */
BOOLEAN 
ConfigIsModified(CONFIG_MANAGER *Manager)
{
    if (Manager == NULL) {
        return FALSE;
    }
    return Manager->Modified;
}

/**
 * Mark as saved
 */
VOID 
ConfigMarkSaved(CONFIG_MANAGER *Manager)
{
    if (Manager != NULL) {
        Manager->Modified = FALSE;
    }
}

/**
 * Get entry count
 */
UINTN 
ConfigGetEntryCount(CONFIG_MANAGER *Manager)
{
    if (Manager == NULL) {
        return 0;
    }
    return Manager->EntryCount;
}

/**
 * Get entry by index
 */
CONFIG_ENTRY* 
ConfigGetEntry(CONFIG_MANAGER *Manager, UINTN Index)
{
    if (Manager == NULL || Index >= Manager->EntryCount) {
        return NULL;
    }
    return &Manager->Entries[Index];
}

/**
 * Clean up configuration manager
 */
VOID 
ConfigCleanup(CONFIG_MANAGER *Manager)
{
    UINTN i;
    CONFIG_ENTRY *Entry;

    if (Manager == NULL) {
        return;
    }

    // Free all entries
    if (Manager->Entries != NULL) {
        for (i = 0; i < Manager->EntryCount; i++) {
            Entry = &Manager->Entries[i];
            
            if (Entry->VariableName != NULL) {
                FreePool(Entry->VariableName);
            }
            if (Entry->Value != NULL) {
                FreePool(Entry->Value);
            }
            if (Entry->Description != NULL) {
                FreePool(Entry->Description);
            }
        }
        
        FreePool(Manager->Entries);
    }

    if (Manager->ConfigFilePath != NULL) {
        FreePool(Manager->ConfigFilePath);
    }

    ZeroMem(Manager, sizeof(CONFIG_MANAGER));
}
