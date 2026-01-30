#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Library/PrintLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/FormBrowserEx.h>
#include <Protocol/FormBrowserEx2.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/DisplayProtocol.h>
#include <Protocol/HiiPopup.h>
#include <Library/MemoryAllocationLib.h>
#include "Utility.h"
#include "Opcode.h"
#include "BiosDetector.h"
#include "AutoPatcher.h"
#include "MenuUI.h"
#include "HiiBrowser.h"
#define SREP_VERSION L"0.3.0"

EFI_BOOT_SERVICES *_gBS = NULL;
EFI_RUNTIME_SERVICES *_gRS = NULL;
EFI_FILE *LogFile = NULL;
char Log[512];
enum
{
    OFFSET = 1,
    PATTERN,
    REL_NEG_OFFSET,
    REL_POS_OFFSET
};

enum OPCODE
{
    NO_OP,
    LOADED,
    LOAD_FS,
    LOAD_FV,
    PATCH,
    EXEC
};

struct OP_DATA
{
    enum OPCODE ID;
    CHAR8 *Name;
    BOOLEAN Name_Dyn_Alloc;
    UINT64 PatterType;
    BOOLEAN PatterType_Dyn_Alloc;
    INT64 ARG3;
    BOOLEAN ARG3_Dyn_Alloc;
    UINT64 ARG4;
    BOOLEAN ARG4_Dyn_Alloc;
    UINT64 ARG5;
    BOOLEAN ARG5_Dyn_Alloc;
    UINT64 ARG6;
    BOOLEAN ARG6_Dyn_Alloc;
    UINT64 ARG7;
    BOOLEAN ARG7_Dyn_Alloc;
    struct OP_DATA *next;
    struct OP_DATA *prev;
};


void LogToFile( EFI_FILE *LogFile, char *String)
{
        UINTN Size = AsciiStrLen(String);
        LogFile->Write(LogFile,&Size,String);
        LogFile->Flush(LogFile);
}


VOID Add_OP_CODE(struct OP_DATA *Start, struct OP_DATA *opCode)
{
    struct OP_DATA *next = Start;
    while (next->next != NULL)
    {
        next = next->next;
    }
    next->next = opCode;
    opCode->prev = next;
}

VOID PrintOPChain(struct OP_DATA *Start)
{
    struct OP_DATA *next = Start;
    while (next != NULL)
    {
        AsciiSPrint(Log,512,"%a","OPCODE : ");
        LogToFile(LogFile,Log);
        switch (next->ID)
        {
        case NO_OP:
            AsciiSPrint(Log,512,"%a","NOP\n\r");
            LogToFile(LogFile,Log);
            break;
        case LOADED:
            AsciiSPrint(Log,512,"%a","LOADED\n\r");
            LogToFile(LogFile,Log);
            break;
        case LOAD_FS:
            AsciiSPrint(Log,512,"%a","LOAD_FS\n\r");
            LogToFile(LogFile,Log);
            AsciiSPrint(Log,512,"%a","\t FileName %a\n\r", next->Name);
            LogToFile(LogFile,Log);
            break;
        case LOAD_FV:
            AsciiSPrint(Log,512,"%a","LOAD_FV\n\r");
            LogToFile(LogFile,Log);
            AsciiSPrint(Log,512,"%a","\t FileName %a\n\r", next->Name);
            LogToFile(LogFile,Log);
            break;
        case PATCH:
            AsciiSPrint(Log,512,"%a","PATCH\n\r");
            LogToFile(LogFile,Log);
            break;
        case EXEC:
            AsciiSPrint(Log,512,"%a","EXEC\n\r");
            LogToFile(LogFile,Log);
            break;

        default:
            break;
        }
        next = next->next;
    }
}

VOID PrintDump(UINT16 Size, UINT8 *DUMP)
{
    for (UINT16 i = 0; i < Size; i++)
    {
        if (i % 0x10 == 0)
        {
            AsciiSPrint(Log,512,"%a","\n\t");
            LogToFile(LogFile,Log);
        }
        AsciiSPrint(Log,512,"%02x ", DUMP[i]);
        LogToFile(LogFile,Log);
    }
    AsciiSPrint(Log,512,"%a","\n\t");
    LogToFile(LogFile,Log);
}


