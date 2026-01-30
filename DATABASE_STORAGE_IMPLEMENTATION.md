# Database Storage Implementation for Configuration Values

## Overview

This document explains how configuration values are stored in the BIOS database and how the SmokelessRuntimeEFIPatcher (SREP) database system manages these configurations.

## BIOS Database Structure Analysis

### NVRAM Variable Format

Based on analysis of the o.bin BIOS file, the BIOS uses NVRAM (Non-Volatile RAM) variables to store configuration data:

```
NVAR Header Format:
+00h: NVAR signature (4 bytes: "NVAR")
+04h: Size (2 bytes)
+06h: Flags (2 bytes)
+08h: Index (1 byte)
+09h: Variable name (null-terminated string)
+xxh: Variable data (binary blob)
```

### Common Setup Variables Found

From hexdump analysis of o.bin:
- `Setup`: Main BIOS configuration (0xEE bytes = 238 bytes)
- `HPSetupData`: HP-specific configuration (0x89 bytes = 137 bytes)
- `NewHPSetupData`: Updated HP configuration (0x41A bytes = 1050 bytes)
- `AMITSESetup`: AMI Text Setup Engine config (0x58 bytes = 88 bytes)
- `SecureBootSetup`: Secure Boot settings
- `ALCSetup`: Audio Logic Controller settings (0x114 bytes = 276 bytes)

### Variable Storage Format

Each question in the IFR forms maps to:
1. **Variable Name**: The NVRAM variable to store in (e.g., "Setup")
2. **Variable GUID**: Unique identifier for the variable
3. **Offset**: Byte offset within the variable data
4. **Size**: Number of bytes (1, 2, 4, or 8)
5. **Value**: The actual configuration value

Example from IFR parsing:
```c
// Checkbox question at offset 0x02 in "Setup" variable
Question ID: 0x0123
Variable: "Setup"
GUID: {0xEC87D643, 0xEBA4, 0x4BB5, ...}
Offset: 0x02
Size: 1 byte (UINT8)
Value: 0x01 (Enabled)
```

## SREP Database System

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   HII Browser                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ IFR Parser   │→ │  Database    │→ │ NVRAM Manager│ │
│  │              │  │              │  │              │ │
│  │ Parse Forms  │  │ Store Config │  │ Write NVRAM  │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────┘
         ↓                   ↓                   ↓
    Form Info         Database Entry        NVRAM Variable
```

### Database Entry Structure

```c
typedef struct {
    UINT16 QuestionId;      // IFR Question ID (unique identifier)
    CHAR16 *VariableName;   // NVRAM variable name ("Setup", etc.)
    EFI_GUID VariableGuid;  // Variable GUID
    UINTN Offset;           // Offset within variable (bytes)
    UINTN Size;             // Size of value (1, 2, 4, or 8 bytes)
    UINT8 Type;             // Question type (checkbox, numeric, etc.)
    UINT64 Value;           // Current value
    BOOLEAN Modified;       // Has been modified flag
} DATABASE_ENTRY;
```

### Database Context

```c
typedef struct {
    DATABASE_ENTRY *Entries;  // Array of entries
    UINTN EntryCount;         // Number of entries
    UINTN EntryCapacity;      // Allocated capacity
} DATABASE_CONTEXT;
```

## Database Operations

### 1. Initialization

```c
EFI_STATUS DatabaseInitialize(DATABASE_CONTEXT *DbContext);
```

**Purpose**: Create and initialize the database
**Actions**:
- Allocates initial capacity (100 entries)
- Zeroes memory
- Prepares for entry addition

**Usage**:
```c
DATABASE_CONTEXT *Database = AllocateZeroPool(sizeof(DATABASE_CONTEXT));
DatabaseInitialize(Database);
```

### 2. Adding Entries

```c
EFI_STATUS DatabaseAddEntry(
    DATABASE_CONTEXT *DbContext,
    UINT16 QuestionId,
    CHAR16 *VariableName,
    EFI_GUID *VariableGuid,
    UINTN Offset,
    UINTN Size,
    UINT8 Type,
    UINT64 Value
);
```

**Purpose**: Register a configuration question in the database
**Actions**:
- Expands capacity if needed (doubles when full)
- Copies variable name and GUID
- Stores all metadata
- Marks as unmodified

**Example**:
```c
// Add "Boot Timeout" question
DatabaseAddEntry(
    Database,
    0x0123,                    // Question ID
    L"Setup",                  // Variable name
    &gSetupVariableGuid,       // Variable GUID
    0x10,                      // Offset = 16 bytes
    2,                         // Size = 2 bytes (UINT16)
    EFI_IFR_NUMERIC_OP,       // Type = Numeric
    5                          // Current value = 5 seconds
);
```

### 3. Loading from NVRAM

```c
EFI_STATUS DatabaseLoadFromNvram(
    DATABASE_CONTEXT *DbContext,
    NVRAM_MANAGER *NvramManager
);
```

**Purpose**: Read current values from NVRAM variables
**Actions**:
- For each database entry:
  - Find corresponding NVRAM variable
  - Read value at specified offset
  - Update entry's value field
- Returns count of loaded values

**Algorithm**:
```
For each DATABASE_ENTRY:
    Find NVRAM_VARIABLE with matching name and GUID
    Calculate address: Variable->Data + Entry->Offset
    Read value based on Entry->Size:
        1 byte  → Read as UINT8
        2 bytes → Read as UINT16
        4 bytes → Read as UINT32
        8 bytes → Read as UINT64
    Store in Entry->Value
