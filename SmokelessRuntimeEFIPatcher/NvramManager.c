#include "NvramManager.h"
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

// Common Setup variable GUIDs
EFI_GUID gSetupVariableGuid = {0xEC87D643, 0xEBA4, 0x4BB5, {0xA1, 0xE5, 0x3F, 0x3E, 0x36, 0xB2, 0x0D, 0xA9}};
EFI_GUID gAmiSetupGuid = {0x560bf58a, 0x1e0d, 0x4d7e, {0x95, 0x3f, 0x29, 0x80, 0xa2, 0x61, 0xe0, 0x31}};
EFI_GUID gIntelSetupGuid = {0xEC87D643, 0xEBA4, 0x4BB5, {0xA1, 0xE5, 0x3F, 0x3E, 0x36, 0xB2, 0x0D, 0xA9}};

// Vendor-specific Setup GUIDs
// HP-specific GUIDs (common HP BIOS GUIDs)
EFI_GUID gHpSetupGuid = {0xB540A530, 0x6978, 0x4DA7, {0x91, 0xCB, 0x72, 0x7E, 0xD1, 0x9D, 0xD8, 0x55}};
// AMD CBS/PBS GUIDs
EFI_GUID gAmdCbsGuid = {0x61F7CA61, 0xC5F8, 0x4024, {0x9A, 0xC8, 0x0A, 0x76, 0xF4, 0x6B, 0xBA, 0x1A}};
EFI_GUID gAmdPbsGuid = {0x50EA1035, 0x3F4E, 0x4D1C, {0x9E, 0x1C, 0x6B, 0x3D, 0x1E, 0x5C, 0xF8, 0x35}};
// Intel ME/SA GUIDs
EFI_GUID gIntelMeGuid = {0x5432122D, 0xD034, 0x49D2, {0xA6, 0xDE, 0x65, 0xD5, 0x5A, 0x0E, 0xE5, 0x70}};
EFI_GUID gIntelSaGuid = {0x72C5E28C, 0x7783, 0x43A1, {0x87, 0x67, 0xFA, 0xD7, 0x3F, 0xCC, 0xAF, 0xA2}};

/**
 * Initialize NVRAM manager
 */
