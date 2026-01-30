#include "HiiBrowser.h"
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
        if (HiiHandles)
            FreePool(HiiHandles);
        return Status;
    }
    
    // Allocate form info array
    Context->Forms = AllocateZeroPool(HandleCount * sizeof(HII_FORM_INFO) * 10); // Estimate 10 forms per handle
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
    
    if (HiiHandles)
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
 * Get current value of a question
 */
EFI_STATUS HiiBrowserGetQuestionValue(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    VOID *Value
)
{
    if (Context == NULL || Question == NULL || Value == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Placeholder - would use HII Config Access Protocol
    return EFI_UNSUPPORTED;
}

/**
 * Set value of a question
 */
EFI_STATUS HiiBrowserSetQuestionValue(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    VOID *Value
)
{
    if (Context == NULL || Question == NULL || Value == NULL)
        return EFI_INVALID_PARAMETER;
    
    // Placeholder - would use HII Config Access Protocol
    return EFI_UNSUPPORTED;
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
}
