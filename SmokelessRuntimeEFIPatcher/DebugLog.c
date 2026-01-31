#include "DebugLog.h"
#include <Library/BaseMemoryLib.h>

// Global log configuration
LOG_LEVEL gCurrentLogLevel = LOG_LEVEL_INFO;
BOOLEAN gLoggingEnabled = TRUE;

// Log level strings
STATIC CONST CHAR8 *LogLevelStrings[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR"
};

/**
 * Initialize logging system
 */
VOID LogInitialize(LOG_LEVEL DefaultLevel)
{
    gCurrentLogLevel = DefaultLevel;
    gLoggingEnabled = TRUE;
    Print(L"\n=== Logging System Initialized (Level: %a) ===\n", LogLevelStrings[DefaultLevel]);
}

/**
 * Set log level
 */
VOID LogSetLevel(LOG_LEVEL Level)
{
    if (Level <= LOG_LEVEL_NONE)
    {
        gCurrentLogLevel = Level;
    }
}

/**
 * Enable or disable logging
 */
VOID LogSetEnabled(BOOLEAN Enabled)
{
    gLoggingEnabled = Enabled;
}

/**
 * Core logging function
 * NOTE: Format string should use %s for CHAR16* strings
 * This function will handle the conversion properly
 */
VOID LogPrint(LOG_LEVEL Level, CONST CHAR8 *Module, CONST CHAR8 *Format, ...)
{
    VA_LIST Marker;
    CHAR16 FormatBuffer[512];
    CHAR16 OutputBuffer[512];
    UINTN FormatLen;
    
    // Check if logging is enabled and level is appropriate
    if (!gLoggingEnabled || Level < gCurrentLogLevel)
        return;
    
    // Convert ASCII format string to Unicode
    FormatLen = AsciiStrLen(Format);
    for (UINTN i = 0; i < FormatLen && i < 511; i++)
    {
        FormatBuffer[i] = (CHAR16)Format[i];
    }
    FormatBuffer[FormatLen < 511 ? FormatLen : 511] = L'\0';
    
    // Format the output with Unicode format string
    VA_START(Marker, Format);
    UnicodeVSPrint(OutputBuffer, sizeof(OutputBuffer), FormatBuffer, Marker);
    VA_END(Marker);
    
    // Print with level and module prefix
    if (Module != NULL)
    {
        Print(L"[%a][%a] %s\n",
              Level < LOG_LEVEL_NONE ? LogLevelStrings[Level] : "UNKNOWN",
              Module,
              OutputBuffer);
    }
    else
    {
        Print(L"[%a] %s\n",
              Level < LOG_LEVEL_NONE ? LogLevelStrings[Level] : "UNKNOWN",
              OutputBuffer);
    }
}

/**
 * Log navigation event with context
 */
VOID LogNavigation(CONST CHAR16 *FromPage, CONST CHAR16 *ToPage, UINTN Depth)
{
    if (!gLoggingEnabled || LOG_LEVEL_DEBUG < gCurrentLogLevel)
        return;
    
    Print(L"[DEBUG][NAV] Navigation: '%s' -> '%s' (Depth: %u)\n", 
          FromPage ? FromPage : L"(null)", 
          ToPage ? ToPage : L"(null)", 
          Depth);
}

/**
 * Log question edit event
 */
VOID LogQuestionEdit(UINT16 QuestionId, CONST CHAR16 *Prompt, UINT8 Type, UINT64 OldValue, UINT64 NewValue)
{
    if (!gLoggingEnabled || LOG_LEVEL_INFO < gCurrentLogLevel)
        return;
    
    Print(L"[INFO ][EDIT] Question #%u '%s' Type=%u: %lu -> %lu\n", 
          QuestionId, 
          Prompt ? Prompt : L"(unnamed)", 
          Type, 
          OldValue, 
          NewValue);
}

/**
 * Log NVRAM operation
 */
VOID LogNvramOperation(CONST CHAR8 *Operation, CONST CHAR16 *VariableName, EFI_STATUS Status)
{
    if (!gLoggingEnabled || LOG_LEVEL_INFO < gCurrentLogLevel)
        return;
    
    Print(L"[INFO ][NVRAM] %a '%s': %r\n", 
          Operation ? Operation : "Operation", 
          VariableName ? VariableName : L"(null)", 
          Status);
}

/**
 * Log form parsing event
 */
VOID LogFormParsing(CONST CHAR16 *FormTitle, UINT16 FormId, UINTN QuestionCount)
{
    if (!gLoggingEnabled || LOG_LEVEL_DEBUG < gCurrentLogLevel)
        return;
    
    Print(L"[DEBUG][PARSE] Form '%s' (ID=%u): %u questions\n", 
          FormTitle ? FormTitle : L"(unnamed)", 
          FormId, 
          QuestionCount);
}

/**
 * Dump buffer in hex format for debugging
 */
VOID LogDumpBuffer(LOG_LEVEL Level, CONST CHAR8 *Module, CONST CHAR8 *Description, CONST VOID *Buffer, UINTN Size)
{
    UINTN i;
    CONST UINT8 *Data;
    
    if (!gLoggingEnabled || Level < gCurrentLogLevel || Buffer == NULL || Size == 0)
        return;
    
    Data = (CONST UINT8 *)Buffer;
    
    // Use %a for ASCII strings (CHAR8*), not casting to CHAR16*
    Print(L"[%a][%a] %a (%u bytes):\n", 
          Level < LOG_LEVEL_NONE ? LogLevelStrings[Level] : "UNKNOWN",
          Module ? Module : "",
          Description ? Description : "Buffer",
          Size);
    
    for (i = 0; i < Size; i++)
    {
        if (i % 16 == 0)
        {
            if (i > 0)
                Print(L"\n");
            Print(L"  %04x: ", i);
        }
        Print(L"%02x ", Data[i]);
    }
    Print(L"\n");
}
