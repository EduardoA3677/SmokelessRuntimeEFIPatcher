#include "BiosDetector.h"
#include <Guid/SmBios.h>
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

extern char Log[512];
extern EFI_FILE *LogFile;
void LogToFile(EFI_FILE *LogFile, char *String);

CHAR16 *GetBiosTypeString(BIOS_TYPE Type)
{
    switch (Type)
    {
    case BIOS_TYPE_AMI:
        return L"AMI";
    case BIOS_TYPE_AMI_HP_CUSTOM:
        return L"AMI (HP Customized)";
    case BIOS_TYPE_INSYDE:
        return L"Insyde H2O";
    case BIOS_TYPE_PHOENIX:
        return L"Phoenix";
    case BIOS_TYPE_AWARD:
        return L"Award";
    default:
        return L"Unknown";
    }
}

/**
 * Check if a string contains a substring (case insensitive)
 */
BOOLEAN ContainsString(CHAR16 *Haystack, CHAR16 *Needle)
{
    if (Haystack == NULL || Needle == NULL)
        return FALSE;

    UINTN HaystackLen = StrLen(Haystack);
    UINTN NeedleLen = StrLen(Needle);

    if (NeedleLen > HaystackLen)
        return FALSE;

    for (UINTN i = 0; i <= HaystackLen - NeedleLen; i++)
    {
        BOOLEAN Match = TRUE;
        for (UINTN j = 0; j < NeedleLen; j++)
        {
            CHAR16 c1 = Haystack[i + j];
            CHAR16 c2 = Needle[j];
            // Simple case-insensitive compare for ASCII
            if (c1 >= L'a' && c1 <= L'z')
                c1 = c1 - L'a' + L'A';
            if (c2 >= L'a' && c2 <= L'z')
                c2 = c2 - L'a' + L'A';
            if (c1 != c2)
            {
                Match = FALSE;
                break;
            }
        }
        if (Match)
            return TRUE;
    }
    return FALSE;
}

/**
 * Detect BIOS type from SMBIOS information
 */
EFI_STATUS DetectBiosType(BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_SMBIOS_PROTOCOL *Smbios;
    EFI_SMBIOS_HANDLE SmbiosHandle;
    EFI_SMBIOS_TABLE_HEADER *Record;
    SMBIOS_TABLE_TYPE0 *Type0Record;

    if (BiosInfo == NULL)
        return EFI_INVALID_PARAMETER;

    // Initialize structure
    ZeroMem(BiosInfo, sizeof(BIOS_INFO));
    BiosInfo->Type = BIOS_TYPE_UNKNOWN;

    AsciiSPrint(Log, 512, "Detecting BIOS Type...\n\r");
    LogToFile(LogFile, Log);

    // Try to get SMBIOS protocol
    Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "SMBIOS Protocol not found: %r\n\r", Status);
        LogToFile(LogFile, Log);
        // Continue with firmware volume detection
    }
    else
    {
        // Get BIOS Information (Type 0)
        SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
        Status = Smbios->GetNext(Smbios, &SmbiosHandle, NULL, &Record, NULL);
        while (!EFI_ERROR(Status))
        {
            if (Record->Type == SMBIOS_TYPE_BIOS_INFORMATION)
            {
                Type0Record = (SMBIOS_TABLE_TYPE0 *)Record;
                
                // Get strings from SMBIOS
                CHAR8 *StringPtr = (CHAR8 *)Record + Record->Length;
                
                // Extract Vendor string
                if (Type0Record->Vendor > 0)
                {
                    CHAR8 *VendorStr = StringPtr;
                    for (UINTN i = 1; i < Type0Record->Vendor; i++)
                    {
                        VendorStr += AsciiStrLen(VendorStr) + 1;
                    }
                    UnicodeSPrint(BiosInfo->VendorName, sizeof(BiosInfo->VendorName), L"%a", VendorStr);
                    
                    AsciiSPrint(Log, 512, "BIOS Vendor: %a\n\r", VendorStr);
                    LogToFile(LogFile, Log);
                }
                
                // Extract Version string
                if (Type0Record->BiosVersion > 0)
                {
                    CHAR8 *VersionStr = StringPtr;
                    for (UINTN i = 1; i < Type0Record->BiosVersion; i++)
                    {
                        VersionStr += AsciiStrLen(VersionStr) + 1;
                    }
                    UnicodeSPrint(BiosInfo->Version, sizeof(BiosInfo->Version), L"%a", VersionStr);
                    
                    AsciiSPrint(Log, 512, "BIOS Version: %a\n\r", VersionStr);
                    LogToFile(LogFile, Log);
                }
                
                break;
            }
            Status = Smbios->GetNext(Smbios, &SmbiosHandle, NULL, &Record, NULL);
        }
    }

    // Detect BIOS type based on vendor name
    if (ContainsString(BiosInfo->VendorName, L"American Megatrends") || 
        ContainsString(BiosInfo->VendorName, L"AMI"))
    {
        BiosInfo->Type = BIOS_TYPE_AMI;
        
        // Check for HP customization
        if (ContainsString(BiosInfo->VendorName, L"HP") || 
            ContainsString(BiosInfo->VendorName, L"Hewlett") ||
            ContainsString(BiosInfo->VendorName, L"Compaq"))
        {
            BiosInfo->Type = BIOS_TYPE_AMI_HP_CUSTOM;
            BiosInfo->IsCustomOEM = TRUE;
        }
        
        // Set default AMI module names
        StrCpyS(BiosInfo->SetupModuleName, 64, L"Setup");
        StrCpyS(BiosInfo->FormBrowserName, 64, L"FormBrowser");
    }
    else if (ContainsString(BiosInfo->VendorName, L"Insyde"))
    {
        BiosInfo->Type = BIOS_TYPE_INSYDE;
        StrCpyS(BiosInfo->SetupModuleName, 64, L"SetupUtilityApp");
        StrCpyS(BiosInfo->FormBrowserName, 64, L"H2OFormBrowserDxe");
    }
    else if (ContainsString(BiosInfo->VendorName, L"Phoenix"))
    {
        BiosInfo->Type = BIOS_TYPE_PHOENIX;
        StrCpyS(BiosInfo->SetupModuleName, 64, L"Setup");
        StrCpyS(BiosInfo->FormBrowserName, 64, L"SetupBrowser");
    }

    AsciiSPrint(Log, 512, "Detected BIOS Type: %s\n\r", GetBiosTypeString(BiosInfo->Type));
    LogToFile(LogFile, Log);

    // Find actual module names in firmware volumes
    FindSetupModules(BiosInfo);

    return EFI_SUCCESS;
}

