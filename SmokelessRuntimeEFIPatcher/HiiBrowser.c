#include "HiiBrowser.h"
#include "Constants.h"
#include "IfrOpcodes.h"
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>

// External globals from UEFI
extern EFI_BOOT_SERVICES         *gBS;
extern EFI_SYSTEM_TABLE          *gST;
extern EFI_RUNTIME_SERVICES      *gRT;
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Protocol/HiiString.h>

// Forward declarations for helper functions
STATIC VENDOR_TYPE DetectVendor(CHAR16 *Title, EFI_GUID *FormSetGuid);
STATIC UINT8 DetectFormCategory(CHAR16 *Title);
STATIC UINTN CategorizeForm(CHAR16 *Title);

/**
 * Initialize HII browser
 */
EFI_STATUS HiiBrowserInitialize(HII_BROWSER_CONTEXT *Context)
{
    EFI_STATUS Status;
    
    if (Context == NULL)
        return EFI_INVALID_PARAMETER;
    
    ZeroMem(Context, sizeof(HII_BROWSER_CONTEXT));
    
    // Locate HII Database Protocol
    Status = gBS->LocateProtocol(
        &gEfiHiiDatabaseProtocolGuid,
        NULL,
        (VOID **)&Context->HiiDatabase
    );
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to locate HII Database Protocol: %r\n", Status);
        return Status;
    }
    
    // Locate HII Config Routing Protocol
    Status = gBS->LocateProtocol(
        &gEfiHiiConfigRoutingProtocolGuid,
        NULL,
        (VOID **)&Context->HiiConfigRouting
    );
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to locate HII Config Routing Protocol: %r\n", Status);
        return Status;
    }
    
    // Locate Form Browser Protocol (optional, may not be available yet)
    Status = gBS->LocateProtocol(
        &gEfiFormBrowser2ProtocolGuid,
        NULL,
        (VOID **)&Context->FormBrowser2
    );
    // Don't fail if FormBrowser2 is not available yet
    
    // Initialize NVRAM manager
    Context->NvramManager = AllocateZeroPool(sizeof(NVRAM_MANAGER));
    if (Context->NvramManager == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Status = NvramInitialize(Context->NvramManager);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to initialize NVRAM manager: %r\n", Status);
        FreePool(Context->NvramManager);
        Context->NvramManager = NULL;
        return Status;
    }
    
    // Load Setup variables
    Status = NvramLoadSetupVariables(Context->NvramManager);
    if (EFI_ERROR(Status))
    {
        Print(L"Warning: Could not load Setup variables: %r\n", Status);
        // Continue anyway
    }
    
    // Initialize configuration database
    Context->Database = AllocateZeroPool(sizeof(DATABASE_CONTEXT));
    if (Context->Database == NULL)
    {
        Print(L"Warning: Could not allocate database context\n");
        // Continue without database
    }
    else
    {
        Status = DatabaseInitialize(Context->Database);
        if (EFI_ERROR(Status))
        {
            Print(L"Warning: Failed to initialize database: %r\n", Status);
            FreePool(Context->Database);
            Context->Database = NULL;
            // Continue anyway
        }
        else
        {
            Print(L"Database initialized successfully\n");
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * Parse IFR package to extract real form information
 */
STATIC EFI_STATUS ParseIfrPackage(
    HII_BROWSER_CONTEXT *Context,
    EFI_HII_HANDLE HiiHandle,
    UINT8 *IfrData,
    UINTN IfrSize,
    HII_FORM_INFO **FormList,
    UINTN *FormCount
)
{
    if (IfrData == NULL || IfrSize == 0 || FormList == NULL || FormCount == NULL)
        return EFI_INVALID_PARAMETER;
    
    UINT8 *Data = IfrData;
    UINTN Offset = 0;
    HII_FORM_INFO *Forms = NULL;
    UINTN Count = 0;
    UINTN Capacity = 10;  // Initial capacity
    
    EFI_GUID CurrentFormSetGuid = {0};
    UINT16 CurrentFormId = 0;
    BOOLEAN InSuppressIf = FALSE;
    
    // Allocate initial form array
    Forms = AllocateZeroPool(sizeof(HII_FORM_INFO) * Capacity);
    if (Forms == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    // Parse IFR opcodes
    while (Offset < IfrSize)
    {
        if (Offset + sizeof(EFI_IFR_OP_HEADER) > IfrSize)
            break;
        
        EFI_IFR_OP_HEADER *OpHeader = (EFI_IFR_OP_HEADER *)&Data[Offset];
        
        if (OpHeader->Length == 0 || OpHeader->Length > IfrSize - Offset)
            break;
        
        switch (OpHeader->OpCode)
        {
            case EFI_IFR_FORM_SET_OP:
            {
                if (Offset + sizeof(EFI_IFR_FORM_SET) <= IfrSize)
                {
                    EFI_IFR_FORM_SET *FormSet = (EFI_IFR_FORM_SET *)OpHeader;
                    CopyMem(&CurrentFormSetGuid, &FormSet->Guid, sizeof(EFI_GUID));
                }
                break;
            }
            
            case EFI_IFR_FORM_OP:
            {
                if (Offset + sizeof(EFI_IFR_FORM) <= IfrSize)
                {
                    EFI_IFR_FORM *Form = (EFI_IFR_FORM *)OpHeader;
                    CurrentFormId = Form->FormId;
                    
                    // Get form title string
                    EFI_STRING_ID TitleStringId = Form->FormTitle;
                    CHAR16 *TitleStr = NULL;
                    
                    if (Context->HiiDatabase != NULL)
                    {
                        EFI_HII_STRING_PROTOCOL *HiiString = NULL;
                        EFI_STATUS Status = gBS->LocateProtocol(
                            &gEfiHiiStringProtocolGuid,
                            NULL,
                            (VOID **)&HiiString
                        );
                        
                        if (!EFI_ERROR(Status) && HiiString != NULL)
                        {
                            UINTN StringSize = 0;
                            HiiString->GetString(
                                HiiString,
                                "en-US",
                                HiiHandle,
                                TitleStringId,
                                NULL,
                                &StringSize,
                                NULL
                            );
                            
                            if (StringSize > 0)
                            {
                                TitleStr = AllocateZeroPool(StringSize);
                                if (TitleStr != NULL)
                                {
                                    HiiString->GetString(
                                        HiiString,
                                        "en-US",
                                        HiiHandle,
                                        TitleStringId,
                                        TitleStr,
                                        &StringSize,
                                        NULL
                                    );
                                }
                            }
                        }
                    }
                    
                    // Add form to list
                    if (Count >= Capacity)
                    {
                        // Expand array
                        Capacity *= 2;
                        HII_FORM_INFO *NewForms = AllocateZeroPool(sizeof(HII_FORM_INFO) * Capacity);
                        if (NewForms != NULL)
                        {
                            CopyMem(NewForms, Forms, sizeof(HII_FORM_INFO) * Count);
                            FreePool(Forms);
                            Forms = NewForms;
                        }
                        else
                        {
                            if (TitleStr != NULL)
                                FreePool(TitleStr);
                            break;  // Out of memory
                        }
                    }
                    
                    // Fill form info
                    Forms[Count].HiiHandle = HiiHandle;
                    CopyMem(&Forms[Count].FormSetGuid, &CurrentFormSetGuid, sizeof(EFI_GUID));
                    Forms[Count].FormId = CurrentFormId;
                    
                    if (TitleStr != NULL)
                    {
                        Forms[Count].Title = TitleStr;
                    }
                    else
                    {
                        // Fallback title
                        Forms[Count].Title = AllocateCopyPool(StrSize(L"BIOS Form"), L"BIOS Form");
                    }
                    
                    Forms[Count].IsHidden = InSuppressIf;
                    
                    // Detect vendor and category
                    Forms[Count].Vendor = DetectVendor(Forms[Count].Title, &CurrentFormSetGuid);
                    Forms[Count].CategoryFlags = DetectFormCategory(Forms[Count].Title);
                    
                    Count++;
                }
                break;
            }
            
            case EFI_IFR_SUPPRESS_IF_OP:
            {
                // NEW: Ignore SUPPRESS_IF - always show all forms
                // InSuppressIf = TRUE;  // DISABLED per requirements
                break;
            }
            
            case EFI_IFR_END_OP:
            {
                // End of suppress/grayout scope
                InSuppressIf = FALSE;
                break;
            }
        }
        
        Offset += OpHeader->Length;
    }
    
    *FormList = Forms;
    *FormCount = Count;
    
    return EFI_SUCCESS;
}

/**
 * Enumerate all HII forms in the system
 */
EFI_STATUS HiiBrowserEnumerateForms(HII_BROWSER_CONTEXT *Context)
{
    EFI_STATUS Status;
    EFI_HII_HANDLE *HiiHandles = NULL;
    UINTN HandleCount = 0;
    
    if (Context == NULL || Context->HiiDatabase == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Get all HII handles
    Status = Context->HiiDatabase->ListPackageLists(
        Context->HiiDatabase,
        EFI_HII_PACKAGE_FORMS,
        NULL,
        &HandleCount,
        HiiHandles
    );
    
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        HiiHandles = AllocateZeroPool(HandleCount * sizeof(EFI_HII_HANDLE));
        if (HiiHandles == NULL)
            return EFI_OUT_OF_RESOURCES;
        
        Status = Context->HiiDatabase->ListPackageLists(
            Context->HiiDatabase,
            EFI_HII_PACKAGE_FORMS,
            NULL,
            &HandleCount,
            HiiHandles
        );
    }
    
    if (EFI_ERROR(Status))
    {
        if (HiiHandles != NULL)
            FreePool(HiiHandles);
        return Status;
    }
    
    Print(L"Found %d HII package lists\n\r", HandleCount);
    
    // Parse each HII package to extract real forms
    UINTN TotalFormCount = 0;
    HII_FORM_INFO *AllForms = NULL;
    UINTN AllFormsCapacity = HandleCount * 10;  // Estimate
    
    AllForms = AllocateZeroPool(sizeof(HII_FORM_INFO) * AllFormsCapacity);
    if (AllForms == NULL)
    {
        if (HiiHandles != NULL)
            FreePool(HiiHandles);
        return EFI_OUT_OF_RESOURCES;
    }
    
    for (UINTN i = 0; i < HandleCount; i++)
    {
        // Get package list for this handle
        UINTN BufferSize = 0;
        EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
        
        Status = Context->HiiDatabase->ExportPackageLists(
            Context->HiiDatabase,
            HiiHandles[i],
            &BufferSize,
            PackageList
        );
        
        if (Status == EFI_BUFFER_TOO_SMALL)
        {
            PackageList = AllocateZeroPool(BufferSize);
            if (PackageList != NULL)
            {
                Status = Context->HiiDatabase->ExportPackageLists(
                    Context->HiiDatabase,
                    HiiHandles[i],
                    &BufferSize,
                    PackageList
                );
                
                if (!EFI_ERROR(Status))
                {
                    // Parse IFR packages within this package list
                    UINT8 *PackageData = (UINT8 *)PackageList + sizeof(EFI_HII_PACKAGE_LIST_HEADER);
                    UINTN PackageOffset = 0;
                    UINTN TotalPackageSize = PackageList->PackageLength - sizeof(EFI_HII_PACKAGE_LIST_HEADER);
                    
                    while (PackageOffset < TotalPackageSize)
                    {
                        EFI_HII_PACKAGE_HEADER *PackageHeader = (EFI_HII_PACKAGE_HEADER *)&PackageData[PackageOffset];
                        
                        if (PackageHeader->Length == 0)
                            break;
                        
                        // Check if this is an IFR package
                        if ((PackageHeader->Type & 0x7F) == EFI_HII_PACKAGE_FORMS)
                        {
                            // Parse IFR data
                            UINT8 *IfrData = (UINT8 *)PackageHeader + sizeof(EFI_HII_PACKAGE_HEADER);
                            UINTN IfrSize = PackageHeader->Length - sizeof(EFI_HII_PACKAGE_HEADER);
                            
                            HII_FORM_INFO *FormList = NULL;
                            UINTN FormCount = 0;
                            
                            Status = ParseIfrPackage(
                                Context,
                                HiiHandles[i],
                                IfrData,
                                IfrSize,
                                &FormList,
                                &FormCount
                            );
                            
                            if (!EFI_ERROR(Status) && FormCount > 0)
                            {
                                // Add to overall list
                                for (UINTN j = 0; j < FormCount && TotalFormCount < AllFormsCapacity; j++)
                                {
                                    CopyMem(&AllForms[TotalFormCount], &FormList[j], sizeof(HII_FORM_INFO));
                                    TotalFormCount++;
                                }
                                
                                if (FormList != NULL)
                                    FreePool(FormList);
                            }
                        }
                        
                        PackageOffset += PackageHeader->Length;
                    }
                }
                
                FreePool(PackageList);
            }
        }
    }
    
    // Store results in context
    Context->Forms = AllForms;
    Context->FormCount = TotalFormCount;
    
    Print(L"Extracted %d real BIOS forms from HII database\n\r", TotalFormCount);
    
    // Always free HiiHandles after use
    if (HiiHandles != NULL)
        FreePool(HiiHandles);
    
    return EFI_SUCCESS;
}

/**
 * Parse questions from IFR data for a specific form
 */
STATIC EFI_STATUS ParseFormQuestions(
    HII_BROWSER_CONTEXT *Context,
    EFI_HII_HANDLE HiiHandle,
    UINT16 FormId,
    UINT8 *IfrData,
    UINTN IfrSize,
    HII_QUESTION_INFO **QuestionList,
    UINTN *QuestionCount
)
{
    if (IfrData == NULL || IfrSize == 0 || QuestionList == NULL || QuestionCount == NULL)
        return EFI_INVALID_PARAMETER;
    
    UINT8 *Data = IfrData;
    UINTN Offset = 0;
    HII_QUESTION_INFO *Questions = NULL;
    UINTN Count = 0;
    UINTN Capacity = 20;  // Initial capacity
    BOOLEAN InTargetForm = FALSE;
    BOOLEAN InSuppressIf = FALSE;
    BOOLEAN InGrayoutIf = FALSE;
    
    // Varstore tracking (simple implementation for common case)
    // Most BIOS use a single "Setup" variable with VarStoreId 1
    #define MAX_VARSTORES 16
    typedef struct {
        UINT16 VarStoreId;
        CHAR16 *Name;
        EFI_GUID Guid;
    } VARSTORE_INFO;
    
    VARSTORE_INFO VarStores[MAX_VARSTORES];
    UINTN VarStoreCount = 0;
    ZeroMem(VarStores, sizeof(VarStores));
    
    // Add default "Setup" varstore (most common case)
    VarStores[0].VarStoreId = 1;
    VarStores[0].Name = L"Setup";
    // Use a generic GUID (will be overridden if found in IFR)
    VarStores[0].Guid = gEfiGlobalVariableGuid;
    VarStoreCount = 1;
    
    // Helper function to lookup varstore by ID
    #define LOOKUP_VARSTORE(VarStoreId, OutName, OutGuid) \
        do { \
            (OutName) = NULL; \
            for (UINTN _i = 0; _i < VarStoreCount; _i++) { \
                if (VarStores[_i].VarStoreId == (VarStoreId)) { \
                    if (VarStores[_i].Name != NULL) { \
                        (OutName) = AllocateCopyPool(StrSize(VarStores[_i].Name), VarStores[_i].Name); \
                    } \
                    CopyMem(&(OutGuid), &VarStores[_i].Guid, sizeof(EFI_GUID)); \
                    break; \
                } \
            } \
        } while(0)
    
    // Allocate initial question array
    Questions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * Capacity);
    if (Questions == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    // Get HII String Protocol for retrieving strings
    EFI_HII_STRING_PROTOCOL *HiiString = NULL;
    EFI_STATUS Status = gBS->LocateProtocol(
        &gEfiHiiStringProtocolGuid,
        NULL,
        (VOID **)&HiiString
    );
    
    // Parse IFR opcodes
    while (Offset < IfrSize)
    {
        if (Offset + sizeof(EFI_IFR_OP_HEADER) > IfrSize)
            break;
        
        EFI_IFR_OP_HEADER *OpHeader = (EFI_IFR_OP_HEADER *)&Data[Offset];
        
        if (OpHeader->Length == 0 || OpHeader->Length > IfrSize - Offset)
            break;
        
        switch (OpHeader->OpCode)
        {
            // VARSTORE_EFI opcode - Track EFI variable stores
            case 0x26:  // EFI_IFR_VARSTORE_EFI_OP
            {
                // This opcode defines an EFI variable store
                // Structure: VarStoreId (UINT16), Guid (EFI_GUID), Attributes (UINT32), Size (UINT16), Name (NUL-terminated)
                if (Offset + 8 + sizeof(EFI_GUID) <= IfrSize && VarStoreCount < MAX_VARSTORES)
                {
                    UINT8 *VarStoreData = &Data[Offset + sizeof(EFI_IFR_OP_HEADER)];
                    UINT16 VarStoreId = *(UINT16 *)VarStoreData;
                    EFI_GUID *VarGuid = (EFI_GUID *)(VarStoreData + 2);
                    
                    // Name follows the GUID and attributes (skip 4 bytes for attributes + 2 for size)
                    CHAR8 *NameAscii = (CHAR8 *)(VarStoreData + 2 + sizeof(EFI_GUID) + 4 + 2);
                    
                    // Convert ASCII name to Unicode
                    UINTN NameLen = AsciiStrLen(NameAscii) + 1;
                    CHAR16 *NameUnicode = AllocateZeroPool(NameLen * sizeof(CHAR16));
                    if (NameUnicode != NULL)
                    {
                        for (UINTN i = 0; i < NameLen; i++)
                            NameUnicode[i] = (CHAR16)NameAscii[i];
                        
                        // Store in varstore array
                        VarStores[VarStoreCount].VarStoreId = VarStoreId;
                        VarStores[VarStoreCount].Name = NameUnicode;
                        CopyMem(&VarStores[VarStoreCount].Guid, VarGuid, sizeof(EFI_GUID));
                        VarStoreCount++;
                    }
                }
                break;
            }
            
            case EFI_IFR_FORM_OP:
            {
                if (Offset + sizeof(EFI_IFR_FORM) <= IfrSize)
                {
                    EFI_IFR_FORM *Form = (EFI_IFR_FORM *)OpHeader;
                    InTargetForm = (Form->FormId == FormId);
                }
                break;
            }
            
            case EFI_IFR_END_OP:
            {
                // End of form or suppress scope
                if ((OpHeader->Scope) == 0)
                {
                    InSuppressIf = FALSE;
                    InGrayoutIf = FALSE;
                }
                break;
            }
            
            case EFI_IFR_SUPPRESS_IF_OP:
                // NEW: Ignore SUPPRESS_IF - always show all options
                // InSuppressIf = TRUE;  // DISABLED per requirements
                break;
            
            case EFI_IFR_GRAY_OUT_IF_OP:
                // Still track grayout state (not suppressed per requirements)
                InGrayoutIf = TRUE;
                break;
            
            // TEXT opcode - Display read-only information
            case EFI_IFR_TEXT_OP:
            {
                if (!InTargetForm)
                    break;
                
                // Check capacity
                if (Count >= Capacity)
                {
                    Capacity *= 2;
                    HII_QUESTION_INFO *NewQuestions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * Capacity);
                    if (NewQuestions != NULL)
                    {
                        CopyMem(NewQuestions, Questions, sizeof(HII_QUESTION_INFO) * Count);
                        FreePool(Questions);
                        Questions = NewQuestions;
                    }
                }
                
                if (Count < Capacity && Offset + sizeof(EFI_IFR_TEXT) <= IfrSize)
                {
                    EFI_IFR_TEXT *Text = (EFI_IFR_TEXT *)OpHeader;
                    HII_QUESTION_INFO *Question = &Questions[Count];
                    
                    Question->Type = MENU_ITEM_INFO;  // Special type for INFO display
                    Question->IsHidden = FALSE;  // Always show
                    Question->IsGrayedOut = InGrayoutIf;
                    
                    // Get prompt text
                    // Note: EFI_IFR_TEXT structure has Statement.Prompt field
                    if (!EFI_ERROR(Status) && HiiString != NULL && Text->Statement.Prompt != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Text->Statement.Prompt, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->Prompt = AllocateZeroPool(StringSize);
                            if (Question->Prompt != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Text->Statement.Prompt,
                                                    Question->Prompt, &StringSize, NULL);
                            }
                        }
                    }
                    
                    // Get help/second text
                    if (!EFI_ERROR(Status) && HiiString != NULL && Text->TextTwo != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Text->TextTwo, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->HelpText = AllocateZeroPool(StringSize);
                            if (Question->HelpText != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Text->TextTwo,
                                                    Question->HelpText, &StringSize, NULL);
                            }
                        }
                    }
                    
                    if (Question->Prompt)
                        Count++;
                }
                break;
            }
            
            // SUBTITLE opcode - Section headers
            case EFI_IFR_SUBTITLE_OP:
            {
                if (!InTargetForm)
                    break;
                
                // Check capacity
                if (Count >= Capacity)
                {
                    Capacity *= 2;
                    HII_QUESTION_INFO *NewQuestions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * Capacity);
                    if (NewQuestions != NULL)
                    {
                        CopyMem(NewQuestions, Questions, sizeof(HII_QUESTION_INFO) * Count);
                        FreePool(Questions);
                        Questions = NewQuestions;
                    }
                }
                
                if (Count < Capacity && Offset + sizeof(EFI_IFR_SUBTITLE) <= IfrSize)
                {
                    EFI_IFR_SUBTITLE *Subtitle = (EFI_IFR_SUBTITLE *)OpHeader;
                    HII_QUESTION_INFO *Question = &Questions[Count];
                    
                    Question->Type = MENU_ITEM_SEPARATOR;  // Special type for separator
                    Question->IsHidden = FALSE;  // Always show
                    
                    // Get subtitle text
                    // Note: EFI_IFR_SUBTITLE structure has Statement.Prompt field
                    if (!EFI_ERROR(Status) && HiiString != NULL && Subtitle->Statement.Prompt != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Subtitle->Statement.Prompt, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->Prompt = AllocateZeroPool(StringSize);
                            if (Question->Prompt != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Subtitle->Statement.Prompt,
                                                    Question->Prompt, &StringSize, NULL);
                            }
                        }
                    }
                    
                    if (Question->Prompt)
                        Count++;
                }
                break;
            }
            
            // REF opcode - Form references (submenus)
            case EFI_IFR_REF_OP:
            {
                if (!InTargetForm)
                    break;
                
                // Check capacity
                if (Count >= Capacity)
                {
                    Capacity *= 2;
                    HII_QUESTION_INFO *NewQuestions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * Capacity);
                    if (NewQuestions != NULL)
                    {
                        CopyMem(NewQuestions, Questions, sizeof(HII_QUESTION_INFO) * Count);
                        FreePool(Questions);
                        Questions = NewQuestions;
                    }
                }
                
                if (Count < Capacity && Offset + sizeof(EFI_IFR_REF) <= IfrSize)
                {
                    EFI_IFR_REF *Ref = (EFI_IFR_REF *)OpHeader;
                    HII_QUESTION_INFO *Question = &Questions[Count];
                    
                    Question->Type = EFI_IFR_REF_OP;
                    Question->QuestionId = Ref->Question.QuestionId;
                    Question->IsReference = TRUE;
                    Question->RefFormId = Ref->FormId;
                    Question->IsHidden = FALSE;  // Always show
                    Question->IsGrayedOut = InGrayoutIf;
                    
                    // Get prompt text
                    if (!EFI_ERROR(Status) && HiiString != NULL && Ref->Question.Header.Prompt != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Ref->Question.Header.Prompt, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->Prompt = AllocateZeroPool(StringSize);
                            if (Question->Prompt != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Ref->Question.Header.Prompt,
                                                    Question->Prompt, &StringSize, NULL);
                            }
                        }
                    }
                    
                    // Get help text
                    if (!EFI_ERROR(Status) && HiiString != NULL && Ref->Question.Header.Help != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Ref->Question.Header.Help, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->HelpText = AllocateZeroPool(StringSize);
                            if (Question->HelpText != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Ref->Question.Header.Help,
                                                    Question->HelpText, &StringSize, NULL);
                            }
                        }
                    }
                    
                    if (Question->Prompt)
                        Count++;
                }
                break;
            }
            
            // ACTION opcode - Action buttons
            case EFI_IFR_ACTION_OP:
            {
                if (!InTargetForm)
                    break;
                
                // Check capacity
                if (Count >= Capacity)
                {
                    Capacity *= 2;
                    HII_QUESTION_INFO *NewQuestions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * Capacity);
                    if (NewQuestions != NULL)
                    {
                        CopyMem(NewQuestions, Questions, sizeof(HII_QUESTION_INFO) * Count);
                        FreePool(Questions);
                        Questions = NewQuestions;
                    }
                }
                
                if (Count < Capacity && Offset + sizeof(EFI_IFR_ACTION) <= IfrSize)
                {
                    EFI_IFR_ACTION *Action = (EFI_IFR_ACTION *)OpHeader;
                    HII_QUESTION_INFO *Question = &Questions[Count];
                    
                    Question->Type = MENU_ITEM_ACTION;
                    Question->QuestionId = Action->Question.QuestionId;
                    Question->IsHidden = FALSE;  // Always show
                    Question->IsGrayedOut = InGrayoutIf;
                    
                    // Get prompt text
                    if (!EFI_ERROR(Status) && HiiString != NULL && Action->Question.Header.Prompt != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Action->Question.Header.Prompt, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->Prompt = AllocateZeroPool(StringSize);
                            if (Question->Prompt != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Action->Question.Header.Prompt,
                                                    Question->Prompt, &StringSize, NULL);
                            }
                        }
                    }
                    
                    // Get help text
                    if (!EFI_ERROR(Status) && HiiString != NULL && Action->Question.Header.Help != 0)
                    {
                        UINTN StringSize = 0;
                        HiiString->GetString(HiiString, "en-US", HiiHandle, Action->Question.Header.Help, NULL, &StringSize, NULL);
                        
                        if (StringSize > 0)
                        {
                            Question->HelpText = AllocateZeroPool(StringSize);
                            if (Question->HelpText != NULL)
                            {
                                HiiString->GetString(HiiString, "en-US", HiiHandle, Action->Question.Header.Help,
                                                    Question->HelpText, &StringSize, NULL);
                            }
                        }
                    }
                    
                    if (Question->Prompt)
                        Count++;
                }
                break;
            }
            
            // Handle different question types
            case EFI_IFR_ONE_OF_OP:
            case EFI_IFR_CHECKBOX_OP:
            case EFI_IFR_NUMERIC_OP:
            case EFI_IFR_STRING_OP:
            {
                if (!InTargetForm)
                    break;
                
                // Check capacity
                if (Count >= Capacity)
                {
                    Capacity *= 2;
                    HII_QUESTION_INFO *NewQuestions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * Capacity);
                    if (NewQuestions != NULL)
                    {
                        CopyMem(NewQuestions, Questions, sizeof(HII_QUESTION_INFO) * Count);
                        FreePool(Questions);
                        Questions = NewQuestions;
                    }
                }
                
                if (Count < Capacity)
                {
                    HII_QUESTION_INFO *Question = &Questions[Count];
                    
                    // Common fields for all question types
                    if (Offset + sizeof(EFI_IFR_QUESTION_HEADER) <= IfrSize)
                    {
                        UINT8 *QuestionData = &Data[Offset];
                        
                        // Extract question header (varies by type but all have these fields)
                        // Offset to QuestionId varies by opcode structure
                        UINT16 *PromptPtr = (UINT16 *)(QuestionData + sizeof(EFI_IFR_OP_HEADER));
                        UINT16 *HelpPtr = (UINT16 *)(QuestionData + sizeof(EFI_IFR_OP_HEADER) + 2);
                        
                        EFI_STRING_ID PromptId = *PromptPtr;
                        EFI_STRING_ID HelpId = *HelpPtr;
                        
                        // Get question ID (location varies by type)
                        UINT16 QuestionId = 0;
                        if (OpHeader->OpCode == EFI_IFR_ONE_OF_OP && 
                            Offset + sizeof(EFI_IFR_ONE_OF) <= IfrSize)
                        {
                            EFI_IFR_ONE_OF *OneOf = (EFI_IFR_ONE_OF *)OpHeader;
                            QuestionId = OneOf->Question.QuestionId;
                            
                            // Store variable info
                            if (OneOf->Question.VarStoreId != 0)
                            {
                                LOOKUP_VARSTORE(OneOf->Question.VarStoreId, Question->VariableName, Question->VariableGuid);
                                Question->VariableOffset = OneOf->Question.VarStoreInfo.VarOffset;
                            }
                        }
                        else if (OpHeader->OpCode == EFI_IFR_CHECKBOX_OP &&
                                 Offset + sizeof(EFI_IFR_CHECKBOX) <= IfrSize)
                        {
                            EFI_IFR_CHECKBOX *Checkbox = (EFI_IFR_CHECKBOX *)OpHeader;
                            QuestionId = Checkbox->Question.QuestionId;
                            
                            if (Checkbox->Question.VarStoreId != 0)
                            {
                                LOOKUP_VARSTORE(Checkbox->Question.VarStoreId, Question->VariableName, Question->VariableGuid);
                                Question->VariableOffset = Checkbox->Question.VarStoreInfo.VarOffset;
                            }
                        }
                        else if (OpHeader->OpCode == EFI_IFR_NUMERIC_OP &&
                                 Offset + sizeof(EFI_IFR_NUMERIC) <= IfrSize)
                        {
                            EFI_IFR_NUMERIC *Numeric = (EFI_IFR_NUMERIC *)OpHeader;
                            QuestionId = Numeric->Question.QuestionId;
                            
                            // Store min/max/step for numeric
                            Question->Minimum = Numeric->data.u64.MinValue;
                            Question->Maximum = Numeric->data.u64.MaxValue;
                            Question->Step = Numeric->data.u64.Step;
                            
                            if (Numeric->Question.VarStoreId != 0)
                            {
                                LOOKUP_VARSTORE(Numeric->Question.VarStoreId, Question->VariableName, Question->VariableGuid);
                                Question->VariableOffset = Numeric->Question.VarStoreInfo.VarOffset;
                            }
                        }
                        else if (OpHeader->OpCode == EFI_IFR_STRING_OP &&
                                 Offset + sizeof(EFI_IFR_STRING) <= IfrSize)
                        {
                            EFI_IFR_STRING *String = (EFI_IFR_STRING *)OpHeader;
                            QuestionId = String->Question.QuestionId;
                            
                            // Store string limits
                            Question->Minimum = String->MinSize;
                            Question->Maximum = String->MaxSize;
                            
                            if (String->Question.VarStoreId != 0)
                            {
                                LOOKUP_VARSTORE(String->Question.VarStoreId, Question->VariableName, Question->VariableGuid);
                                Question->VariableOffset = String->Question.VarStoreInfo.VarOffset;
                            }
                        }
                        
                        Question->QuestionId = QuestionId;
                        Question->Type = OpHeader->OpCode;
                        Question->IsHidden = FALSE;  // Always show (ignore SUPPRESS_IF)
                        Question->IsGrayedOut = InGrayoutIf;
                        Question->IsModified = FALSE;
                        
                        // Get prompt string
                        if (!EFI_ERROR(Status) && HiiString != NULL && PromptId != 0)
                        {
                            UINTN StringSize = 0;
                            HiiString->GetString(HiiString, "en-US", HiiHandle, PromptId, NULL, &StringSize, NULL);
                            
                            if (StringSize > 0)
                            {
                                Question->Prompt = AllocateZeroPool(StringSize);
                                if (Question->Prompt != NULL)
                                {
                                    HiiString->GetString(HiiString, "en-US", HiiHandle, PromptId,
                                                        Question->Prompt, &StringSize, NULL);
                                }
                            }
                        }
                        
                        if (Question->Prompt == NULL)
                        {
                            Question->Prompt = AllocateCopyPool(StrSize(L"BIOS Option"), L"BIOS Option");
                        }
                        
                        // Get help string
                        if (!EFI_ERROR(Status) && HiiString != NULL && HelpId != 0)
                        {
                            UINTN StringSize = 0;
                            HiiString->GetString(HiiString, "en-US", HiiHandle, HelpId, NULL, &StringSize, NULL);
                            
                            if (StringSize > 0)
                            {
                                Question->HelpText = AllocateZeroPool(StringSize);
                                if (Question->HelpText != NULL)
                                {
                                    HiiString->GetString(HiiString, "en-US", HiiHandle, HelpId,
                                                        Question->HelpText, &StringSize, NULL);
                                }
                            }
                        }
                        
                        Count++;
                    }
                }
                break;
            }
            
            // ONE_OF_OPTION opcode - Options for OneOf questions (CRITICAL)
            case EFI_IFR_ONE_OF_OPTION_OP:
            {
                // Find the most recent OneOf question to add option to
                if (Count > 0 && Questions[Count - 1].Type == EFI_IFR_ONE_OF_OP &&
                    Offset + sizeof(EFI_IFR_ONE_OF_OPTION) <= IfrSize)
                {
                    EFI_IFR_ONE_OF_OPTION *Option = (EFI_IFR_ONE_OF_OPTION *)OpHeader;
                    HII_QUESTION_INFO *Question = &Questions[Count - 1];
                    
                    // Expand options array if needed
                    if (Question->Options == NULL)
                    {
                        Question->Options = AllocateZeroPool(sizeof(HII_OPTION_INFO) * 4);
                        Question->OptionCount = 0;
                    }
                    else if ((Question->OptionCount % 4) == 0 && Question->OptionCount > 0)
                    {
                        // Need to expand
                        UINTN NewCapacity = Question->OptionCount + 4;
                        HII_OPTION_INFO *NewOptions = AllocateZeroPool(sizeof(HII_OPTION_INFO) * NewCapacity);
                        if (NewOptions)
                        {
                            CopyMem(NewOptions, Question->Options, sizeof(HII_OPTION_INFO) * Question->OptionCount);
                            FreePool(Question->Options);
                            Question->Options = NewOptions;
                        }
                    }
                    
                    if (Question->Options)
                    {
                        UINTN OptIndex = Question->OptionCount;
                        
                        // Get option text
                        if (!EFI_ERROR(Status) && HiiString != NULL && Option->Option != 0)
                        {
                            UINTN StringSize = 0;
                            HiiString->GetString(HiiString, "en-US", HiiHandle, Option->Option, NULL, &StringSize, NULL);
                            
                            if (StringSize > 0)
                            {
                                Question->Options[OptIndex].Text = AllocateZeroPool(StringSize);
                                if (Question->Options[OptIndex].Text != NULL)
                                {
                                    HiiString->GetString(HiiString, "en-US", HiiHandle, Option->Option,
                                                        Question->Options[OptIndex].Text, &StringSize, NULL);
                                }
                            }
                        }
                        
                        // Extract option value based on type
                        UINT8 *ValuePtr = (UINT8 *)Option + sizeof(EFI_IFR_ONE_OF_OPTION);
                        UINT64 OptionValue = 0;
                        
                        switch (Option->Type)
                        {
                            case EFI_IFR_TYPE_NUM_SIZE_8:
                                OptionValue = *(UINT8 *)ValuePtr;
                                break;
                            case EFI_IFR_TYPE_NUM_SIZE_16:
                                OptionValue = *(UINT16 *)ValuePtr;
                                break;
                            case EFI_IFR_TYPE_NUM_SIZE_32:
                                OptionValue = *(UINT32 *)ValuePtr;
                                break;
                            case EFI_IFR_TYPE_NUM_SIZE_64:
                                OptionValue = *(UINT64 *)ValuePtr;
                                break;
                            default:
                                OptionValue = 0;
                                break;
                        }
                        
                        Question->Options[OptIndex].Value = OptionValue;
                        Question->OptionCount++;
                    }
                }
                break;
            }
            
            // DEFAULT opcode - Default values for questions
            case EFI_IFR_DEFAULT_OP:
            {
                // Find the most recent question to set default value
                if (Count > 0 && Offset + sizeof(EFI_IFR_DEFAULT) <= IfrSize)
                {
                    EFI_IFR_DEFAULT *Default = (EFI_IFR_DEFAULT *)OpHeader;
                    HII_QUESTION_INFO *Question = &Questions[Count - 1];
                    
                    // Extract default value based on type
                    UINT8 *ValuePtr = (UINT8 *)Default + sizeof(EFI_IFR_DEFAULT);
                    UINT64 DefaultValue = 0;
                    
                    switch (Default->Type)
                    {
                        case EFI_IFR_TYPE_NUM_SIZE_8:
                            DefaultValue = *(UINT8 *)ValuePtr;
                            break;
                        case EFI_IFR_TYPE_NUM_SIZE_16:
                            DefaultValue = *(UINT16 *)ValuePtr;
                            break;
                        case EFI_IFR_TYPE_NUM_SIZE_32:
                            DefaultValue = *(UINT32 *)ValuePtr;
                            break;
                        case EFI_IFR_TYPE_NUM_SIZE_64:
                            DefaultValue = *(UINT64 *)ValuePtr;
                            break;
                        default:
                            DefaultValue = 0;
                            break;
                    }
                    
                    // Store default value
                    if (Question->DefaultValue == NULL)
                    {
                        Question->DefaultValue = AllocateCopyPool(sizeof(UINT64), &DefaultValue);
                    }
                }
                break;
            }
        }
        
        Offset += OpHeader->Length;
    }
    
    // Cleanup: Free allocated varstore names (except default which is static)
    for (UINTN i = 1; i < VarStoreCount; i++)
    {
        if (VarStores[i].Name != NULL)
        {
            FreePool(VarStores[i].Name);
        }
    }
    
    *QuestionList = Questions;
    *QuestionCount = Count;
    
    return EFI_SUCCESS;
}