```

### 4. Updating Values

```c
EFI_STATUS DatabaseUpdateValue(
    DATABASE_CONTEXT *DbContext,
    UINT16 QuestionId,
    UINT64 NewValue
);
```

**Purpose**: Modify a configuration value
**Actions**:
- Find entry by Question ID
- Update value
- Mark as modified

**Example**:
```c
// User changes Boot Timeout from 5 to 10
DatabaseUpdateValue(Database, 0x0123, 10);
```

### 5. Committing to NVRAM

```c
EFI_STATUS DatabaseCommitToNvram(
    DATABASE_CONTEXT *DbContext,
    NVRAM_MANAGER *NvramManager
);
```

**Purpose**: Write modified values back to NVRAM variables
**Actions**:
1. For each modified entry:
   - Find NVRAM variable
   - Calculate write address (Data + Offset)
   - Write value based on size
   - Mark variable as modified
2. Commit all modified variables to NVRAM

**Algorithm**:
```
For each DATABASE_ENTRY where Modified == TRUE:
    Find NVRAM_VARIABLE with matching name and GUID
    
    Validate offset + size <= variable data size
    
    Calculate write address: Variable->Data + Entry->Offset
    
    Write value based on Entry->Size:
        1 byte  → *(UINT8*)address = (UINT8)Entry->Value
        2 bytes → *(UINT16*)address = (UINT16)Entry->Value
        4 bytes → *(UINT32*)address = (UINT32)Entry->Value
        8 bytes → *(UINT64*)address = Entry->Value
    
    Mark Variable->Modified = TRUE
    Mark Entry->Modified = FALSE

Call NvramCommitChanges() to save all modified variables
```

### 6. Getting Values

```c
EFI_STATUS DatabaseGetValue(
    DATABASE_CONTEXT *DbContext,
    UINT16 QuestionId,
    UINT64 *Value
);
```

**Purpose**: Retrieve current value of a question
**Usage**:
```c
UINT64 BootTimeout;
DatabaseGetValue(Database, 0x0123, &BootTimeout);
Print(L"Boot Timeout: %d seconds\n", (UINT16)BootTimeout);
```

### 7. Cleanup

```c
VOID DatabaseCleanup(DATABASE_CONTEXT *DbContext);
```

**Purpose**: Free all resources
**Actions**:
- Free all variable name strings
- Free entries array
- Zero memory

## Usage Flow

### Complete Example: Modifying Boot Timeout

```c
// 1. Initialize systems
DATABASE_CONTEXT Database;
NVRAM_MANAGER NvramMgr;

DatabaseInitialize(&Database);
NvramInitialize(&NvramMgr);
NvramLoadSetupVariables(&NvramMgr);

// 2. Parse IFR and populate database
// (From IFR parsing, we discover a numeric question)
DatabaseAddEntry(
    &Database,
    0x0123,                   // Question ID from IFR
    L"Setup",                 // Variable name from IFR VarStore
    &gSetupVariableGuid,      // GUID from IFR VarStoreEfi
    0x10,                     // Offset from IFR question
    2,                        // Size from IFR numeric (UINT16)
    EFI_IFR_NUMERIC_OP,       // Type
    0                         // Initial value (unknown)
);

// 3. Load current values from NVRAM
DatabaseLoadFromNvram(&Database, &NvramMgr);
// Now Entry->Value contains current boot timeout

// 4. User changes value
UINT64 CurrentValue;
DatabaseGetValue(&Database, 0x0123, &CurrentValue);
Print(L"Current Boot Timeout: %d seconds\n", (UINT16)CurrentValue);

// User selects new value: 10 seconds
DatabaseUpdateValue(&Database, 0x0123, 10);

// 5. Commit changes
DatabaseCommitToNvram(&Database, &NvramMgr);
// This writes the value to Setup variable at offset 0x10

