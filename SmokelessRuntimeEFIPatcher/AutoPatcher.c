#include "AutoPatcher.h"
#include "Opcode.h"
#include "Utility.h"
#include <Library/PrintLib.h>

extern char Log[512];
extern EFI_FILE *LogFile;
void LogToFile(EFI_FILE *LogFile, char *String);

/**
 * Main auto-patching function
 */
EFI_STATUS AutoPatchBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;

    if (BiosInfo == NULL)
        return EFI_INVALID_PARAMETER;

    AsciiSPrint(Log, 512, "\n=== Starting Auto-Patch Process ===\n\r");
    LogToFile(LogFile, Log);
    AsciiSPrint(Log, 512, "BIOS Type: %s\n\r", GetBiosTypeString(BiosInfo->Type));
    LogToFile(LogFile, Log);
    AsciiSPrint(Log, 512, "Vendor: %s\n\r", BiosInfo->VendorName);
    LogToFile(LogFile, Log);
    AsciiSPrint(Log, 512, "Setup Module: %s\n\r", BiosInfo->SetupModuleName);
    LogToFile(LogFile, Log);
    AsciiSPrint(Log, 512, "FormBrowser Module: %s\n\r", BiosInfo->FormBrowserName);
    LogToFile(LogFile, Log);

    // Dispatch to appropriate patcher based on BIOS type
    switch (BiosInfo->Type)
    {
    case BIOS_TYPE_AMI:
    case BIOS_TYPE_AMI_HP_CUSTOM:
        Status = PatchAmiBios(ImageHandle, BiosInfo);
        break;

    case BIOS_TYPE_INSYDE:
        Status = PatchInsydeBios(ImageHandle, BiosInfo);
        break;

    default:
        AsciiSPrint(Log, 512, "Warning: Unknown BIOS type, attempting generic patches...\n\r");
        LogToFile(LogFile, Log);
        // Try both strategies
        PatchAmiBios(ImageHandle, BiosInfo);
        Status = PatchInsydeBios(ImageHandle, BiosInfo);
        break;
    }

    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "Patching completed with warnings: %r\n\r", Status);
        LogToFile(LogFile, Log);
    }
    else
    {
        AsciiSPrint(Log, 512, "Patching completed successfully\n\r");
        LogToFile(LogFile, Log);
    }

    // Execute the Setup browser
    AsciiSPrint(Log, 512, "\n=== Launching Setup Browser ===\n\r");
    LogToFile(LogFile, Log);
    Status = ExecuteSetupBrowser(ImageHandle, BiosInfo);

    return Status;
}

/**
 * Patch AMI BIOS
 */
EFI_STATUS PatchAmiBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    CHAR8 FormBrowserName[128];
    CHAR8 SetupName[128];

    AsciiSPrint(Log, 512, "\n--- AMI BIOS Patching ---\n\r");
    LogToFile(LogFile, Log);

    // Convert module names to ASCII
    UnicodeStrToAsciiStrS(BiosInfo->FormBrowserName, FormBrowserName, sizeof(FormBrowserName));
    UnicodeStrToAsciiStrS(BiosInfo->SetupModuleName, SetupName, sizeof(SetupName));

    // Try to find already-loaded FormBrowser module
    Status = FindLoadedImageFromName(ImageHandle, FormBrowserName, &ImageInfo);
    
    if (!EFI_ERROR(Status) && ImageInfo != NULL)
    {
        AsciiSPrint(Log, 512, "Found loaded FormBrowser at 0x%x (size: 0x%x)\n\r", 
                   ImageInfo->ImageBase, ImageInfo->ImageSize);
        LogToFile(LogFile, Log);

        // Disable write protections
        UINTN ProtCount = DisableWriteProtections(ImageInfo->ImageBase, ImageInfo->ImageSize);
        AsciiSPrint(Log, 512, "Disabled %d write protections\n\r", ProtCount);
        LogToFile(LogFile, Log);

        // Apply AMI-specific form patches
        (VOID)PatchAmiForms(ImageInfo->ImageBase, ImageInfo->ImageSize);

        // Parse IFR data and apply patches
        IFR_PATCH *PatchList = NULL;
        Status = ParseIfrData(ImageInfo->ImageBase, ImageInfo->ImageSize, &PatchList);
        if (!EFI_ERROR(Status) && PatchList != NULL)
        {
            ApplyIfrPatches(ImageInfo->ImageBase, ImageInfo->ImageSize, PatchList);
            FreeIfrPatchList(PatchList);
        }

        AsciiSPrint(Log, 512, "AMI FormBrowser patching complete\n\r");
        LogToFile(LogFile, Log);
    }
    else
    {
        AsciiSPrint(Log, 512, "FormBrowser not found in loaded modules (will be patched when Setup loads)\n\r");
        LogToFile(LogFile, Log);
    }

    // Also try to patch Setup module if already loaded
    Status = FindLoadedImageFromName(ImageHandle, SetupName, &ImageInfo);
    if (!EFI_ERROR(Status) && ImageInfo != NULL)
    {
        AsciiSPrint(Log, 512, "Found loaded Setup module at 0x%x\n\r", ImageInfo->ImageBase);
        LogToFile(LogFile, Log);

        DisableWriteProtections(ImageInfo->ImageBase, ImageInfo->ImageSize);
        (VOID)PatchAmiForms(ImageInfo->ImageBase, ImageInfo->ImageSize);

        IFR_PATCH *PatchList = NULL;
        Status = ParseIfrData(ImageInfo->ImageBase, ImageInfo->ImageSize, &PatchList);
        if (!EFI_ERROR(Status) && PatchList != NULL)
        {
            ApplyIfrPatches(ImageInfo->ImageBase, ImageInfo->ImageSize, PatchList);
            FreeIfrPatchList(PatchList);
        }
    }

    return EFI_SUCCESS;
}