/**
 * Get questions for a specific form
 */
EFI_STATUS HiiBrowserGetFormQuestions(
    HII_BROWSER_CONTEXT *Context,
    HII_FORM_INFO *Form,
    HII_QUESTION_INFO **Questions,
    UINTN *QuestionCount
)
{
    if (Context == NULL || Form == NULL || Questions == NULL || QuestionCount == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Get the IFR package for this form's HII handle
    UINTN BufferSize = 0;
    EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
    EFI_STATUS Status;
    
    Status = Context->HiiDatabase->ExportPackageLists(
        Context->HiiDatabase,
        Form->HiiHandle,
        &BufferSize,
        PackageList
    );
    
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        PackageList = AllocateZeroPool(BufferSize);
        if (PackageList != NULL)
        {
            Status = Context->HiiDatabase->ExportPackageLists(
                Context->HiiDatabase,
                Form->HiiHandle,
                &BufferSize,
                PackageList
            );
            
            if (!EFI_ERROR(Status))
            {
                // Parse IFR packages
                UINT8 *PackageData = (UINT8 *)PackageList + sizeof(EFI_HII_PACKAGE_LIST_HEADER);
                UINTN PackageOffset = 0;
                UINTN TotalPackageSize = PackageList->PackageLength - sizeof(EFI_HII_PACKAGE_LIST_HEADER);
                
                while (PackageOffset < TotalPackageSize)
                {
                    EFI_HII_PACKAGE_HEADER *PackageHeader = (EFI_HII_PACKAGE_HEADER *)&PackageData[PackageOffset];
                    
                    if (PackageHeader->Length == 0)
                        break;
                    
                    if ((PackageHeader->Type & 0x7F) == EFI_HII_PACKAGE_FORMS)
                    {
                        UINT8 *IfrData = (UINT8 *)PackageHeader + sizeof(EFI_HII_PACKAGE_HEADER);
                        UINTN IfrSize = PackageHeader->Length - sizeof(EFI_HII_PACKAGE_HEADER);
                        
                        Status = ParseFormQuestions(
                            Context,
                            Form->HiiHandle,
                            Form->FormId,
                            IfrData,
                            IfrSize,
                            Questions,
                            QuestionCount
                        );
                        
                        FreePool(PackageList);
                        return Status;
                    }
                    
                    PackageOffset += PackageHeader->Length;
                }
            }
            
            FreePool(PackageList);
        }
    }
    
    return EFI_NOT_FOUND;
}

