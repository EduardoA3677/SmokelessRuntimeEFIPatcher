#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

// IFR OpCodes (subset relevant for menu hiding)
#define EFI_IFR_FORM_OP                     0x01
#define EFI_IFR_SUBTITLE_OP                 0x02
#define EFI_IFR_TEXT_OP                     0x03
#define EFI_IFR_GRAPHIC_OP                  0x04
#define EFI_IFR_ONE_OF_OP                   0x05
#define EFI_IFR_CHECKBOX_OP                 0x06
#define EFI_IFR_NUMERIC_OP                  0x07
#define EFI_IFR_PASSWORD_OP                 0x08
#define EFI_IFR_ONE_OF_OPTION_OP            0x09
#define EFI_IFR_SUPPRESS_IF_OP              0x0A
#define EFI_IFR_END_FORM_OP                 0x0B
#define EFI_IFR_HIDDEN_OP                   0x0C
#define EFI_IFR_END_FORM_SET_OP             0x0D
#define EFI_IFR_FORM_SET_OP                 0x0E
#define EFI_IFR_REF_OP                      0x0F
#define EFI_IFR_NO_SUBMIT_IF_OP             0x10
#define EFI_IFR_INCONSISTENT_IF_OP          0x11
#define EFI_IFR_EQ_ID_VAL_OP                0x12
#define EFI_IFR_EQ_ID_ID_OP                 0x13
#define EFI_IFR_EQ_ID_LIST_OP               0x14
#define EFI_IFR_AND_OP                      0x15
#define EFI_IFR_OR_OP                       0x16
#define EFI_IFR_NOT_OP                      0x17
#define EFI_IFR_END_IF_OP                   0x18
#define EFI_IFR_GRAYOUT_IF_OP               0x19
#define EFI_IFR_DATE_OP                     0x1A
#define EFI_IFR_TIME_OP                     0x1B
#define EFI_IFR_STRING_OP                   0x1C
#define EFI_IFR_REFRESH_OP                  0x1D
#define EFI_IFR_DISABLE_IF_OP               0x1E
#define EFI_IFR_TO_LOWER_OP                 0x20
#define EFI_IFR_TO_UPPER_OP                 0x21
#define EFI_IFR_ORDERED_LIST_OP             0x23
#define EFI_IFR_VARSTORE_OP                 0x24
#define EFI_IFR_VARSTORE_NAME_VALUE_OP      0x25
#define EFI_IFR_VARSTORE_EFI_OP             0x26
#define EFI_IFR_TRUE_OP                     0x29
#define EFI_IFR_FALSE_OP                    0x2A
#define EFI_IFR_UINT8_OP                    0x2C
#define EFI_IFR_UINT16_OP                   0x2D
#define EFI_IFR_UINT32_OP                   0x2E
#define EFI_IFR_UINT64_OP                   0x2F

// IFR OpCode Header
#pragma pack(1)
typedef struct {
    UINT8  OpCode;
    UINT8  Length:7;
    UINT8  Scope:1;
} EFI_IFR_OP_HEADER;
#pragma pack()

// Patch information structure
typedef struct _IFR_PATCH {
    UINTN Offset;                // Offset in the image
    UINT8 OriginalOpCode;        // Original opcode
    UINT8 NewOpCode;             // New opcode to patch (usually EFI_IFR_TRUE_OP)
    CHAR16 Description[128];     // Description of the patch
    struct _IFR_PATCH *Next;
} IFR_PATCH;

// Form information
typedef struct {
    UINT16 FormId;
    EFI_GUID FormSetGuid;
    CHAR16 Title[64];
    BOOLEAN IsHidden;            // Hidden by SuppressIf or GrayoutIf
} FORM_INFO;

/**
 * Parse IFR data from a loaded module image
 * 
 * @param ImageBase     Base address of the loaded module
 * @param ImageSize     Size of the module image
 * @param PatchList     Pointer to receive list of patches (caller frees)
 * @return EFI_SUCCESS if parsing successful
 */
EFI_STATUS ParseIfrData(
    VOID *ImageBase,
    UINTN ImageSize,
    IFR_PATCH **PatchList);

/**
 * Apply IFR patches to a module
 * 
 * @param ImageBase     Base address of the loaded module
 * @param ImageSize     Size of the module image
 * @param PatchList     List of patches to apply
 * @return EFI_SUCCESS if patching successful
 */
EFI_STATUS ApplyIfrPatches(
    VOID *ImageBase,
    UINTN ImageSize,
    IFR_PATCH *PatchList);

/**
 * Free IFR patch list
 * 
 * @param PatchList     List of patches to free
 */
VOID FreeIfrPatchList(IFR_PATCH *PatchList);

/**
 * Find and patch common AMI BIOS form structures
 * 
 * @param ImageBase     Base address of the loaded module
 * @param ImageSize     Size of the module image
 * @return Number of patches applied
 */
UINTN PatchAmiForms(VOID *ImageBase, UINTN ImageSize);

/**
 * Find and patch common Insyde H2O form structures
 * 
 * @param ImageBase     Base address of the loaded module
 * @param ImageSize     Size of the module image
 * @return Number of patches applied
 */
UINTN PatchInsydeForms(VOID *ImageBase, UINTN ImageSize);
