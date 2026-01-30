// Additional IFR Opcode definitions for comprehensive BIOS support
// Based on UEFI spec and real BIOS analysis

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

// SUBTITLE structure
typedef struct {
    EFI_IFR_OP_HEADER Header;
    EFI_IFR_STATEMENT_HEADER Statement;
    EFI_STRING_ID Prompt;
    UINT8 Flags;
} EFI_IFR_SUBTITLE;

// TEXT structure
typedef struct {
    EFI_IFR_OP_HEADER Header;
    EFI_IFR_STATEMENT_HEADER Statement;
    EFI_STRING_ID Prompt;
    EFI_STRING_ID Help;
    EFI_STRING_ID TextTwo;
} EFI_IFR_TEXT;

// ONE_OF_OPTION structure
typedef struct {
    EFI_IFR_OP_HEADER Header;
    EFI_STRING_ID Option;
    UINT8 Flags;
    UINT8 Type;
    // Value follows based on Type
} EFI_IFR_ONE_OF_OPTION;

// REF structure
typedef struct {
    EFI_IFR_OP_HEADER Header;
    EFI_IFR_QUESTION_HEADER Question;
    EFI_FORM_ID FormId;
} EFI_IFR_REF;

// REF2 structure (with FormSetGuid)
typedef struct {
    EFI_IFR_OP_HEADER Header;
    EFI_IFR_QUESTION_HEADER Question;
    EFI_FORM_ID FormId;
    EFI_QUESTION_ID QuestionId;
    EFI_GUID FormSetId;
} EFI_IFR_REF2;

// DEFAULT structure
typedef struct {
    EFI_IFR_OP_HEADER Header;
    UINT16 DefaultId;
    UINT8 Type;
    // Value follows based on Type
} EFI_IFR_DEFAULT;

// ACTION structure
typedef struct {
    EFI_IFR_OP_HEADER Header;
    EFI_IFR_QUESTION_HEADER Question;
    EFI_STRING_ID QuestionConfig;
} EFI_IFR_ACTION;

#endif // _IFR_OPCODES_H_