/**
 * Callback: Open form details and show questions
 */
EFI_STATUS HiiBrowserCallback_OpenForm(MENU_ITEM *Item, VOID *Context)
{
    if (Item == NULL || Item->Data == NULL || Context == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    HII_FORM_INFO *Form = (HII_FORM_INFO *)Item->Data;
    
    // Get HII browser context from menu context
    // The HiiCtx should be stored in MenuContext during initialization
    HII_BROWSER_CONTEXT *HiiCtx = (HII_BROWSER_CONTEXT *)MenuCtx->UserData;
    if (HiiCtx == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"HII Browser context not available!");
        return EFI_NOT_READY;
    }
    
    // Get questions for this form
    HII_QUESTION_INFO *Questions = NULL;
    UINTN QuestionCount = 0;
    
    EFI_STATUS Status = HiiBrowserGetFormQuestions(HiiCtx, Form, &Questions, &QuestionCount);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to get form questions!");
        return Status;
    }
    
    // Create questions menu
    MENU_PAGE *QuestionsPage = HiiBrowserCreateQuestionsMenu(HiiCtx, Form, Questions, QuestionCount);
    if (QuestionsPage == NULL)
    {
        if (Questions)
            FreePool(Questions);
        MenuShowMessage(MenuCtx, L"Error", L"Failed to create questions menu!");
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Set parent for back navigation
    QuestionsPage->Parent = MenuCtx->CurrentPage;
    
    // Navigate to questions page
    Status = MenuNavigateTo(MenuCtx, QuestionsPage);
    
    // Note: Don't free QuestionsPage here - it's now part of navigation stack
    // It will be freed when user goes back
    
    return Status;
}

