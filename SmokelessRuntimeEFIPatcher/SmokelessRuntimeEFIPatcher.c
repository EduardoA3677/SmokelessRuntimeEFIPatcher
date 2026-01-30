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
#include "ConfigManager.h"
#include "NvramManager.h"
#define SREP_VERSION L"0.3.1"

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
        L"SmokelessRuntimeEFIPatcher v0.3.1\n"
        L"Enhanced with:\n"
        L"- Auto-Detection & Intelligent Patching\n"
        L"- Interactive Menu Interface\n"
        L"- Direct BIOS NVRAM Configuration\n"
        L"- HP AMI BIOS Support"
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
 * Create BIOS-style tabbed menu interface
 */
EFI_STATUS CreateBiosStyleTabbedMenu(SREP_CONTEXT *SrepCtx)
{
    EFI_STATUS Status;
    MENU_CONTEXT *MenuCtx = SrepCtx->MenuContext;
    
    // Initialize tab mode with 6 tabs
    Status = MenuInitializeTabs(MenuCtx, 6);
    if (EFI_ERROR(Status))
    {
        return Status;
    }
    
    // Create Main tab page
    MENU_PAGE *MainPage = MenuCreatePage(L"Main Configuration", 8);
    if (MainPage == NULL) return EFI_OUT_OF_RESOURCES;
    
    MenuAddInfoItem(MainPage, 0, L"BIOS Information");
    MenuAddSeparator(MainPage, 1, NULL);
    MenuAddActionItem(MainPage, 2, L"Auto-Detect BIOS Type", 
                     L"Automatically detect and display BIOS vendor information", 
                     MenuCallback_AutoPatch, SrepCtx);
    MenuAddActionItem(MainPage, 3, L"Browse BIOS Settings", 
                     L"View all available BIOS configuration options", 
                     MenuCallback_BrowseSettings, MenuCtx);
    MenuAddSeparator(MainPage, 4, NULL);
    MenuAddInfoItem(MainPage, 5, L"System Date and Time");
    MenuAddInfoItem(MainPage, 6, L"System Memory: (Auto-detected)");
    MenuAddInfoItem(MainPage, 7, L"Processor: (Auto-detected)");
    
    // Create Advanced tab page
    MENU_PAGE *AdvancedPage = MenuCreatePage(L"Advanced Configuration", 7);
    if (AdvancedPage == NULL) return EFI_OUT_OF_RESOURCES;
    
    MenuAddInfoItem(AdvancedPage, 0, L"Advanced BIOS Features");
    MenuAddSeparator(AdvancedPage, 1, NULL);
    MenuAddActionItem(AdvancedPage, 2, L"Load and Edit BIOS Settings", 
                     L"Load BIOS modules and modify configuration values", 
                     MenuCallback_LoadAndEdit, SrepCtx);
    MenuAddActionItem(AdvancedPage, 3, L"Launch Native Setup Browser", 
                     L"Open the native BIOS configuration interface", 
                     MenuCallback_LaunchSetup, MenuCtx);
    MenuAddSeparator(AdvancedPage, 4, NULL);
    MenuAddInfoItem(AdvancedPage, 5, L"CPU Configuration");
    MenuAddInfoItem(AdvancedPage, 6, L"Chipset Configuration");
    
    // Create Power tab page
    MENU_PAGE *PowerPage = MenuCreatePage(L"Power Management", 5);
    if (PowerPage == NULL) return EFI_OUT_OF_RESOURCES;
    
    MenuAddInfoItem(PowerPage, 0, L"Power Management Options");
    MenuAddSeparator(PowerPage, 1, NULL);
    MenuAddInfoItem(PowerPage, 2, L"ACPI Settings");
    MenuAddInfoItem(PowerPage, 3, L"Wake Events");
    MenuAddInfoItem(PowerPage, 4, L"Power Button Configuration");
    
    // Create Boot tab page
    MENU_PAGE *BootPage = MenuCreatePage(L"Boot Configuration", 5);
    if (BootPage == NULL) return EFI_OUT_OF_RESOURCES;
    
    MenuAddInfoItem(BootPage, 0, L"Boot Options");
    MenuAddSeparator(BootPage, 1, NULL);
    MenuAddInfoItem(BootPage, 2, L"Boot Order");
    MenuAddInfoItem(BootPage, 3, L"Boot Mode: UEFI");
    MenuAddInfoItem(BootPage, 4, L"Fast Boot: Enabled");
    
    // Create Security tab page
    MENU_PAGE *SecurityPage = MenuCreatePage(L"Security", 6);
    if (SecurityPage == NULL) return EFI_OUT_OF_RESOURCES;
    
    MenuAddInfoItem(SecurityPage, 0, L"Security Settings");
    MenuAddSeparator(SecurityPage, 1, NULL);
    MenuAddInfoItem(SecurityPage, 2, L"Secure Boot Configuration");
    MenuAddInfoItem(SecurityPage, 3, L"Password Protection");
    MenuAddInfoItem(SecurityPage, 4, L"TPM Configuration");
    MenuAddInfoItem(SecurityPage, 5, L"Secure Boot Status: Enabled");
    
    // Create Save & Exit tab page
    MENU_PAGE *ExitPage = MenuCreatePage(L"Save & Exit", 7);
    if (ExitPage == NULL) return EFI_OUT_OF_RESOURCES;
    
    MenuAddInfoItem(ExitPage, 0, L"Exit Options");
    MenuAddSeparator(ExitPage, 1, NULL);
    MenuAddActionItem(ExitPage, 2, L"Save Changes and Exit", 
                     L"Save all NVRAM changes and exit to UEFI Shell", 
                     MenuCallback_Exit, SrepCtx);
    MenuAddActionItem(ExitPage, 3, L"Discard Changes and Exit", 
                     L"Exit without saving changes", 
                     MenuCallback_Exit, SrepCtx);
    MenuAddSeparator(ExitPage, 4, NULL);
    MenuAddActionItem(ExitPage, 5, L"About SREP", 
                     L"Version and copyright information", 
                     MenuCallback_About, SrepCtx);
    MenuAddInfoItem(ExitPage, 6, L"SREP v0.3.1 - Enhanced UEFI BIOS Patcher");
    
    // Add all tabs to the menu
    MenuAddTab(MenuCtx, 0, L"Main", MainPage);
    MenuAddTab(MenuCtx, 1, L"Advanced", AdvancedPage);
    MenuAddTab(MenuCtx, 2, L"Power", PowerPage);
    MenuAddTab(MenuCtx, 3, L"Boot", BootPage);
    MenuAddTab(MenuCtx, 4, L"Security", SecurityPage);
    MenuAddTab(MenuCtx, 5, L"Save & Exit", ExitPage);
    
    // Start with Main tab
    MenuSwitchTab(MenuCtx, 0);
    
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
        Root->Close(Root);
        return Status;
    }
    AsciiSPrint(Log,512,"Welcome to SREP (Smokeless Runtime EFI Patcher) %s\n\r", SREP_VERSION);
    LogToFile(LogFile,Log);
    AsciiSPrint(Log,512,"Enhanced with Auto-Detection and Intelligent Patching\n\r");
    LogToFile(LogFile,Log);
    
    // Check for interactive mode flag file
    EFI_FILE *InteractiveFlag;
    Status = Root->Open(Root, &InteractiveFlag, L"SREP_Interactive.flag", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        InteractiveFlag->Close(InteractiveFlag);
        UseInteractiveMode = TRUE;
        AsciiSPrint(Log,512,"Interactive mode flag found\n\r");
        LogToFile(LogFile,Log);
    }
    
    // Check for auto mode flag
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
    
    // Check for BIOS-style tab interface flag
    BOOLEAN UseBiosTabInterface = FALSE;
    EFI_FILE *BiosTabFlag;
    Status = Root->Open(Root, &BiosTabFlag, L"SREP_BiosTab.flag", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status))
    {
        BiosTabFlag->Close(BiosTabFlag);
        UseBiosTabInterface = TRUE;
        AsciiSPrint(Log,512,"BIOS-style tab interface mode enabled\n\r");
        LogToFile(LogFile,Log);
    }
    
    // INTERACTIVE MODE: Show menu interface
    if (UseInteractiveMode)
    {
        AsciiSPrint(Log,512,"\n=== INTERACTIVE MODE: Starting Menu ===\n\r");
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
        
        MenuCleanup(&MenuCtx);
        
        LogFile->Close(LogFile);
        Root->Close(Root);
        return Status;
    }
    
    // AUTO MODE: Detect and patch automatically
    AsciiSPrint(Log,512,"\n=== AUTO MODE: Detecting BIOS Type ===\n\r");
    LogToFile(LogFile,Log);
    
    // Initialize SREP context
    ZeroMem(&SrepCtx, sizeof(SREP_CONTEXT));
    SrepCtx.ImageHandle = ImageHandle;
    
    // Detect BIOS type
    Status = DetectBiosType(&SrepCtx.BiosInfo);
    if (EFI_ERROR(Status))
    {
        AsciiSPrint(Log,512,"BIOS detection failed: %r\n\r", Status);
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
        AsciiSPrint(Log,512,"Auto-patching failed: %r\n\r", Status);
        LogToFile(LogFile,Log);
        Print(L"Auto-patching failed: %r\n\r", Status);
    }
    
    LogFile->Close(LogFile);
    Root->Close(Root);
    return Status;
}
