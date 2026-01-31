#include "MenuUI.h"
#include "HiiBrowser.h"
#include "DebugLog.h"
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

// Default color scheme (EFI colors)
// AMI BIOS Color Scheme - White background, professional appearance
#define COLOR_TITLE         (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define COLOR_NORMAL        (EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY)
#define COLOR_SELECTED      (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define COLOR_DISABLED      (EFI_DARKGRAY | EFI_BACKGROUND_LIGHTGRAY)
#define COLOR_HIDDEN        (EFI_DARKGRAY | EFI_BACKGROUND_LIGHTGRAY)
#define COLOR_DESCRIPTION   (EFI_BLUE | EFI_BACKGROUND_LIGHTGRAY)
#define COLOR_BACKGROUND    (EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY)
#define COLOR_TAB_ACTIVE    (EFI_YELLOW | EFI_BACKGROUND_BLUE)
#define COLOR_TAB_INACTIVE  (EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY)

/**
 * Initialize menu system
 */
EFI_STATUS MenuInitialize(MENU_CONTEXT *Context)
{
    if (Context == NULL)
        return EFI_INVALID_PARAMETER;

    ZeroMem(Context, sizeof(MENU_CONTEXT));
    
    Context->TextIn = gST->ConIn;
    Context->TextOut = gST->ConOut;
    Context->Running = TRUE;
    
    // Get screen dimensions from current mode
    UINTN Columns = 80, Rows = 25;  // Default fallback
    EFI_STATUS Status = gST->ConOut->QueryMode(
        gST->ConOut,
        gST->ConOut->Mode->Mode,
        &Columns,
        &Rows
    );
    
    if (!EFI_ERROR(Status))
    {
        Context->ScreenWidth = Columns;
        Context->ScreenHeight = Rows;
    }
    else
    {
        // Fallback to standard 80x25
        Context->ScreenWidth = 80;
        Context->ScreenHeight = 25;
    }
    
    // Set up default color scheme
    Context->Colors.TitleColor = COLOR_TITLE;
    Context->Colors.NormalColor = COLOR_NORMAL;
    Context->Colors.SelectedColor = COLOR_SELECTED;
    Context->Colors.DisabledColor = COLOR_DISABLED;
    Context->Colors.HiddenColor = COLOR_HIDDEN;
    Context->Colors.DescriptionColor = COLOR_DESCRIPTION;
    Context->Colors.BackgroundColor = COLOR_BACKGROUND;
    Context->Colors.TabActiveColor = COLOR_TAB_ACTIVE;
    Context->Colors.TabInactiveColor = COLOR_TAB_INACTIVE;
    
    // Initialize tab support (disabled by default)
    Context->Tabs = NULL;
    Context->TabCount = 0;
    Context->CurrentTabIndex = 0;
    Context->UseTabMode = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Create a new menu page
 */
MENU_PAGE *MenuCreatePage(CHAR16 *Title, UINTN ItemCount)
{
    if (Title == NULL || ItemCount == 0)
        return NULL;
    
    MENU_PAGE *Page = AllocateZeroPool(sizeof(MENU_PAGE));
    if (Page == NULL)
        return NULL;
    
    Page->Title = AllocateCopyPool(StrSize(Title), Title);
    if (Page->Title == NULL)
    {
        FreePool(Page);
        return NULL;
    }
    
    Page->Items = AllocateZeroPool(sizeof(MENU_ITEM) * ItemCount);
    if (Page->Items == NULL)
    {
        FreePool(Page->Title);
        FreePool(Page);
        return NULL;
    }
    
    Page->ItemCount = ItemCount;
    Page->SelectedIndex = 0;
    Page->Parent = NULL;
    Page->Depth = 0;
    Page->IsRootMenu = FALSE;
    Page->IsSubMenu = FALSE;
    Page->FormId = 0;
    
    LOG_MENU_DEBUG("Created menu page '%s' with %u items", Title, ItemCount);
    
    return Page;
}

/**
 * Add an action item to a menu page
 */
EFI_STATUS MenuAddActionItem(
    MENU_PAGE *Page,
    UINTN Index,
    CHAR16 *Title,
    CHAR16 *Description,
    MENU_ITEM_CALLBACK Callback,
    VOID *Data
)
{
    if (Page == NULL || Index >= Page->ItemCount || Title == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_ITEM *Item = &Page->Items[Index];
    Item->Type = MENU_ITEM_ACTION;
    Item->Title = AllocateCopyPool(StrSize(Title), Title);
    if (Item->Title == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Item->Description = Description ? AllocateCopyPool(StrSize(Description), Description) : NULL;
    Item->Callback = Callback;
    Item->Data = Data;
    Item->Enabled = TRUE;
    Item->Hidden = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Add a submenu item to a menu page
 */
EFI_STATUS MenuAddSubmenuItem(
    MENU_PAGE *Page,
    UINTN Index,
    CHAR16 *Title,
    CHAR16 *Description,
    MENU_PAGE *Submenu
)
{
    if (Page == NULL || Index >= Page->ItemCount || Submenu == NULL || Title == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_ITEM *Item = &Page->Items[Index];
    Item->Type = MENU_ITEM_SUBMENU;
    Item->Title = AllocateCopyPool(StrSize(Title), Title);
    if (Item->Title == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Item->Description = Description ? AllocateCopyPool(StrSize(Description), Description) : NULL;
    Item->Submenu = Submenu;
    Item->Enabled = TRUE;
    Item->Hidden = FALSE;
    
    // Set parent relationship and update depth
    Submenu->Parent = Page;
    Submenu->Depth = Page->Depth + 1;
    Submenu->IsSubMenu = TRUE;
    
    LOG_MENU_DEBUG("Added submenu '%s' to page '%s' (depth %u -> %u)", 
                   Title, Page->Title ? Page->Title : L"(unnamed)", 
                   Page->Depth, Submenu->Depth);
    
    return EFI_SUCCESS;
}

/**
 * Add a separator item
 */
EFI_STATUS MenuAddSeparator(MENU_PAGE *Page, UINTN Index, CHAR16 *Title)
{
    if (Page == NULL || Index >= Page->ItemCount)
        return EFI_INVALID_PARAMETER;
    
    MENU_ITEM *Item = &Page->Items[Index];
    Item->Type = MENU_ITEM_SEPARATOR;
    Item->Title = Title ? AllocateCopyPool(StrSize(Title), Title) : NULL;
    Item->Enabled = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Add an info item
 */
EFI_STATUS MenuAddInfoItem(MENU_PAGE *Page, UINTN Index, CHAR16 *Title)
{
    if (Page == NULL || Index >= Page->ItemCount || Title == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_ITEM *Item = &Page->Items[Index];
    Item->Type = MENU_ITEM_INFO;
    Item->Title = AllocateCopyPool(StrSize(Title), Title);
    if (Item->Title == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Item->Enabled = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Initialize tab mode
 */
EFI_STATUS MenuInitializeTabs(MENU_CONTEXT *Context, UINTN TabCount)
{
    if (Context == NULL || TabCount == 0)
        return EFI_INVALID_PARAMETER;
    
    // Allocate tab array
    Context->Tabs = AllocateZeroPool(sizeof(MENU_TAB) * TabCount);
    if (Context->Tabs == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Context->TabCount = TabCount;
    Context->CurrentTabIndex = 0;
    Context->UseTabMode = TRUE;
    
    return EFI_SUCCESS;
}

/**
 * Add a tab to the menu
 */
EFI_STATUS MenuAddTab(
    MENU_CONTEXT *Context,
    UINTN Index,
    CHAR16 *Name,
    MENU_PAGE *Page
)
{
    if (Context == NULL || Index >= Context->TabCount || Name == NULL || Page == NULL)
        return EFI_INVALID_PARAMETER;
    
    if (!Context->UseTabMode || Context->Tabs == NULL)
        return EFI_NOT_READY;
    
    MENU_TAB *Tab = &Context->Tabs[Index];
    Tab->Name = AllocateCopyPool(StrSize(Name), Name);
    if (Tab->Name == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    Tab->Page = Page;
    Tab->Enabled = TRUE;
    Tab->Tag = Index;
    
    return EFI_SUCCESS;
}

/**
 * Switch to a different tab
 */
EFI_STATUS MenuSwitchTab(MENU_CONTEXT *Context, UINTN TabIndex)
{
    if (Context == NULL || !Context->UseTabMode || Context->Tabs == NULL)
        return EFI_INVALID_PARAMETER;
    
    if (TabIndex >= Context->TabCount)
        return EFI_INVALID_PARAMETER;
    
    if (!Context->Tabs[TabIndex].Enabled)
        return EFI_ACCESS_DENIED;
    
    Context->CurrentTabIndex = TabIndex;
    Context->CurrentPage = Context->Tabs[TabIndex].Page;
    
    // Reset selection to first enabled item
    if (Context->CurrentPage != NULL && Context->CurrentPage->ItemCount > 0)
    {
        Context->CurrentPage->SelectedIndex = 0;
        // Find first enabled item
        for (UINTN i = 0; i < Context->CurrentPage->ItemCount; i++)
        {
            if (Context->CurrentPage->Items[i].Enabled)
            {
                Context->CurrentPage->SelectedIndex = i;
                break;
            }
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * Draw tab bar at the top
 */
STATIC VOID MenuDrawTabs(MENU_CONTEXT *Context)
{
    if (Context == NULL || !Context->UseTabMode || Context->Tabs == NULL)
        return;
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = Context->TextOut;
    
    if (ConOut == NULL)
        return;
    
    // Draw tab bar on line 1 (below title)
    ConOut->SetCursorPosition(ConOut, 0, 1);
    
    UINTN Col = 2;
    for (UINTN i = 0; i < Context->TabCount; i++)
    {
        MENU_TAB *Tab = &Context->Tabs[i];
        
        // NULL safety check for tab and tab name
        if (Tab == NULL || Tab->Name == NULL)
            continue;
        
        // Set color based on active/inactive state
        if (i == Context->CurrentTabIndex)
            ConOut->SetAttribute(ConOut, Context->Colors.TabActiveColor);
        else
            ConOut->SetAttribute(ConOut, Context->Colors.TabInactiveColor);
        
        // Draw tab
        ConOut->SetCursorPosition(ConOut, Col, 1);
        
        if (i == Context->CurrentTabIndex)
            ConOut->OutputString(ConOut, L"[");
        else
            ConOut->OutputString(ConOut, L" ");
        
        ConOut->OutputString(ConOut, Tab->Name);
        
        if (i == Context->CurrentTabIndex)
            ConOut->OutputString(ConOut, L"]");
        else
            ConOut->OutputString(ConOut, L" ");
        
        // Move to next tab position
        Col += StrLen(Tab->Name) + 3;
        
        // Don't overflow screen width
        if (Col >= Context->ScreenWidth - 10)
            break;
    }
    
    // Fill rest of line with normal color
    ConOut->SetAttribute(ConOut, Context->Colors.BackgroundColor);
    for (; Col < Context->ScreenWidth; Col++)
    {
        ConOut->SetCursorPosition(ConOut, Col, 1);
        ConOut->OutputString(ConOut, L" ");
    }
    
    // Draw a separator line below tabs for BIOS-like appearance
    ConOut->SetCursorPosition(ConOut, 0, 2);
    ConOut->SetAttribute(ConOut, Context->Colors.DisabledColor);
    for (UINTN i = 0; i < Context->ScreenWidth; i++)
    {
        ConOut->OutputString(ConOut, L"─");
    }
}

/**
 * Draw the current menu page
 */
VOID MenuDraw(MENU_CONTEXT *Context)
{
    if (Context == NULL || Context->CurrentPage == NULL)
        return;
    
    MENU_PAGE *Page = Context->CurrentPage;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = Context->TextOut;
    
    if (ConOut == NULL)
        return;
    
    // Clear screen
    ConOut->ClearScreen(ConOut);
    ConOut->SetAttribute(ConOut, Context->Colors.BackgroundColor);
    
    // Draw title bar
    ConOut->SetCursorPosition(ConOut, 0, 0);
    ConOut->SetAttribute(ConOut, Context->Colors.TitleColor);
    
    // Initialize title to show
    CHAR16 *TitleToShow = Page->Title ? Page->Title : L"BIOS Setup";
    
    // Build breadcrumb navigation for hierarchical menus
    CHAR16 BreadcrumbText[256];
    if (Page->Parent != NULL && !Context->UseTabMode)
    {
        // Show hierarchical path: Parent > Current
        if (Page->Parent->Title != NULL && TitleToShow != NULL)
        {
            UnicodeSPrint(BreadcrumbText, sizeof(BreadcrumbText),
                         L"%s > %s", Page->Parent->Title, TitleToShow);
            TitleToShow = BreadcrumbText;
        }
    }
    else if (Context->UseTabMode && Context->Tabs != NULL && Context->CurrentTabIndex < Context->TabCount)
    {
        // In tab mode, show: Tab Name > Form Name (when in submenu)
        MENU_TAB *CurrentTab = &Context->Tabs[Context->CurrentTabIndex];
        if (CurrentTab != NULL && CurrentTab->Name != NULL && Page->Parent != NULL)
        {
            UnicodeSPrint(BreadcrumbText, sizeof(BreadcrumbText),
                         L"%s > %s", CurrentTab->Name, TitleToShow);
            TitleToShow = BreadcrumbText;
        }
    }
    
    // Center the title - with NULL check
    UINTN TitleLen = StrLen(TitleToShow);
    UINTN Padding = (Context->ScreenWidth - TitleLen) / 2;
    
    for (UINTN i = 0; i < Context->ScreenWidth; i++)
        ConOut->OutputString(ConOut, L" ");
    
    ConOut->SetCursorPosition(ConOut, Padding, 0);
    ConOut->OutputString(ConOut, TitleToShow);
    
    // Always draw tab bar if tab mode is enabled (even in submenus)
    // This keeps the BIOS-like interface consistent
    if (Context->UseTabMode && Context->Tabs != NULL)
    {
        MenuDrawTabs(Context);
    }
    
    // Draw menu items (start at row 4 if tabs enabled due to separator, row 2 otherwise)
    UINTN Row = Context->UseTabMode ? 4 : 2;
    for (UINTN i = 0; i < Page->ItemCount; i++)
    {
        MENU_ITEM *Item = &Page->Items[i];
        
        // Safety check
        if (Item == NULL)
            continue;
        
        // Clear entire line first with background color to prevent color bleeding
        ConOut->SetAttribute(ConOut, Context->Colors.BackgroundColor);
        ConOut->SetCursorPosition(ConOut, 0, Row);
        for (UINTN Col = 0; Col < Context->ScreenWidth; Col++)
            ConOut->OutputString(ConOut, L" ");
        
        // Now draw the item
        ConOut->SetCursorPosition(ConOut, 2, Row);
        
        // Choose color based on item state
        UINTN Color;
        if (!Item->Enabled)
            Color = Context->Colors.DisabledColor;
        else if (i == Page->SelectedIndex)
            Color = Context->Colors.SelectedColor;
        else if (Item->Hidden)
            Color = Context->Colors.HiddenColor;
        else if (Item->Type == MENU_ITEM_INFO)
            Color = Context->Colors.DisabledColor;  // Info items in gray
        else
            Color = Context->Colors.NormalColor;
        
        ConOut->SetAttribute(ConOut, Color);
        
        // Draw selection indicator (not for INFO items)
        if (Item->Type != MENU_ITEM_INFO)
        {
            if (i == Page->SelectedIndex && Item->Enabled)
                ConOut->OutputString(ConOut, L"> ");
            else
                ConOut->OutputString(ConOut, L"  ");
        }
        else
        {
            // INFO items have no selector, just indent
            ConOut->OutputString(ConOut, L"  ");
        }
        
        // Draw item title with NULL check
        if (Item->Type == MENU_ITEM_SEPARATOR)
        {
            ConOut->SetAttribute(ConOut, Context->Colors.DisabledColor);
            ConOut->OutputString(ConOut, L"─────");
            if (Item->Title != NULL)
            {
                ConOut->OutputString(ConOut, L" ");
                ConOut->OutputString(ConOut, Item->Title);
                ConOut->OutputString(ConOut, L" ");
            }
            ConOut->OutputString(ConOut, L"─────");
        }
        else if (Item->Type == MENU_ITEM_SUBMENU)
        {
            if (Item->Title != NULL)
            {
                ConOut->OutputString(ConOut, Item->Title);
                ConOut->OutputString(ConOut, L" >");
            }
            else
            {
                ConOut->OutputString(ConOut, L"[Submenu] >");
            }
        }
        else if (Item->Type == MENU_ITEM_INFO)
        {
            // Info items are just displayed, not selectable
            if (Item->Title != NULL)
                ConOut->OutputString(ConOut, Item->Title);
        }
        else
        {
            if (Item->Title != NULL)
                ConOut->OutputString(ConOut, Item->Title);
            else
                ConOut->OutputString(ConOut, L"[Item]");
        }
        
        // Reset to background color after each item to prevent bleeding
        ConOut->SetAttribute(ConOut, Context->Colors.BackgroundColor);
        
        Row++;
        
        // Don't exceed screen height
        if (Row >= Context->ScreenHeight - 3)
            break;
    }
    
    // Fill remaining lines with background color to ensure consistency
    ConOut->SetAttribute(ConOut, Context->Colors.BackgroundColor);
    for (; Row < Context->ScreenHeight - 2; Row++)
    {
        ConOut->SetCursorPosition(ConOut, 0, Row);
        for (UINTN Col = 0; Col < Context->ScreenWidth; Col++)
            ConOut->OutputString(ConOut, L" ");
    }
    
    // Draw AMI BIOS-style status bar at bottom
    ConOut->SetAttribute(ConOut, Context->Colors.TitleColor);
    ConOut->SetCursorPosition(ConOut, 0, Context->ScreenHeight - 2);
    
    // Fill entire bottom line with title color background (AMI style)
    for (UINTN i = 0; i < Context->ScreenWidth; i++)
        ConOut->OutputString(ConOut, L" ");
    
    // Show keyboard shortcuts AMI BIOS style
    ConOut->SetCursorPosition(ConOut, 2, Context->ScreenHeight - 2);
    if (Context->UseTabMode)
    {
        ConOut->OutputString(ConOut, L"← →: Select Tab | ↑ ↓: Select Item | Enter: Select | F1: Help | F9: Defaults | F10: Save | ESC: Exit");
    }
    else
    {
        ConOut->OutputString(ConOut, L"↑ ↓: Select Item | Enter: Select | F1: Help | ESC: Exit");
    }
    
    // Draw description of selected item with NULL checks
    if (Page->SelectedIndex < Page->ItemCount)
    {
        MENU_ITEM *SelectedItem = &Page->Items[Page->SelectedIndex];
        if (SelectedItem != NULL && SelectedItem->Description != NULL)
        {
            ConOut->SetCursorPosition(ConOut, 0, Context->ScreenHeight - 1);
            ConOut->SetAttribute(ConOut, Context->Colors.DescriptionColor);
            ConOut->OutputString(ConOut, SelectedItem->Description);
        }
    }
}

/**
 * Find first enabled item in a page
 */
static UINTN FindFirstEnabledItem(MENU_PAGE *Page)
{
    if (Page == NULL || Page->ItemCount == 0)
        return 0;
    
    for (UINTN i = 0; i < Page->ItemCount; i++)
    {
        if (Page->Items[i].Enabled)
            return i;
    }
    
    // No enabled items found, return 0 as fallback
    return 0;
}

/**
 * Find last enabled item in a page
 */
static UINTN FindLastEnabledItem(MENU_PAGE *Page)
{
    if (Page == NULL || Page->ItemCount == 0)
        return 0;
    
    // Search backwards from the end
    for (INTN i = (INTN)(Page->ItemCount - 1); i >= 0; i--)
    {
        if (Page->Items[i].Enabled)
            return (UINTN)i;
    }
    
    // No enabled items found, return 0 as fallback
    return 0;
}

/**
 * Find next selectable item
 */
static UINTN FindNextSelectableItem(MENU_PAGE *Page, UINTN StartIndex, BOOLEAN Forward)
{
    if (Page == NULL || Page->ItemCount == 0)
        return 0;
    
    UINTN Index = StartIndex;
    UINTN Count = 0;
    
    while (Count < Page->ItemCount)
    {
        if (Forward)
        {
            Index = (Index + 1) % Page->ItemCount;
        }
        else
        {
            Index = (Index == 0) ? (Page->ItemCount - 1) : (Index - 1);
        }
        
        if (Page->Items[Index].Enabled)
            return Index;
        
        Count++;
    }
    
    // No enabled items found, return first enabled item or 0
    return FindFirstEnabledItem(Page);
}

/**
 * Handle keyboard input
 */
EFI_STATUS MenuHandleInput(MENU_CONTEXT *Context, EFI_INPUT_KEY *Key)
{
    if (Context == NULL || Context->CurrentPage == NULL || Key == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_PAGE *Page = Context->CurrentPage;
    EFI_STATUS Status;
    
    // Handle arrow keys
    if (Key->ScanCode == SCAN_UP)
    {
        Page->SelectedIndex = FindNextSelectableItem(Page, Page->SelectedIndex, FALSE);
        MenuDraw(Context);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_DOWN)
    {
        Page->SelectedIndex = FindNextSelectableItem(Page, Page->SelectedIndex, TRUE);
        MenuDraw(Context);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_HOME)
    {
        // Home: Jump to first enabled item
        Page->SelectedIndex = FindFirstEnabledItem(Page);
        MenuDraw(Context);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_END)
    {
        // End: Jump to last enabled item
        Page->SelectedIndex = FindLastEnabledItem(Page);
        MenuDraw(Context);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_PAGE_UP)
    {
        // Page Up: Jump up by 10 items or to first item
        UINTN ItemsToSkip = 10;
        UINTN CurrentIndex = Page->SelectedIndex;
        for (UINTN i = 0; i < ItemsToSkip; i++)
        {
            CurrentIndex = FindNextSelectableItem(Page, CurrentIndex, FALSE);
            if (CurrentIndex == FindFirstEnabledItem(Page) && i > 0)
                break;  // Reached the top
        }
        Page->SelectedIndex = CurrentIndex;
        MenuDraw(Context);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_PAGE_DOWN)
    {
        // Page Down: Jump down by 10 items or to last item
        UINTN ItemsToSkip = 10;
        UINTN CurrentIndex = Page->SelectedIndex;
        for (UINTN i = 0; i < ItemsToSkip; i++)
        {
            CurrentIndex = FindNextSelectableItem(Page, CurrentIndex, TRUE);
            if (CurrentIndex == FindLastEnabledItem(Page) && i > 0)
                break;  // Reached the bottom
        }
        Page->SelectedIndex = CurrentIndex;
        MenuDraw(Context);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_LEFT && Context->UseTabMode)
    {
        // Switch to previous tab (stop at first tab, no wrapping)
        if (Context->CurrentTabIndex > 0)
        {
            UINTN NewTabIndex = Context->CurrentTabIndex - 1;
            Status = MenuSwitchTab(Context, NewTabIndex);
            if (!EFI_ERROR(Status))
            {
                MenuDraw(Context);
            }
        }
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_RIGHT && Context->UseTabMode)
    {
        // Switch to next tab (stop at last tab, no wrapping)
        if (Context->CurrentTabIndex + 1 < Context->TabCount)
        {
            UINTN NewTabIndex = Context->CurrentTabIndex + 1;
            Status = MenuSwitchTab(Context, NewTabIndex);
            if (!EFI_ERROR(Status))
            {
                MenuDraw(Context);
            }
        }
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_ESC)
    {
        // Go back to parent menu
        return MenuGoBack(Context);
    }
    
    // Handle AMI BIOS function keys
    if (Key->ScanCode == SCAN_F1)
    {
        // F1: Help (Context-sensitive help)
        CHAR16 HelpText[256];
        UnicodeSPrint(HelpText, sizeof(HelpText), 
            L"Navigation: ↑↓=Select  ←→=Tabs  Home/End  PgUp/PgDn\r\n"
            L"Actions: Enter=Modify  F9=Defaults  F10=Save  ESC=Exit");
        MenuShowMessage(Context, L"Help", HelpText);
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_F9)
    {
        // F9: Load Setup Defaults / Optimized Defaults
        MenuShowMessage(Context, L"Load Defaults", 
            L"This feature is not yet implemented.\r\n"
            L"Future versions will restore all settings to defaults.");
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_F10)
    {
        // F10: Save and Exit
        if (Context->UserData != NULL)
        {
            HII_BROWSER_CONTEXT *HiiCtx = (HII_BROWSER_CONTEXT *)Context->UserData;
            HiiBrowserShowSaveDialog(HiiCtx);
        }
        else
        {
            MenuShowMessage(Context, L"Save & Exit", 
                L"Configuration will be saved on exit.\r\n"
                L"Press ESC again to exit the application.");
        }
        return EFI_SUCCESS;
    }
    else if (Key->ScanCode == SCAN_F5 || Key->ScanCode == SCAN_F6)
    {
        // F5/F6: Change values (reserved for future item value modification)
        return EFI_SUCCESS;
    }
    
    // Handle Enter key
    if (Key->UnicodeChar == CHAR_CARRIAGE_RETURN)
    {
        // Validate bounds before accessing
        if (Page->SelectedIndex >= Page->ItemCount)
        {
            // Invalid selection, reset to first enabled item
            Page->SelectedIndex = FindFirstEnabledItem(Page);
            MenuDraw(Context);
            return EFI_SUCCESS;
        }
        
        MENU_ITEM *Item = &Page->Items[Page->SelectedIndex];
        
        if (Item == NULL || !Item->Enabled)
            return EFI_SUCCESS;
        
        if (Item->Type == MENU_ITEM_SUBMENU && Item->Submenu != NULL)
        {
            return MenuNavigateTo(Context, Item->Submenu);
        }
        else if (Item->Type == MENU_ITEM_ACTION && Item->Callback != NULL)
        {
            return Item->Callback(Item, Context);
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * Run the menu system (main loop)
 */
EFI_STATUS MenuRun(MENU_CONTEXT *Context, MENU_PAGE *StartPage)
{
    if (Context == NULL)
        return EFI_INVALID_PARAMETER;
    
    // For tab mode, CurrentPage is already set by MenuSwitchTab
    // For regular mode, we need StartPage
    if (!Context->UseTabMode)
    {
        if (StartPage == NULL)
            return EFI_INVALID_PARAMETER;
        
        Context->CurrentPage = StartPage;
        Context->RootPage = StartPage;
        
        // Ensure we start with a valid enabled item
        if (StartPage->ItemCount > 0)
        {
            StartPage->SelectedIndex = FindFirstEnabledItem(StartPage);
        }
    }
    
    Context->Running = TRUE;
    
    // Initial draw
    MenuDraw(Context);
    
    // Main event loop
    while (Context->Running)
    {
        EFI_INPUT_KEY Key;
        EFI_STATUS Status;
        
        // Wait for key press
        UINTN Index;
        gBS->WaitForEvent(1, &Context->TextIn->WaitForKey, &Index);
        
        Status = Context->TextIn->ReadKeyStroke(Context->TextIn, &Key);
        if (EFI_ERROR(Status))
            continue;
        
        // Handle the key
        MenuHandleInput(Context, &Key);
    }
    
    return EFI_SUCCESS;
}

/**
 * Navigate to a page
 */
EFI_STATUS MenuNavigateTo(MENU_CONTEXT *Context, MENU_PAGE *Page)
{
    if (Context == NULL || Page == NULL)
        return EFI_INVALID_PARAMETER;
    
    CHAR16 *FromTitle = Context->CurrentPage ? Context->CurrentPage->Title : L"(none)";
    CHAR16 *ToTitle = Page->Title ? Page->Title : L"(unnamed)";
    
    LOG_MENU_DEBUG("Navigating from '%s' (depth %u) to '%s' (depth %u)", 
                   FromTitle, 
                   Context->CurrentPage ? Context->CurrentPage->Depth : 0,
                   ToTitle, 
                   Page->Depth);
    
    Context->CurrentPage = Page;
    
    // Ensure we select a valid enabled item
    if (Page->ItemCount > 0)
    {
        Page->SelectedIndex = FindFirstEnabledItem(Page);
    }
    
    LogNavigation(FromTitle, ToTitle, Page->Depth);
    MenuDraw(Context);
    
    return EFI_SUCCESS;
}

/**
 * Go back to parent page
 */
EFI_STATUS MenuGoBack(MENU_CONTEXT *Context)
{
    if (Context == NULL)
        return EFI_INVALID_PARAMETER;
    
    // In tab mode with submenus open
    if (Context->UseTabMode && Context->CurrentPage != NULL && Context->CurrentPage->Parent != NULL)
    {
        // If current page has a parent AND is not a root menu, go back to it
        // This allows navigation: Tab Root -> Submenu -> Sub-submenu with proper back navigation
        if (!Context->CurrentPage->Parent->IsRootMenu || Context->CurrentPage->Depth > 1)
        {
            CHAR16 *FromTitle = Context->CurrentPage->Title ? Context->CurrentPage->Title : L"(unnamed)";
            CHAR16 *ToTitle = Context->CurrentPage->Parent->Title ? Context->CurrentPage->Parent->Title : L"(unnamed)";
            
            LOG_MENU_DEBUG("Tab mode back navigation: '%s' (depth %u) -> '%s' (depth %u)", 
                           FromTitle, Context->CurrentPage->Depth,
                           ToTitle, Context->CurrentPage->Parent->Depth);
            
            Context->CurrentPage = Context->CurrentPage->Parent;
            
            // Ensure we select a valid enabled item
            if (Context->CurrentPage->ItemCount > 0)
            {
                Context->CurrentPage->SelectedIndex = FindFirstEnabledItem(Context->CurrentPage);
            }
            
            LogNavigation(FromTitle, ToTitle, Context->CurrentPage->Depth);
            MenuDraw(Context);
            return EFI_SUCCESS;
        }
        else
        {
            // We're at a root tab menu, don't go back further, just stay here
            LOG_MENU_DEBUG("At root tab level, not going back");
            return EFI_SUCCESS;
        }
    }
    
    // In non-tab mode or at root level
    if (Context->CurrentPage == Context->RootPage || Context->CurrentPage->Parent == NULL)
    {
        // At root level, exit menu system
        LOG_MENU_INFO("At root menu, exiting menu system");
        Context->Running = FALSE;
        return EFI_SUCCESS;
    }
    
    if (Context->CurrentPage->Parent != NULL)
    {
        CHAR16 *FromTitle = Context->CurrentPage->Title ? Context->CurrentPage->Title : L"(unnamed)";
        CHAR16 *ToTitle = Context->CurrentPage->Parent->Title ? Context->CurrentPage->Parent->Title : L"(unnamed)";
        
        LOG_MENU_DEBUG("Standard back navigation: '%s' (depth %u) -> '%s' (depth %u)", 
                       FromTitle, Context->CurrentPage->Depth,
                       ToTitle, Context->CurrentPage->Parent->Depth);
        
        Context->CurrentPage = Context->CurrentPage->Parent;
        
        // Ensure we select a valid enabled item
        if (Context->CurrentPage->ItemCount > 0)
        {
            Context->CurrentPage->SelectedIndex = FindFirstEnabledItem(Context->CurrentPage);
        }
        
        LogNavigation(FromTitle, ToTitle, Context->CurrentPage->Depth);
        MenuDraw(Context);
    }
    
    return EFI_SUCCESS;
}

/**
 * Show a message box with OK button
 */
EFI_STATUS MenuShowMessage(MENU_CONTEXT *Context, CHAR16 *Title, CHAR16 *Message)
{
    if (Context == NULL || Message == NULL)
        return EFI_INVALID_PARAMETER;
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = Context->TextOut;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn = Context->TextIn;
    
    // Draw message box
    ConOut->SetAttribute(ConOut, Context->Colors.NormalColor);
    ConOut->SetCursorPosition(ConOut, 10, 10);
    ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
    
    if (Title)
    {
        ConOut->SetCursorPosition(ConOut, 10, 11);
        ConOut->OutputString(ConOut, L"| ");
        ConOut->SetAttribute(ConOut, Context->Colors.TitleColor);
        // Truncate title if too long (max 46 chars to fit in box)
        CHAR16 SafeTitle[48];
        UINTN TitleLen = StrLen(Title);
        if (TitleLen > 46)
        {
            StrnCpyS(SafeTitle, 48, Title, 46);
        }
        else
        {
            StrCpyS(SafeTitle, 48, Title);
        }
        ConOut->OutputString(ConOut, SafeTitle);
        ConOut->SetAttribute(ConOut, Context->Colors.NormalColor);
    }
    
    // Handle multi-line messages (split by \r\n)
    CHAR16 *MessageCopy = AllocateCopyPool(StrSize(Message), Message);
    if (MessageCopy != NULL)
    {
        CHAR16 *Line = MessageCopy;
        CHAR16 *NextLine;
        UINTN Row = 12;
        UINTN MaxRows = 5;  // Maximum message lines to display
        
        while (Line != NULL && Row < 12 + MaxRows)
        {
            // Find next line break
            NextLine = StrStr(Line, L"\r\n");
            if (NextLine != NULL)
            {
                *NextLine = L'\0';
                NextLine += 2;  // Skip \r\n
            }
            
            ConOut->SetCursorPosition(ConOut, 10, Row);
            ConOut->OutputString(ConOut, L"|                                                  |");
            ConOut->SetCursorPosition(ConOut, 12, Row);
            
            // Truncate line if too long (max 46 chars)
            CHAR16 SafeLine[48];
            UINTN LineLen = StrLen(Line);
            if (LineLen > 46)
            {
                StrnCpyS(SafeLine, 48, Line, 46);
            }
            else
            {
                StrCpyS(SafeLine, 48, Line);
            }
            ConOut->OutputString(ConOut, SafeLine);
            
            Row++;
            Line = NextLine;
        }
        
        FreePool(MessageCopy);
        
        // Fill remaining rows
        while (Row < 12 + MaxRows)
        {
            ConOut->SetCursorPosition(ConOut, 10, Row);
            ConOut->OutputString(ConOut, L"|                                                  |");
            Row++;
        }
    }
    else
    {
        // Fallback: single line message if allocation fails
        ConOut->SetCursorPosition(ConOut, 10, 12);
        ConOut->OutputString(ConOut, L"|                                                  |");
        ConOut->SetCursorPosition(ConOut, 12, 12);
        ConOut->OutputString(ConOut, Message);
        
        for (UINTN Row = 13; Row < 17; Row++)
        {
            ConOut->SetCursorPosition(ConOut, 10, Row);
            ConOut->OutputString(ConOut, L"|                                                  |");
        }
    }
    
    ConOut->SetCursorPosition(ConOut, 10, 17);
    ConOut->OutputString(ConOut, L"|              Press any key to continue           |");
    ConOut->SetCursorPosition(ConOut, 10, 18);
    ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
    
    // Wait for key
    UINTN Index;
    gBS->WaitForEvent(1, &ConIn->WaitForKey, &Index);
    
    EFI_INPUT_KEY Key;
    ConIn->ReadKeyStroke(ConIn, &Key);
    
    // Redraw menu
    MenuDraw(Context);
    
    return EFI_SUCCESS;
}

/**
 * Show a yes/no confirmation dialog
 */
EFI_STATUS MenuShowConfirm(MENU_CONTEXT *Context, CHAR16 *Title, CHAR16 *Message, BOOLEAN *Result)
{
    if (Context == NULL || Message == NULL || Result == NULL)
        return EFI_INVALID_PARAMETER;
    
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = Context->TextOut;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn = Context->TextIn;
    
    // Draw confirmation box
    ConOut->SetAttribute(ConOut, Context->Colors.NormalColor);
    ConOut->SetCursorPosition(ConOut, 10, 10);
    ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
    
    if (Title)
    {
        ConOut->SetCursorPosition(ConOut, 10, 11);
        ConOut->OutputString(ConOut, L"| ");
        ConOut->SetAttribute(ConOut, Context->Colors.TitleColor);
        ConOut->OutputString(ConOut, Title);
        ConOut->SetAttribute(ConOut, Context->Colors.NormalColor);
    }
    
    ConOut->SetCursorPosition(ConOut, 10, 12);
    ConOut->OutputString(ConOut, L"|                                                  |");
    ConOut->SetCursorPosition(ConOut, 12, 12);
    ConOut->OutputString(ConOut, Message);
    
    ConOut->SetCursorPosition(ConOut, 10, 13);
    ConOut->OutputString(ConOut, L"|                                                  |");
    ConOut->SetCursorPosition(ConOut, 10, 14);
    ConOut->OutputString(ConOut, L"|         Press Y for Yes, N for No               |");
    ConOut->SetCursorPosition(ConOut, 10, 15);
    ConOut->OutputString(ConOut, L"+--------------------------------------------------+");
    
    // Wait for Y or N
    while (TRUE)
    {
        UINTN Index;
        gBS->WaitForEvent(1, &ConIn->WaitForKey, &Index);
        
        EFI_INPUT_KEY Key;
        ConIn->ReadKeyStroke(ConIn, &Key);
        
        if (Key.UnicodeChar == L'Y' || Key.UnicodeChar == L'y')
        {
            *Result = TRUE;
            break;
        }
        else if (Key.UnicodeChar == L'N' || Key.UnicodeChar == L'n')
        {
            *Result = FALSE;
            break;
        }
    }
    
    // Redraw menu
    MenuDraw(Context);
    
    return EFI_SUCCESS;
}

/**
 * Free a menu page and its items
 */
VOID MenuFreePage(MENU_PAGE *Page)
{
    if (Page == NULL)
        return;
    
    if (Page->Title)
        FreePool(Page->Title);
    
    if (Page->Items)
    {
        for (UINTN i = 0; i < Page->ItemCount; i++)
        {
            MENU_ITEM *Item = &Page->Items[i];
            if (Item->Title)
                FreePool(Item->Title);
            if (Item->Description)
                FreePool(Item->Description);
        }
        FreePool(Page->Items);
    }
    
    FreePool(Page);
}

/**
 * Clean up menu resources
 */
VOID MenuCleanup(MENU_CONTEXT *Context)
{
    if (Context == NULL)
        return;
    
    // Reset console
    Context->TextOut->ClearScreen(Context->TextOut);
    Context->TextOut->SetAttribute(Context->TextOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
}