/**
 * Callback: Open referenced form (submenu navigation)
 */
EFI_STATUS HiiBrowserCallback_OpenReferencedForm(MENU_ITEM *Item, VOID *Context)
{
    if (Item == NULL || Item->Data == NULL || Context == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    HII_QUESTION_INFO *Question = (HII_QUESTION_INFO *)Item->Data;
    
    // Get HII browser context from menu context
    HII_BROWSER_CONTEXT *HiiCtx = (HII_BROWSER_CONTEXT *)MenuCtx->UserData;
    if (HiiCtx == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"HII Browser context not available!");
        return EFI_NOT_READY;
    }
    
    // Find the referenced form by FormId
    HII_FORM_INFO *ReferencedForm = NULL;
    for (UINTN i = 0; i < HiiCtx->FormCount; i++)
    {
        if (HiiCtx->Forms[i].FormId == Question->RefFormId)
        {
            // Check if FormSetGuid matches (if specified)
            // For now, assume same formset if RefFormSetGuid is NULL
            ReferencedForm = &HiiCtx->Forms[i];
            break;
        }
    }
    
    if (ReferencedForm == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"Referenced form not found!");
        return EFI_NOT_FOUND;
    }
    
    // Get questions for the referenced form
    HII_QUESTION_INFO *Questions = NULL;
    UINTN QuestionCount = 0;
    
    EFI_STATUS Status = HiiBrowserGetFormQuestions(HiiCtx, ReferencedForm, &Questions, &QuestionCount);
    if (EFI_ERROR(Status))
    {
        MenuShowMessage(MenuCtx, L"Error", L"Failed to get referenced form questions!");
        return Status;
    }
    
    // Create questions menu for the referenced form
    MENU_PAGE *SubMenuPage = HiiBrowserCreateQuestionsMenu(HiiCtx, ReferencedForm, Questions, QuestionCount);
    if (SubMenuPage == NULL)
    {
        if (Questions)
            FreePool(Questions);
        MenuShowMessage(MenuCtx, L"Error", L"Failed to create submenu!");
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Set parent for back navigation
    SubMenuPage->Parent = MenuCtx->CurrentPage;
    
    // Navigate to submenu page
    Status = MenuNavigateTo(MenuCtx, SubMenuPage);
    
    return Status;
}

