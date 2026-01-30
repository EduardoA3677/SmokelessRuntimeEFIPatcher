#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include "BiosDetector.h"
#include "IfrParser.h"

/**
 * Auto-detect and patch BIOS based on detected type
 * This replaces the config file approach with automatic detection
 * 
 * @param ImageHandle   The image handle
 * @param BiosInfo      Detected BIOS information
 * @return EFI_SUCCESS if successful
 */
EFI_STATUS AutoPatchBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo);

/**
 * Disable write protections in common locations
 * 
 * @param ImageBase     Base address of module to patch
 * @param ImageSize     Size of the module
 * @return Number of protections disabled
 */
UINTN DisableWriteProtections(VOID *ImageBase, UINTN ImageSize);

/**
 * Find and patch AMI BIOS specific structures
 * 
 * @param ImageHandle   The image handle
 * @param BiosInfo      BIOS information
 * @return EFI_SUCCESS if successful
 */
EFI_STATUS PatchAmiBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo);

/**
 * Find and patch Insyde H2O BIOS specific structures
 * 
 * @param ImageHandle   The image handle
 * @param BiosInfo      BIOS information
 * @return EFI_SUCCESS if successful
 */
EFI_STATUS PatchInsydeBios(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo);

/**
 * Execute the Setup browser after patching
 * 
 * @param ImageHandle   The image handle
 * @param BiosInfo      BIOS information
 * @return EFI_SUCCESS if Setup launched successfully
 */
EFI_STATUS ExecuteSetupBrowser(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo);
