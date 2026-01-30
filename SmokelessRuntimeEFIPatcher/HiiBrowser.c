#include "HiiBrowser.h"
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
    
    // For now, create placeholder questions
    // A full implementation would parse the form's IFR data
    *QuestionCount = 5;
    *Questions = AllocateZeroPool(sizeof(HII_QUESTION_INFO) * (*QuestionCount));
    
    if (*Questions == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    for (UINTN i = 0; i < *QuestionCount; i++)
    {
        HII_QUESTION_INFO *Question = &(*Questions)[i];
        Question->QuestionId = (UINT16)i;
        Question->Prompt = AllocateCopyPool(StrSize(L"BIOS Option"), L"BIOS Option");
        Question->HelpText = AllocateCopyPool(StrSize(L"Help text"), L"Help text");
        Question->Type = 0;
        Question->IsHidden = FALSE;
        Question->IsGrayedOut = FALSE;
    }
    
    return EFI_SUCCESS;
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
 */
STATIC UINTN CategorizeForm(CHAR16 *Title)
{
    if (Title == NULL)
        return 0;  // Main by default
    
    // Convert to uppercase for comparison
    CHAR16 Upper[128];
    UINTN Len = StrLen(Title);
    if (Len >= 128) Len = 127;
    
    for (UINTN i = 0; i < Len; i++)
    {
        if (Title[i] >= L'a' && Title[i] <= L'z')
            Upper[i] = Title[i] - 32;
        else
            Upper[i] = Title[i];
    }
    Upper[Len] = 0;
    
    // Check for keywords
    if (StrStr(Upper, L"MAIN") != NULL || StrStr(Upper, L"SYSTEM") != NULL || 
        StrStr(Upper, L"INFO") != NULL)
        return 0;  // Main tab
    
    if (StrStr(Upper, L"ADVANCED") != NULL || StrStr(Upper, L"CPU") != NULL || 
        StrStr(Upper, L"CHIPSET") != NULL || StrStr(Upper, L"PERIPHERAL") != NULL)
        return 1;  // Advanced tab
    
    if (StrStr(Upper, L"POWER") != NULL || StrStr(Upper, L"ACPI") != NULL || 
        StrStr(Upper, L"THERMAL") != NULL)
        return 2;  // Power tab
    
    if (StrStr(Upper, L"BOOT") != NULL || StrStr(Upper, L"STARTUP") != NULL)
        return 3;  // Boot tab
    
    if (StrStr(Upper, L"SECURITY") != NULL || StrStr(Upper, L"PASSWORD") != NULL || 
        StrStr(Upper, L"TPM") != NULL || StrStr(Upper, L"SECURE") != NULL)
        return 4;  // Security tab
    
    if (StrStr(Upper, L"EXIT") != NULL || StrStr(Upper, L"SAVE") != NULL)
        return 5;  // Save & Exit tab
    
    return 0;  // Default to Main
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
                CHAR16 InfoText[100];
                UnicodeSPrint(InfoText, sizeof(InfoText), L"%d configuration form(s) in this category", TabFormCounts[t]);
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
