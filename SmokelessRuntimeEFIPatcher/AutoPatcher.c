#include "AutoPatcher.h"
#include "Opcode.h"
#include "Utility.h"
#include <Library/PrintLib.h>

extern char Log[512];
extern EFI_FILE *LogFile;
void LogToFile(EFI_FILE *LogFile, char *String);

/**
 * Patch all loaded modules that contain IFR data
 */
EFI_STATUS PatchAllLoadedModules(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    UINTN HandleSize = 0;
    EFI_HANDLE *Handles;
    UINTN TotalPatches = 0;

    AsciiSPrint(Log, 512, "\n--- Scanning All Loaded Modules ---\n\r");
    LogToFile(LogFile, Log);
    Print(L"Scanning loaded modules...\n\r");

    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(HandleSize);
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &HandleSize, Handles);
        
        if (!EFI_ERROR(Status))
        {
            UINTN ModuleCount = HandleSize / sizeof(EFI_HANDLE);
            AsciiSPrint(Log, 512, "Found %d loaded modules to scan\n\r", ModuleCount);
            LogToFile(LogFile, Log);
            Print(L"Scanning %d modules for IFR data...\n\r", ModuleCount);

            for (UINTN i = 0; i < ModuleCount; i++)
            {
                // Show progress every 20 modules
                if (i > 0 && i % 20 == 0)
                {
                    Print(L"  Progress: %d/%d modules scanned\n\r", i, ModuleCount);
                }
                
                EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
                Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)&ImageInfo);
                
                if (!EFI_ERROR(Status) && ImageInfo != NULL && ImageInfo->ImageBase != NULL)
                {
                    // Get module name if available
                    CHAR16 *ModuleName = FindLoadedImageFileName(ImageInfo);
                    
                    // Parse for IFR data
                    IFR_PATCH *PatchList = NULL;
                    Status = ParseIfrData(ImageInfo->ImageBase, ImageInfo->ImageSize, &PatchList);
                    
                    if (!EFI_ERROR(Status) && PatchList != NULL)
                    {
                        if (ModuleName != NULL)
                        {
                            AsciiSPrint(Log, 512, "Patching module: %s\n\r", ModuleName);
                            LogToFile(LogFile, Log);
                        }
                        
                        DisableWriteProtections(ImageInfo->ImageBase, ImageInfo->ImageSize);
                        
                        // Apply vendor-specific patches
                        if (BiosInfo->Type == BIOS_TYPE_AMI || BiosInfo->Type == BIOS_TYPE_AMI_HP_CUSTOM)
                        {
                            (VOID)PatchAmiForms(ImageInfo->ImageBase, ImageInfo->ImageSize);
                        }
                        else if (BiosInfo->Type == BIOS_TYPE_INSYDE)
                        {
                            (VOID)PatchInsydeForms(ImageInfo->ImageBase, ImageInfo->ImageSize);
                        }
                        
                        ApplyIfrPatches(ImageInfo->ImageBase, ImageInfo->ImageSize, PatchList);
                        FreeIfrPatchList(PatchList);
                        TotalPatches++;
                    }
                }
            }
            
            FreePool(Handles);
        }
    }

    AsciiSPrint(Log, 512, "Patched %d modules with IFR data\n\r", TotalPatches);
    LogToFile(LogFile, Log);
    Print(L"Patched %d modules with IFR data\n\r", TotalPatches);

    return EFI_SUCCESS;
}

/**
 * Load and patch a module and its dependencies from firmware volume
 */
EFI_STATUS LoadAndPatchModule(EFI_HANDLE ImageHandle, CHAR8 *ModuleName, BIOS_INFO *BiosInfo, BOOLEAN Execute)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    EFI_HANDLE AppImageHandle = NULL;

    AsciiSPrint(Log, 512, "Loading module: %a\n\r", ModuleName);
    LogToFile(LogFile, Log);

    // Try to load module from Firmware Volume
    Status = LoadFV(ImageHandle, ModuleName, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
    
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "Could not load %a from FV: %r\n\r", ModuleName, Status);
        LogToFile(LogFile, Log);
        return Status;
    }

    AsciiSPrint(Log, 512, "Module %a loaded at 0x%x (size: 0x%x)\n\r", 
               ModuleName, ImageInfo->ImageBase, ImageInfo->ImageSize);
    LogToFile(LogFile, Log);

    // Apply patches
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

    // Execute if requested
    if (Execute)
    {
        AsciiSPrint(Log, 512, "Executing %a...\n\r", ModuleName);
        LogToFile(LogFile, Log);
        
        Status = Exec(&AppImageHandle);
        
        AsciiSPrint(Log, 512, "%a returned: %r\n\r", ModuleName, Status);
        LogToFile(LogFile, Log);
    }

    return EFI_SUCCESS;
}

