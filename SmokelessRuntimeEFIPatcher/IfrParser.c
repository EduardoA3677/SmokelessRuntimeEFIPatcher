#include "IfrParser.h"
#include <Library/PrintLib.h>

extern char Log[512];
extern EFI_FILE *LogFile;
void LogToFile(EFI_FILE *LogFile, char *String);

/**
 * Helper: Check if data is likely IFR opcode stream
 */
BOOLEAN IsIfrData(UINT8 *Data, UINTN Size)
{
    if (Size < 4)
        return FALSE;

    // Look for IFR FormSet opcode at the start
    if (Data[0] == EFI_IFR_FORM_SET_OP && Data[1] >= 0x1C)
        return TRUE;

    return FALSE;
}

/**
 * Parse IFR data and generate patch list
 */
EFI_STATUS ParseIfrData(VOID *ImageBase, UINTN ImageSize, IFR_PATCH **PatchList)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    UINTN Offset = 0;
    IFR_PATCH *Head = NULL;
    IFR_PATCH *Current = NULL;
    UINTN PatchCount = 0;

    if (ImageBase == NULL || PatchList == NULL)
        return EFI_INVALID_PARAMETER;

    *PatchList = NULL;

    // Don't log for every module - too verbose
    // Only log when patches are found

    // Search for IFR data in the image
    for (Offset = 0; Offset < ImageSize - 4; Offset++)
    {
        if (IsIfrData(&Data[Offset], ImageSize - Offset))
        {
            // Don't log every IFR data location - too verbose
            // Will log summary at the end

            UINTN IfrOffset = Offset;
            UINTN IfrEnd = ImageSize;

            // Parse IFR opcodes
            while (IfrOffset < IfrEnd - 2)
            {
                EFI_IFR_OP_HEADER *OpHeader = (EFI_IFR_OP_HEADER *)&Data[IfrOffset];

                if (OpHeader->Length == 0 || OpHeader->Length > 0xFF)
                    break;

                // Check for opcodes that hide forms/menus
                if (OpHeader->OpCode == EFI_IFR_SUPPRESS_IF_OP ||
                    OpHeader->OpCode == EFI_IFR_GRAY_OUT_IF_OP ||
                    OpHeader->OpCode == EFI_IFR_DISABLE_IF_OP)
                {
                    // Don't log every hideable element - too verbose
                    // Will create patch and log when applied

                    // Strategy: Patch the condition following these opcodes
                    // Look at the next opcode to see what condition it is
                    UINTN NextOffset = IfrOffset + OpHeader->Length;
                    if (NextOffset < IfrEnd - 2)
                    {
                        EFI_IFR_OP_HEADER *NextOp = (EFI_IFR_OP_HEADER *)&Data[NextOffset];
                        
                        // Create a patch to change the condition to FALSE
                        // This way, the suppress/grayout/disable will never trigger
                        IFR_PATCH *Patch = AllocateZeroPool(sizeof(IFR_PATCH));
                        if (Patch != NULL)
                        {
                            Patch->Offset = NextOffset;
                            Patch->OriginalOpCode = NextOp->OpCode;
                            Patch->NewOpCode = EFI_IFR_FALSE_OP;
                            
                            if (OpHeader->OpCode == EFI_IFR_SUPPRESS_IF_OP)
                                UnicodeSPrint(Patch->Description, sizeof(Patch->Description), 
                                            L"Patch SuppressIf condition at 0x%x", NextOffset);
                            else if (OpHeader->OpCode == EFI_IFR_GRAY_OUT_IF_OP)
                                UnicodeSPrint(Patch->Description, sizeof(Patch->Description), 
                                            L"Patch GrayoutIf condition at 0x%x", NextOffset);
                            else
                                UnicodeSPrint(Patch->Description, sizeof(Patch->Description), 
                                            L"Patch DisableIf condition at 0x%x", NextOffset);

                            // Add to linked list
                            if (Head == NULL)
                            {
                                Head = Patch;
                                Current = Patch;
                            }
                            else
                            {
                                Current->Next = Patch;
                                Current = Patch;
                            }
                            PatchCount++;
                        }
                    }
                }

                IfrOffset += OpHeader->Length;

                // Safety check: if we hit EndFormSet, we're done with this IFR section
                if (OpHeader->OpCode == EFI_IFR_END_OP)
                    break;
            }
        }
    }

    *PatchList = Head;
    
    // Only log if patches were found (reduce verbosity)
    if (PatchCount > 0)
    {
        AsciiSPrint(Log, 512, "Found %d IFR patches in module\n\r", PatchCount);
        LogToFile(LogFile, Log);
    }

    return PatchCount > 0 ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/**
 * Apply IFR patches to a module
 */
