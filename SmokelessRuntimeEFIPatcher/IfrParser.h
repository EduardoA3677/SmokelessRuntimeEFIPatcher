#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Uefi/UefiInternalFormRepresentation.h>

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