// ========== MENU CALLBACK FUNCTIONS ==========

// Global context for menu callbacks
typedef struct {
    EFI_HANDLE ImageHandle;
    BIOS_INFO BiosInfo;
} SREP_CONTEXT;

/**
 * Callback: Auto-detect and patch BIOS
 */
EFI_STATUS MenuCallback_AutoPatch(MENU_ITEM *Item, VOID *Context)
{
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    SREP_CONTEXT *SrepCtx = (SREP_CONTEXT *)Item->Data;
    EFI_STATUS Status;
    
    MenuShowMessage(MenuCtx, L"Auto-Patch", L"Detecting BIOS and applying patches...");
    
    // Detect BIOS type
    Status = DetectBiosType(&SrepCtx->BiosInfo);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"BIOS detection failed!");
        return Status;
    }
    
    // Auto-patch
    Status = AutoPatchBios(SrepCtx->ImageHandle, &SrepCtx->BiosInfo);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Auto-patching failed!");
        return Status;
    }
    
    MenuShowMessage(MenuCtx, L"Success", L"Patching complete! Setup browser will launch.");
    
    // This will not return (enters infinite loop after Setup)
    return EFI_SUCCESS;
}

/**
 * Callback: Browse BIOS settings (read-only)
 */
EFI_STATUS MenuCallback_BrowseSettings(MENU_ITEM *Item, VOID *Context)
{
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    HII_BROWSER_CONTEXT HiiCtx;
    EFI_STATUS Status;
    
    MenuShowMessage(MenuCtx, L"Loading", L"Loading BIOS modules and NVRAM data...");
    
    // Initialize HII browser (includes NVRAM loading)
    Status = HiiBrowserInitialize(&HiiCtx);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to initialize HII browser!");
        return Status;
    }
    
    HiiCtx.MenuContext = MenuCtx;
    
    // Enumerate forms
    Status = HiiBrowserEnumerateForms(&HiiCtx);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to enumerate BIOS forms!");
        HiiBrowserCleanup(&HiiCtx);
        return Status;
    }
    
    // Create and show forms menu
    MENU_PAGE *FormsMenu = HiiBrowserCreateFormsMenu(&HiiCtx);
    if (FormsMenu == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to create forms menu!");
        HiiBrowserCleanup(&HiiCtx);
        return EFI_OUT_OF_RESOURCES;
    }
    
    MenuNavigateTo(MenuCtx, FormsMenu);
    
    // Cleanup
    MenuFreePage(FormsMenu);
    HiiBrowserCleanup(&HiiCtx);
    
    return EFI_SUCCESS;
}

/**
 * Callback: Load BIOS modules and edit settings
 */