/**
 * Patch Insyde H2O BIOS
 */
EFI_STATUS PatchInsydeBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    CHAR8 FormBrowserName[128];

    AsciiSPrint(Log, 512, "\n--- Insyde H2O BIOS Patching ---\n\r");
    LogToFile(LogFile, Log);

    UnicodeStrToAsciiStrS(BiosInfo->FormBrowserName, FormBrowserName, sizeof(FormBrowserName));

    // Find H2OFormBrowserDxe
    Status = FindLoadedImageFromName(ImageHandle, FormBrowserName, &ImageInfo);
    
    if (!EFI_ERROR(Status) && ImageInfo != NULL)
    {
        AsciiSPrint(Log, 512, "Found H2OFormBrowserDxe at 0x%x (size: 0x%x)\n\r", 
                   ImageInfo->ImageBase, ImageInfo->ImageSize);
        LogToFile(LogFile, Log);

        // Disable write protections
        UINTN ProtCount = DisableWriteProtections(ImageInfo->ImageBase, ImageInfo->ImageSize);
        AsciiSPrint(Log, 512, "Disabled %d write protections\n\r", ProtCount);
        LogToFile(LogFile, Log);

        // Apply Insyde-specific patches (form visibility flags)
        (VOID)PatchInsydeForms(ImageInfo->ImageBase, ImageInfo->ImageSize);

        // Also parse IFR data
        IFR_PATCH *PatchList = NULL;
        Status = ParseIfrData(ImageInfo->ImageBase, ImageInfo->ImageSize, &PatchList);
        if (!EFI_ERROR(Status) && PatchList != NULL)
        {
            ApplyIfrPatches(ImageInfo->ImageBase, ImageInfo->ImageSize, PatchList);
            FreeIfrPatchList(PatchList);
        }

        AsciiSPrint(Log, 512, "Insyde H2O patching complete\n\r");
        LogToFile(LogFile, Log);
    }
    else
    {
        AsciiSPrint(Log, 512, "H2OFormBrowserDxe not found in loaded modules\n\r");
        LogToFile(LogFile, Log);
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

/**
 * Execute Setup Browser
 */
EFI_STATUS ExecuteSetupBrowser(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    EFI_HANDLE AppImageHandle = NULL;
    CHAR8 SetupName[128];

    UnicodeStrToAsciiStrS(BiosInfo->SetupModuleName, SetupName, sizeof(SetupName));

    AsciiSPrint(Log, 512, "Loading Setup module: %a\n\r", SetupName);
    LogToFile(LogFile, Log);

    // Try to load Setup from Firmware Volume
    Status = LoadFV(ImageHandle, SetupName, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
    
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "Failed to load Setup from FV: %r\n\r", Status);
        LogToFile(LogFile, Log);
        
        // Try alternate names
        if (BiosInfo->Type == BIOS_TYPE_INSYDE)
        {
            AsciiSPrint(Log, 512, "Trying alternate name: SetupUtilityApp\n\r");
            LogToFile(LogFile, Log);
            Status = LoadFV(ImageHandle, "SetupUtilityApp", &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
        }
        else
        {
            AsciiSPrint(Log, 512, "Trying alternate name: Setup\n\r");
            LogToFile(LogFile, Log);
            Status = LoadFV(ImageHandle, "Setup", &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
        }
    }

    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "Could not load Setup module: %r\n\r", Status);
        LogToFile(LogFile, Log);
        return Status;
    }

    AsciiSPrint(Log, 512, "Setup module loaded successfully at 0x%x\n\r", ImageInfo->ImageBase);
    LogToFile(LogFile, Log);

    // Apply patches to Setup module before executing
    DisableWriteProtections(ImageInfo->ImageBase, ImageInfo->ImageSize);
    
    if (BiosInfo->Type == BIOS_TYPE_AMI || BiosInfo->Type == BIOS_TYPE_AMI_HP_CUSTOM)
    {
        (VOID)PatchAmiForms(ImageInfo->ImageBase, ImageInfo->ImageSize);
    }
    else if (BiosInfo->Type == BIOS_TYPE_INSYDE)
    {
        (VOID)PatchInsydeForms(ImageInfo->ImageBase, ImageInfo->ImageSize);
    }

    IFR_PATCH *PatchList = NULL;
    Status = ParseIfrData(ImageInfo->ImageBase, ImageInfo->ImageSize, &PatchList);
    if (!EFI_ERROR(Status) && PatchList != NULL)
    {
        ApplyIfrPatches(ImageInfo->ImageBase, ImageInfo->ImageSize, PatchList);
        FreeIfrPatchList(PatchList);
    }

    // Execute Setup
    AsciiSPrint(Log, 512, "Executing Setup Browser...\n\r");
    LogToFile(LogFile, Log);
    
    Status = Exec(&AppImageHandle);
    
    AsciiSPrint(Log, 512, "Setup Browser returned: %r\n\r", Status);
    LogToFile(LogFile, Log);

    return Status;
}

