#pragma once
#include <Uefi.h>

// Application version
#define SREP_VERSION_STRING L"0.3.1"

// Buffer sizes
#define LOG_BUFFER_SIZE         512
#define MAX_TITLE_LENGTH        128
#define MAX_DESCRIPTION_LENGTH  256
#define MAX_STRING_INPUT_LENGTH 256
#define MAX_VARIABLE_NAME_LENGTH 64

// Initial capacities
#define INITIAL_NVRAM_CAPACITY  100
#define INITIAL_FORM_CAPACITY   50
#define INITIAL_QUESTION_CAPACITY 20

// Screen dimensions
#define DEFAULT_SCREEN_WIDTH    80
#define DEFAULT_SCREEN_HEIGHT   25

// Menu limits
#define MAX_TABS                10
#define MAX_MENU_ITEMS          100

// File names
#define AUTO_MODE_FLAG_FILE     L"SREP_Auto.flag"
#define INTERACTIVE_FLAG_FILE   L"SREP_Interactive.flag"
#define BIOS_TAB_FLAG_FILE      L"SREP_BiosTab.flag"
#define LOG_FILE_NAME           L"SREP.log"

// Common string lengths
#define SMALL_BUFFER_SIZE       64
#define MEDIUM_BUFFER_SIZE      128
#define LARGE_BUFFER_SIZE       512

// Question types (from UEFI spec)
#define QUESTION_TYPE_CHECKBOX  0x03
#define QUESTION_TYPE_NUMERIC   0x07
#define QUESTION_TYPE_STRING    0x08
#define QUESTION_TYPE_ONE_OF    0x05
#define QUESTION_TYPE_ORDERED_LIST 0x0B

// Error messages
#define ERROR_MSG_INIT_FAILED   L"Initialization failed"
#define ERROR_MSG_OUT_OF_MEMORY L"Out of memory"
#define ERROR_MSG_INVALID_PARAM L"Invalid parameter"
#define ERROR_MSG_NOT_FOUND     L"Resource not found"

// Success messages  
#define SUCCESS_MSG_COMPLETE    L"Operation completed successfully"
#define SUCCESS_MSG_SAVED       L"Changes saved to NVRAM"

// Tab category keywords
#define KEYWORD_MAIN            L"MAIN"
#define KEYWORD_SYSTEM          L"SYSTEM"
#define KEYWORD_INFO            L"INFO"
#define KEYWORD_ADVANCED        L"ADVANCED"
#define KEYWORD_CPU             L"CPU"
#define KEYWORD_CHIPSET         L"CHIPSET"
#define KEYWORD_PERIPHERAL      L"PERIPHERAL"
#define KEYWORD_POWER           L"POWER"
#define KEYWORD_ACPI            L"ACPI"
#define KEYWORD_THERMAL         L"THERMAL"
#define KEYWORD_BOOT            L"BOOT"
#define KEYWORD_STARTUP         L"STARTUP"
#define KEYWORD_SECURITY        L"SECURITY"
#define KEYWORD_PASSWORD        L"PASSWORD"
#define KEYWORD_TPM             L"TPM"
#define KEYWORD_SECURE          L"SECURE"
#define KEYWORD_EXIT            L"EXIT"
#define KEYWORD_SAVE            L"SAVE"

// Tab indices
#define TAB_INDEX_MAIN          0
#define TAB_INDEX_ADVANCED      1
#define TAB_INDEX_POWER         2
#define TAB_INDEX_BOOT          3
#define TAB_INDEX_SECURITY      4
#define TAB_INDEX_SAVE_EXIT     5
#define TAB_COUNT               6

// Return codes
#define SREP_SUCCESS            EFI_SUCCESS
#define SREP_ERROR              EFI_DEVICE_ERROR
#define SREP_OUT_OF_RESOURCES   EFI_OUT_OF_RESOURCES
#define SREP_INVALID_PARAMETER  EFI_INVALID_PARAMETER
#define SREP_NOT_FOUND          EFI_NOT_FOUND
