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

// Unused callback functions and CreateMainMenu removed for direct BIOS editor launch

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
    
    // Note: HiiCtx is now stored in MenuCtx->UserData
    // It will be cleaned up when MenuCleanup is called
    
    return EFI_SUCCESS;
}

/**
 * Main entry point - Direct BIOS Editor Launch
 */
EFI_STATUS EFIAPI SREPEntry(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root;
    SREP_CONTEXT SrepCtx;
    MENU_CONTEXT MenuCtx;
    
    Print(L"Welcome to SREP (Smokeless Runtime EFI Patcher) %s\n\r", SREP_VERSION_STRING);
    Print(L"AMI BIOS Configuration Editor\n\r");
    
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
    AsciiSPrint(Log, LOG_BUFFER_SIZE, "AMI BIOS Configuration Editor - Direct Launch Mode\n\r");
    LogToFile(LogFile,Log);
    
    // Always use BIOS-style interface (direct launch)
    AsciiSPrint(Log, LOG_BUFFER_SIZE, "\n=== BIOS EDITOR MODE: Launching directly ===\n\r");
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
    
    // Launch BIOS-style tabbed interface directly
    Print(L"\nInitializing BIOS configuration interface...\n\r");
    Status = CreateBiosStyleTabbedMenu(&SrepCtx);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to create BIOS interface: %r\n\r", Status);
        LogFile->Close(LogFile);
        Root->Close(Root);
        return Status;
    }
    
    // Run menu loop directly (no StartPage needed with tabs)
    Status = MenuRun(&MenuCtx, NULL);
    
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
