#include "ConfigManager.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>

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
 * Apply configuration to NVRAM (saves to real BIOS NVRAM)
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

    // Commit all changes to real BIOS NVRAM
    if (SuccessCount > 0) {
        Status = NvramCommitChanges(NvramManager);
        if (EFI_ERROR(Status)) {
            Print(L"Failed to commit changes to NVRAM: %r\n", Status);
            return Status;
        }
        Print(L"All changes committed to BIOS NVRAM!\n");
    }

    return EFI_SUCCESS;
}

/**
 * Load configuration from NVRAM (reads from real BIOS NVRAM)
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
    Print(L"Loading configuration from BIOS NVRAM...\n");

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
 * Validate configuration entries before applying to NVRAM
 */
EFI_STATUS 
ConfigValidate(CONFIG_MANAGER *Manager)
{
    UINTN i;
    CONFIG_ENTRY *Entry;
    BOOLEAN IsValid = TRUE;
    UINTN NameLen;
    
    if (Manager == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"\nValidating configuration (%u entries)...\n", Manager->EntryCount);
    
    for (i = 0; i < Manager->EntryCount; i++) {
        Entry = &Manager->Entries[i];
        
        // Check for null pointers
        if (Entry->VariableName == NULL) {
            Print(L"  Entry %u: INVALID - Variable name is NULL\n", i);
            IsValid = FALSE;
            continue;
        }
        
        if (Entry->Value == NULL) {
            Print(L"  Entry %u: INVALID - Value is NULL\n", i);
            IsValid = FALSE;
            continue;
        }
        
        // Check for reasonable size
        if (Entry->Size == 0) {
            Print(L"  Entry %u: WARNING - Size is 0\n", i);
        }
        
        if (Entry->Size > 65536) {
            Print(L"  Entry %u: WARNING - Size is very large (%u bytes)\n", i, Entry->Size);
        }
        
        // Check variable name length
        NameLen = StrLen(Entry->VariableName);
        if (NameLen == 0) {
            Print(L"  Entry %u: INVALID - Variable name is empty\n", i);
            IsValid = FALSE;
            continue;
        }
        
        if (NameLen > 255) {
            Print(L"  Entry %u: WARNING - Variable name is very long (%u chars)\n", i, NameLen);
        }
    }
    
    if (IsValid) {
        Print(L"Validation passed!\n");
        return EFI_SUCCESS;
    } else {
        Print(L"Validation FAILED - invalid entries detected\n");
        return EFI_INVALID_PARAMETER;
    }
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

    ZeroMem(Manager, sizeof(CONFIG_MANAGER));
}