/**
 * Load and patch Setup dependencies
 */
EFI_STATUS PatchSetupDependencies(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    AsciiSPrint(Log, 512, "\n--- Patching Setup Dependencies ---\n\r");
    LogToFile(LogFile, Log);

    // Common Setup dependencies based on AMI BIOS analysis
    CHAR8 *Dependencies[] = {
        "HiiDatabase",           // HII Database - critical for forms
        "TcgPlatformSetupPolicy", // TCG/TPM Setup
        "NvmeDynamicSetup",      // NVMe configuration
        "PciDynamicSetup",       // PCI configuration
        "NetworkStackSetupScreen", // Network setup
        NULL
    };

    UINTN PatchedCount = 0;
    for (UINTN i = 0; Dependencies[i] != NULL; i++)
    {
        // Try to load and patch each dependency
        // Don't fail if a dependency is not found
        EFI_STATUS Status = LoadAndPatchModule(ImageHandle, Dependencies[i], BiosInfo, FALSE);
        if (!EFI_ERROR(Status))
        {
            PatchedCount++;
        }
    }

    AsciiSPrint(Log, 512, "Patched %d Setup dependencies\n\r", PatchedCount);
    LogToFile(LogFile, Log);

    return EFI_SUCCESS;
}

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

    // Phase 1: Patch all currently loaded modules
    AsciiSPrint(Log, 512, "\n=== Phase 1: Patching All Loaded Modules ===\n\r");
    LogToFile(LogFile, Log);
    PatchAllLoadedModules(ImageHandle, BiosInfo);

    // Phase 2: Patch Setup dependencies
    AsciiSPrint(Log, 512, "\n=== Phase 2: Patching Setup Dependencies ===\n\r");
    LogToFile(LogFile, Log);
    PatchSetupDependencies(ImageHandle, BiosInfo);

    // Phase 3: Vendor-specific patching
    AsciiSPrint(Log, 512, "\n=== Phase 3: Vendor-Specific Patching ===\n\r");
    LogToFile(LogFile, Log);
    switch (BiosInfo->Type)
    {
    case BIOS_TYPE_AMI:
        Status = PatchAmiBios(ImageHandle, BiosInfo);
        break;

    case BIOS_TYPE_AMI_HP_CUSTOM:
        // HP customized AMI needs special handling
        Status = PatchAmiBios(ImageHandle, BiosInfo);
        if (!EFI_ERROR(Status))
        {
            // Apply HP-specific patches on top
            Status = PatchHpAmiBios(ImageHandle, BiosInfo);
        }
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
 * Execute Setup Browser using FormBrowser2 Protocol
 */
EFI_STATUS ExecuteSetupBrowser(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    EFI_HANDLE AppImageHandle = NULL;
    CHAR8 SetupName[128];
    EFI_FORM_BROWSER2_PROTOCOL *FormBrowser2 = NULL;

    UnicodeStrToAsciiStrS(BiosInfo->SetupModuleName, SetupName, sizeof(SetupName));

    AsciiSPrint(Log, 512, "\n=== Launching Setup Browser ===\n\r");
    LogToFile(LogFile, Log);
    Print(L"\n\r=== Launching Setup Browser ===\n\r");

    // Step 1: Load the Setup module first (this registers HII forms)
    AsciiSPrint(Log, 512, "Loading Setup module: %a\n\r", SetupName);
    LogToFile(LogFile, Log);
    Print(L"Loading Setup module: %a\n\r", SetupName);

    Status = LoadFV(ImageHandle, SetupName, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
    
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "Failed to load %a, trying alternate name: Setup\n\r", SetupName);
        LogToFile(LogFile, Log);
        
        if (BiosInfo->Type == BIOS_TYPE_INSYDE)
        {
            Status = LoadFV(ImageHandle, "SetupUtilityApp", &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
        }
        else
        {
            Status = LoadFV(ImageHandle, "Setup", &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
        }
    }

    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, 512, "ERROR: Could not load Setup module: %r\n\r", Status);
        LogToFile(LogFile, Log);
        Print(L"ERROR: Could not load Setup module: %r\n\r", Status);
        Print(L"System will not boot to OS. Press Ctrl+Alt+Del to restart.\n\r");
        
        if (LogFile != NULL) LogFile->Close(LogFile);
        while (TRUE) gBS->Stall(1000000);
        
        return Status;
    }

    AsciiSPrint(Log, 512, "Setup module loaded at 0x%x (size: 0x%x)\n\r", 
               ImageInfo->ImageBase, ImageInfo->ImageSize);
    LogToFile(LogFile, Log);
    Print(L"Setup module loaded successfully\n\r");

    // Step 2: Patch the loaded Setup module
    AsciiSPrint(Log, 512, "Patching Setup module...\n\r");
    LogToFile(LogFile, Log);
    Print(L"Patching Setup module...\n\r");

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

    // Step 3: Start the Setup module (this will register HII forms)
    AsciiSPrint(Log, 512, "Starting Setup module to register HII forms...\n\r");
    LogToFile(LogFile, Log);
    Print(L"Starting Setup module...\n\r");
    
    Status = gBS->StartImage(AppImageHandle, NULL, NULL);
    
    AsciiSPrint(Log, 512, "Setup module StartImage returned: %r\n\r", Status);
    LogToFile(LogFile, Log);

    // Step 4: Now try to use FormBrowser2 Protocol
    AsciiSPrint(Log, 512, "Locating FormBrowser2 Protocol...\n\r");
    LogToFile(LogFile, Log);
    Print(L"Locating FormBrowser2 Protocol...\n\r");
    
    Status = gBS->LocateProtocol(&gEfiFormBrowser2ProtocolGuid, NULL, (VOID **)&FormBrowser2);
    if (!EFI_ERROR(Status) && FormBrowser2 != NULL)
    {
        AsciiSPrint(Log, 512, "FormBrowser2 Protocol found! Launching Setup UI...\n\r");
        LogToFile(LogFile, Log);
        Print(L"\n\r*** Launching BIOS Setup UI ***\n\r");
        Print(L"*** All hidden menus have been unlocked ***\n\r\n\r");
        
        // Give a moment for message to display
        gBS->Stall(2000000);
        
        // SendForm will display all registered HII forms
        Status = FormBrowser2->SendForm(
            FormBrowser2,
            NULL,           // HiiHandles (NULL = show all)
            0,              // HandleCount
            NULL,           // FormSetGuid (NULL = show all)
            0,              // FormId
            NULL,           // ScreenDimensions
            NULL            // ActionRequest
        );
        
        AsciiSPrint(Log, 512, "FormBrowser2->SendForm returned: %r\n\r", Status);
        LogToFile(LogFile, Log);
    }
    else
    {
        AsciiSPrint(Log, 512, "WARNING: FormBrowser2 Protocol not available: %r\n\r", Status);
        LogToFile(LogFile, Log);
        Print(L"WARNING: FormBrowser2 Protocol not available\n\r");
        Print(L"The Setup module was loaded and patched, but UI could not launch.\n\r");
    }

    // Step 5: After everything, prevent boot to OS
    Print(L"\n\r=====================================\n\r");
    Print(L"Setup session complete.\n\r");
    Print(L"Changes are temporary (in RAM only).\n\r");
    Print(L"System will NOT boot to OS.\n\r");
    Print(L"Press Ctrl+Alt+Del to restart.\n\r");
    Print(L"=====================================\n\r");
    
    AsciiSPrint(Log, 512, "Preventing boot to OS - entering infinite loop\n\r");
    LogToFile(LogFile, Log);
    
    if (LogFile != NULL)
    {
        LogFile->Close(LogFile);
        LogFile = NULL;
    }
    
    // Infinite loop to prevent OS boot
    while (TRUE)
    {
        gBS->Stall(1000000);
    }

    return EFI_SUCCESS;
}

