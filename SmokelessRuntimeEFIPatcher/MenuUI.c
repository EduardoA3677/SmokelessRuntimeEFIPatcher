#include "MenuUI.h"
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

// Default color scheme (EFI colors)
#define COLOR_TITLE         (EFI_YELLOW | EFI_BACKGROUND_BLUE)
#define COLOR_NORMAL        (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define COLOR_SELECTED      (EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY)
#define COLOR_DISABLED      (EFI_DARKGRAY | EFI_BACKGROUND_BLACK)
#define COLOR_HIDDEN        (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)
#define COLOR_DESCRIPTION   (EFI_CYAN | EFI_BACKGROUND_BLACK)
#define COLOR_BACKGROUND    (EFI_BLACK | EFI_BACKGROUND_BLACK)

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
    
    // Get screen dimensions
    Context->ScreenWidth = gST->ConOut->Mode->MaxMode > 0 ? 80 : gST->ConOut->Mode->MaxMode;
    Context->ScreenHeight = 25;
    
    // Set up default color scheme
    Context->Colors.TitleColor = COLOR_TITLE;
    Context->Colors.NormalColor = COLOR_NORMAL;
    Context->Colors.SelectedColor = COLOR_SELECTED;
    Context->Colors.DisabledColor = COLOR_DISABLED;
    Context->Colors.HiddenColor = COLOR_HIDDEN;
    Context->Colors.DescriptionColor = COLOR_DESCRIPTION;
    Context->Colors.BackgroundColor = COLOR_BACKGROUND;
    
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
    
    Submenu->Parent = Page;
    
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
 * Draw the current menu page
 */
VOID MenuDraw(MENU_CONTEXT *Context)
{
    if (Context == NULL || Context->CurrentPage == NULL)
        return;
    
    MENU_PAGE *Page = Context->CurrentPage;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = Context->TextOut;
    
    // Clear screen
    ConOut->ClearScreen(ConOut);
    ConOut->SetAttribute(ConOut, Context->Colors.BackgroundColor);
    
    // Draw title bar
    ConOut->SetCursorPosition(ConOut, 0, 0);
    ConOut->SetAttribute(ConOut, Context->Colors.TitleColor);
    
    // Center the title
    UINTN TitleLen = StrLen(Page->Title);
    UINTN Padding = (Context->ScreenWidth - TitleLen) / 2;
    
    for (UINTN i = 0; i < Context->ScreenWidth; i++)
        ConOut->OutputString(ConOut, L" ");
    
    ConOut->SetCursorPosition(ConOut, Padding, 0);
    ConOut->OutputString(ConOut, Page->Title);
    
    // Draw menu items
    UINTN Row = 2;
    for (UINTN i = 0; i < Page->ItemCount; i++)
    {
        MENU_ITEM *Item = &Page->Items[i];
        
        ConOut->SetCursorPosition(ConOut, 2, Row);
        
        // Choose color based on item state
        UINTN Color;
        if (!Item->Enabled)
            Color = Context->Colors.DisabledColor;
        else if (i == Page->SelectedIndex)
            Color = Context->Colors.SelectedColor;
        else if (Item->Hidden)
            Color = Context->Colors.HiddenColor;
        else
            Color = Context->Colors.NormalColor;
        
        ConOut->SetAttribute(ConOut, Color);
        
        // Draw selection indicator
        if (i == Page->SelectedIndex && Item->Enabled)
            ConOut->OutputString(ConOut, L"> ");
        else
            ConOut->OutputString(ConOut, L"  ");
        
        // Draw item title
        if (Item->Type == MENU_ITEM_SEPARATOR)
        {
            ConOut->SetAttribute(ConOut, Context->Colors.DisabledColor);
            ConOut->OutputString(ConOut, L"---");
            if (Item->Title)
            {
                ConOut->OutputString(ConOut, L" ");
                ConOut->OutputString(ConOut, Item->Title);
                ConOut->OutputString(ConOut, L" ");
            }
            ConOut->OutputString(ConOut, L"---");
        }
        else if (Item->Type == MENU_ITEM_SUBMENU)
        {
            ConOut->OutputString(ConOut, Item->Title);
            ConOut->OutputString(ConOut, L" >");
        }
        else
        {
            ConOut->OutputString(ConOut, Item->Title);
        }
        
        Row++;
        
        // Don't exceed screen height
        if (Row >= Context->ScreenHeight - 3)
            break;
    }
    
    // Draw help bar at bottom
    ConOut->SetAttribute(ConOut, Context->Colors.DescriptionColor);
    ConOut->SetCursorPosition(ConOut, 0, Context->ScreenHeight - 2);
    ConOut->OutputString(ConOut, L"Use Arrow Keys to navigate | Enter to select | ESC to go back");
    
    // Draw description of selected item
    if (Page->SelectedIndex < Page->ItemCount)
    {
        MENU_ITEM *SelectedItem = &Page->Items[Page->SelectedIndex];
        if (SelectedItem->Description)
        {
            ConOut->SetCursorPosition(ConOut, 0, Context->ScreenHeight - 1);
            ConOut->SetAttribute(ConOut, Context->Colors.DescriptionColor);
            ConOut->OutputString(ConOut, SelectedItem->Description);
        }
    }
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
    
    return StartIndex;
}

/**
 * Handle keyboard input
 */
EFI_STATUS MenuHandleInput(MENU_CONTEXT *Context, EFI_INPUT_KEY *Key)
{
    if (Context == NULL || Context->CurrentPage == NULL || Key == NULL)
        return EFI_INVALID_PARAMETER;
    
    MENU_PAGE *Page = Context->CurrentPage;
    
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
    else if (Key->ScanCode == SCAN_ESC)
    {
        // Go back to parent menu
        return MenuGoBack(Context);
    }
    
    // Handle Enter key
    if (Key->UnicodeChar == CHAR_CARRIAGE_RETURN)
    {
        if (Page->SelectedIndex >= Page->ItemCount)
            return EFI_SUCCESS;
        
        MENU_ITEM *Item = &Page->Items[Page->SelectedIndex];
        
        if (!Item->Enabled)
            return EFI_SUCCESS;
        
        if (Item->Type == MENU_ITEM_SUBMENU && Item->Submenu)
        {
            return MenuNavigateTo(Context, Item->Submenu);
        }
        else if (Item->Type == MENU_ITEM_ACTION && Item->Callback)
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
    if (Context == NULL || StartPage == NULL)
        return EFI_INVALID_PARAMETER;
    
    Context->CurrentPage = StartPage;
    Context->RootPage = StartPage;
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
    
    Context->CurrentPage = Page;
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
    
    if (Context->CurrentPage == Context->RootPage)
    {
        // At root level, exit menu system
        Context->Running = FALSE;
        return EFI_SUCCESS;
    }
    
    if (Context->CurrentPage->Parent)
    {
        Context->CurrentPage = Context->CurrentPage->Parent;
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
    ConOut->OutputString(ConOut, L"|              Press any key to continue           |");
    ConOut->SetCursorPosition(ConOut, 10, 15);
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
