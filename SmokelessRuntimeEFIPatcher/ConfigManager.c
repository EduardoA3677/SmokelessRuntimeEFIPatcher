#include "ConfigManager.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/FileInfo.h>

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
ConfigSaveToFile(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root;
    EFI_FILE *ConfigFile;
    UINTN i;
    CONFIG_ENTRY *Entry;
    CHAR8 Buffer[512];
    UINTN BufferSize;

    if (Manager == NULL || FilePath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Get file system access
    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get loaded image protocol: %r\n", Status);
        return Status;
    }

    Status = gBS->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FileSystem
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file system protocol: %r\n", Status);
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    // Create/open file for writing
    Status = Root->Open(
        Root,
        &ConfigFile,
        FilePath,
        EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create config file: %r\n", Status);
        Root->Close(Root);
        return Status;
    }

    // Write header
    AsciiSPrint(Buffer, sizeof(Buffer), "# SREP Configuration File\n");
    BufferSize = AsciiStrLen(Buffer);
    ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

    AsciiSPrint(Buffer, sizeof(Buffer), "# Total Entries: %u\n\n", Manager->EntryCount);
    BufferSize = AsciiStrLen(Buffer);
    ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

    // Write each entry
    for (i = 0; i < Manager->EntryCount; i++) {
        Entry = &Manager->Entries[i];
        
        // Write entry header
        AsciiSPrint(Buffer, sizeof(Buffer), "[Entry_%u]\n", i);
        BufferSize = AsciiStrLen(Buffer);
        ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

        // Variable name (convert from CHAR16 to CHAR8)
        AsciiSPrint(Buffer, sizeof(Buffer), "Variable=");
        BufferSize = AsciiStrLen(Buffer);
        ConfigFile->Write(ConfigFile, &BufferSize, Buffer);
        
        // Write wide string as ASCII (simplified)
        for (UINTN j = 0; Entry->VariableName[j] != 0 && j < 100; j++) {
            CHAR8 c = (CHAR8)Entry->VariableName[j];
            BufferSize = 1;
            ConfigFile->Write(ConfigFile, &BufferSize, &c);
        }
        BufferSize = 1;
        ConfigFile->Write(ConfigFile, &BufferSize, "\n");

        // Write GUID
        AsciiSPrint(Buffer, sizeof(Buffer), 
            "GUID=%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
            Entry->Guid.Data1,
            Entry->Guid.Data2,
            Entry->Guid.Data3,
            Entry->Guid.Data4[0], Entry->Guid.Data4[1],
            Entry->Guid.Data4[2], Entry->Guid.Data4[3],
            Entry->Guid.Data4[4], Entry->Guid.Data4[5],
            Entry->Guid.Data4[6], Entry->Guid.Data4[7]
        );
        BufferSize = AsciiStrLen(Buffer);
        ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

        // Write offset and size
        AsciiSPrint(Buffer, sizeof(Buffer), "Offset=0x%X\n", Entry->Offset);
        BufferSize = AsciiStrLen(Buffer);
        ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

        AsciiSPrint(Buffer, sizeof(Buffer), "Size=%u\n", Entry->Size);
        BufferSize = AsciiStrLen(Buffer);
        ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

        // Write description if available
        if (Entry->Description != NULL) {
            AsciiSPrint(Buffer, sizeof(Buffer), "Description=%a\n", Entry->Description);
            BufferSize = AsciiStrLen(Buffer);
            ConfigFile->Write(ConfigFile, &BufferSize, Buffer);
        }

        // Write value as hex
        AsciiSPrint(Buffer, sizeof(Buffer), "Value=");
        BufferSize = AsciiStrLen(Buffer);
        ConfigFile->Write(ConfigFile, &BufferSize, Buffer);

        for (UINTN j = 0; j < Entry->Size && j < 256; j++) {
            AsciiSPrint(Buffer, sizeof(Buffer), "%02X", ((UINT8*)Entry->Value)[j]);
            BufferSize = AsciiStrLen(Buffer);
            ConfigFile->Write(ConfigFile, &BufferSize, Buffer);
        }
        BufferSize = 1;
        ConfigFile->Write(ConfigFile, &BufferSize, "\n");

        // Empty line between entries
        BufferSize = 1;
        ConfigFile->Write(ConfigFile, &BufferSize, "\n");
    }

    ConfigFile->Flush(ConfigFile);
    ConfigFile->Close(ConfigFile);
    Root->Close(Root);

    Print(L"Configuration saved to %s (%u entries)\n", FilePath, Manager->EntryCount);
    Manager->Modified = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Load configuration from file
 */
EFI_STATUS 
ConfigLoadFromFile(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root;
    EFI_FILE *ConfigFile;
    EFI_FILE_INFO *FileInfo;
    UINTN FileInfoSize;
    CHAR8 *FileBuffer;
    UINTN FileSize;

    if (Manager == NULL || FilePath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Get file system access
    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get loaded image protocol: %r\n", Status);
        return Status;
    }

    Status = gBS->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FileSystem
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file system protocol: %r\n", Status);
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    // Open file for reading
    Status = Root->Open(
        Root,
        &ConfigFile,
        FilePath,
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open config file: %r\n", Status);
        Root->Close(Root);
        return Status;
    }

    // Get file size
    FileInfoSize = SIZE_OF_EFI_FILE_INFO + 512;
    FileInfo = AllocateZeroPool(FileInfoSize);
    if (FileInfo == NULL) {
        ConfigFile->Close(ConfigFile);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = ConfigFile->GetInfo(ConfigFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file info: %r\n", Status);
        FreePool(FileInfo);
        ConfigFile->Close(ConfigFile);
        Root->Close(Root);
        return Status;
    }

    FileSize = (UINTN)FileInfo->FileSize;
    FreePool(FileInfo);

    // Read file content
    FileBuffer = AllocateZeroPool(FileSize + 1);
    if (FileBuffer == NULL) {
        ConfigFile->Close(ConfigFile);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = ConfigFile->Read(ConfigFile, &FileSize, FileBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read file: %r\n", Status);
        FreePool(FileBuffer);
        ConfigFile->Close(ConfigFile);
        Root->Close(Root);
        return Status;
    }

    ConfigFile->Close(ConfigFile);
    Root->Close(Root);

    // Parse the file (simplified - just print for now)
    // A full implementation would parse the INI-like format and reconstruct entries
    Print(L"Configuration file loaded (%u bytes)\n", FileSize);
    Print(L"Note: Full parsing implementation is a work in progress\n");
    Print(L"File contents:\n%a\n", FileBuffer);

    FreePool(FileBuffer);
    
    return EFI_SUCCESS;
}

/**
 * Export configuration in human-readable format
 */
EFI_STATUS 
ConfigExportToText(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath)
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
 * Import configuration from text file
 */
EFI_STATUS 
ConfigImportFromText(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *FilePath)
{
    if (Manager == NULL || FilePath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // TODO: Implement actual file I/O to read text format configuration
    // This would parse a text file in the format exported by ConfigExportToText
    // Format expected:
    //   Entry N:
    //     Variable: <name>
    //     GUID: <guid>
    //     Offset: 0xXXXX
    //     Size: N bytes
    //     Description: <desc>
    //     Value: XX XX XX ...
    
    Print(L"ConfigImportFromText: Importing configuration from %s\n", FilePath);
    Print(L"  Note: Text import feature is currently a placeholder\n");
    Print(L"  Use ConfigLoadFromFile for binary configuration files\n");

    return EFI_UNSUPPORTED;
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
 * Create a backup of configuration before applying changes
 */
EFI_STATUS 
ConfigCreateBackup(CONFIG_MANAGER *Manager, EFI_HANDLE ImageHandle, CHAR16 *BackupPath)
{
    EFI_STATUS Status;
    CHAR16 TimestampedPath[256];
    
    if (Manager == NULL || ImageHandle == NULL || BackupPath == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Create a timestamped backup filename
    // For simplicity, just use the provided path
    // In production, might want to add timestamp
    UnicodeSPrint(TimestampedPath, sizeof(TimestampedPath), L"%s.backup", BackupPath);
    
    Print(L"Creating configuration backup at %s\n", TimestampedPath);
    
    // Save current configuration to backup file
    Status = ConfigSaveToFile(Manager, ImageHandle, TimestampedPath);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create backup: %r\n", Status);
        return Status;
    }
    
    Print(L"Backup created successfully\n");
    return EFI_SUCCESS;
}

/**
 * Validate configuration entries before applying
 */
EFI_STATUS 
ConfigValidate(CONFIG_MANAGER *Manager)
{
    UINTN i;
    CONFIG_ENTRY *Entry;
    BOOLEAN IsValid = TRUE;
    
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
        UINTN NameLen = StrLen(Entry->VariableName);
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

    if (Manager->ConfigFilePath != NULL) {
        FreePool(Manager->ConfigFilePath);
    }

    ZeroMem(Manager, sizeof(CONFIG_MANAGER));
}