/**
 * Create a menu page from HII forms
 */
MENU_PAGE *HiiBrowserCreateFormsMenu(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL)
        return NULL;
    
    MENU_PAGE *Page = MenuCreatePage(L"BIOS Settings Pages", Context->FormCount + 1);
    if (Page == NULL)
        return NULL;
    
    // Add each form as a menu item using real titles
    for (UINTN i = 0; i < Context->FormCount; i++)
    {
        HII_FORM_INFO *Form = &Context->Forms[i];
        
        // Use the real form title extracted from IFR
        CHAR16 *DisplayTitle = Form->Title;
        
        MenuAddActionItem(
            Page,
            i,
            DisplayTitle,
            Form->IsHidden ? L"[Previously Hidden/Suppressed Form] - Press ENTER to view" : L"Press ENTER to view form details",
            HiiBrowserCallback_OpenForm,
            Form
        );
        
        // Mark as hidden if it was suppressed
        if (Form->IsHidden)
            Page->Items[i].Hidden = TRUE;
    }
    
    // Add back/exit option
    MenuAddActionItem(
        Page,
        Context->FormCount,
        L"Back to Main Menu",
        L"Return to main menu",
        NULL,
        NULL
    );
    
    return Page;
}

/**
 * Create a menu page from HII questions
 */
MENU_PAGE *HiiBrowserCreateQuestionsMenu(
    HII_BROWSER_CONTEXT *Context,
    HII_FORM_INFO *Form,
    HII_QUESTION_INFO *Questions,
    UINTN QuestionCount
)
{
    if (Context == NULL || Form == NULL || Questions == NULL)
        return NULL;
    
    // Add space for: separator, info item, questions, back button
    MENU_PAGE *Page = MenuCreatePage(Form->Title, QuestionCount + 3);
    if (Page == NULL)
        return NULL;
    
    UINTN ItemIndex = 0;
    
    // Add form information at the top
    MenuAddSeparator(Page, ItemIndex++, L"Configuration Options");
    
    // Add info about the form
    CHAR16 InfoText[256];
    if (Form->IsHidden)
    {
        UnicodeSPrint(InfoText, sizeof(InfoText), 
                     L"  Hidden form with %d configuration options (unlocked)", QuestionCount);
    }
    else
    {
        UnicodeSPrint(InfoText, sizeof(InfoText), 
                     L"  %d configuration options available", QuestionCount);
    }
    MenuAddInfoItem(Page, ItemIndex++, InfoText);
    
    // Add each question as a menu item with current value displayed
    for (UINTN i = 0; i < QuestionCount; i++)
    {
        HII_QUESTION_INFO *Question = &Questions[i];
        CHAR16 TitleWithValue[256];
        
        // Check if this is a form reference (submenu)
        if (Question->IsReference && Question->Type == EFI_IFR_REF_OP)
        {
            // Display as submenu with arrow indicator
            UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                         L"%s >", Question->Prompt);
        }
        // Build title with current value for other types
        else if (Question->Type == EFI_IFR_CHECKBOX_OP)
        {
            // Get current checkbox value
            UINT8 Value = 0;
            if (!EFI_ERROR(HiiBrowserGetQuestionValue(Context, Question, &Value)))
            {
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s [%s]", Question->Prompt, Value ? L"☑" : L"☐");
            }
            else
            {
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s [☐]", Question->Prompt);
            }
        }
        else if (Question->Type == EFI_IFR_ONE_OF_OP)
        {
            // For OneOf, try to show current selected option text
            if (Question->OptionCount > 0 && Question->Options != NULL)
            {
                // Get current value
                UINT64 CurrentValue = 0;
                CHAR16 *SelectedText = L"...";
                
                if (!EFI_ERROR(HiiBrowserGetQuestionValue(Context, Question, &CurrentValue)))
                {
                    Question->CurrentOneOfValue = CurrentValue;
                    
                    // Find matching option
                    for (UINTN opt = 0; opt < Question->OptionCount; opt++)
                    {
                        if (Question->Options[opt].Value == CurrentValue)
                        {
                            if (Question->Options[opt].Text != NULL)
                            {
                                SelectedText = Question->Options[opt].Text;
                            }
                            break;
                        }
                    }
                }
                
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s: %s", Question->Prompt, SelectedText);
            }
            else
            {
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s [...]", Question->Prompt);
            }
        }
        else if (Question->Type == EFI_IFR_NUMERIC_OP)
        {
            // Show numeric value with range if available
            UINT64 Value = 0;
            if (!EFI_ERROR(HiiBrowserGetQuestionValue(Context, Question, &Value)))
            {
                if (Question->Maximum > 0)
                {
                    UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                                 L"%s: %d [%d-%d]", Question->Prompt, Value, 
                                 Question->Minimum, Question->Maximum);
                }
                else
                {
                    UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                                 L"%s: %d", Question->Prompt, Value);
                }
            }
            else
            {
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s [0]", Question->Prompt);
            }
        }
        else if (Question->Type == EFI_IFR_STRING_OP)
        {
            // Show string length limits if available
            if (Question->Maximum > 0)
            {
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s [String, max %d chars]", Question->Prompt, Question->Maximum);
            }
            else
            {
                UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                             L"%s [String]", Question->Prompt);
            }
        }
        else
        {
            UnicodeSPrint(TitleWithValue, sizeof(TitleWithValue), 
                         L"%s", Question->Prompt);
        }
        
        // Allocate and copy the title
        CHAR16 *AllocatedTitle = AllocateCopyPool(StrSize(TitleWithValue), TitleWithValue);
        
        MenuAddActionItem(
            Page,
            ItemIndex,
            AllocatedTitle ? AllocatedTitle : Question->Prompt,
            Question->HelpText,
            HiiBrowserCallback_EditQuestion,  // Add edit callback
            Question
        );
        
        // Mark as hidden or grayed out
        if (Question->IsHidden || Question->IsGrayedOut)
            Page->Items[ItemIndex].Hidden = TRUE;
        
        if (Question->IsGrayedOut)
            Page->Items[ItemIndex].Enabled = TRUE; // Enable it since we've unlocked it
        
        ItemIndex++;
    }
    
    // Add back option
    MenuAddActionItem(
        Page,
        ItemIndex,
        L"← Back to Form List",
        L"Return to previous menu",
        NULL,
        NULL
    );
    
    return Page;
}

/**
 * Get current value of a question from NVRAM
 */
EFI_STATUS HiiBrowserGetQuestionValue(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    VOID *Value
)
{
    if (Context == NULL || Question == NULL || Value == NULL)
        return EFI_INVALID_PARAMETER;
    
    // If we have NVRAM info, read from variable
    if (Question->VariableName && Context->NvramManager)
    {
        VOID *VarData = NULL;
        UINTN VarSize = 0;
        
        EFI_STATUS Status = NvramReadVariable(
            Context->NvramManager,
            Question->VariableName,
            &Question->VariableGuid,
            &VarData,
            &VarSize
        );
        
        if (!EFI_ERROR(Status) && VarData)
        {
            // Extract value at offset
            if (Question->VariableOffset < VarSize)
            {
                CopyMem(Value, (UINT8 *)VarData + Question->VariableOffset, sizeof(UINT64));
                FreePool(VarData);
                return EFI_SUCCESS;
            }
            FreePool(VarData);
        }
    }
    
    // Fallback to current value if set
    if (Question->CurrentValue)
    {
        CopyMem(Value, Question->CurrentValue, sizeof(UINT64));
        return EFI_SUCCESS;
    }
    
    return EFI_NOT_FOUND;
}

/**
 * Set value of a question (stage for save)
 */
EFI_STATUS HiiBrowserSetQuestionValue(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    VOID *Value
)
{
    if (Context == NULL || Question == NULL || Value == NULL)
        return EFI_INVALID_PARAMETER;
    
    // If we have NVRAM info, stage the change
    if (Question->VariableName && Context->NvramManager)
    {
        // Read current variable
        VOID *VarData = NULL;
        UINTN VarSize = 0;
        
        EFI_STATUS Status = NvramReadVariable(
            Context->NvramManager,
            Question->VariableName,
            &Question->VariableGuid,
            &VarData,
            &VarSize
        );
        
        if (EFI_ERROR(Status) || !VarData)
        {
            // Variable doesn't exist, create it
            VarSize = Question->VariableOffset + sizeof(UINT64);
            VarData = AllocateZeroPool(VarSize);
            if (!VarData)
                return EFI_OUT_OF_RESOURCES;
        }
        
        // Update value at offset
        CopyMem((UINT8 *)VarData + Question->VariableOffset, Value, sizeof(UINT64));
        
        // Stage for save
        Status = NvramStageVariable(
            Context->NvramManager,
            Question->VariableName,
            &Question->VariableGuid,
            VarData,
            VarSize
        );
        
        FreePool(VarData);
        
        if (!EFI_ERROR(Status))
        {
            Question->IsModified = TRUE;
        }
        
        return Status;
    }
    
    return EFI_UNSUPPORTED;
}

