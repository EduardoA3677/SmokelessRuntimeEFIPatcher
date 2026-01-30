# IFR Opcode Implementation Guide

## Overview
This document provides a comprehensive guide for implementing the missing IFR opcodes in SmokelessRuntimeEFIPatcher to achieve complete BIOS configuration functionality.

## BIOS Analysis Results

**File**: o.bin (8MB HP AMI BIOS F.73)
**Setup Module**: Present with complete IFR data
**Forms**: Multiple vendor-specific configurations (HP, AMD, Intel)

## Implementation Phases

### Phase 1: ONE_OF_OPTION Support (CRITICAL) ✅

**Purpose**: Enable selection menus for multiple-choice questions

**Implementation Location**: `ParseFormQuestions()` in HiiBrowser.c

**Algorithm**:
```c
// After parsing EFI_IFR_ONE_OF_OP:
case EFI_IFR_ONE_OF_OPTION_OP:
{
    if (CurrentQuestion && CurrentQuestion->Type == EFI_IFR_ONE_OF_OP)
    {
        EFI_IFR_ONE_OF_OPTION *Option = (EFI_IFR_ONE_OF_OPTION *)OpHeader;
        
        // Expand options array if needed
        if (CurrentQuestion->OptionCount >= OptionCapacity)
        {
            OptionCapacity = OptionCapacity ? OptionCapacity * 2 : 4;
            CurrentQuestion->Options = ReallocatePool(
                CurrentQuestion->OptionCount * sizeof(HII_OPTION_INFO),
                OptionCapacity * sizeof(HII_OPTION_INFO),
                CurrentQuestion->Options
            );
        }
        
        // Get option text from HII String Protocol
        CHAR16 *OptionText = NULL;
        Status = HiiString->GetString(HiiString, "en-US", HiiHandle,
                                     Option->Option, OptionText, &StringSize, NULL);
        
        // Extract value based on type
        UINT64 OptionValue = 0;
        UINT8 *ValuePtr = (UINT8 *)Option + sizeof(EFI_IFR_ONE_OF_OPTION);
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
        }
        
        // Store option
        CurrentQuestion->Options[CurrentQuestion->OptionCount].Text = OptionText;
        CurrentQuestion->Options[CurrentQuestion->OptionCount].Value = OptionValue;
        CurrentQuestion->OptionCount++;
    }
    break;
}
```

**UI Integration**: In `HiiBrowserCallback_EditQuestion()`:
```c
if (Question->Type == EFI_IFR_ONE_OF_OP && Question->OptionCount > 0)
{
    // Show selection menu
    MENU_PAGE *OptionPage = MenuCreatePage(L"Select Option");
    
    for (UINTN i = 0; i < Question->OptionCount; i++)
    {
        MENU_ITEM *OptionItem = MenuAddItem(OptionPage, 
                                           Question->Options[i].Text,
                                           L"",
                                           MENU_ITEM_ACTION);
        OptionItem->UserData = (VOID *)(UINTN)i;
    }
    
    // Show dialog and get selection
    // Update Question->CurrentOneOfValue with selected option's value
    // Call HiiBrowserSetQuestionValue()
}
```

### Phase 2: TEXT and SUBTITLE Support

**Purpose**: Display read-only information and section headers

**Implementation**:
```c
case EFI_IFR_TEXT_OP:
{
    EFI_IFR_TEXT *Text = (EFI_IFR_TEXT *)OpHeader;
    
    // Get text strings
    CHAR16 *PromptText = GetHiiString(HiiHandle, Text->Prompt);
    CHAR16 *HelpText = GetHiiString(HiiHandle, Text->Help);
    CHAR16 *TextTwo = GetHiiString(HiiHandle, Text->TextTwo);
    
    // Create INFO type question
    HII_QUESTION_INFO *InfoItem = &Questions[QuestionIndex++];
    InfoItem->Type = MENU_ITEM_INFO;  // Special type for display-only
    InfoItem->Prompt = PromptText;
    InfoItem->HelpText = TextTwo;  // Show second text as help
    
    break;
}

case EFI_IFR_SUBTITLE_OP:
{
    EFI_IFR_SUBTITLE *Subtitle = (EFI_IFR_SUBTITLE *)OpHeader;
    
    // Get subtitle text
    CHAR16 *SubtitleText = GetHiiString(HiiHandle, Subtitle->Prompt);
    
    // Create SEPARATOR type question
    HII_QUESTION_INFO *SepItem = &Questions[QuestionIndex++];
    SepItem->Type = MENU_ITEM_SEPARATOR;
    SepItem->Prompt = SubtitleText;
    
    break;
}
```