/**
 * Find Setup-related modules in firmware volumes
 */
EFI_STATUS FindSetupModules(BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN NumberOfHandles;
    UINTN Index;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance;

    AsciiSPrint(Log, 512, "Searching for Setup modules in Firmware Volumes...\n\r");
    LogToFile(LogFile, Log);

    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolume2ProtocolGuid,
        NULL,
        &NumberOfHandles,
        &HandleBuffer);

    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "Failed to locate Firmware Volumes: %r\n\r", Status);
        LogToFile(LogFile, Log);
        return Status;
    }

    AsciiSPrint(Log, 512, "Found %d Firmware Volume instances\n\r", NumberOfHandles);
    LogToFile(LogFile, Log);

    BOOLEAN SetupFound = FALSE;
    BOOLEAN FormBrowserFound = FALSE;

    for (Index = 0; Index < NumberOfHandles; Index++)
    {
        Status = gBS->HandleProtocol(
            HandleBuffer[Index],
            &gEfiFirmwareVolume2ProtocolGuid,
            (VOID **)&FvInstance);
        
        if (EFI_ERROR(Status))
            continue;

        EFI_FV_FILETYPE FileType;
        EFI_FV_FILE_ATTRIBUTES FileAttributes;
        UINTN FileSize;
        EFI_GUID NameGuid;
        VOID *Keys = AllocateZeroPool(FvInstance->KeySize);

        while (TRUE)
        {
            FileType = EFI_FV_FILETYPE_ALL;
            ZeroMem(&NameGuid, sizeof(EFI_GUID));
            Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
            
            if (EFI_ERROR(Status))
                break;

            VOID *String = NULL;
            UINTN StringSize = 0;
            UINT32 AuthenticationStatus;
            
            Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 
                                            0, &String, &StringSize, &AuthenticationStatus);
            
            if (!EFI_ERROR(Status) && String != NULL)
            {
                CHAR16 *ModuleName = (CHAR16 *)String;
                
                // Look for Setup module variants
                if (!SetupFound && (ContainsString(ModuleName, L"Setup") || 
                    ContainsString(ModuleName, L"SetupUtility")))
                {
                    StrCpyS(BiosInfo->SetupModuleName, 64, ModuleName);
                    SetupFound = TRUE;
                    AsciiSPrint(Log, 512, "Found Setup Module: %s\n\r", ModuleName);
                    LogToFile(LogFile, Log);
                }
                
                // Look for FormBrowser variants
                if (!FormBrowserFound && ContainsString(ModuleName, L"FormBrowser"))
                {
                    StrCpyS(BiosInfo->FormBrowserName, 64, ModuleName);
                    FormBrowserFound = TRUE;
                    AsciiSPrint(Log, 512, "Found FormBrowser Module: %s\n\r", ModuleName);
                    LogToFile(LogFile, Log);
                }
                
                FreePool(String);
            }
            
            if (SetupFound && FormBrowserFound)
                break;
        }
        
        FreePool(Keys);
        
        if (SetupFound && FormBrowserFound)
            break;
    }

    FreePool(HandleBuffer);

    if (!SetupFound)
    {
        AsciiSPrint(Log, 512, "Warning: Setup module not found, using default: %s\n\r", 
                    BiosInfo->SetupModuleName);
        LogToFile(LogFile, Log);
    }

    if (!FormBrowserFound)
    {
        AsciiSPrint(Log, 512, "Warning: FormBrowser module not found, using default: %s\n\r", 
                    BiosInfo->FormBrowserName);
        LogToFile(LogFile, Log);
    }

    return EFI_SUCCESS;
}