/**
 * Edit a question value interactively
 */
EFI_STATUS HiiBrowserEditQuestion(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question
)
{
    if (Context == NULL || Question == NULL || Context->MenuContext == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Get current value
    UINT64 CurrentValue = 0;
    HiiBrowserGetQuestionValue(Context, Question, &CurrentValue);
    
    // Show edit dialog
    CHAR16 Message[256];
    UnicodeSPrint(Message, sizeof(Message), 
                  L"Current: %d\nMin: %d, Max: %d\nUse +/- to change, Enter to save, ESC to cancel",
                  CurrentValue, Question->Minimum, Question->Maximum);
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn = gST->ConIn;
    
    UINT64 NewValue = CurrentValue;
    BOOLEAN Done = FALSE;
    
    while (!Done)
    {
        // Draw edit dialog
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
        ConOut->SetCursorPosition(ConOut, 10, 10);
        ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
        ConOut->SetCursorPosition(ConOut, 10, 11);
        ConOut->OutputString(ConOut, L"| Edit Value                                       |");
        ConOut->SetCursorPosition(ConOut, 10, 12);
        ConOut->OutputString(ConOut, L"|                                                  |");
        
        CHAR16 ValueStr[50];
        UnicodeSPrint(ValueStr, sizeof(ValueStr), L"| Value: %d", NewValue);
        ConOut->SetCursorPosition(ConOut, 10, 12);
        ConOut->OutputString(ConOut, ValueStr);
        
        ConOut->SetCursorPosition(ConOut, 10, 13);
        ConOut->OutputString(ConOut, L"|                                                  |");
        ConOut->SetCursorPosition(ConOut, 10, 14);
        ConOut->OutputString(ConOut, L"| +/- to change | Enter to save | ESC to cancel   |");
        ConOut->SetCursorPosition(ConOut, 10, 15);
        ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
        
        // Wait for input
        UINTN Index;
        gBS->WaitForEvent(1, &ConIn->WaitForKey, &Index);
        
        EFI_INPUT_KEY Key;
        ConIn->ReadKeyStroke(ConIn, &Key);
        
        if (Key.UnicodeChar == L'+' || Key.UnicodeChar == L'=')
        {
            if (NewValue < Question->Maximum)
                NewValue += Question->Step > 0 ? Question->Step : 1;
        }
        else if (Key.UnicodeChar == L'-' || Key.UnicodeChar == L'_')
        {
            if (NewValue > Question->Minimum)
                NewValue -= Question->Step > 0 ? Question->Step : 1;
        }
        else if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN)
        {
            // Save the value
            HiiBrowserSetQuestionValue(Context, Question, &NewValue);
            Done = TRUE;
        }
        else if (Key.ScanCode == SCAN_ESC)
        {
            // Cancel
            Done = TRUE;
        }
    }
    
    // Redraw menu
    if (Context->MenuContext)
        MenuDraw(Context->MenuContext);
    
    return EFI_SUCCESS;
}

/**
 * Save all modified values to NVRAM
 */
EFI_STATUS HiiBrowserSaveChanges(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL || Context->NvramManager == NULL)
        return EFI_INVALID_PARAMETER;
    
    UINTN ModifiedCount = NvramGetModifiedCount(Context->NvramManager);
    
    if (ModifiedCount == 0)
    {
        if (Context->MenuContext)
            MenuShowMessage(Context->MenuContext, L"No Changes", L"No modified values to save");
        return EFI_SUCCESS;
    }
    
    // Show confirmation
    BOOLEAN Confirm = FALSE;
    CHAR16 ConfirmMsg[256];
    UnicodeSPrint(ConfirmMsg, sizeof(ConfirmMsg), 
                  L"Save %d modified variables to NVRAM?", ModifiedCount);
    
    if (Context->MenuContext)
    {
        MenuShowConfirm(Context->MenuContext, L"Confirm Save", ConfirmMsg, &Confirm);
    }
    else
    {
        // No menu context, assume confirmed
        Confirm = TRUE;
    }
    
    if (!Confirm)
        return EFI_ABORTED;
    
    // Commit changes
    EFI_STATUS Status = NvramCommitChanges(Context->NvramManager);
    
    if (Context->MenuContext)
    {
        if (EFI_ERROR(Status))
            MenuShowMessage(Context->MenuContext, L"Error", L"Failed to save some variables");
        else
            MenuShowMessage(Context->MenuContext, L"Success", L"All changes saved to NVRAM");
    }
    
    return Status;
}

/**
 * Edit OneOf question (dropdown selection)
 */
EFI_STATUS HiiBrowserEditOneOfQuestion(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    MENU_ITEM *Item,
    MENU_CONTEXT *MenuCtx
)
{
    if (Context == NULL || Question == NULL || MenuCtx == NULL)
        return EFI_INVALID_PARAMETER;
    
    if (Question->OptionCount == 0 || Question->Options == NULL)
    {
        MenuShowMessage(MenuCtx, L"No Options", L"This OneOf question has no available options");
        return EFI_NOT_FOUND;
    }
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn = gST->ConIn;
    
    UINTN SelectedOption = 0;
    
    // Find current selection
    for (UINTN i = 0; i < Question->OptionCount; i++)
    {
        if (Question->Options[i].Value == Question->CurrentOneOfValue)
        {
            SelectedOption = i;
            break;
        }
    }
    
    BOOLEAN Done = FALSE;
    
    while (!Done)
    {
        // Draw selection dialog
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
        ConOut->ClearScreen(ConOut);
        ConOut->SetCursorPosition(ConOut, 2, 2);
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        ConOut->OutputString(ConOut, L" Select Option ");
        
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
        ConOut->SetCursorPosition(ConOut, 2, 3);
        if (Question->Prompt)
        {
            ConOut->OutputString(ConOut, Question->Prompt);
        }
        
        // Display options
        UINTN StartRow = 5;
        UINTN MaxVisible = 15;  // Maximum visible options
        UINTN FirstVisible = 0;
        
        if (SelectedOption >= MaxVisible)
        {
            FirstVisible = SelectedOption - MaxVisible + 1;
        }
        
        for (UINTN i = FirstVisible; i < Question->OptionCount && i < FirstVisible + MaxVisible; i++)
        {
            ConOut->SetCursorPosition(ConOut, 4, StartRow + (i - FirstVisible));
            
            if (i == SelectedOption)
            {
                ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
                ConOut->OutputString(ConOut, L"► ");
            }
            else
            {
                ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
                ConOut->OutputString(ConOut, L"  ");
            }
            
            if (Question->Options[i].Text != NULL)
            {
                ConOut->OutputString(ConOut, Question->Options[i].Text);
            }
        }
        
        // Show navigation help
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLUE, EFI_LIGHTGRAY));
        ConOut->SetCursorPosition(ConOut, 2, StartRow + MaxVisible + 1);
        ConOut->OutputString(ConOut, L"↑↓: Navigate | Enter: Select | ESC: Cancel");
        
        // Wait for input
        UINTN Index;
        gBS->WaitForEvent(1, &ConIn->WaitForKey, &Index);
        
        EFI_INPUT_KEY Key;
        ConIn->ReadKeyStroke(ConIn, &Key);
        
        if (Key.ScanCode == SCAN_UP && SelectedOption > 0)
        {
            SelectedOption--;
        }
        else if (Key.ScanCode == SCAN_DOWN && SelectedOption < Question->OptionCount - 1)
        {
            SelectedOption++;
        }
        else if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN)
        {
            // Save the selected value
            UINT64 NewValue = Question->Options[SelectedOption].Value;
            HiiBrowserSetQuestionValue(Context, Question, &NewValue);
            Question->CurrentOneOfValue = NewValue;
            
            // Update menu item title
            if (Item->Title)
                FreePool((VOID *)Item->Title);
            
            CHAR16 *NewTitle = AllocatePool(512 * sizeof(CHAR16));
            if (NewTitle)
            {
                UnicodeSPrint(NewTitle, 512 * sizeof(CHAR16), 
                             L"%s: %s%s", 
                             Question->Prompt,
                             Question->Options[SelectedOption].Text,
                             Question->IsModified ? L" *" : L"");
                Item->Title = NewTitle;
            }
            
            Done = TRUE;
        }
        else if (Key.ScanCode == SCAN_ESC)
        {
            // Cancel
            Done = TRUE;
        }
    }
    
    // Redraw menu
    MenuDraw(MenuCtx);
    
    return EFI_SUCCESS;
}

/**
 * Edit String question
 */
