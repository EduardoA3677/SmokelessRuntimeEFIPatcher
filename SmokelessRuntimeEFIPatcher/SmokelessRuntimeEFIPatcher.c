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
#include "Constants.h"
#include "Utility.h"
#include "Opcode.h"
#include "BiosDetector.h"
#include "AutoPatcher.h"
#include "MenuUI.h"
#include "HiiBrowser.h"
#include "ConfigManager.h"
#include "NvramManager.h"

EFI_BOOT_SERVICES *_gBS = NULL;
EFI_RUNTIME_SERVICES *_gRS = NULL;
EFI_FILE *LogFile = NULL;
char Log[LOG_BUFFER_SIZE];
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
    MENU_CONTEXT *MenuContext;
    NVRAM_MANAGER *NvramManager;
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
    EFI_STATUS Status;
    
    MenuShowMessage(MenuCtx, L"Loading", L"Loading BIOS modules and NVRAM data...");
    
    // Allocate HiiCtx dynamically
    HII_BROWSER_CONTEXT *HiiCtx = AllocateZeroPool(sizeof(HII_BROWSER_CONTEXT));
    if (HiiCtx == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to allocate HII browser context!");
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Initialize HII browser (includes NVRAM loading)
    Status = HiiBrowserInitialize(HiiCtx);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to initialize HII browser!");
        FreePool(HiiCtx);
        return Status;
    }
    
    HiiCtx->MenuContext = MenuCtx;
    
    // Store HiiCtx temporarily for this browsing session
    VOID *PreviousUserData = MenuCtx->UserData;
    MenuCtx->UserData = (VOID *)HiiCtx;
    
    // Enumerate forms
    Status = HiiBrowserEnumerateForms(HiiCtx);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to enumerate BIOS forms!");
        HiiBrowserCleanup(HiiCtx);
        FreePool(HiiCtx);
        MenuCtx->UserData = PreviousUserData;
        return Status;
    }
    
    // Create and show forms menu
    MENU_PAGE *FormsMenu = HiiBrowserCreateFormsMenu(HiiCtx);
    if (FormsMenu == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to create forms menu!");
        HiiBrowserCleanup(HiiCtx);
        FreePool(HiiCtx);
        MenuCtx->UserData = PreviousUserData;
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Set parent for navigation
    FormsMenu->Parent = MenuCtx->CurrentPage;
    
    // Navigate to forms menu (this will allow interactive browsing)
    Status = MenuNavigateTo(MenuCtx, FormsMenu);
    
    // Note: Don't cleanup here - the user will navigate back
    // Cleanup will happen when they exit to parent menu
    // For now, we keep HiiCtx alive
    
    return Status;
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
 * Helper: Wait for key press
 */
VOID WaitForKey(VOID)
{
    EFI_INPUT_KEY Key;
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
}

/**
 * Callback: About dialog
 */
EFI_STATUS MenuCallback_About(MENU_ITEM *Item, VOID *Context)
{
    SREP_CONTEXT *SrepCtx = (SREP_CONTEXT *)Context;
    MENU_CONTEXT *MenuCtx = SrepCtx->MenuContext;
    
    MenuShowMessage(MenuCtx, 
        L"About SREP",
        L"SmokelessRuntimeEFIPatcher " SREP_VERSION_STRING L"\n"
        L"Enhanced with:\n"
        L"- Auto-Detection & Intelligent Patching\n"
        L"- Interactive Menu Interface\n"
        L"- Direct BIOS NVRAM Configuration\n"
        L"- HP AMI BIOS Support\n"
        L"- Dynamic Form Extraction"
    );
    
    return EFI_SUCCESS;
}

/**
 * Callback: Exit application
 */
EFI_STATUS MenuCallback_Exit(MENU_ITEM *Item, VOID *Context)
{
    SREP_CONTEXT *SrepCtx = (SREP_CONTEXT *)Context;
    MENU_CONTEXT *MenuCtx = SrepCtx->MenuContext;
    
    MenuCtx->Running = FALSE;
    return EFI_SUCCESS;
}

/**
 * Initialize main menu
 */
EFI_STATUS CreateMainMenu(SREP_CONTEXT *SrepCtx, MENU_PAGE **OutMenu)
{
    MENU_PAGE *MainMenu;
    
    // Create main menu page with 11 items
    MainMenu = MenuCreatePage(L"SREP - SmokelessRuntimeEFIPatcher v0.3.1", 11);
    if (MainMenu == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Menu items
    MenuAddActionItem(
        MainMenu, 0,
        L"Auto-Detect and Patch BIOS",
        L"Automatically detect BIOS type and apply patches",
        MenuCallback_AutoPatch,
        SrepCtx
    );
    
    MenuAddActionItem(
        MainMenu, 1,
        L"Browse BIOS Settings (Read-Only)",
        L"View available BIOS forms and settings",
        MenuCallback_BrowseSettings,
        SrepCtx->MenuContext
    );
    
    MenuAddActionItem(
        MainMenu, 2,
        L"Load Modules and Edit Settings",
        L"Load BIOS modules, modify settings, save to NVRAM",
        MenuCallback_LoadAndEdit,
        SrepCtx
    );
    
    MenuAddSeparator(MainMenu, 3, NULL);
    
    MenuAddActionItem(
        MainMenu, 4,
        L"Launch BIOS Setup Browser",
        L"Launch the native BIOS Setup interface",
        MenuCallback_LaunchSetup,
        SrepCtx->MenuContext
    );
    
    MenuAddActionItem(
        MainMenu, 5,
        L"About",
        L"About SmokelessRuntimeEFIPatcher",
        MenuCallback_About,
        SrepCtx
    );
    
    MenuAddSeparator(MainMenu, 6, NULL);
    
    MenuAddInfoItem(MainMenu, 7, L"All changes are saved directly to BIOS NVRAM");
    MenuAddInfoItem(MainMenu, 8, L"Settings persist across reboots (stored in real BIOS NVRAM)");
    
    MenuAddSeparator(MainMenu, 9, NULL);
    
    MenuAddActionItem(
        MainMenu, 10,
        L"Exit",
        L"Exit to UEFI Shell or Boot Menu",
        MenuCallback_Exit,
        SrepCtx
    );
    
    *OutMenu = MainMenu;
    return EFI_SUCCESS;
}

/**
 * Create BIOS-style tabbed menu interface with dynamic form extraction
 */
EFI_STATUS CreateBiosStyleTabbedMenu(SREP_CONTEXT *SrepCtx)
{
    EFI_STATUS Status;
    MENU_CONTEXT *MenuCtx = SrepCtx->MenuContext;
    
    // Allocate HiiCtx dynamically so it persists
    HII_BROWSER_CONTEXT *HiiCtx = AllocateZeroPool(sizeof(HII_BROWSER_CONTEXT));
    if (HiiCtx == NULL)
    {
        Print(L"Failed to allocate HII browser context\n");
        return EFI_OUT_OF_RESOURCES;
    }
    
    Print(L"\n=== Extracting Real BIOS Forms ===\n");
    
    // Initialize HII browser to extract forms
    Status = HiiBrowserInitialize(HiiCtx);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to initialize HII browser: %r\n", Status);
        FreePool(HiiCtx);
        return Status;
    }
    
    HiiCtx->MenuContext = MenuCtx;
    
    // Store HiiCtx in MenuContext for callbacks to access
    MenuCtx->UserData = (VOID *)HiiCtx;
    
    // Enumerate and parse real BIOS forms
    Status = HiiBrowserEnumerateForms(HiiCtx);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to enumerate BIOS forms: %r\n", Status);
        HiiBrowserCleanup(HiiCtx);
        FreePool(HiiCtx);
        MenuCtx->UserData = NULL;
        return Status;
    }
    
    // Create dynamic tabs based on extracted forms
    Status = HiiBrowserCreateDynamicTabs(HiiCtx, MenuCtx);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to create dynamic tabs: %r\n", Status);
        HiiBrowserCleanup(HiiCtx);
        FreePool(HiiCtx);
        MenuCtx->UserData = NULL;
        return Status;
    }
    
    Print(L"\nPress any key to enter BIOS-style interface...\n");
    WaitForKey();
    
    // Note: HiiCtx is now stored in MenuCtx->UserData
    // It will be cleaned up when MenuCleanup is called
    
    return EFI_SUCCESS;
}

