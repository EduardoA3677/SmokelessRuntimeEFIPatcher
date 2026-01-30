#include "HiiBrowser.h"
#include "Constants.h"
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
    CHAR16 *CurrentFormTitle = NULL;
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
                    }
                    
                    if (Count < Capacity)
                    {
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
                        Count++;
                    }
                }
                break;
            }
            
            case EFI_IFR_SUPPRESS_IF_OP:
            {
                InSuppressIf = TRUE;
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
                InSuppressIf = TRUE;
                break;
            
            case EFI_IFR_GRAY_OUT_IF_OP:
                InGrayoutIf = TRUE;
                break;
            
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
                                Question->VariableName = NULL;  // Would need varstore lookup
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
                                Question->VariableName = NULL;
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
                                Question->VariableName = NULL;
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
                                Question->VariableName = NULL;
                                Question->VariableOffset = String->Question.VarStoreInfo.VarOffset;
                            }
                        }
                        
                        Question->QuestionId = QuestionId;
                        Question->Type = OpHeader->OpCode;
                        Question->IsHidden = InSuppressIf;
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
        }
        
        Offset += OpHeader->Length;
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
 * Create a menu page from HII forms
 */
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
            Form->IsHidden ? L"[Previously Hidden/Suppressed Form]" : L"BIOS Configuration Form",
            NULL,
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
    
    MENU_PAGE *Page = MenuCreatePage(Form->Title, QuestionCount + 1);
    if (Page == NULL)
        return NULL;
    
    // Add each question as a menu item
    for (UINTN i = 0; i < QuestionCount; i++)
    {
        HII_QUESTION_INFO *Question = &Questions[i];
        
        MenuAddActionItem(
            Page,
            i,
            Question->Prompt,
            Question->HelpText,
            NULL,
            Question
        );
        
        // Mark as hidden or grayed out
        if (Question->IsHidden || Question->IsGrayedOut)
            Page->Items[i].Hidden = TRUE;
        
        if (Question->IsGrayedOut)
            Page->Items[i].Enabled = TRUE; // Enable it since we've unlocked it
    }
    
    // Add back option
    MenuAddActionItem(
        Page,
        QuestionCount,
        L"Back",
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
                    MenuAddActionItem(
                        TabPages[t],
                        ItemIndex,
                        Context->Forms[i].Title,
                        Context->Forms[i].IsHidden ? L"[Previously Hidden]" : L"BIOS Configuration",
                        NULL,
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