/**
 * Disable write protections - look for common protection checks
 */
UINTN DisableWriteProtections(VOID *ImageBase, UINTN ImageSize)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    UINTN PatchCount = 0;

    // Don't log "Searching for..." for every module - too verbose

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
                    }
                }
            }
        }
    }

    // Only log if patches were applied
    if (PatchCount > 0)
    {
        AsciiSPrint(Log, 512, "Disabled %d write protections\n\r", PatchCount);
        LogToFile(LogFile, Log);
    }

    return PatchCount;
}

/**
 * Patch HP-specific AMI BIOS structures
 * HP uses additional protections and custom form hiding
 */
EFI_STATUS 
PatchHpAmiBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    UINTN FormsUnlocked = 0;
    
    Print(L"\n=== HP AMI BIOS Specific Patching ===\n");
    AsciiSPrint(Log, 512, "Applying HP-specific patches...\n\r");
    LogToFile(LogFile, Log);
    
    // Try to find HPSetupData or NewHPSetupData module
    Status = FindLoadedImageFromName(ImageHandle, "HPSetupData", &ImageInfo);
    if (!EFI_ERROR(Status) && ImageInfo != NULL)
    {
        Print(L"Found HPSetupData module at 0x%p\n", ImageInfo->ImageBase);
        
        // Unlock forms in HP Setup Data
        FormsUnlocked += UnlockHiddenForms(
            ImageInfo->ImageBase,
            ImageInfo->ImageSize,
            BiosInfo
        );
    }
    
    // Also try NewHPSetupData
    Status = FindLoadedImageFromName(ImageHandle, "NewHPSetupData", &ImageInfo);
    if (!EFI_ERROR(Status) && ImageInfo != NULL)
    {
        Print(L"Found NewHPSetupData module at 0x%p\n", ImageInfo->ImageBase);
        
        FormsUnlocked += UnlockHiddenForms(
            ImageInfo->ImageBase,
            ImageInfo->ImageSize,
            BiosInfo
        );
    }
    
    // Try AMITSESetup (HP uses customized AMI TSE)
    Status = FindLoadedImageFromName(ImageHandle, "AMITSESetup", &ImageInfo);
    if (!EFI_ERROR(Status) && ImageInfo != NULL)
    {
        Print(L"Found AMITSESetup module at 0x%p\n", ImageInfo->ImageBase);
        
        // Disable write protections
        DisableWriteProtections(ImageInfo->ImageBase, ImageInfo->ImageSize);
        
        // Unlock forms
        FormsUnlocked += UnlockHiddenForms(
            ImageInfo->ImageBase,
            ImageInfo->ImageSize,
            BiosInfo
        );
    }
    
    Print(L"HP AMI patching complete: %u forms unlocked\n", FormsUnlocked);
    AsciiSPrint(Log, 512, "HP AMI patching complete: %u forms unlocked\n\r", FormsUnlocked);
    LogToFile(LogFile, Log);
    
    return EFI_SUCCESS;
}

