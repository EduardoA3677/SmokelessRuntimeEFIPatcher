// Additional IFR Opcode definitions for comprehensive BIOS support
// Based on UEFI spec and real BIOS analysis
//
// NOTE: Structure definitions are provided by EDK2 headers (UefiInternalFormRepresentation.h)
// This file only defines opcode constants that may not be defined in older EDK2 versions

#ifndef _IFR_OPCODES_H_
#define _IFR_OPCODES_H_

// IFR Opcode definitions
#define EFI_IFR_FORM_SET_OP           0x0E
#define EFI_IFR_END_OP                0x29
#define EFI_IFR_FORM_OP               0x01
#define EFI_IFR_SUBTITLE_OP           0x02
#define EFI_IFR_TEXT_OP               0x03
#define EFI_IFR_ONE_OF_OP             0x05
#define EFI_IFR_CHECKBOX_OP           0x06
#define EFI_IFR_NUMERIC_OP            0x07
#define EFI_IFR_ONE_OF_OPTION_OP      0x09
#define EFI_IFR_SUPPRESS_IF_OP        0x0A
#define EFI_IFR_ACTION_OP             0x0C
#define EFI_IFR_REF_OP                0x0F
#define EFI_IFR_GRAY_OUT_IF_OP        0x19
#define EFI_IFR_STRING_OP             0x1C
#define EFI_IFR_DEFAULT_OP            0x5B

// Structure definitions are already provided by:
// MdePkg/Include/Uefi/UefiInternalFormRepresentation.h
// Including: EFI_IFR_SUBTITLE, EFI_IFR_TEXT, EFI_IFR_ONE_OF_OPTION,
//            EFI_IFR_REF, EFI_IFR_REF2, EFI_IFR_DEFAULT, EFI_IFR_ACTION

#endif // _IFR_OPCODES_H_