/**
 * Main entry point
 */
EFI_STATUS EFIAPI SREPEntry(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root;
    BOOLEAN UseInteractiveMode = TRUE;
    SREP_CONTEXT SrepCtx;
    MENU_CONTEXT MenuCtx;
    
    Print(L"Welcome to SREP (Smokeless Runtime EFI Patcher) %s\n\r", SREP_VERSION_STRING);
    Print(L"Enhanced with Auto-Detection and Intelligent Patching\n\r");
    
    gBS->SetWatchdogTimer(0, 0, 0, 0);
    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    Status = Root->Open(Root, &LogFile, LOG_FILE_NAME, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
    if (Status != EFI_SUCCESS)
    {
        Print(L"Failed on Opening Log File : %r\n\r", Status);
        Root->Close(Root);
        return Status;
    }
    AsciiSPrint(Log, LOG_BUFFER_SIZE, "Welcome to SREP (Smokeless Runtime EFI Patcher) %s\n\r", SREP_VERSION_STRING);
    LogToFile(LogFile,Log);
    AsciiSPrint(Log, LOG_BUFFER_SIZE, "Enhanced with Auto-Detection and Intelligent Patching\n\r");
    LogToFile(LogFile,Log);
    
    // Check for interactive mode flag file
    EFI_FILE *InteractiveFlag;
    Status = Root->Open(Root, &InteractiveFlag, INTERACTIVE_FLAG_FILE, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        InteractiveFlag->Close(InteractiveFlag);
        UseInteractiveMode = TRUE;
        AsciiSPrint(Log, LOG_BUFFER_SIZE, "Interactive mode flag found\n\r");
        LogToFile(LogFile,Log);
    }
    
    // Check for auto mode flag
    EFI_FILE *AutoFlag;
    Status = Root->Open(Root, &AutoFlag, AUTO_MODE_FLAG_FILE, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        AutoFlag->Close(AutoFlag);
        UseInteractiveMode = FALSE;
        AsciiSPrint(Log, LOG_BUFFER_SIZE, "Auto mode flag found, skipping interactive menu\n\r");
        LogToFile(LogFile,Log);
    }
    else
    {
        // Default to interactive mode
        UseInteractiveMode = TRUE;
    }
    
    // Check for BIOS-style tab interface flag
    BOOLEAN UseBiosTabInterface = FALSE;
    EFI_FILE *BiosTabFlag;
    Status = Root->Open(Root, &BiosTabFlag, BIOS_TAB_FLAG_FILE, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        BiosTabFlag->Close(BiosTabFlag);
        UseBiosTabInterface = TRUE;
        AsciiSPrint(Log, LOG_BUFFER_SIZE, "BIOS-style tab interface mode enabled\n\r");
        LogToFile(LogFile,Log);
    }
    
    // INTERACTIVE MODE: Show menu interface
    if (UseInteractiveMode)
    {
        AsciiSPrint(Log, LOG_BUFFER_SIZE, "\n=== INTERACTIVE MODE: Starting Menu ===\n\r");
        LogToFile(LogFile,Log);
        
        // Initialize menu system
        Status = MenuInitialize(&MenuCtx);
        if (EFI_ERROR(Status))
        {
            Print(L"Failed to initialize menu system: %r\n\r", Status);
            LogFile->Close(LogFile);
            Root->Close(Root);
            return Status;
        }
        
        // Initialize SREP context
        ZeroMem(&SrepCtx, sizeof(SREP_CONTEXT));
        SrepCtx.ImageHandle = ImageHandle;
        SrepCtx.MenuContext = &MenuCtx;
        SrepCtx.NvramManager = NULL;
        
        // Use BIOS-style tabbed interface if enabled
        if (UseBiosTabInterface)
        {
            Print(L"\nInitializing BIOS-style tabbed interface...\n\r");
            Status = CreateBiosStyleTabbedMenu(&SrepCtx);
            if (EFI_ERROR(Status))
            {
                Print(L"Failed to create tabbed menu: %r\n\r", Status);
                LogFile->Close(LogFile);
                Root->Close(Root);
                return Status;
            }
            
            // Run menu loop (no specific page needed with tabs)
            Status = MenuRun(&MenuCtx, NULL);
        }
        else
        {
            // Create traditional main menu
            MENU_PAGE *MainMenu;
            Status = CreateMainMenu(&SrepCtx, &MainMenu);
            if (EFI_ERROR(Status))
            {
                Print(L"Failed to create main menu: %r\n\r", Status);
                LogFile->Close(LogFile);
                Root->Close(Root);
                return Status;
            }
            
            // Run menu loop
            Status = MenuRun(&MenuCtx, MainMenu);
            
            // Cleanup
            MenuFreePage(MainMenu);
        }
        
        // Clean up HII browser context if it was allocated
        if (MenuCtx.UserData != NULL)
        {
            HII_BROWSER_CONTEXT *HiiCtx = (HII_BROWSER_CONTEXT *)MenuCtx.UserData;
            HiiBrowserCleanup(HiiCtx);
            FreePool(HiiCtx);
            MenuCtx.UserData = NULL;
        }
        
        MenuCleanup(&MenuCtx);
        
        LogFile->Close(LogFile);
        Root->Close(Root);
        return Status;
    }
    
    // AUTO MODE: Detect and patch automatically
    AsciiSPrint(Log, LOG_BUFFER_SIZE, "\n=== AUTO MODE: Detecting BIOS Type ===\n\r");
    LogToFile(LogFile,Log);
    
    // Initialize SREP context
    ZeroMem(&SrepCtx, sizeof(SREP_CONTEXT));
    SrepCtx.ImageHandle = ImageHandle;
    
    // Detect BIOS type
    Status = DetectBiosType(&SrepCtx.BiosInfo);
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, LOG_BUFFER_SIZE, "BIOS detection failed: %r\n\r", Status);
        LogToFile(LogFile,Log);
        Print(L"BIOS detection failed: %r\n\r", Status);
        Print(L"Press any key to exit...\n\r");
        WaitForKey();
        LogFile->Close(LogFile);
        Root->Close(Root);
        return Status;
    }
    
    Print(L"\nDetected BIOS: %s\n\r", GetBiosTypeString(SrepCtx.BiosInfo.Type));
    Print(L"Vendor: %s\n\r", SrepCtx.BiosInfo.VendorName);
    Print(L"Version: %s\n\r", SrepCtx.BiosInfo.Version);
    Print(L"\nStarting automatic patching...\n\r");
    
    // Auto-patch based on detected BIOS
    Status = AutoPatchBios(ImageHandle, &SrepCtx.BiosInfo);
    
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log, LOG_BUFFER_SIZE, "Auto-patching failed: %r\n\r", Status);
        LogToFile(LogFile,Log);
        Print(L"Auto-patching failed: %r\n\r", Status);
    }
    
    LogFile->Close(LogFile);
    Root->Close(Root);
    return Status;
}