EFI_STATUS MenuCallback_LoadAndEdit(MENU_ITEM *Item, VOID *Context)
{
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    SREP_CONTEXT *SrepCtx = (SREP_CONTEXT *)Item->Data;
    HII_BROWSER_CONTEXT HiiCtx;
    EFI_STATUS Status;
    
    MenuShowMessage(MenuCtx, L"Loading", L"Detecting BIOS and loading modules...");
    
    // Detect BIOS type
    Status = DetectBiosType(&SrepCtx->BiosInfo);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"BIOS detection failed!");
        return Status;
    }
    
    // Load Setup modules (without launching yet)
    Print(L"\nLoading BIOS modules...\n\r");
    Print(L"BIOS Type: %s\n\r", GetBiosTypeString(SrepCtx->BiosInfo.Type));
    Print(L"Vendor: %s\n\r", SrepCtx->BiosInfo.VendorName);
    
    // Initialize HII browser with NVRAM
    Status = HiiBrowserInitialize(&HiiCtx);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to initialize HII browser!");
        return Status;
    }
    
    HiiCtx.MenuContext = MenuCtx;
    
    // Enumerate forms
    Status = HiiBrowserEnumerateForms(&HiiCtx);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to enumerate BIOS forms!");
        HiiBrowserCleanup(&HiiCtx);
        return Status;
    }
    
    // Show success
    CHAR16 SuccessMsg[256];
    UnicodeSPrint(SuccessMsg, sizeof(SuccessMsg),
                  L"Loaded %d BIOS forms\n%d NVRAM variables loaded",
                  HiiCtx.FormCount,
                  HiiCtx.NvramManager ? HiiCtx.NvramManager->VariableCount : 0);
    
    MenuShowMessage(MenuCtx, L"Success", SuccessMsg);
    
    // Create forms menu with edit capability
    MENU_PAGE *FormsMenu = HiiBrowserCreateFormsMenu(&HiiCtx);
    if (FormsMenu == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to create forms menu!");
        HiiBrowserCleanup(&HiiCtx);
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Add save option at the end
    MENU_PAGE *EditMenu = MenuCreatePage(L"BIOS Settings Editor", FormsMenu->ItemCount + 2);
    if (EditMenu)
    {
        // Copy form items
        for (UINTN i = 0; i < FormsMenu->ItemCount; i++)
        {
            EditMenu->Items[i] = FormsMenu->Items[i];
        }
        
        // Add separator and save option
        MenuAddSeparator(EditMenu, FormsMenu->ItemCount, NULL);
        MenuAddActionItem(
            EditMenu,
            FormsMenu->ItemCount + 1,
            L"Save Changes to NVRAM",
            L"Write modified values to BIOS NVRAM",
            NULL,  // We'll handle this inline
            &HiiCtx
        );
        
        MenuNavigateTo(MenuCtx, EditMenu);
        
        MenuFreePage(EditMenu);
    }
    
    MenuFreePage(FormsMenu);
    
    // Save changes if any modifications were made
    if (HiiCtx.NvramManager && NvramGetModifiedCount(HiiCtx.NvramManager) > 0)
    {
        HiiBrowserSaveChanges(&HiiCtx);
    }
    
    HiiBrowserCleanup(&HiiCtx);
    
    return EFI_SUCCESS;
}

/**
 * Callback: Launch Setup Browser directly
 */
EFI_STATUS MenuCallback_LaunchSetup(MENU_ITEM *Item, VOID *Context)
{
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    EFI_FORM_BROWSER2_PROTOCOL *FormBrowser2;
    EFI_STATUS Status;
    
    // Try to locate FormBrowser2 protocol
    Status = gBS->LocateProtocol(&gEfiFormBrowser2ProtocolGuid, NULL, (VOID **)&FormBrowser2);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"FormBrowser2 Protocol not available!");
        return Status;
    }
    
    MenuShowMessage(MenuCtx, L"Launching", L"Starting BIOS Setup Browser...");
    
    // Clear screen and launch Setup
    gST->ConOut->ClearScreen(gST->ConOut);
    
    Status = FormBrowser2->SendForm(FormBrowser2, NULL, 0, NULL, 0, NULL, NULL);
    
    // Redraw menu after Setup returns
    MenuDraw(MenuCtx);
    
    return EFI_SUCCESS;
}

/**
 * Callback: About dialog
 */
EFI_STATUS MenuCallback_About(MENU_ITEM *Item, VOID *Context)
{
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    
    MenuShowMessage(MenuCtx, 
        L"About SREP",
        L"SmokelessRuntimeEFIPatcher v0.3.0\nInteractive BIOS Patcher"
    );
    
    return EFI_SUCCESS;
}

/**
 * Callback: Exit application
 */
EFI_STATUS MenuCallback_Exit(MENU_ITEM *Item, VOID *Context)
{
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    BOOLEAN Confirm = FALSE;
    
    MenuShowConfirm(MenuCtx, L"Exit", L"Are you sure you want to exit?", &Confirm);
    
    if (Confirm)
    {
        MenuCtx->Running = FALSE;
    }
    
    return EFI_SUCCESS;
}

/**
 * Create the main menu
 */
