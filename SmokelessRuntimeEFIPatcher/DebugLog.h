#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>

// Log levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4
} LOG_LEVEL;

// Global log level (can be configured at runtime)
extern LOG_LEVEL gCurrentLogLevel;

// Enable/disable logging globally
extern BOOLEAN gLoggingEnabled;

/**
 * Initialize logging system
 */
VOID LogInitialize(LOG_LEVEL DefaultLevel);

/**
 * Set log level
 */
VOID LogSetLevel(LOG_LEVEL Level);

/**
 * Enable or disable logging
 */
VOID LogSetEnabled(BOOLEAN Enabled);

/**
 * Core logging function
 */
VOID LogPrint(LOG_LEVEL Level, CONST CHAR8 *Module, CONST CHAR8 *Format, ...);

// Convenience macros for logging
#define LOG_DEBUG(Module, ...) LogPrint(LOG_LEVEL_DEBUG, Module, __VA_ARGS__)
#define LOG_INFO(Module, ...) LogPrint(LOG_LEVEL_INFO, Module, __VA_ARGS__)
#define LOG_WARN(Module, ...) LogPrint(LOG_LEVEL_WARN, Module, __VA_ARGS__)
#define LOG_ERROR(Module, ...) LogPrint(LOG_LEVEL_ERROR, Module, __VA_ARGS__)

// Module-specific logging macros
#define LOG_MENU_DEBUG(...) LOG_DEBUG("MENU", __VA_ARGS__)
#define LOG_MENU_INFO(...) LOG_INFO("MENU", __VA_ARGS__)
#define LOG_MENU_WARN(...) LOG_WARN("MENU", __VA_ARGS__)
#define LOG_MENU_ERROR(...) LOG_ERROR("MENU", __VA_ARGS__)

#define LOG_HII_DEBUG(...) LOG_DEBUG("HII", __VA_ARGS__)
#define LOG_HII_INFO(...) LOG_INFO("HII", __VA_ARGS__)
#define LOG_HII_WARN(...) LOG_WARN("HII", __VA_ARGS__)
#define LOG_HII_ERROR(...) LOG_ERROR("HII", __VA_ARGS__)

#define LOG_NVRAM_DEBUG(...) LOG_DEBUG("NVRAM", __VA_ARGS__)
#define LOG_NVRAM_INFO(...) LOG_INFO("NVRAM", __VA_ARGS__)
#define LOG_NVRAM_WARN(...) LOG_WARN("NVRAM", __VA_ARGS__)
#define LOG_NVRAM_ERROR(...) LOG_ERROR("NVRAM", __VA_ARGS__)

#define LOG_CONFIG_DEBUG(...) LOG_DEBUG("CONFIG", __VA_ARGS__)
#define LOG_CONFIG_INFO(...) LOG_INFO("CONFIG", __VA_ARGS__)
#define LOG_CONFIG_WARN(...) LOG_WARN("CONFIG", __VA_ARGS__)
#define LOG_CONFIG_ERROR(...) LOG_ERROR("CONFIG", __VA_ARGS__)

#define LOG_IFR_DEBUG(...) LOG_DEBUG("IFR", __VA_ARGS__)
#define LOG_IFR_INFO(...) LOG_INFO("IFR", __VA_ARGS__)
#define LOG_IFR_WARN(...) LOG_WARN("IFR", __VA_ARGS__)
#define LOG_IFR_ERROR(...) LOG_ERROR("IFR", __VA_ARGS__)

/**
 * Log navigation event with context
 */
VOID LogNavigation(CONST CHAR16 *FromPage, CONST CHAR16 *ToPage, UINTN Depth);

/**
 * Log question edit event
 */
VOID LogQuestionEdit(UINT16 QuestionId, CONST CHAR16 *Prompt, UINT8 Type, UINT64 OldValue, UINT64 NewValue);

/**
 * Log NVRAM operation
 */
VOID LogNvramOperation(CONST CHAR8 *Operation, CONST CHAR16 *VariableName, EFI_STATUS Status);

/**
 * Log form parsing event
 */
VOID LogFormParsing(CONST CHAR16 *FormTitle, UINT16 FormId, UINTN QuestionCount);

/**
 * Dump buffer in hex format for debugging
 */
VOID LogDumpBuffer(LOG_LEVEL Level, CONST CHAR8 *Module, CONST CHAR8 *Description, CONST VOID *Buffer, UINTN Size);