EFI_STATUS ApplyIfrPatches(VOID *ImageBase, UINTN ImageSize, IFR_PATCH *PatchList)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    IFR_PATCH *Current = PatchList;
    UINTN AppliedCount = 0;

    if (ImageBase == NULL || PatchList == NULL)
        return EFI_INVALID_PARAMETER;

    // Don't log "Applying IFR patches..." for every module - too verbose
    // Will log summary at the end

    while (Current != NULL)
    {
        if (Current->Offset < ImageSize)
        {
            // Verify original opcode matches
            if (Data[Current->Offset] == Current->OriginalOpCode)
            {
                Data[Current->Offset] = Current->NewOpCode;
                AppliedCount++;
                
                // Only log individual patches if there are few of them
                // Otherwise just log the count
                if (AppliedCount <= 5)
                {
                    AsciiSPrint(Log, 512, "  Patched: %s (0x%02x -> 0x%02x)\n\r",
                               Current->Description, Current->OriginalOpCode, Current->NewOpCode);
                    LogToFile(LogFile, Log);
                }
            }
            // Don't log warnings for mismatches - already patched locations will show this
        }
        Current = Current->Next;
    }

    // Only log if patches were applied
    if (AppliedCount > 0)
    {
        AsciiSPrint(Log, 512, "Applied %d IFR patches\n\r", AppliedCount);
        LogToFile(LogFile, Log);
    }

    return EFI_SUCCESS;
}

/**
 * Free IFR patch list
 */
VOID FreeIfrPatchList(IFR_PATCH *PatchList)
{
    IFR_PATCH *Current = PatchList;
    while (Current != NULL)
    {
        IFR_PATCH *Next = Current->Next;
        FreePool(Current);
        Current = Next;
    }
}

/**
 * Patch AMI BIOS forms - look for specific patterns
 */
UINTN PatchAmiForms(VOID *ImageBase, UINTN ImageSize)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    UINTN PatchCount = 0;

    // Don't log "Applying AMI-specific..." for every module - too verbose

    // AMI often uses specific patterns for advanced menus
    // Look for common suppress patterns
    for (UINTN i = 0; i < ImageSize - 16; i++)
    {
        // Pattern: SuppressIf { TRUE } around advanced menus
        if (Data[i] == EFI_IFR_SUPPRESS_IF_OP &&
            i + 4 < ImageSize &&
            Data[i + 3] == EFI_IFR_TRUE_OP)
        {
            // Patch TRUE to FALSE
            Data[i + 3] = EFI_IFR_FALSE_OP;
            PatchCount++;
        }
    }

    // Only log if patches were applied
    if (PatchCount > 0)
    {
        AsciiSPrint(Log, 512, "Applied %d AMI-specific patches\n\r", PatchCount);
        LogToFile(LogFile, Log);
    }

    return PatchCount;
}

/**
 * Patch Insyde H2O forms
 */
UINTN PatchInsydeForms(VOID *ImageBase, UINTN ImageSize)
{
    UINT8 *Data = (UINT8 *)ImageBase;
    UINTN PatchCount = 0;

    AsciiSPrint(Log, 512, "Applying Insyde-specific form patches...\n\r");
    LogToFile(LogFile, Log);

    // Insyde H2O uses a different mechanism - form visibility flags
    // The README example shows patching uint32_t isShown flags from 0 to 1
    // Look for GUID + 00000000 pattern and change to GUID + 01000000

    // This is handled separately in the main code by searching for specific form GUIDs
    // Here we can look for generic patterns

    for (UINTN i = 0; i < ImageSize - 20; i++)
    {
        // Look for 16-byte GUID followed by 4-byte zero (little-endian)
        // This might be a form visibility structure
        if (Data[i + 16] == 0x00 &&
            Data[i + 17] == 0x00 &&
            Data[i + 18] == 0x00 &&
            Data[i + 19] == 0x00)
        {
            // Check if this looks like it might be after a GUID
            // (very heuristic - would need more context in real implementation)
            BOOLEAN MightBeFormStruct = TRUE;
            
            // Simple heuristic: check if bytes before look somewhat random (like a GUID)
            if (i >= 16)
            {
                // Check for some variation in the previous 16 bytes
                UINT8 FirstByte = Data[i - 16];
                BOOLEAN HasVariation = FALSE;
                for (UINTN j = 1; j < 16; j++)
                {
                    if (Data[i - 16 + j] != FirstByte)
                    {
                        HasVariation = TRUE;
                        break;
                    }
                }
                MightBeFormStruct = HasVariation;
            }

            if (MightBeFormStruct)
            {
                // Patch to 0x01000000
                Data[i + 16] = 0x01;
                PatchCount++;
            }
        }
    }

    // Only log if patches were applied
    if (PatchCount > 0)
    {
        AsciiSPrint(Log, 512, "Applied %d Insyde-specific patches\n\r", PatchCount);
        LogToFile(LogFile, Log);
    }

    return PatchCount;
}