MENU_PAGE *CreateMainMenu(SREP_CONTEXT *SrepCtx)
{
    MENU_PAGE *MainMenu = MenuCreatePage(L"SmokelessRuntimeEFIPatcher - Main Menu", 10);
    if (MainMenu == NULL)
        return NULL;
    
    MenuAddInfoItem(MainMenu, 0, L"SREP v0.3.0 - Interactive BIOS Patcher");
    MenuAddSeparator(MainMenu, 1, NULL);
    
    MenuAddActionItem(
        MainMenu, 2,
        L"Auto-Detect and Patch BIOS",
        L"Automatically detect BIOS type and apply patches",
        MenuCallback_AutoPatch,
        SrepCtx
    );
    
    MenuAddActionItem(
        MainMenu, 3,
        L"Load Modules and Edit Settings",
        L"Load BIOS modules and edit NVRAM settings",
        MenuCallback_LoadAndEdit,
        SrepCtx
    );
    
    MenuAddActionItem(
        MainMenu, 4,
        L"Browse BIOS Settings (Read-Only)",
        L"View BIOS settings without editing",
        MenuCallback_BrowseSettings,
        NULL
    );
    
    MenuAddActionItem(
        MainMenu, 5,
        L"Launch Setup Browser",
        L"Launch BIOS Setup Browser directly",
        MenuCallback_LaunchSetup,
        NULL
    );
    
    MenuAddSeparator(MainMenu, 6, NULL);
    
    MenuAddInfoItem(MainMenu, 7, L"Changes saved to NVRAM persist across reboots");
    
    MenuAddSeparator(MainMenu, 8, NULL);
    
    MenuAddActionItem(
        MainMenu, 9,
        L"Exit",
        L"Exit to UEFI Shell or Boot Menu",
        MenuCallback_Exit,
        NULL
    );
    
    return MainMenu;
}


