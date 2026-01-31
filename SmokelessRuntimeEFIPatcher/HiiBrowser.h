#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiConfigRouting.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/FormBrowser2.h>
#include "MenuUI.h"
#include "NvramManager.h"

// Vendor type enumeration
typedef enum {
    VENDOR_UNKNOWN = 0,
    VENDOR_HP,
    VENDOR_AMD,
    VENDOR_INTEL,
    VENDOR_GENERIC
} VENDOR_TYPE;

// Form category flags
#define FORM_CATEGORY_STANDARD      0x00
#define FORM_CATEGORY_MANUFACTURING 0x01
#define FORM_CATEGORY_ENGINEERING   0x02
#define FORM_CATEGORY_DEBUG         0x04
#define FORM_CATEGORY_DEMO          0x08
#define FORM_CATEGORY_OEM           0x10
#define FORM_CATEGORY_HIDDEN        0x20

// HII Form information
typedef struct {
    EFI_HII_HANDLE HiiHandle;
    EFI_GUID FormSetGuid;
    UINT16 FormId;
    CHAR16 *Title;
    BOOLEAN IsHidden;       // Was this form suppressed/hidden
    VENDOR_TYPE Vendor;     // Detected vendor (HP, AMD, Intel, etc.)
    UINT8 CategoryFlags;    // Form category flags (manufacturing, engineering, etc.)
} HII_FORM_INFO;

// HII OneOf Option information
typedef struct {
    CHAR16 *Text;           // Option display text
    UINT64 Value;           // Option value
} HII_OPTION_INFO;

// HII Question/Option information
typedef struct {
    UINT16 QuestionId;
    CHAR16 *Prompt;
    CHAR16 *HelpText;
    UINT8 Type;             // Question type (checkbox, numeric, etc)
    VOID *CurrentValue;
    VOID *DefaultValue;
    UINT64 Minimum;         // For numeric types
    UINT64 Maximum;
    UINT64 Step;
    CHAR16 *VariableName;   // NVRAM variable name
    EFI_GUID VariableGuid;  // NVRAM variable GUID
    UINTN VariableOffset;   // Offset in variable
    BOOLEAN IsHidden;       // Was this option suppressed/hidden
    BOOLEAN IsGrayedOut;    // Was this option grayed out
    BOOLEAN IsModified;     // Has value been changed
    
    // OneOf options
    HII_OPTION_INFO *Options;  // Array of options for OneOf questions
    UINTN OptionCount;         // Number of options
    UINT64 CurrentOneOfValue;  // Current selected value for OneOf
    
    // Form reference (for submenus)
    BOOLEAN IsReference;    // Is this a form reference/submenu?
    UINT16 RefFormId;       // Referenced form ID
    EFI_GUID RefFormSetGuid; // Referenced formset GUID
} HII_QUESTION_INFO;

// HII Browser context
typedef struct {
    EFI_HII_DATABASE_PROTOCOL *HiiDatabase;
    EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
    EFI_FORM_BROWSER2_PROTOCOL *FormBrowser2;
    
    HII_FORM_INFO *Forms;
    UINTN FormCount;
    
    NVRAM_MANAGER *NvramManager;  // NVRAM manager
    DATABASE_CONTEXT *Database;   // Configuration database
    MENU_CONTEXT *MenuContext;
} HII_BROWSER_CONTEXT;

/**
 * Initialize HII browser
 */
EFI_STATUS HiiBrowserInitialize(HII_BROWSER_CONTEXT *Context);

/**
 * Enumerate all HII forms in the system
 */
EFI_STATUS HiiBrowserEnumerateForms(HII_BROWSER_CONTEXT *Context);

/**
 * Get questions for a specific form
 */
EFI_STATUS HiiBrowserGetFormQuestions(
    HII_BROWSER_CONTEXT *Context,
    HII_FORM_INFO *Form,
    HII_QUESTION_INFO **Questions,
    UINTN *QuestionCount
);

/**
 * Create a menu page from HII forms
 */
MENU_PAGE *HiiBrowserCreateFormsMenu(HII_BROWSER_CONTEXT *Context);

/**
 * Create a menu page from HII questions
 */
MENU_PAGE *HiiBrowserCreateQuestionsMenu(
    HII_BROWSER_CONTEXT *Context,
    HII_FORM_INFO *Form,
    HII_QUESTION_INFO *Questions,
    UINTN QuestionCount
);

/**
 * Create dynamic tabs from extracted BIOS forms
 * Organizes forms into tabs based on form titles and categories
 */
EFI_STATUS HiiBrowserCreateDynamicTabs(
    HII_BROWSER_CONTEXT *Context,
    MENU_CONTEXT *MenuCtx
);

/**
 * Get current value of a question from NVRAM
 */
EFI_STATUS HiiBrowserGetQuestionValue(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    VOID *Value
);

/**
 * Set value of a question (stage for save)
 */
EFI_STATUS HiiBrowserSetQuestionValue(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    VOID *Value
);

/**
 * Edit a question value interactively
 */
EFI_STATUS HiiBrowserEditQuestion(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question
);

/**
 * Edit OneOf question (dropdown selection)
 */
EFI_STATUS HiiBrowserEditOneOfQuestion(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    MENU_ITEM *Item,
    MENU_CONTEXT *MenuCtx
);

/**
 * Edit String question
 */
EFI_STATUS HiiBrowserEditStringQuestion(
    HII_BROWSER_CONTEXT *Context,
    HII_QUESTION_INFO *Question,
    MENU_ITEM *Item,
    MENU_CONTEXT *MenuCtx
);

/**
 * Callback: Edit question value (for ENTER key)
 */
EFI_STATUS HiiBrowserCallback_EditQuestion(MENU_ITEM *Item, VOID *Context);

/**
 * Save all modified values to NVRAM
 */
EFI_STATUS HiiBrowserSaveChanges(HII_BROWSER_CONTEXT *Context);

/**
 * Show save confirmation dialog (for F10)
 */
EFI_STATUS HiiBrowserShowSaveDialog(HII_BROWSER_CONTEXT *Context);

/**
 * Check if there are any unsaved changes
 */
BOOLEAN HiiBrowserHasChanges(HII_BROWSER_CONTEXT *Context);

/**
 * Clean up HII browser resources
 */
VOID HiiBrowserCleanup(HII_BROWSER_CONTEXT *Context);
