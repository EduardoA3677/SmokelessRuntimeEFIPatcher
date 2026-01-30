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
