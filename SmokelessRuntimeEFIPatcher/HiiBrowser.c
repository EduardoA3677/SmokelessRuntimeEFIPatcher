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
    
    // Allocate form info array
    Context->Forms = AllocateZeroPool(HandleCount * sizeof(HII_FORM_INFO) * 10); // Estimate 10 forms per handle
    if (Context->Forms == NULL)
    {
        if (HiiHandles != NULL)
            FreePool(HiiHandles);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Context->FormCount = 0;
    
    // For now, just create placeholder entries for each HII handle
    // A full implementation would parse the IFR package data
    for (UINTN i = 0; i < HandleCount; i++)
    {
        HII_FORM_INFO *FormInfo = &Context->Forms[Context->FormCount];
        FormInfo->HiiHandle = HiiHandles[i];
        FormInfo->FormId = 0;
        FormInfo->Title = AllocateCopyPool(StrSize(L"BIOS Form"), L"BIOS Form");
        FormInfo->IsHidden = FALSE;
        Context->FormCount++;
    }
    
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
MENU_PAGE *HiiBrowserCreateFormsMenu(HII_BROWSER_CONTEXT *Context)
{
    if (Context == NULL)
        return NULL;
    
    MENU_PAGE *Page = MenuCreatePage(L"BIOS Settings Pages", Context->FormCount + 1);
    if (Page == NULL)
        return NULL;
    
    // Add each form as a menu item
    for (UINTN i = 0; i < Context->FormCount; i++)
    {
        HII_FORM_INFO *Form = &Context->Forms[i];
        
        // For now, create simple entries
        CHAR16 Title[100];
        UnicodeSPrint(Title, sizeof(Title), L"BIOS Page %d", i + 1);
        
        MenuAddActionItem(
            Page,
            i,
            Title,
            Form->IsHidden ? L"[Hidden/Unlocked Option]" : L"Standard BIOS Page",
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