**UI Rendering**: In MenuUI.c `MenuDraw()`:
- INFO items: Render in gray without selection indicator
- SEPARATOR items: Render with horizontal line decoration

### Phase 3: REF Support (Form Navigation)

**Purpose**: Enable recursive form references and submenu navigation

**Implementation**:
```c
case EFI_IFR_REF_OP:
{
    EFI_IFR_REF *Ref = (EFI_IFR_REF *)OpHeader;
    
    // Get question prompt
    CHAR16 *PromptText = GetHiiString(HiiHandle, Ref->Question.Header.Prompt);
    
    // Create question with reference flag
    HII_QUESTION_INFO *RefQuestion = &Questions[QuestionIndex++];
    RefQuestion->Type = EFI_IFR_REF_OP;  // Special type
    RefQuestion->Prompt = PromptText;
    RefQuestion->IsReference = TRUE;
    RefQuestion->RefFormId = Ref->FormId;
    RefQuestion->RefFormSetGuid = CurrentFormSetGuid;  // From parent
    
    break;
}
```

**Navigation Handler**: In `HiiBrowserCallback_EditQuestion()`:
```c
if (Question->IsReference)
{
    // Find referenced form
    HII_FORM_INFO *RefForm = FindFormById(HiiCtx, 
                                         &Question->RefFormSetGuid,
                                         Question->RefFormId);
    
    if (RefForm)
    {
        // Open referenced form (recursive)
        HiiBrowserCallback_OpenForm(MenuCtx, Item);
    }
}
```

### Phase 4: DEFAULT Support

**Purpose**: Store default values for F9 reset functionality

**Implementation**:
```c
case EFI_IFR_DEFAULT_OP:
{
    if (CurrentQuestion)
    {
        EFI_IFR_DEFAULT *Default = (EFI_IFR_DEFAULT *)OpHeader;
        
        // Extract default value based on type
        UINT8 *ValuePtr = (UINT8 *)Default + sizeof(EFI_IFR_DEFAULT);
        UINT64 DefaultValue = 0;
        
        switch (Default->Type)
        {
            case EFI_IFR_TYPE_NUM_SIZE_8:
                DefaultValue = *(UINT8 *)ValuePtr;
                break;
            // ... handle other types
        }
        
        // Store in question
        CurrentQuestion->DefaultValue = AllocateCopyPool(sizeof(UINT64), &DefaultValue);
    }
    break;
}
```

**F9 Handler**: In MenuUI.c `MenuHandleInput()`:
```c
if (Key->ScanCode == SCAN_F9)
{
    // Reset all questions to defaults
    for (UINTN i = 0; i < QuestionCount; i++)
    {
        if (Questions[i].DefaultValue)
        {
            // Copy default to current
            CopyMem(Questions[i].CurrentValue, 
                   Questions[i].DefaultValue, 
                   GetValueSize(Questions[i].Type));
            
            // Mark as modified
            Questions[i].IsModified = TRUE;
        }
    }
    
    // Redraw menu
    MenuDraw(Context);
}
```

### Phase 5: ACTION Support

**Purpose**: Enable action buttons (Reset, Save, etc.)

**Implementation**:
```c
case EFI_IFR_ACTION_OP:
{
    EFI_IFR_ACTION *Action = (EFI_IFR_ACTION *)OpHeader;
    
    // Get action text
    CHAR16 *ActionText = GetHiiString(HiiHandle, Action->Question.Header.Prompt);
    
    // Create action question
    HII_QUESTION_INFO *ActionItem = &Questions[QuestionIndex++];
    ActionItem->Type = MENU_ITEM_ACTION;
    ActionItem->Prompt = ActionText;
    ActionItem->QuestionId = Action->Question.Header.QuestionId;
    
    break;
}
```