/**
 * Unlock hidden forms by patching visibility flags
 * Searches for form visibility structures and enables them
 */
UINTN 
UnlockHiddenForms(VOID *ImageBase, UINTN ImageSize, BIOS_INFO *BiosInfo)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    UINTN UnlockCount = 0;
    UINTN i, j;
    UINT32 *VisibilityFlag;
    BOOLEAN LooksLikeGuid;
    UINT8 ZeroCount, FFCount;
    
    if (ImageBase == NULL || ImageSize == 0)
    {
        return 0;
    }
    
    // Pattern 1: AMI form suppression (suppressif TRUE)
    // Look for: 0x0A 0x82 (SUPPRESSIF opcode) followed by condition
    for (i = 0; i < ImageSize - 10; i++)
    {
        // SUPPRESSIF opcode (0x0A) with TRUE condition
        if (Data[i] == 0x0A && Data[i + 1] == 0x82)
        {
            // Check if this is a TRUE condition (0x46)
            if (Data[i + 2] == 0x46 || Data[i + 3] == 0x46)
            {
                // Replace with FALSE condition (0x47) or remove suppressif
                // Convert SUPPRESSIF TRUE to SUPPRESSIF FALSE
                for (j = 0; j < 4 && (i + j) < ImageSize; j++)
                {
                    if (Data[i + j] == 0x46)
                    {
                        Data[i + j] = 0x47;  // TRUE -> FALSE
                        UnlockCount++;
                        break;
                    }
                }
            }
        }
        
        // GRAYOUTIF opcode (0x19)
        if (Data[i] == 0x19 && i + 3 < ImageSize)
        {
            if (Data[i + 2] == 0x46)
            {
                Data[i + 2] = 0x47;  // TRUE -> FALSE
                UnlockCount++;
            }
        }
        
        // DISABLEIF opcode (0x1E)
        if (Data[i] == 0x1E && i + 3 < ImageSize)
        {
            if (Data[i + 2] == 0x46)
            {
                Data[i + 2] = 0x47;  // TRUE -> FALSE
                UnlockCount++;
            }
        }
    }
    
    // Pattern 2: HP-specific form visibility flags
    // HP uses a different structure for form visibility
    // Search for patterns like: [GUID][uint32 visibility flag]
    // When flag is 0x00000000, form is hidden
    for (i = 0; i < ImageSize - 20; i++)
    {
        // Look for GUID pattern followed by 0x00000000 (hidden flag)
        // GUIDs have specific structure, we can detect them
        if (i + 20 < ImageSize)
        {
            // Check if next 4 bytes after potential GUID (16 bytes) are 0x00000000
            VisibilityFlag = (UINT32 *)&Data[i + 16];
            
            // If we find a 0x00000000 flag that looks like a visibility control
            // we can try changing it to 0x01000000 (visible)
            // This is a heuristic and should be used carefully
            if (*VisibilityFlag == 0x00000000)
            {
                // Verify this looks like a form structure
                // Check for GUID-like pattern (not all zeros, not all FFs)
                LooksLikeGuid = FALSE;
                ZeroCount = 0;
                FFCount = 0;
                
                for (j = 0; j < 16; j++)
                {
                    if (Data[i + j] == 0x00) ZeroCount++;
                    if (Data[i + j] == 0xFF) FFCount++;
                }
                
                // GUID should have mix of values
                if (ZeroCount < 14 && FFCount < 14)
                {
                    LooksLikeGuid = TRUE;
                }
                
                if (LooksLikeGuid)
                {
                    // Enable this form
                    *VisibilityFlag = 0x01000000;
                    UnlockCount++;
                }
            }
        }
    }
    
    return UnlockCount;
}