// 6. Cleanup
DatabaseCleanup(&Database);
NvramCleanup(&NvramMgr);
```

## Benefits

### Structured Approach
- Clear separation between question metadata and values
- Easy to add new questions
- Supports multiple variables simultaneously

### Efficient Updates
- Only modified entries are written
- Batch updates to same variable
- Minimal NVRAM writes

### Type Safety
- Proper size handling (1/2/4/8 bytes)
- Offset validation
- Bounds checking

### Easy Integration
- Simple API
- Automatic capacity management
- Memory cleanup

## Integration with IFR Parsing

### Auto-Population from IFR

When parsing IFR forms, questions are automatically registered:

```c
// In ParseFormQuestions()
case EFI_IFR_NUMERIC_OP:
{
    EFI_IFR_NUMERIC *Numeric = (EFI_IFR_NUMERIC *)OpHeader;
    
    // Register in database
    if (Context->Database != NULL)
    {
        DatabaseAddEntry(
            Context->Database,
            Numeric->Question.QuestionId,
            VariableName,           // From VarStore
            &VariableGuid,          // From VarStoreEfi
            Numeric->Question.VarStoreInfo.VarOffset,
            GetSizeFromType(Numeric->Flags),
            EFI_IFR_NUMERIC_OP,
            0                       // Will be loaded from NVRAM
        );
    }
    
    break;
}
```

### Question Types Supported

| IFR Type | Size | Database Type |
|----------|------|---------------|
| CHECKBOX | 1 byte | UINT8 (0/1) |
| NUMERIC (UINT8) | 1 byte | UINT8 |
| NUMERIC (UINT16) | 2 bytes | UINT16 |
| NUMERIC (UINT32) | 4 bytes | UINT32 |
| NUMERIC (UINT64) | 8 bytes | UINT64 |
| ONE_OF | 1-8 bytes | Depends on type |
| STRING | Variable | Not supported in database |

## NVRAM Commit Process

### Step-by-Step

1. **Database Commit Phase**:
   ```
   DatabaseCommitToNvram()
   ↓
   For each modified entry:
       Update value in NVRAM variable buffer
       Mark variable as modified
   ```

2. **NVRAM Commit Phase**:
   ```
   NvramCommitChanges()
   ↓
   For each modified variable:
       Call gRT->SetVariable()
       Write to actual NVRAM
   ```

3. **Result**:
   - Changes persist across reboots
   - BIOS reads new values on next boot
   - Settings take effect

## Error Handling

### Common Errors

**Entry Not Found**:
```c
Status = DatabaseUpdateValue(Db, 0x9999, 10);
if (Status == EFI_NOT_FOUND) {
    Print(L"Question ID 0x9999 not in database\n");
}
```

**Variable Not Found**:
```c
// DatabaseCommitToNvram() warns:
// "Warning: Variable XYZ not found for QuestionId 123"
```

**Offset Out of Bounds**:
```c
// DatabaseCommitToNvram() errors:
// "Error: Offset 100 + Size 4 exceeds variable size 50"
```

**Out of Memory**:
```c
Status = DatabaseAddEntry(...);
if (Status == EFI_OUT_OF_RESOURCES) {
    Print(L"Failed to expand database capacity\n");
}
```

## Performance Considerations

### Memory Usage
- Each entry: ~80 bytes
- 100 entries: ~8 KB
- Auto-expands as needed

### NVRAM Writes
- Batched by variable
- Only modified entries
- Single SetVariable() call per variable

### Optimization
- Pre-allocate capacity if entry count known
- Group questions by variable for better locality
- Load values once at startup

## Security Considerations

### Variable Protection
- Respects NVRAM variable attributes
- Cannot write to read-only variables
- GUID verification prevents cross-variable writes

### Validation
- Offset bounds checking
- Size validation
- Type verification

### Audit Trail
- Modified flag tracking
- Original value preservation (in NVRAM manager)
- Rollback capability (via NVRAM manager)

## Future Enhancements

### Potential Improvements
1. **Default Value Storage**: Store defaults for F9 reset
2. **Value Ranges**: Min/max validation
3. **Dependencies**: Track question dependencies
4. **Batch Load**: Optimize multiple variable reads
5. **Change Notification**: Callback on value changes
6. **Persistence**: Save database to file
7. **Search**: Find entries by variable name or type
8. **Statistics**: Track access patterns

## Troubleshooting

### Debug Output

Enable database debug output:
```c
Print(L"Database: %d entries, %d modified\n", 
      DbContext->EntryCount, ModifiedCount);

// List all entries
for (UINTN i = 0; i < DbContext->EntryCount; i++) {
    Print(L"Q %04x: %s[%04x] = %08x %s\n",
          Entry->QuestionId,
          Entry->VariableName,
          Entry->Offset,
          Entry->Value,
          Entry->Modified ? L"*" : L"");
}
```

### Common Issues

**Values Not Saving**:
- Check DatabaseCommitToNvram() return status
- Verify NvramCommitChanges() succeeds
- Ensure variable is writable

**Wrong Values**:
- Verify offset is correct
- Check size matches IFR definition
- Confirm GUID matches

**Out of Memory**:
- Reduce entry count
- Increase initial capacity
- Check for memory leaks

## References

### Source Files
- `NvramManager.h` - Database structures
- `NvramManager.c` - Database implementation
- `HiiBrowser.c` - Integration

### Related Documentation
- UEFI Specification 2.9 - Runtime Services
- PI Specification 1.7 - IFR Opcodes
- `VENDOR_FORMS_IMPLEMENTATION.md` - Vendor detection

### Standards
- UEFI Variable Services
- IFR Variable Storage
- NVRAM Format Specification

---

**Note**: This database system provides a clean abstraction over UEFI NVRAM variables, making configuration management straightforward and reliable.