/**
 * Disable write protections - look for common protection checks
 */
UINTN DisableWriteProtections(VOID *ImageBase, UINTN ImageSize)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    UINTN PatchCount = 0;

    AsciiSPrint(Log, 512, "Searching for write protection checks...\n\r");
    LogToFile(LogFile, Log);

    // Common patterns for write protection checks:
    // 1. Flash write enable/disable checks
    // 2. Variable attribute checks (EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)
    // 3. Memory protection attributes

    // Pattern 1: Look for common "test al, al" + "jz" pattern after protection check
    // This is: test al, al (84 C0) followed by jz (74 XX) or jnz (75 XX)
    for (UINTN i = 0; i < ImageSize - 8; i++)
    {
        if (Data[i] == 0x84 && Data[i + 1] == 0xC0)  // test al, al
        {
            if ((Data[i + 2] == 0x74 || Data[i + 2] == 0x75) && Data[i + 3] < 0x20)  // jz/jnz short
            {
                // Patch to jmp (always jump / never jump depending on what makes sense)
                // For protection checks, we usually want to skip the protection code
                // jz -> jmp means "always take the zero branch"
                if (Data[i + 2] == 0x75)  // jnz - change to jmp to always skip protection
                {
                    Data[i + 2] = 0xEB;  // jmp unconditional
                    PatchCount++;
                    AsciiSPrint(Log, 512, "Patched protection check at 0x%x (jnz -> jmp)\n\r", i);
                    LogToFile(LogFile, Log);
                }
            }
        }
    }

    // Pattern 2: Look for return value checks (cmp eax, 0 / test eax, eax)
    for (UINTN i = 0; i < ImageSize - 8; i++)
    {
        // cmp eax, 0: 83 F8 00 or 3D 00 00 00 00
        if ((Data[i] == 0x83 && Data[i + 1] == 0xF8 && Data[i + 2] == 0x00) ||
            (Data[i] == 0x3D && Data[i + 1] == 0x00 && Data[i + 2] == 0x00 && 
             Data[i + 3] == 0x00 && Data[i + 4] == 0x00))
        {
            UINTN JmpOffset = (Data[i] == 0x83) ? (i + 3) : (i + 5);
            if (JmpOffset < ImageSize - 2)
            {
                // Look for jnz/jne after the comparison
                if (Data[JmpOffset] == 0x75 || Data[JmpOffset] == 0x0F)  // jnz short or long
                {
                    // Patch comparison to always return zero (success)
                    // Change cmp eax, 0 to xor eax, eax (31 C0) + nop
                    if (Data[i] == 0x83)
                    {
                        Data[i] = 0x31;      // xor
                        Data[i + 1] = 0xC0;  // eax, eax
                        Data[i + 2] = 0x90;  // nop
                        PatchCount++;
                        AsciiSPrint(Log, 512, "Patched return check at 0x%x\n\r", i);
                        LogToFile(LogFile, Log);
                    }
                }
            }
        }
    }

    AsciiSPrint(Log, 512, "Write protection patching complete. Applied %d patches.\n\r", PatchCount);
    LogToFile(LogFile, Log);

    return PatchCount;
}