EFI_STATUS NvramInitialize(NVRAM_MANAGER *Manager)
{
    if (Manager == NULL)
        return EFI_INVALID_PARAMETER;
    
    ZeroMem(Manager, sizeof(NVRAM_MANAGER));
    
    // Allocate initial variable array with reasonable capacity
    Manager->VariableCapacity = 100;
    Manager->Variables = AllocateZeroPool(sizeof(NVRAM_VARIABLE) * Manager->VariableCapacity);
    if (Manager->Variables == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Manager->VariableCount = 0;
    Manager->ModifiedCount = 0;
    
    return EFI_SUCCESS;
}

/**
 * Expand variable array capacity if needed
 */
STATIC EFI_STATUS NvramExpandCapacity(NVRAM_MANAGER *Manager)
{
    if (Manager == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Double the capacity
    UINTN NewCapacity = Manager->VariableCapacity * 2;
    NVRAM_VARIABLE *NewVariables = AllocateZeroPool(sizeof(NVRAM_VARIABLE) * NewCapacity);
    
    if (NewVariables == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    // Copy existing variables
    CopyMem(NewVariables, Manager->Variables, sizeof(NVRAM_VARIABLE) * Manager->VariableCount);
    
    // Free old array and use new one
    FreePool(Manager->Variables);
    Manager->Variables = NewVariables;
    Manager->VariableCapacity = NewCapacity;
    
    return EFI_SUCCESS;
}

/**
 * Read a variable from NVRAM
 */
EFI_STATUS NvramReadVariable(
    NVRAM_MANAGER *Manager,
    CHAR16 *Name,
    EFI_GUID *Guid,
    VOID **Data,
    UINTN *DataSize
)
{
    EFI_STATUS Status;
    UINTN Size = 0;
    UINT32 Attributes;
    
    if (Name == NULL || Guid == NULL || Data == NULL || DataSize == NULL)
        return EFI_INVALID_PARAMETER;
    
    // First call to get size
    Status = gRT->GetVariable(Name, Guid, &Attributes, &Size, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(Status))
        return Status;
    
    // Allocate buffer
    *Data = AllocateZeroPool(Size);
    if (*Data == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    // Second call to get data
    Status = gRT->GetVariable(Name, Guid, &Attributes, &Size, *Data);
    if (EFI_ERROR(Status))
    {
        FreePool(*Data);
        *Data = NULL;
        return Status;
    }
    
    *DataSize = Size;
    return EFI_SUCCESS;
}

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
)
{
    if (Name == NULL || Guid == NULL || Data == NULL)
        return EFI_INVALID_PARAMETER;
    
    return gRT->SetVariable(Name, Guid, Attributes, DataSize, Data);
}

/**
 * Load all Setup-related variables
 */
EFI_STATUS NvramLoadSetupVariables(NVRAM_MANAGER *Manager)
{
    EFI_STATUS Status;
    
    if (Manager == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Try to load common Setup variables
    CHAR16 *CommonVarNames[] = {
        L"Setup",
        L"SetupVolatile",
        L"SetupDefault",
        L"PreviousBoot",
        L"BootOrder",
        // HP-specific variables
        L"HPSetupData",
        L"NewHPSetupData",
        L"HPALCSetup",
        L"HPSystemConfig",
        // AMD-specific variables
        L"AmdCbsSetup",
        L"AmdPbsSetup",
        L"AmdSetup",
        // Intel-specific variables
        L"IntelSetup",
        L"MeSetup",
        L"SaSetup",
        // Standard UEFI variables
        L"AMITSESetup",
        L"SecureBootSetup",
        L"ALCSetup",
        L"SetupCpuFeatures",
        // Manufacturing/Engineering variables
        L"ManufacturingSetup",
        L"EngineeringSetup",
        L"DebugSetup",
        L"OemSetup",
        NULL
    };
    
    EFI_GUID *CommonGuids[] = {
        &gSetupVariableGuid,
        &gAmiSetupGuid,
        &gIntelSetupGuid,
        &gHpSetupGuid,
        &gAmdCbsGuid,
        &gAmdPbsGuid,
        &gIntelMeGuid,
        &gIntelSaGuid,
        &gEfiGlobalVariableGuid
    };
    
    UINTN GuidCount = 9;
    UINTN LoadedCount = 0;
    
    // Try each combination
    for (UINTN g = 0; g < GuidCount; g++)
    {
        for (UINTN v = 0; CommonVarNames[v] != NULL; v++)
        {
            VOID *Data = NULL;
            UINTN DataSize = 0;
            
            Status = NvramReadVariable(Manager, CommonVarNames[v], CommonGuids[g], &Data, &DataSize);
            if (!EFI_ERROR(Status))
            {
                // Check if we need to expand capacity
                if (Manager->VariableCount >= Manager->VariableCapacity)
                {
                    Status = NvramExpandCapacity(Manager);
                    if (EFI_ERROR(Status))
                    {
                        FreePool(Data);
                        Print(L"Warning: Failed to expand NVRAM capacity\n");
                        continue;
                    }
                }
                
                // Add to our list
                NVRAM_VARIABLE *Var = &Manager->Variables[Manager->VariableCount];
                Var->Name = AllocateCopyPool(StrSize(CommonVarNames[v]), CommonVarNames[v]);
                CopyMem(&Var->Guid, CommonGuids[g], sizeof(EFI_GUID));
                Var->Data = Data;
                Var->DataSize = DataSize;
                Var->OriginalData = AllocateCopyPool(DataSize, Data);
                Var->Modified = FALSE;
                
                // Get attributes
                gRT->GetVariable(Var->Name, &Var->Guid, &Var->Attributes, &DataSize, NULL);
                
                Manager->VariableCount++;
                LoadedCount++;
            }
        }
    }
    
    Print(L"Loaded %d NVRAM variables\n\r", LoadedCount);
    
    return LoadedCount > 0 ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/**
 * Mark a variable as modified (staged for save)
 */
EFI_STATUS NvramStageVariable(
    NVRAM_MANAGER *Manager,
    CHAR16 *Name,
    EFI_GUID *Guid,
    VOID *NewData,
    UINTN DataSize
)
{
    EFI_STATUS Status;
    
    if (Manager == NULL || Name == NULL || Guid == NULL || NewData == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Find the variable
    for (UINTN i = 0; i < Manager->VariableCount; i++)
    {
        NVRAM_VARIABLE *Var = &Manager->Variables[i];
        
        if (StrCmp(Var->Name, Name) == 0 && 
            CompareMem(&Var->Guid, Guid, sizeof(EFI_GUID)) == 0)
        {
            // Update the data
            if (Var->Data && Var->DataSize != DataSize)
            {
                FreePool(Var->Data);
                Var->Data = AllocateZeroPool(DataSize);
            }
            else if (!Var->Data)
            {
                Var->Data = AllocateZeroPool(DataSize);
            }
            
            CopyMem(Var->Data, NewData, DataSize);
            Var->DataSize = DataSize;
            
            // Mark as modified
            if (!Var->Modified)
            {
                Var->Modified = TRUE;
                Manager->ModifiedCount++;
            }
            
            return EFI_SUCCESS;
        }
    }
    
    // Variable not found, add it
    // Check if we need to expand capacity
    if (Manager->VariableCount >= Manager->VariableCapacity)
    {
        Status = NvramExpandCapacity(Manager);
        if (EFI_ERROR(Status))
        {
            return EFI_OUT_OF_RESOURCES;
        }
    }
    
    NVRAM_VARIABLE *Var = &Manager->Variables[Manager->VariableCount];
    Var->Name = AllocateCopyPool(StrSize(Name), Name);
    CopyMem(&Var->Guid, Guid, sizeof(EFI_GUID));
    Var->Data = AllocateCopyPool(DataSize, NewData);
    Var->DataSize = DataSize;
    Var->OriginalData = NULL;
    Var->Modified = TRUE;
    Var->Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
    
    Manager->VariableCount++;
    Manager->ModifiedCount++;
    
    return EFI_SUCCESS;
}

/**
 * Save all modified variables to NVRAM
 */
EFI_STATUS NvramCommitChanges(NVRAM_MANAGER *Manager)
{
    EFI_STATUS Status;
    UINTN SavedCount = 0;
    UINTN FailCount = 0;
    
    if (Manager == NULL)
        return EFI_INVALID_PARAMETER;
    
    Print(L"Saving %d modified variables to NVRAM...\n\r", Manager->ModifiedCount);
    
    for (UINTN i = 0; i < Manager->VariableCount; i++)
    {
        NVRAM_VARIABLE *Var = &Manager->Variables[i];
        
        if (Var->Modified)
        {
            Status = gRT->SetVariable(
                Var->Name,
                &Var->Guid,
                Var->Attributes,
                Var->DataSize,
                Var->Data
            );
            
            if (EFI_ERROR(Status))
            {
                Print(L"Failed to save %s: %r\n\r", Var->Name, Status);
                FailCount++;
            }
            else
            {
                Print(L"Saved %s\n\r", Var->Name);
                SavedCount++;
                
                // Update original data
                if (Var->OriginalData)
                    FreePool(Var->OriginalData);
                Var->OriginalData = AllocateCopyPool(Var->DataSize, Var->Data);
                Var->Modified = FALSE;
            }
        }
    }
    
    Manager->ModifiedCount = FailCount;
    
    Print(L"Save complete: %d succeeded, %d failed\n\r", SavedCount, FailCount);
    
    return FailCount == 0 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

/**
 * Discard all staged changes
 */
EFI_STATUS NvramRollback(NVRAM_MANAGER *Manager)
{
    if (Manager == NULL)
        return EFI_INVALID_PARAMETER;
    
    for (UINTN i = 0; i < Manager->VariableCount; i++)
    {
        NVRAM_VARIABLE *Var = &Manager->Variables[i];
        
        if (Var->Modified && Var->OriginalData)
        {
            // Restore original data
            CopyMem(Var->Data, Var->OriginalData, Var->DataSize);
            Var->Modified = FALSE;
        }
    }
    
    Manager->ModifiedCount = 0;
    
    return EFI_SUCCESS;
}

/**
 * Get modified variable count
 */
UINTN NvramGetModifiedCount(NVRAM_MANAGER *Manager)
{
    if (Manager == NULL)
        return 0;
    
    return Manager->ModifiedCount;
}

/**
 * List all variables (for debugging)
 */
EFI_STATUS NvramListVariables(NVRAM_MANAGER *Manager)
{
    if (Manager == NULL)
        return EFI_INVALID_PARAMETER;
    
    Print(L"\n=== NVRAM Variables (%d total, %d modified) ===\n\r", 
          Manager->VariableCount, Manager->ModifiedCount);
    
    for (UINTN i = 0; i < Manager->VariableCount; i++)
    {
        NVRAM_VARIABLE *Var = &Manager->Variables[i];
        Print(L"%s: %d bytes %s\n\r", 
              Var->Name, 
              Var->DataSize,
              Var->Modified ? L"[MODIFIED]" : L"");
    }
    
    return EFI_SUCCESS;
}

/**
 * Clean up NVRAM manager
 */
VOID NvramCleanup(NVRAM_MANAGER *Manager)
{
    if (Manager == NULL)
        return;
    
    for (UINTN i = 0; i < Manager->VariableCount; i++)
    {
        NVRAM_VARIABLE *Var = &Manager->Variables[i];
        
        if (Var->Name)
            FreePool(Var->Name);
        if (Var->Data)
            FreePool(Var->Data);
        if (Var->OriginalData)
            FreePool(Var->OriginalData);
    }
    
    if (Manager->Variables)
        FreePool(Manager->Variables);
    
    ZeroMem(Manager, sizeof(NVRAM_MANAGER));
}