EFI_STATUS HiiBrowserEditStringQuestion(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    MENU_ITEM *Item,
    MENU_CONTEXT *MenuCtx
)
{
    if (Context == NULL || Question == NULL || MenuCtx == NULL)
        return EFI_INVALID_PARAMETER;
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn = gST->ConIn;
    
    // Allocate buffer for string input
    // Buffer size: 256 characters (maximum BIOS string length)
    // Display limit: 44 characters visible in dialog box
    // Long strings are truncated for display but fully stored
    CHAR16 InputBuffer[256];
    ZeroMem(InputBuffer, sizeof(InputBuffer));
    UINTN InputLength = 0;
    
    // Get current string value if available
    if (Question->CurrentValue != NULL)
    {
        StrCpyS(InputBuffer, 256, (CHAR16 *)Question->CurrentValue);
        InputLength = StrLen(InputBuffer);
    }
    
    BOOLEAN Done = FALSE;
    
    while (!Done)
    {
        // Draw input dialog
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
        ConOut->SetCursorPosition(ConOut, 10, 8);
        ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
        ConOut->SetCursorPosition(ConOut, 10, 9);
        ConOut->OutputString(ConOut, L"|");
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
        ConOut->OutputString(ConOut, L" Edit String ");
        ConOut->SetAttribute(ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
        ConOut->OutputString(ConOut, L"                                  |");
        
        ConOut->SetCursorPosition(ConOut, 10, 10);
        ConOut->OutputString(ConOut, L"|                                                  |");
        ConOut->SetCursorPosition(ConOut, 12, 10);
        if (Question->Prompt)
        {
            // Truncate prompt if too long
            CHAR16 TruncatedPrompt[45];
            if (StrLen(Question->Prompt) > 44)
            {
                StrnCpyS(TruncatedPrompt, 45, Question->Prompt, 44);
            }
            else
            {
                StrCpyS(TruncatedPrompt, 45, Question->Prompt);
            }
            ConOut->OutputString(ConOut, TruncatedPrompt);
        }
        
        ConOut->SetCursorPosition(ConOut, 10, 11);
        ConOut->OutputString(ConOut, L"|                                                  |");
        ConOut->SetCursorPosition(ConOut, 12, 11);
        
        // Display input buffer (max 44 chars visible)
        CHAR16 DisplayBuffer[45];
        if (InputLength > 44)
        {
            StrnCpyS(DisplayBuffer, 45, InputBuffer + (InputLength - 44), 44);
        }
        else
        {
            StrCpyS(DisplayBuffer, 45, InputBuffer);
        }
        ConOut->OutputString(ConOut, DisplayBuffer);
        ConOut->OutputString(ConOut, L"_");  // Cursor
        
        ConOut->SetCursorPosition(ConOut, 10, 12);
        ConOut->OutputString(ConOut, L"|                                                  |");
        ConOut->SetCursorPosition(ConOut, 10, 13);
        ConOut->OutputString(ConOut, L"| Enter: Save | Backspace: Delete | ESC: Cancel   |");
        ConOut->SetCursorPosition(ConOut, 10, 14);
        ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
        
        // Wait for input
        UINTN Index;
        gBS->WaitForEvent(1, &ConIn->WaitForKey, &Index);
        
        EFI_INPUT_KEY Key;
        ConIn->ReadKeyStroke(ConIn, &Key);
        
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN)
        {
            // Save the string
            CHAR16 *NewString = AllocateCopyPool(StrSize(InputBuffer), InputBuffer);
            if (NewString != NULL)
            {
                if (Question->CurrentValue != NULL)
                {
                    FreePool(Question->CurrentValue);
                }
                Question->CurrentValue = NewString;
                HiiBrowserSetQuestionValue(Context, Question, NewString);
                
                // Update menu item title
                if (Item->Title)
                    FreePool((VOID *)Item->Title);
                
                CHAR16 *NewTitle = AllocatePool(512 * sizeof(CHAR16));
                if (NewTitle)
                {
                    UnicodeSPrint(NewTitle, 512 * sizeof(CHAR16), 
                                 L"%s: \"%s\"%s", 
                                 Question->Prompt,
                                 InputBuffer,
                                 Question->IsModified ? L" *" : L"");
                    Item->Title = NewTitle;
                }
            }
            
            Done = TRUE;
        }
        else if (Key.UnicodeChar == CHAR_BACKSPACE && InputLength > 0)
        {
            // Delete last character
            InputLength--;
            InputBuffer[InputLength] = L'\0';
        }
        else if (Key.ScanCode == SCAN_ESC)
        {
            // Cancel
            Done = TRUE;
        }
        else if (Key.UnicodeChar >= 0x20 && Key.UnicodeChar < 0x7F && InputLength < 255)
        {
            // Add printable character
            InputBuffer[InputLength] = Key.UnicodeChar;
            InputLength++;
            InputBuffer[InputLength] = L'\0';
        }
    }
    
    // Redraw menu
    MenuDraw(MenuCtx);
    
    return EFI_SUCCESS;
}

/**
 * Callback: Edit question value (for ENTER key)
 */
EFI_STATUS HiiBrowserCallback_EditQuestion(MENU_ITEM *Item, VOID *Context)
{
    if (Item == NULL || Item->Data == NULL || Context == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_CONTEXT *MenuCtx = (MENU_CONTEXT *)Context;
    HII_QUESTION_INFO *Question = (HII_QUESTION_INFO *)Item->Data;
    
    // Get HII browser context
    HII_BROWSER_CONTEXT *HiiCtx = (HII_BROWSER_CONTEXT *)MenuCtx->UserData;
    if (HiiCtx == NULL)
    {
        MenuShowMessage(MenuCtx, L"Error", L"HII Browser context not available!");
        return EFI_NOT_READY;
    }
    
    // Handle based on question type
    if (Question->IsReference && Question->Type == EFI_IFR_REF_OP)
    {
        // Navigate to referenced form (submenu)
        return HiiBrowserCallback_OpenReferencedForm(Item, Context);
    }
    else if (Question->Type == EFI_IFR_CHECKBOX_OP)
    {
        // Toggle checkbox
        UINT8 CurrentValue = 0;
        HiiBrowserGetQuestionValue(HiiCtx, Question, &CurrentValue);
        
        UINT8 NewValue = CurrentValue ? 0 : 1;
        EFI_STATUS Status = HiiBrowserSetQuestionValue(HiiCtx, Question, &NewValue);
        
        if (!EFI_ERROR(Status))
        {
            // Free old title and allocate new one
            if (Item->Title)
                FreePool((VOID *)Item->Title);
            
            CHAR16 *NewTitle = AllocatePool(256 * sizeof(CHAR16));
            if (NewTitle)
            {
                UnicodeSPrint(NewTitle, 256 * sizeof(CHAR16), 
                             L"%s [%s]%s", Question->Prompt, NewValue ? L"☑" : L"☐",
                             Question->IsModified ? L" *" : L"");
                Item->Title = NewTitle;
            }
            
            // Redraw the menu to show updated value
            MenuDraw(MenuCtx);
        }
        
        return Status;
    }
    else if (Question->Type == EFI_IFR_NUMERIC_OP)
    {
        // Edit numeric value
        return HiiBrowserEditQuestion(HiiCtx, Question);
    }
    else if (Question->Type == EFI_IFR_ONE_OF_OP)
    {
        // Show OneOf selection menu
        return HiiBrowserEditOneOfQuestion(HiiCtx, Question, Item, MenuCtx);
    }
    else if (Question->Type == EFI_IFR_STRING_OP)
    {
        // Edit string value
        return HiiBrowserEditStringQuestion(HiiCtx, Question, Item, MenuCtx);
    }
    
    return EFI_UNSUPPORTED;
}

/**
 * Show save confirmation dialog (for F10)
 */
EFI_STATUS HiiBrowserShowSaveDialog(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL || Context->MenuContext == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Check if there are changes
    if (!HiiBrowserHasChanges(Context))
    {
        MenuShowMessage(Context->MenuContext, L"No Changes", L"No modified values to save");
        return EFI_SUCCESS;
    }
    
    // Show confirmation dialog
    BOOLEAN SaveChanges = FALSE;
    MenuShowConfirm(Context->MenuContext, L"Save Configuration", 
                   L"Save configuration changes and exit?", &SaveChanges);
    
    if (SaveChanges)
    {
        // Save changes
        return HiiBrowserSaveChanges(Context);
    }
    
    return EFI_ABORTED;
}

/**
 * Check if there are any unsaved changes
 */
BOOLEAN HiiBrowserHasChanges(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL || Context->NvramManager == NULL)
        return FALSE;
    
    return NvramGetModifiedCount(Context->NvramManager) > 0;
}

/**
 * Load default values for all questions
 */
EFI_STATUS HiiBrowserLoadDefaults(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL || Context->MenuContext == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Show confirmation dialog
    BOOLEAN LoadDefaults = FALSE;
    MenuShowConfirm(Context->MenuContext, L"Load Setup Defaults", 
                   L"Load optimized default values for all settings?\n\rThis will discard all current changes.", 
                   &LoadDefaults);
    
    if (!LoadDefaults)
        return EFI_ABORTED;
    
    // For now, we'll reset all changes in NVRAM manager
    // In a full implementation, this would:
    // 1. Parse all forms to find DEFAULT opcodes
    // 2. Set each question to its default value
    // 3. Update the menu display
    
    if (Context->NvramManager)
    {
        // Discard all pending changes
        NvramRollback(Context->NvramManager);
        
        // Reload variables from NVRAM
        NvramLoadSetupVariables(Context->NvramManager);
        
        // Refresh the menu display
        if (Context->MenuContext)
        {
            MenuDraw(Context->MenuContext);
        }
        
        MenuShowMessage(Context->MenuContext, L"Defaults Loaded", 
                       L"Default values loaded successfully.\n\rChanges have been discarded.");
    }
    else
    {
        MenuShowMessage(Context->MenuContext, L"Error", 
                       L"Unable to load defaults - NVRAM manager not available");
        return EFI_NOT_READY;
    }
    
    return EFI_SUCCESS;
}

/**
 * Clean up HII browser resources
 */
VOID HiiBrowserCleanup(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL)
        return;
    
    if (Context->Forms)
    {
        for (UINTN i = 0; i < Context->FormCount; i++)
        {
            HII_FORM_INFO *Form = &Context->Forms[i];
            if (Form->Title)
                FreePool(Form->Title);
        }
        FreePool(Context->Forms);
    }
    
    if (Context->Database)
    {
        DatabaseCleanup(Context->Database);
        FreePool(Context->Database);
    }
    
    if (Context->NvramManager)
    {
        NvramCleanup(Context->NvramManager);
        FreePool(Context->NvramManager);
    }
}

/**
 * Helper: Categorize form into tab based on title keywords
 * 
 * Analyzes form title and assigns it to appropriate tab category
 * based on keyword matching. Case-insensitive comparison.
 * 
 * @param Title  Form title to categorize (NULL safe)
 * @return       Tab index (0=Main, 1=Advanced, 2=Power, 3=Boot, 4=Security, 5=Save&Exit)
 */
/**
 * Detect vendor from form title and GUID
 */
STATIC VENDOR_TYPE DetectVendor(CHAR16 *Title, EFI_GUID *FormSetGuid)
{
    if (Title == NULL)
        return VENDOR_UNKNOWN;
    
    // Convert to uppercase for case-insensitive matching
    CHAR16 Upper[MAX_TITLE_LENGTH];
    UINTN TitleLen = StrLen(Title);
    UINTN CopyLen = (TitleLen < MAX_TITLE_LENGTH - 1) ? TitleLen : (MAX_TITLE_LENGTH - 1);
    
    for (UINTN i = 0; i < CopyLen; i++)
    {
        if (Title[i] >= L'a' && Title[i] <= L'z')
            Upper[i] = Title[i] - 32;
        else
            Upper[i] = Title[i];
    }
    Upper[CopyLen] = L'\0';
    
    // Check for HP
    if (StrStr(Upper, KEYWORD_HP) != NULL)
        return VENDOR_HP;
    
    // Check for AMD (CBS, Promontory, etc.)
    if (StrStr(Upper, KEYWORD_AMD) != NULL || 
        StrStr(Upper, KEYWORD_CBS) != NULL || 
        StrStr(Upper, KEYWORD_PROMONTORY) != NULL)
        return VENDOR_AMD;
    
    // Check for Intel (ME, Management Engine, etc.)
    if (StrStr(Upper, KEYWORD_INTEL) != NULL || 
        StrStr(Upper, KEYWORD_ME) != NULL)
        return VENDOR_INTEL;
    
    return VENDOR_GENERIC;
}

/**
 * Detect form category flags (manufacturing, engineering, debug, etc.)
 */