EFI_STATUS EFIAPI SREPEntry(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root;
    EFI_FILE *ConfigFile;
    CHAR16 FileName[255];
    BOOLEAN UseAutoMode = TRUE;
    
    Print(L"Welcome to SREP (Smokeless Runtime EFI Patcher) %s\n\r", SREP_VERSION);
    Print(L"Enhanced with Auto-Detection and Intelligent Patching\n\r");
    
    gBS->SetWatchdogTimer(0, 0, 0, 0);
    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    Status = Root->Open(Root, &LogFile, L"SREP.log", EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
    if (Status != EFI_SUCCESS)
    {
        Print(L"Failed on Opening Log File : %r\n\r", Status);
        return Status;
    }
    AsciiSPrint(Log,512,"Welcome to SREP (Smokeless Runtime EFI Patcher) %s\n\r", SREP_VERSION);
    LogToFile(LogFile,Log);
    AsciiSPrint(Log,512,"Enhanced with Auto-Detection and Intelligent Patching\n\r");
    LogToFile(LogFile,Log);
    
    // Check for interactive mode flag file
    BOOLEAN UseInteractiveMode = FALSE;
    EFI_FILE *InteractiveFlag;
    Status = Root->Open(Root, &InteractiveFlag, L"SREP_Interactive.flag", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        InteractiveFlag->Close(InteractiveFlag);
        UseInteractiveMode = TRUE;
        AsciiSPrint(Log,512,"Interactive mode flag found\n\r");
        LogToFile(LogFile,Log);
    }
    
    // If no flag file, default to interactive mode (new behavior in v0.3.0)
    // Users can create SREP_Auto.flag to skip interactive mode
    EFI_FILE *AutoFlag;
    Status = Root->Open(Root, &AutoFlag, L"SREP_Auto.flag", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        AutoFlag->Close(AutoFlag);
        UseInteractiveMode = FALSE;
        AsciiSPrint(Log,512,"Auto mode flag found, skipping interactive menu\n\r");
        LogToFile(LogFile,Log);
    }
    else
    {
        // Default to interactive mode
        UseInteractiveMode = TRUE;
    }
    
    // INTERACTIVE MODE: Show menu interface
    if (UseInteractiveMode)
    {
        AsciiSPrint(Log,512,"\n=== INTERACTIVE MODE: Starting Menu ===\n\r");
        LogToFile(LogFile,Log);
        
        // Initialize menu system
        MENU_CONTEXT MenuCtx;
        Status = MenuInitialize(&MenuCtx);
        if (EFI_ERROR(Status))
        {
            Print(L"Failed to initialize menu system: %r\n\r", Status);
            LogFile->Close(LogFile);
            return Status;
        }
        
        // Create SREP context for callbacks
        SREP_CONTEXT SrepCtx;
        SrepCtx.ImageHandle = ImageHandle;
        ZeroMem(&SrepCtx.BiosInfo, sizeof(BIOS_INFO));
        
        // Create main menu
        MENU_PAGE *MainMenu = CreateMainMenu(&SrepCtx);
        if (MainMenu == NULL)
        {
            Print(L"Failed to create main menu\n\r");
            LogFile->Close(LogFile);
            return EFI_OUT_OF_RESOURCES;
        }
        
        // Run menu loop
        Status = MenuRun(&MenuCtx, MainMenu);
        
        // Cleanup
        MenuFreePage(MainMenu);
        MenuCleanup(&MenuCtx);
        
        LogFile->Close(LogFile);
        return Status;
    }
    
    // Check if config file exists (for backward compatibility)
    UnicodeSPrint(FileName, sizeof(FileName), L"%a", "SREP_Config.cfg");
    Status = Root->Open(Root, &ConfigFile, FileName, EFI_FILE_MODE_READ, 0);
    if (Status != EFI_SUCCESS)
    {
        AsciiSPrint(Log,512,"Config file not found, using AUTO mode\n\r");
        LogToFile(LogFile,Log);
        UseAutoMode = TRUE;
    }
    else
    {
        AsciiSPrint(Log,512,"Config file found, using MANUAL mode\n\r");
        LogToFile(LogFile,Log);
        UseAutoMode = FALSE;
    }
    
    // AUTO MODE: Detect and patch automatically
    if (UseAutoMode)
    {
        BIOS_INFO BiosInfo;
        ZeroMem(&BiosInfo, sizeof(BIOS_INFO));
        
        AsciiSPrint(Log,512,"\n=== AUTO MODE: Detecting BIOS Type ===\n\r");
        LogToFile(LogFile,Log);
        
        // Detect BIOS type
        Status = DetectBiosType(&BiosInfo);
        if (EFI_ERROR(Status))
        {
            AsciiSPrint(Log,512,"BIOS detection failed: %r\n\r", Status);
            LogToFile(LogFile,Log);
            Print(L"BIOS detection failed: %r\n\r", Status);
            Print(L"Press any key to exit...\n\r");
            UINTN Index;
            gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
            return Status;
        }
        
        Print(L"\nDetected BIOS: %s\n\r", GetBiosTypeString(BiosInfo.Type));
        Print(L"Vendor: %s\n\r", BiosInfo.VendorName);
        Print(L"Version: %s\n\r", BiosInfo.Version);
        Print(L"\nStarting automatic patching...\n\r");
        
        // Auto-patch based on detected BIOS
        Status = AutoPatchBios(ImageHandle, &BiosInfo);
        
        if (EFI_ERROR(Status))
        {
            AsciiSPrint(Log,512,"Auto-patching failed: %r\n\r", Status);
            LogToFile(LogFile,Log);
            Print(L"Auto-patching failed: %r\n\r", Status);
        }
        
        LogFile->Close(LogFile);
        return Status;
    }
    
    // MANUAL MODE: Use config file (legacy behavior)
    AsciiSPrint(Log,512,"\n=== MANUAL MODE: Using Config File ===\n\r");
    LogToFile(LogFile,Log);

   AsciiSPrint(Log,512,"%a","Opened SREP_Config\n\r");
   LogToFile(LogFile,Log);
    EFI_GUID gFileInfo = EFI_FILE_INFO_ID;
    EFI_FILE_INFO *FileInfo = NULL;
    UINTN FileInfoSize = 0;
    Status = ConfigFile->GetInfo(ConfigFile, &gFileInfo, &FileInfoSize, &FileInfo);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        FileInfo = AllocatePool(FileInfoSize);
        Status = ConfigFile->GetInfo(ConfigFile, &gFileInfo, &FileInfoSize, FileInfo);
        if (Status != EFI_SUCCESS)
        {
            AsciiSPrint(Log,512,"Failed Getting SREP_Config Info: %r\n\r", Status);
            LogToFile(LogFile,Log);
            return Status;
        }
    }
    UINTN ConfigDataSize = FileInfo->FileSize + 1; // Add Last null Terminalto
    AsciiSPrint(Log,512,"Config Size: 0x%x\n\r", ConfigDataSize);
    LogToFile(LogFile,Log);
    CHAR8 *ConfigData = AllocateZeroPool(ConfigDataSize);
    FreePool(FileInfo);

    Status = ConfigFile->Read(ConfigFile, &ConfigDataSize, ConfigData);
    if (Status != EFI_SUCCESS)
    {
        AsciiSPrint(Log,512,"Failed on Reading SREP_Config : %r\n\r", Status);
         LogToFile(LogFile,Log);
        return Status;
    }
    AsciiSPrint(Log,512,"%a","Parsing Config\n\r");
     LogToFile(LogFile,Log);
    ConfigFile->Close(ConfigFile);
    AsciiSPrint(Log,512,"%a","Stripping NewLine, Carriage and tab Return\n\r");
     LogToFile(LogFile,Log);
    for (UINTN i = 0; i < ConfigDataSize; i++)
    {
        if (ConfigData[i] == '\n' || ConfigData[i] == '\r' || ConfigData[i] == '\t')
        {
            ConfigData[i] = '\0';
        }
    }
    UINTN curr_pos = 0;

    struct OP_DATA *Start = AllocateZeroPool(sizeof(struct OP_DATA));
    struct OP_DATA *Prev_OP;
    Start->ID = NO_OP;
    Start->next = NULL;
    BOOLEAN NullByteSkipped = FALSE;
    while (curr_pos < ConfigDataSize)
    {

        if (curr_pos != 0 && !NullByteSkipped)
            curr_pos += AsciiStrLen(&ConfigData[curr_pos]);
        if (ConfigData[curr_pos] == '\0')
        {
            curr_pos += 1;
            NullByteSkipped = TRUE;
            continue;
        }
        NullByteSkipped = FALSE;
        AsciiSPrint(Log,512,"Current Parsing %a\n\r", &ConfigData[curr_pos]);
         LogToFile(LogFile,Log);
        if (AsciiStrStr(&ConfigData[curr_pos], "End"))
        {
            AsciiSPrint(Log,512,"%a","End OP Detected\n\r");
             LogToFile(LogFile,Log);
            continue;
        }
        if (AsciiStrStr(&ConfigData[curr_pos], "Op"))
        {
            AsciiSPrint(Log,512,"%a","OP Detected\n\r");
             LogToFile(LogFile,Log);
            curr_pos += 3;
            AsciiSPrint(Log,512,"Commnand %a \n\r", &ConfigData[curr_pos]);
             LogToFile(LogFile,Log);
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFS"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOAD_FS;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFV"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOAD_FV;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Loaded"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = LOADED;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Patch"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = PATCH;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }

            if (AsciiStrStr(&ConfigData[curr_pos], "Exec"))
            {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = EXEC;
                Add_OP_CODE(Start, Prev_OP);
                continue;
            }
            AsciiSPrint(Log,512,"Commnand %a Invalid \n\r", &ConfigData[curr_pos]);
             LogToFile(LogFile,Log);
            return EFI_INVALID_PARAMETER;
        }
        if ((Prev_OP->ID == LOAD_FS || Prev_OP->ID == LOAD_FV || Prev_OP->ID == LOADED) && Prev_OP->Name == 0)
        {
            AsciiSPrint(Log,512,"Found File %a \n\r", &ConfigData[curr_pos]);
             LogToFile(LogFile,Log);
            UINTN FileNameLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            CHAR8 *FileName = AllocateZeroPool(FileNameLength);
            CopyMem(FileName, &ConfigData[curr_pos], FileNameLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            continue;
        }
        if (Prev_OP->ID == PATCH && Prev_OP->PatterType == 0)
        {
            if (AsciiStrStr(&ConfigData[curr_pos], "Offset"))
            {
                AsciiSPrint(Log,512,"%a","Found Offset\n\r");
                 LogToFile(LogFile,Log);
                Prev_OP->PatterType = OFFSET;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Pattern"))
            {
                AsciiSPrint(Log,512,"%a","Found Pattern\n\r");
                 LogToFile(LogFile,Log);
                Prev_OP->PatterType = PATTERN;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "RelNegOffset"))
            {
                AsciiSPrint(Log,512,"%a","Found Offset\n\r");
                 LogToFile(LogFile,Log);
                Prev_OP->PatterType = REL_NEG_OFFSET;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "RelPosOffset"))
            {
                AsciiSPrint(Log,512,"%a","Found Offset\n\r");
                 LogToFile(LogFile,Log);
                Prev_OP->PatterType = REL_POS_OFFSET;
            }
            continue;
        }

        //  this new itereration whe are just in from of the Pattern
        if (Prev_OP->ID == PATCH && Prev_OP->PatterType != 0 && Prev_OP->ARG3 == 0)
        {
            if (Prev_OP->PatterType == OFFSET || Prev_OP->PatterType == REL_NEG_OFFSET || Prev_OP->PatterType == REL_POS_OFFSET)
            {
                AsciiSPrint(Log,512,"%a","Decode Offset\n\r");
                 LogToFile(LogFile,Log);
                Prev_OP->ARG3 = AsciiStrHexToUint64(&ConfigData[curr_pos]);
            }
            if (Prev_OP->PatterType == PATTERN)
            {
                Prev_OP->ARG3 = 0xFFFFFFFF;
                Prev_OP->ARG6 = AsciiStrLen(&ConfigData[curr_pos]) / 2;
                AsciiSPrint(Log,512,"Found %d Bytes\n\r", Prev_OP->ARG6);
                 LogToFile(LogFile,Log);
                Prev_OP->ARG7 = (UINT64)AllocateZeroPool(Prev_OP->ARG6);
                AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->ARG6 * 2, (UINT8 *)Prev_OP->ARG7, Prev_OP->ARG6);
            }
            continue;
        }

        if (Prev_OP->ID == PATCH && Prev_OP->PatterType != 0 && Prev_OP->ARG3 != 0)
        {

            Prev_OP->ARG4 = AsciiStrLen(&ConfigData[curr_pos]) / 2;
            AsciiSPrint(Log,512,"Found %d Bytes\n\r", Prev_OP->ARG4);
             LogToFile(LogFile,Log);
            Prev_OP->ARG5_Dyn_Alloc = TRUE;
            Prev_OP->ARG5 = (UINT64)AllocateZeroPool(Prev_OP->ARG4);
            AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->ARG4 * 2, (UINT8 *)Prev_OP->ARG5, Prev_OP->ARG4);
            AsciiSPrint(Log,512,"%a","Patch Byte\n\r");
             LogToFile(LogFile,Log);
            PrintDump(Prev_OP->ARG4,  (UINT8 *)Prev_OP->ARG5);
            continue;
        }
    }
    FreePool(ConfigData);
   // PrintOPChain(Start);
    // dispatch
    struct OP_DATA *next;
    EFI_HANDLE AppImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo;
    INT64 BaseOffset;
    for (next = Start; next != NULL; next = next->next)
    {
        switch (next->ID)
        {
        case NO_OP:
            // AsciiSPrint(Log,512,"NOP\n\r");
            break;
        case LOADED:
            AsciiSPrint(Log,512,"%a","Executing Loaded OP\n\r");
             LogToFile(LogFile,Log);
            Status = FindLoadedImageFromName(ImageHandle, next->Name, &ImageInfo);
            AsciiSPrint(Log,512,"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
             LogToFile(LogFile,Log);
            break;
        case LOAD_FS:
            AsciiSPrint(Log,512,"%a","Executing Load from FS\n\r");    
            LogToFile(LogFile,Log);
            Status = LoadFS(ImageHandle, next->Name, &ImageInfo, &AppImageHandle);
            AsciiSPrint(Log,512,"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
             LogToFile(LogFile,Log);
            // AsciiSPrint(Log,512,"\t FileName %a\n\r", next->ARG2);
            break;
        case LOAD_FV:
            AsciiSPrint(Log,512,"%a","Executing Load from FV\n\r");    
            LogToFile(LogFile,Log);
            Status = LoadFV(ImageHandle, next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32);
            AsciiSPrint(Log,512,"Loaded Image %r -> %x\n\r", Status, ImageInfo->ImageBase);
            LogToFile(LogFile,Log);
            break;
        case PATCH:
            AsciiSPrint(Log,512,"%a","Executing Patch\n\r");    
            LogToFile(LogFile,Log);
            AsciiSPrint(Log,512,"Patching Image Size %x: \n\r", ImageInfo->ImageSize);
            LogToFile(LogFile,Log);
            PrintDump(next->ARG6, (UINT8 *)next->ARG7);

            PrintDump(next->ARG6, ((UINT8 *)ImageInfo->ImageBase) + 0x1A383);
            // PrintDump(0x200, (UINT8 *)(LoadedImage->ImageBase));
            if (next->PatterType == PATTERN)
            {

                AsciiSPrint(Log,512,"%a","Finding Offset\n\r");
                LogToFile(LogFile,Log);
                for (UINTN i = 0; i < ImageInfo->ImageSize - next->ARG6; i += 1)
                {
                    if (CompareMem(((UINT8 *)ImageInfo->ImageBase) + i, (UINT8 *)next->ARG7, next->ARG6) == 0)
                    {
                        next->ARG3 = i;
                        AsciiSPrint(Log,512,"Found at %x\n\r", i);
                        LogToFile(LogFile,Log);
                        break;
                    }
                }
                if (next->ARG3 == 0xFFFFFFFF)
                {
                    AsciiSPrint(Log,512,"%a","No Patter Found\n\r");
                    LogToFile(LogFile,Log);
                    //goto cleanup;
                break;
                }
            }
            if (next->PatterType == REL_POS_OFFSET)
            {
                next->ARG3 = BaseOffset + next->ARG3;
            }
            if (next->PatterType == REL_NEG_OFFSET)
            {
                next->ARG3 = BaseOffset - next->ARG3;
            }
            BaseOffset = next->ARG3;
            AsciiSPrint(Log,512,"Offset %x\n\r", next->ARG3);
            LogToFile(LogFile,Log);
            // PrintDump(next->ARG4+10,ImageInfo->ImageBase + next->ARG3 -5 );
            CopyMem(ImageInfo->ImageBase + next->ARG3, (UINT8 *)next->ARG5, next->ARG4);
            AsciiSPrint(Log,512,"%a","Patched\n\r");
            LogToFile(LogFile,Log);
            // PrintDump(next->ARG4+10,ImageInfo->ImageBase + next->ARG3 -5 );
            break;
        case EXEC:
            Exec(&AppImageHandle);
            AsciiSPrint(Log,512,"%a","EXEC %r\n\r", Status);
            LogToFile(LogFile,Log);
            break;

        default:
            break;
        }
    }
//cleanup:
    for (next = Start; next != NULL; next = next->next)
    {
        if (next->Name_Dyn_Alloc)
            FreePool((VOID *)next->Name);
        if (next->PatterType_Dyn_Alloc)
            FreePool((VOID *)next->PatterType);
        if (next->ARG3_Dyn_Alloc)
            FreePool((VOID *)next->ARG3);
        if (next->ARG4_Dyn_Alloc)
            FreePool((VOID *)next->ARG4);
        if (next->ARG5_Dyn_Alloc)
            FreePool((VOID *)next->ARG5);
        if (next->ARG6_Dyn_Alloc)
            FreePool((VOID *)next->ARG6);
        if (next->ARG7_Dyn_Alloc)
            FreePool((VOID *)next->ARG7);
    }
    next = Start;
    while (next->next != NULL)
    {
        struct OP_DATA *tmp = next;
        next = next->next;
        FreePool(tmp);
    }
    return EFI_SUCCESS;
    // FindBaseAddressFromName(L"H2OFormBrowserDxe");
    // UINT8 *Buffer = NULL;
    // UINTN BufferSize = 0;
    // LocateAndLoadFvFromName(L"SetupUtilityApp", EFI_SECTION_PE32, &Buffer, &BufferSize);
}