/**
 * Save patched values to BIOS data structures
 * This writes directly to in-memory structures, not NVRAM
 */
EFI_STATUS 
PatchBiosData(
    VOID *ImageBase,
    UINTN ImageSize,
    CHAR16 *VarName,
    UINTN Offset,
    VOID *Value,
    UINTN ValueSize
)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    CHAR8 AsciiVarName[128];
    UINTN VarNameLen;
    UINTN MatchCount = 0;
    
    if (ImageBase == NULL || VarName == NULL || Value == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }
    
    if (ImageSize == 0 || ValueSize == 0)
    {
        return EFI_INVALID_PARAMETER;
    }
    
    // Convert variable name to ASCII for searching
    VarNameLen = StrLen(VarName);
    if (VarNameLen >= sizeof(AsciiVarName))
    {
        return EFI_INVALID_PARAMETER;
    }
    
    for (UINTN i = 0; i < VarNameLen; i++)
    {
        AsciiVarName[i] = (CHAR8)VarName[i];
    }
    AsciiVarName[VarNameLen] = 0;
    
    Print(L"Searching for variable '%s' in BIOS data...\n", VarName);
    
    // Search for variable name in the image
    for (UINTN i = 0; i < ImageSize - VarNameLen; i++)
    {
        if (CompareMem(&Data[i], AsciiVarName, VarNameLen) == 0)
        {
            Print(L"Found variable reference at offset 0x%X\n", i);
            MatchCount++;
            
            // Check if we can apply the patch at the specified offset
            if (i + Offset + ValueSize <= ImageSize)
            {
                Print(L"Patching %u bytes at offset 0x%X + 0x%X\n", 
                      ValueSize, i, Offset);
                
                // Apply the patch
                CopyMem(&Data[i + Offset], Value, ValueSize);
                
                Print(L"Patch applied successfully\n");
                return EFI_SUCCESS;
            }
        }
    }
    
    if (MatchCount == 0)
    {
        Print(L"Variable '%s' not found in BIOS data\n", VarName);
        return EFI_NOT_FOUND;
    }
    
    Print(L"Found variable but could not apply patch at specified offset\n");
    return EFI_INVALID_PARAMETER;
}