**Action Handler**: In callback:
```c
if (Question->Type == MENU_ITEM_ACTION)
{
    // Execute action based on QuestionId or config string
    // Could trigger save, load, reset, etc.
    Print(L"Action triggered: %s\n", Question->Prompt);
}
```

### Phase 6: Enhanced END_OP Handling

**Purpose**: Properly track scope depth for nested structures

**Implementation**:
```c
// At start of parsing:
UINTN ScopeDepth = 0;
BOOLEAN InSuppressIf = FALSE;
BOOLEAN InOneOf = FALSE;

// During parsing:
case EFI_IFR_ONE_OF_OP:
    ScopeDepth++;
    InOneOf = TRUE;
    CurrentQuestion = ...;
    break;

case EFI_IFR_SUPPRESS_IF_OP:
    ScopeDepth++;
    InSuppressIf = TRUE;
    break;

case EFI_IFR_END_OP:
    if (ScopeDepth > 0)
    {
        ScopeDepth--;
        
        // Check what scope we're closing
        if (InOneOf && ScopeDepth == OneOfStartDepth)
        {
            InOneOf = FALSE;
            CurrentQuestion = NULL;  // Done with this question
        }
        
        if (InSuppressIf && ScopeDepth == SuppressStartDepth)
        {
            InSuppressIf = FALSE;
        }
    }
    break;
```

## Custom Form Loading

### Vendor-Specific Forms

**HP Forms**: Look for FormSetGuid patterns like:
- `{ 0x..., ... }` with "HP" in strings

**AMD Forms**: Look for "AMD", "CBS", "Promontory"

**Intel Forms**: Look for "Intel", "ME", "Management Engine"

**Implementation**:
```c
// In HiiBrowserEnumerateForms():
for each HII package:
    if (FormSetGuid matches known vendor pattern)
    {
        // Mark as vendor-specific
        Form->Vendor = DetectVendor(FormSetGuid, Title);
        
        // Create special tab or section
        if (Form->Vendor == VENDOR_HP)
            AddToTab(TAB_HP_CUSTOM);
        else if (Form->Vendor == VENDOR_AMD)
            AddToTab(TAB_AMD_CUSTOM);
    }
```

### Database Loading

**Setup Data**: Already loaded in `NvramLoadSetupVariables()`

**Additional Databases**:
```c
// Load all standard UEFI variables
LoadVariable(L"Setup", &gEfiGlobalVariableGuid);
LoadVariable(L"SetupDefault", &gEfiGlobalVariableGuid);
LoadVariable(L"PlatformConfig", &VendorGuid);
LoadVariable(L"CustomSetup", &VendorGuid);

// Scan for vendor-specific variables
EnumerateAllVariables();
```

## Testing Checklist

- [ ] OneOf questions show option list
- [ ] TEXT items display information
- [ ] SUBTITLE creates visual sections
- [ ] REF navigation opens subforms
- [ ] DEFAULT values stored
- [ ] F9 resets to defaults
- [ ] ACTION buttons trigger
- [ ] Vendor forms load (HP, AMD)
- [ ] All databases accessible
- [ ] No memory leaks

## Integration Points

1. **HiiBrowser.c**: Add all opcode parsing in `ParseFormQuestions()`
2. **MenuUI.c**: Handle new item types (INFO, SEPARATOR, ACTION)
3. **NvramManager.c**: Ensure all variables loadable
4. **SmokelessRuntimeEFIPatcher.c**: Initialize all subsystems

## Performance Considerations

- Use dynamic allocation for option arrays
- Cache HII strings to avoid repeated protocol calls
- Implement circular reference detection for REF opcodes
- Lazy-load referenced forms only when accessed

## Summary

This implementation will transform SREP into a complete BIOS editor with:
- ✅ Full OneOf selection menus
- ✅ Information display (TEXT/SUBTITLE)
- ✅ Recursive form navigation (REF)
- ✅ Default value management (DEFAULT)
- ✅ Action button support (ACTION)
- ✅ Vendor-specific form loading
- ✅ Complete database access

All critical BIOS configuration capabilities will be available!