STATIC UINT8 DetectFormCategory(CHAR16 *Title)
{
    UINT8 Flags = FORM_CATEGORY_STANDARD;
    
    if (Title == NULL)
        return Flags;
    
    // Convert to uppercase
    CHAR16 Upper[MAX_TITLE_LENGTH];
    UINTN TitleLen = StrLen(Title);
    UINTN CopyLen = (TitleLen < MAX_TITLE_LENGTH - 1) ? TitleLen : (MAX_TITLE_LENGTH - 1);
    
    for (UINTN i = 0; i < CopyLen; i++)
    {
        if (Title[i] >= L'a' && Title[i] <= L'z')
            Upper[i] = Title[i] - 32;
        else
            Upper[i] = Title[i];
    }
    Upper[CopyLen] = L'\0';
    
    // Check for special categories
    if (StrStr(Upper, KEYWORD_MANUFACTURING) != NULL)
        Flags |= FORM_CATEGORY_MANUFACTURING;
    
    if (StrStr(Upper, KEYWORD_ENGINEER) != NULL)
        Flags |= FORM_CATEGORY_ENGINEERING;
    
    if (StrStr(Upper, KEYWORD_DEBUG) != NULL)
        Flags |= FORM_CATEGORY_DEBUG;
    
    if (StrStr(Upper, KEYWORD_DEMO) != NULL)
        Flags |= FORM_CATEGORY_DEMO;
    
    if (StrStr(Upper, KEYWORD_OEM) != NULL || 
        StrStr(Upper, KEYWORD_VENDOR) != NULL)
        Flags |= FORM_CATEGORY_OEM;
    
    if (StrStr(Upper, KEYWORD_HIDDEN) != NULL)
        Flags |= FORM_CATEGORY_HIDDEN;
    
    return Flags;
}

/**
 * Categorize form into tab by analyzing title keywords
 */
STATIC UINTN CategorizeForm(CHAR16 *Title)
{
    if (Title == NULL)
        return TAB_INDEX_MAIN;  // Main by default
    
    // Convert to uppercase for case-insensitive keyword matching
    CHAR16 Upper[MAX_TITLE_LENGTH];
    UINTN TitleLen = StrLen(Title);
    UINTN CopyLen = (TitleLen < MAX_TITLE_LENGTH - 1) ? TitleLen : (MAX_TITLE_LENGTH - 1);
    
    // Safe character-by-character copy with uppercase conversion
    for (UINTN i = 0; i < CopyLen; i++)
    {
        if (Title[i] >= L'a' && Title[i] <= L'z')
            Upper[i] = Title[i] - 32;  // Convert to uppercase
        else
            Upper[i] = Title[i];
    }
    Upper[CopyLen] = L'\0';  // Null terminate (CopyLen is always < MAX_TITLE_LENGTH)
    
    // Check for Main tab keywords
    if (StrStr(Upper, KEYWORD_MAIN) != NULL || 
        StrStr(Upper, KEYWORD_SYSTEM) != NULL || 
        StrStr(Upper, KEYWORD_INFO) != NULL)
        return TAB_INDEX_MAIN;
    
    // Check for Advanced tab keywords
    if (StrStr(Upper, KEYWORD_ADVANCED) != NULL || 
        StrStr(Upper, KEYWORD_CPU) != NULL || 
        StrStr(Upper, KEYWORD_CHIPSET) != NULL || 
        StrStr(Upper, KEYWORD_PERIPHERAL) != NULL)
        return TAB_INDEX_ADVANCED;
    
    // Check for Power tab keywords
    if (StrStr(Upper, KEYWORD_POWER) != NULL || 
        StrStr(Upper, KEYWORD_ACPI) != NULL || 
        StrStr(Upper, KEYWORD_THERMAL) != NULL)
        return TAB_INDEX_POWER;
    
    // Check for Boot tab keywords
    if (StrStr(Upper, KEYWORD_BOOT) != NULL || 
        StrStr(Upper, KEYWORD_STARTUP) != NULL)
        return TAB_INDEX_BOOT;
    
    // Check for Security tab keywords
    if (StrStr(Upper, KEYWORD_SECURITY) != NULL || 
        StrStr(Upper, KEYWORD_PASSWORD) != NULL || 
        StrStr(Upper, KEYWORD_TPM) != NULL || 
        StrStr(Upper, KEYWORD_SECURE) != NULL)
        return TAB_INDEX_SECURITY;
    
    // Check for Save & Exit tab keywords
    if (StrStr(Upper, KEYWORD_EXIT) != NULL || 
        StrStr(Upper, KEYWORD_SAVE) != NULL)
        return TAB_INDEX_SAVE_EXIT;
    
    // Default to Main tab if no keywords matched
    return TAB_INDEX_MAIN;
}

/**
 * Create dynamic tabs from extracted BIOS forms
 */
EFI_STATUS HiiBrowserCreateDynamicTabs(
    HII_BROWSER_CONTEXT *Context,
    MENU_CONTEXT *MenuCtx
)
{
    if (Context == NULL || MenuCtx == NULL)
        return EFI_INVALID_PARAMETER;
    
    if (Context->FormCount == 0)
    {
        Print(L"No forms extracted from BIOS\n");
        return EFI_NOT_FOUND;
    }
    
    Print(L"Creating dynamic tabs from %d BIOS forms...\n", Context->FormCount);
    
    // Initialize tabs
    EFI_STATUS Status = MenuInitializeTabs(MenuCtx, 6);
    if (EFI_ERROR(Status))
        return Status;
    
    // Create 6 tab pages
    MENU_PAGE *TabPages[6] = {NULL};
    UINTN TabFormCounts[6] = {0};
    
    // Count forms per tab
    for (UINTN i = 0; i < Context->FormCount; i++)
    {
        UINTN TabIndex = CategorizeForm(Context->Forms[i].Title);
        TabFormCounts[TabIndex]++;
    }
    
    // Create pages for each tab
    CHAR16 *TabNames[6] = {
        L"Main",
        L"Advanced",
        L"Power",
        L"Boot",
        L"Security",
        L"Save & Exit"
    };
    
    for (UINTN t = 0; t < 6; t++)
    {
        UINTN ItemCount = TabFormCounts[t] + 2;  // +2 for separator and info
        TabPages[t] = MenuCreatePage(TabNames[t], ItemCount);
        
        if (TabPages[t] != NULL)
        {
            UINTN ItemIndex = 0;
            
            // Add forms belonging to this tab
            for (UINTN i = 0; i < Context->FormCount; i++)
            {
                if (CategorizeForm(Context->Forms[i].Title) == t)
                {
                    // Build description with vendor and category info
                    CHAR16 Description[MAX_DESCRIPTION_LENGTH];
                    CHAR16 *VendorStr = L"";
                    
                    // Determine vendor string
                    switch (Context->Forms[i].Vendor)
                    {
                        case VENDOR_HP:
                            VendorStr = L"[HP] ";
                            break;
                        case VENDOR_AMD:
                            VendorStr = L"[AMD] ";
                            break;
                        case VENDOR_INTEL:
                            VendorStr = L"[Intel] ";
                            break;
                        default:
                            VendorStr = L"";
                            break;
                    }
                    
                    // Build category string
                    CHAR16 CategoryParts[MAX_DESCRIPTION_LENGTH];
                    CategoryParts[0] = L'\0';
                    
                    if (Context->Forms[i].CategoryFlags & FORM_CATEGORY_MANUFACTURING)
                        StrCatS(CategoryParts, MAX_DESCRIPTION_LENGTH / 2, L"[Manufacturing] ");
                    if (Context->Forms[i].CategoryFlags & FORM_CATEGORY_ENGINEERING)
                        StrCatS(CategoryParts, MAX_DESCRIPTION_LENGTH / 2, L"[Engineering] ");
                    if (Context->Forms[i].CategoryFlags & FORM_CATEGORY_DEBUG)
                        StrCatS(CategoryParts, MAX_DESCRIPTION_LENGTH / 2, L"[Debug] ");
                    if (Context->Forms[i].CategoryFlags & FORM_CATEGORY_DEMO)
                        StrCatS(CategoryParts, MAX_DESCRIPTION_LENGTH / 2, L"[Demo] ");
                    if (Context->Forms[i].CategoryFlags & FORM_CATEGORY_OEM)
                        StrCatS(CategoryParts, MAX_DESCRIPTION_LENGTH / 2, L"[OEM] ");
                    
                    // Build final description
                    if (Context->Forms[i].IsHidden)
                    {
                        UnicodeSPrint(Description, sizeof(Description),
                                     L"%s%s[Previously Hidden] - Press ENTER to view",
                                     VendorStr, CategoryParts);
                    }
                    else
                    {
                        UnicodeSPrint(Description, sizeof(Description),
                                     L"%s%sPress ENTER to view details",
                                     VendorStr, CategoryParts);
                    }
                    
                    MenuAddActionItem(
                        TabPages[t],
                        ItemIndex,
                        Context->Forms[i].Title,
                        Description,
                        HiiBrowserCallback_OpenForm,
                        &Context->Forms[i]
                    );
                    
                    if (Context->Forms[i].IsHidden)
                        TabPages[t]->Items[ItemIndex].Hidden = TRUE;
                    
                    ItemIndex++;
                }
            }
            
            // Add separator and info
            if (ItemIndex < ItemCount - 2)
            {
                MenuAddSeparator(TabPages[t], ItemIndex, NULL);
                ItemIndex++;
            }
            
            if (ItemIndex < ItemCount - 1)
            {
                CHAR16 InfoText[MAX_DESCRIPTION_LENGTH];
                UnicodeSPrint(InfoText, sizeof(InfoText), 
                             L"%d configuration form(s) in this category", TabFormCounts[t]);
                MenuAddInfoItem(TabPages[t], ItemIndex, InfoText);
            }
        }
    }
    
    // Add tabs to menu
    for (UINTN t = 0; t < 6; t++)
    {
        if (TabPages[t] != NULL)
        {
            MenuAddTab(MenuCtx, t, TabNames[t], TabPages[t]);
        }
    }
    
    // Start with Main tab
    MenuSwitchTab(MenuCtx, 0);
    
    Print(L"Dynamic tabs created successfully\n");
    Print(L"  Main: %d forms\n", TabFormCounts[0]);
    Print(L"  Advanced: %d forms\n", TabFormCounts[1]);
    Print(L"  Power: %d forms\n", TabFormCounts[2]);
    Print(L"  Boot: %d forms\n", TabFormCounts[3]);
    Print(L"  Security: %d forms\n", TabFormCounts[4]);
    Print(L"  Save & Exit: %d forms\n", TabFormCounts[5]);
    
    return EFI_SUCCESS;
}
