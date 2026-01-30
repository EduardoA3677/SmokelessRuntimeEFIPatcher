#pragma once
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <PiDxe.h>
#include <Protocol/FirmwareVolume2.h>
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
 * Patch all loaded modules that contain IFR data
 * 
 * @param ImageHandle   The image handle
 * @param BiosInfo      BIOS information
 * @return EFI_SUCCESS if successful
 */
EFI_STATUS PatchAllLoadedModules(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo);

/**
 * Load and patch a module and its dependencies from firmware volume
 * 
 * @param ImageHandle   The image handle
 * @param ModuleName    Name of the module to load
 * @param BiosInfo      BIOS information
 * @param Execute       Whether to execute the module after patching
 * @return EFI_SUCCESS if successful
 */
EFI_STATUS LoadAndPatchModule(EFI_HANDLE ImageHandle, CHAR8 *ModuleName, BIOS_INFO *BiosInfo, BOOLEAN Execute);

/**
 * Load and patch Setup dependencies
 * 
 * @param ImageHandle   The image handle
 * @param BiosInfo      BIOS information
 * @return EFI_SUCCESS if successful
 */
EFI_STATUS PatchSetupDependencies(EFI_HANDLE ImageHandle, BIOS_INFO *BiosInfo);

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
