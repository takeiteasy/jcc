/*
 JCC: JIT C Compiler

 Copyright (C) 2025 George Watson

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef JCC_H
#define JCC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <libgen.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

// libffi support for variadic foreign functions
#ifdef JCC_HAS_FFI
#include <ffi.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*!
 @enum JCC_OP
 @abstract VM instruction opcodes for the JCC bytecode.
 @discussion
 The VM is stack-based with an accumulator `ax`. These opcodes are
 emitted by the code generator and interpreted by the VM executor.
*/
typedef enum {
    LEA, IMM, JMP, CALL, CALLI, JZ, JNZ, ENT, ADJ, LEV, LI, LC, LS, LW, SI, SC, SS, SW, PUSH,
    OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
    // VM memory operations (self-contained, no system calls)
    MALC, MFRE, MCPY,
    // Type conversion instructions
    SX1, SX2, SX4,  // Sign extend 1/2/4 bytes to 8 bytes
    ZX1, ZX2, ZX4,  // Zero extend 1/2/4 bytes to 8 bytes
    // Floating-point instructions
    FLD, FST, FADD, FSUB, FMUL, FDIV, FNEG,
    FEQ, FNE, FLT, FLE, FGT, FGE,
    I2F, F2I, FPUSH,
    // Foreign function interface
    CALLF,
    // Memory safety opcodes
    CHKB,   // Check array bounds
    CHKP,   // Check pointer validity (UAF detection)
    // CHKT,   // Check type on dereference (TODO)
    // SANP,   // Full pointer sanitizer check (TODO)
    // CHKI,   // Check initialization (TODO)
    // MARKI,  // Mark as initialized (TODO)
    // Non-local jump instructions (setjmp/longjmp)
    SETJMP, // Save execution context to jmp_buf, return 0
    LONGJMP // Restore execution context from jmp_buf, return val
} JCC_OP;

/*!
 @struct HashEntry
 @abstract Simple key/value bucket used by the project's HashMap.
 @field key Null-terminated string key.
 @field keylen Length of the key in bytes.
 @field val Pointer to the stored value.
*/
typedef struct HashEntry {
    char *key;
    int keylen;
    void *val;
} HashEntry;

/*!
 @struct HashMap
 @abstract Lightweight open-addressing hashmap used for symbol tables,
           macros, and other small maps.
 @field buckets Pointer to an array of HashEntry buckets.
 @field capacity Number of buckets allocated.
 @field used Number of buckets currently in use.
*/
typedef struct HashMap {
    HashEntry *buckets;
    int capacity;
    int used;
} HashMap;

/*!
 @struct File
 @abstract Represents the contents and metadata of a source file.
 @field name Original filename string.
 @field file_no Unique numeric id for the file.
 @field contents File contents as a NUL-terminated buffer.
 @field display_name Optional name emitted by a `#line` directive.
 @field line_delta Line number delta applied for `#line` handling.
*/
typedef struct File {
    char *name;
    int file_no;
    char *contents;

    // For #line directive
    char *display_name;
    int line_delta;
} File;

/*!
 @struct Relocation
 @abstract A relocation record for a global variable initializer that
           references another global symbol.
 @discussion
 Each relocation describes a pointer-sized slot within a global's
 initializer data that must be patched with the address of another
 global (or label) when codegen finalizes the data segment.
*/
typedef struct Relocation {
    struct Relocation *next;
    int offset;
    char **label;
    long addend;
} Relocation;

/*!
 @struct StringArray
 @abstract Dynamic array of strings used for include paths and similar
           small lists.
 @field data Pointer to N entries of char* strings.
 @field capacity Allocated capacity of the array.
 @field len Current length (number of strings stored).
*/
typedef struct StringArray {
    char **data;
    int capacity;
    int len;
} StringArray;

/*!
 @struct EnumConstant
 @abstract Represents an enumerator constant within an enum type.
 @field name Name of the enumerator.
 @field value Integer value of the enumerator.
 @field next Pointer to the next enumerator in the linked list.
*/
typedef struct EnumConstant {
    char *name;
    int value;
    struct EnumConstant *next;
} EnumConstant;

/*!
 @struct Hideset
 @abstract Represents a set of macro names that have been hidden to
           prevent recursive macro expansion.
*/
typedef struct Hideset {
    struct Hideset *next;
    char *name;
} Hideset;

/*!
 @enum TokenKind
 @abstract Kinds of lexical tokens produced by the tokenizer and
           used by the preprocessor and parser.
*/
typedef enum {
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords
    TK_STR,     // String literals
    TK_NUM,     // Numeric literals
    TK_PP_NUM,  // Preprocessing numbers
    TK_EOF,     // End-of-file markers
} TokenKind;

typedef struct Type Type;

/*!
 @struct Token
 @abstract Token produced by the lexer or by macro expansion.
 @field kind Token kind (see TokenKind).
 @field loc Pointer into the source buffer where the token text begins.
 @field len Number of characters in the token text.
 @field str For string literals: pointer to the unescaped contents.
*/
typedef struct Token {
    TokenKind kind;   // Token kind
    struct Token *next;      // Next token
    int64_t val;      // If kind is TK_NUM, its value
    long double fval; // If kind is TK_NUM, its value
    char *loc;        // Token location
    int len;          // Token length
    Type *ty;         // Used if TK_NUM or TK_STR
    char *str;        // String literal contents including terminating '\0'

    File *file;       // Source location
    char *filename;   // Filename
    int line_no;      // Line number
    int line_delta;   // Line number
    bool at_bol;      // True if this token is at beginning of line
    bool has_space;   // True if this token follows a space character
    Hideset *hideset; // For macro expansion
    struct Token *origin;    // If this is expanded from a macro, the original token
} Token;

/*!
 @enum TypeKind
 @abstract Kind tag for the `Type` structure describing C types.
*/
typedef enum {
    TY_VOID     = 0,
    TY_BOOL     = 1,
    TY_CHAR     = 2,
    TY_SHORT    = 3,
    TY_INT      = 4,
    TY_LONG     = 5,
    TY_FLOAT    = 6,
    TY_DOUBLE   = 7,
    TY_LDOUBLE  = 8,
    TY_ENUM     = 9,
    TY_PTR      = 10,
    TY_FUNC     = 11,
    TY_ARRAY    = 12,
    TY_VLA      = 13, // variable-length array
    TY_STRUCT   = 14,
    TY_UNION    = 15,
} TypeKind;

typedef struct Node Node;
typedef struct Obj Obj;

/*!
 @struct Type
 @abstract Central representation of a C type in the compiler.
 @field kind One of TypeKind indicating the category of this type.
 @field size sizeof() value in bytes.
 @field align Alignment requirement in bytes.
 @field base For pointer/array types: the referenced element/base type.
*/
struct Type {
    TypeKind kind;
    int size;           // sizeof() value
    int align;          // alignment
    bool is_unsigned;   // unsigned or signed
    bool is_atomic;     // true if _Atomic
    bool is_const;      // true if const-qualified
    struct Type *origin;       // for type compatibility check

    // Pointer-to or array-of type. We intentionally use the same member
    // to represent pointer/array duality in C.
    //
    // In many contexts in which a pointer is expected, we examine this
    // member instead of "kind" member to determine whether a type is a
    // pointer or not. That means in many contexts "array of T" is
    // naturally handled as if it were "pointer to T", as required by
    // the C spec.
    struct Type *base;

    // Declaration
    Token *name;
    Token *name_pos;

    // Array
    int array_len;

    // Variable-length array
    Node *vla_len; // # of elements
    Obj *vla_size; // sizeof() value

    // Struct
    struct Member *members;
    bool is_flexible;
    bool is_packed;

    // Enum (tracks enum constants for code generation)
    EnumConstant *enum_constants;

    // Function type
    struct Type *return_ty;
    struct Type *params;
    bool is_variadic;
    struct Type *next;
};

/*!
 @struct Member
 @abstract Member (field) descriptor for struct and union types.
 @field ty Type of the member.
 @field name Token pointing to the member identifier.
 @field offset Byte offset of the member within its aggregate.
*/
typedef struct Member {
    struct Member *next;
    Type *ty;
    Token *tok; // for error message
    Token *name;
    int idx;
    int align;
    int offset;

    // Bitfield
    bool is_bitfield;
    int bit_offset;
    int bit_width;
} Member;

/*!
 @enum NodeKind
 @abstract Kinds of AST nodes produced by the parser.
*/
typedef enum {
    ND_NULL_EXPR = 0, // Do nothing
    ND_ADD       = 1,       // +
    ND_SUB       = 2,       // -
    ND_MUL       = 3,       // *
    ND_DIV       = 4,       // /
    ND_NEG       = 5,       // unary -
    ND_MOD       = 6,       // %
    ND_BITAND    = 7,       // &
    ND_BITOR     = 8,       // |
    ND_BITXOR    = 9,       // ^
    ND_SHL       = 10,      // <<
    ND_SHR       = 11,      // >>
    ND_EQ        = 12,      // ==
    ND_NE        = 13,      // !=
    ND_LT        = 14,      // <
    ND_LE        = 15,      // <=
    ND_ASSIGN    = 16,      // =
    ND_COND      = 17,      // ?:
    ND_COMMA     = 18,      // ,
    ND_MEMBER    = 19,      // . (struct member access)
    ND_ADDR      = 20,      // unary &
    ND_DEREF     = 21,      // unary *
    ND_NOT       = 22,      // !
    ND_BITNOT    = 23,      // ~
    ND_LOGAND    = 24,      // &&
    ND_LOGOR     = 25,      // ||
    ND_RETURN    = 26,      // "return"
    ND_IF        = 27,      // "if"
    ND_FOR       = 28,      // "for" or "while"
    ND_DO        = 29,      // "do"
    ND_SWITCH    = 30,      // "switch"
    ND_CASE      = 31,      // "case"
    ND_BLOCK     = 32,      // { ... }
    ND_GOTO      = 33,      // "goto"
    ND_GOTO_EXPR = 34,      // "goto" labels-as-values
    ND_LABEL     = 35,      // Labeled statement
    ND_LABEL_VAL = 36,      // [GNU] Labels-as-values
    ND_FUNCALL   = 37,      // Function call
    ND_EXPR_STMT = 38,      // Expression statement
    ND_STMT_EXPR = 39,      // Statement expression
    ND_VAR       = 40,      // Variable
    ND_VLA_PTR   = 41,      // VLA designator
    ND_NUM       = 42,      // Integer
    ND_CAST      = 43,      // Type cast
    ND_MEMZERO   = 44,      // Zero-clear a stack variable
    ND_ASM       = 45,      // "asm"
    ND_CAS       = 46,      // Atomic compare-and-swap
    ND_EXCH      = 47,      // Atomic exchange
} NodeKind;

/*!
 @struct Node
 @abstract Represents a node in the parser's abstract syntax tree.
 @field kind Node kind (see NodeKind).
 @field ty Resolved type of this node (after semantic analysis).
 @field lhs Left-hand side child (when applicable).
 @field rhs Right-hand side child (when applicable).
*/
struct Node {
    NodeKind kind; // Node kind
    struct Node *next;    // Next node
    Type *ty;      // Type, e.g. int or pointer to int
    Token *tok;    // Representative token

    struct Node *lhs;     // Left-hand side
    struct Node *rhs;     // Right-hand side

    // "if" or "for" statement
    struct Node *cond;
    struct Node *then;
    struct Node *els;
    struct Node *init;
    struct Node *inc;

    // "break" and "continue" labels
    char *brk_label;
    char *cont_label;

    // Block or statement expression
    struct Node *body;

    // Struct member access
    Member *member;

    // Function call
    Type *func_ty;
    struct Node *args;
    bool pass_by_stack;
    Obj *ret_buffer;

    // Goto or labeled statement, or labels-as-values
    char *label;
    char *unique_label;
    struct Node *goto_next;

    // Switch
    struct Node *case_next;
    struct Node *default_case;

    // Case
    long begin;
    long end;

    // "asm" string literal
    char *asm_str;

    // Atomic compare-and-swap
    struct Node *cas_addr;
    struct Node *cas_old;
    struct Node *cas_new;

    // Atomic op= operators
    Obj *atomic_addr;
    struct Node *atomic_expr;

    // Variable
    Obj *var;

    // Numeric literal
    int64_t val;
    long double fval;
};

/*!
 @struct Obj
 @abstract Represents a C object: either a variable (global/local) or a
           function. The parser and code generator use Obj for symbol
           and storage tracking.
 @field name Identifier name of the object.
 @field ty Type of the object.
 @field is_local True for local (stack) variables; false for globals.
 @field offset For local variables: stack offset. For globals/functions
               some fields (like code_addr) are used instead.
 @field is_function True when this Obj represents a function.
 @field code_addr For functions compiled to VM bytecode: start address
                 in the text segment.
*/
struct Obj {
    struct Obj *next;
    char *name;    // Variable name
    Type *ty;      // Type
    Token *tok;    // representative token
    bool is_local; // local or global/function
    int align;     // alignment

    // Local variable
    int offset;

    // Global variable or function
    bool is_function;
    bool is_definition;
    bool is_static;
    bool is_constexpr;

    // Global variable
    bool is_tentative;
    bool is_tls;
    char *init_data;
    Relocation *rel;
    Node *init_expr;  // For constexpr: AST of initializer expression

    // Function
    bool is_inline;
    Obj *params;
    Node *body;
    Obj *locals;
    Obj *va_area;
    Obj *alloca_bottom;
    int stack_size;

    // Static inline function
    bool is_live;
    bool is_root;
    StringArray refs;

    // Code generation (for VM)
    long long code_addr;  // Address in text segment where function code starts
};

/*!
 @struct CondIncl
 @abstract Stack entry used to track nested #if/#elif/#else processing
           during preprocessing.
*/
typedef struct CondIncl {
    struct CondIncl *next;
    enum { IN_THEN, IN_ELIF, IN_ELSE } ctx;
    Token *tok;
    bool included;
} CondIncl;

/*!
 @struct Scope
 @abstract Represents a parser block scope. Two kinds of block scopes are
           used: one for variables/typedefs and another for tags.
*/
typedef struct Scope {
    struct Scope *next;
    // C has two block scopes; one is for variables/typedefs and
    // the other is for struct/union/enum tags.
    HashMap vars;
    HashMap tags;
} Scope;

/*!
 @struct LabelEntry
 @abstract Tracks labels used for goto and labeled statements and their
           defined addresses in the generated text segment.
*/
typedef struct LabelEntry {
    char *name;           // Label name (for named labels)
    char *unique_label;   // Unique label identifier (for break/continue)
    long long *address;   // Address in text segment where label is defined
} LabelEntry;

/*!
 @struct GotoPatch
 @abstract Records a forward jump (JMP) that must be patched once the
           destination label is defined.
*/
typedef struct GotoPatch {
    char *name;           // Label name to jump to
    char *unique_label;   // Or unique label identifier
    long long *location;  // Location of JMP instruction's address operand
} GotoPatch;

typedef struct JCC JCC;

/*!
 @typedef JCCAsmCallback
 @abstract Callback invoked when an `asm("...")` statement is encountered
           during code generation.
 @param vm The VM/compiler instance.
 @param asm_str The asm string literal content.
 @param user_data User-provided context pointer (set via cc_set_asm_callback).
 @discussion The callback may emit custom bytecode into the VM's text
             segment, perform logging, or otherwise handle the asm string.
*/
typedef void (*JCCAsmCallback)(JCC *vm, const char *asm_str, void *user_data);

/*!
 @struct ForeignFunc
 @abstract Represents a registered foreign (native C) function callable from VM code.
 @field name Function name (for lookup during compilation).
 @field func_ptr Pointer to the native C function.
 @field num_args Number of arguments the function expects (total for non-variadic, fixed args for variadic).
 @field returns_double True if function returns double, false if returns long long.
 @field is_variadic True if function is variadic (accepts ... arguments).
 @field num_fixed_args For variadic functions: number of fixed args before ... (e.g., printf has 1: format string).
 @field cif libffi call interface (only when JCC_HAS_FFI is defined).
 @field arg_types Array of argument types for libffi (NULL if not prepared, only when JCC_HAS_FFI is defined).
*/
typedef struct ForeignFunc {
    char *name;
    void *func_ptr;
    int num_args;
    int returns_double;
    int is_variadic;        // 1 if function is variadic (e.g., printf), 0 otherwise
    int num_fixed_args;     // For variadic functions, number of fixed args (rest are variable)
#ifdef JCC_HAS_FFI
    ffi_cif cif;            // libffi call interface
    ffi_type **arg_types;   // Array of argument types (NULL if not prepared)
#endif
} ForeignFunc;

/*!
 @struct AllocHeader
 @abstract Metadata header stored before each heap allocation for tracking.
 @field size Size of allocation (excluding header), rounded up for alignment
 @field requested_size Original requested size (for bounds checking)
 @field magic Magic number (0xDEADBEEF) for detecting corruption.
 @field canary Front canary value for heap overflow detection (when enabled)
 @field freed Flag indicating if this block has been freed (for UAF detection)
 @field generation Generation counter incremented on each free (for UAF detection)
 @field alloc_pc Program counter at allocation site (for debugging)
*/
typedef struct AllocHeader {
    size_t size;            // Allocated size (rounded, excluding header)
    size_t requested_size;  // Original requested size (for bounds checking)
    int magic;              // Magic number for debugging (0xDEADBEEF)
    long long canary;       // Front canary (if heap canaries enabled)
    int freed;              // 1 if freed (for UAF detection)
    int generation;         // Generation counter (incremented on free)
    long long alloc_pc;     // PC at allocation site (for leak detection)
} AllocHeader;

/*!
 @struct FreeBlock
 @abstract Free list node for tracking freed memory blocks.
 @field next Pointer to next free block in the list.
 @field size Size of this free block (excluding header).
*/
typedef struct FreeBlock {
    struct FreeBlock *next;
    size_t size;
} FreeBlock;

/*!
 @struct AllocRecord
 @abstract Tracks an active heap allocation for leak detection.
 @field next Pointer to next record in the list.
 @field address Address of the allocated memory (user pointer).
 @field size Size of the allocation in bytes.
 @field alloc_pc Program counter at allocation site.
*/
typedef struct AllocRecord {
    struct AllocRecord *next;
    void *address;
    size_t size;
    long long alloc_pc;
} AllocRecord;

/*!
 @struct Breakpoint
 @abstract Represents a debugger breakpoint at a specific program counter location.
 @field pc Program counter address where the breakpoint is set.
 @field enabled Whether this breakpoint is currently active.
 @field hit_count Number of times this breakpoint has been hit.
*/
#ifndef MAX_BREAKPOINTS
#define MAX_BREAKPOINTS 256
#endif

typedef struct Breakpoint {
    long long *pc;      // PC address of breakpoint
    int enabled;        // 1 if enabled, 0 if disabled
    int hit_count;      // Number of times hit
    char *condition;    // Optional condition expression (NULL if unconditional)
} Breakpoint;

/*!
 @struct PragmaMacro
 @abstract Represents a pragma macro function.
 @field name Function name.
 @field body_tokens Original token stream for function body (from preprocessor).
 @field compiled_fn Compiled function pointer (returns Node*).
 @field macro_vm VM instance for this macro (used for recursive macro calls).
 @field next Pointer to next macro in list.
*/
typedef struct PragmaMacro {
    char *name;               // Function name
    Token *body_tokens;       // Original token stream for function body
    void *compiled_fn;        // Compiled function pointer (returns Node*)
    JCC *macro_vm;            // VM instance for this macro
    struct PragmaMacro *next; // Next macro in list
} PragmaMacro;

/*!
 @struct JCC
 @abstract Encapsulates all state for the JCC compiler and virtual
           machine. Instances are independent and support embedding.
 @discussion The structure contains registers, memory segments, frontend
             state (preprocessor, tokenizer, parser) and codegen/VM
             bookkeeping. All public API functions accept an
             `JCC *` as the first parameter.
*/
struct JCC {
    // VM Registers
    long long ax;              // Accumulator register (integer)
    double fax;                // Floating-point accumulator register
    long long *pc;             // Program counter
    long long *bp;             // Base pointer (frame pointer)
    long long *sp;             // Stack pointer
    long long cycle;           // Instruction cycle counter

    // Exit detection (for returning from main)
    long long *initial_sp;     // Initial stack pointer (for exit detection)
    long long *initial_bp;     // Initial base pointer (for exit detection)

    // Memory Segments
    long long *text_seg;       // Text segment (bytecode instructions)
    long long *text_ptr;       // Current write position (for code generation)
    long long *stack_seg;      // Stack segment
    long long *old_text_seg;   // Backup of original text segment pointer
    char *data_seg;            // Data segment (global variables/constants)
    char *data_ptr;            // Current write position in data segment
    char *heap_seg;            // Heap segment (for VM malloc/free)
    char *heap_ptr;            // Current allocation pointer (bump allocator)
    char *heap_end;            // End of heap segment
    FreeBlock *free_list;      // Head of free blocks list (for memory reuse)

    // Memory safety tracking
    AllocRecord *alloc_list;   // List of active allocations (for leak detection)

    // Configuration
    int poolsize;              // Size of memory segments (bytes)
    int debug_vm;              // Enable debug output during execution

    // Memory safety features (all disabled by default)
    int enable_bounds_checks;           // Runtime array bounds checking
    int enable_uaf_detection;           // Use-after-free detection
    int enable_type_checks;             // Runtime type checking on pointer dereferences
    int enable_uninitialized_detection; // Uninitialized variable detection
    int enable_stack_canaries;          // Stack overflow protection
    int enable_heap_canaries;           // Heap overflow protection
    int enable_pointer_sanitizer;       // Full pointer tracking and validation
    int enable_memory_leak_detection;   // Track allocations and report leaks at exit
    int enable_stack_instrumentation;   // Track stack variable lifetimes

    // Debugger state
    int enable_debugger;                // Enable interactive debugger
    Breakpoint breakpoints[MAX_BREAKPOINTS]; // Breakpoint table
    int num_breakpoints;                // Number of active breakpoints
    int single_step;                    // Single-step mode (stop after each instruction)
    int step_over;                      // Step over mode (skip function calls)
    int step_out;                       // Step out mode (run until function returns)
    long long *step_over_return_addr;   // Return address for step over
    long long *step_out_bp;             // Base pointer for step out
    int debugger_attached;              // Debugger REPL is active

    // Preprocessor state
    bool skip_preprocess;  // Skip preprocessing step
    HashMap macros;
    CondIncl *cond_incl;
    HashMap pragma_once;
    int include_next_idx;
    
    // Pragma macro system
    struct PragmaMacro *pragma_macros;  // Linked list of compile-time macros
    bool compiling_pragma_macro;        // True when compiling a pragma macro (skip main() requirement)

    // Tokenization state
    File *current_file;         // Input file
    File **input_files;         // A list of all input files.
    bool at_bol;                // True if the current position is at the beginning of a line
    bool has_space;             // True if the current position follows a space character

    // Parser state
    // All local variable instances created during parsing are
    Obj *locals;                // accumulated to this list.
    // Likewise, global variables are accumulated to this list.
    Obj *globals;
    Scope *scope;
    // Track variable being initialized (for const initialization)
    Obj *initializing_var;
    // Points to the function object the parser is currently parsing.
    Obj *current_fn;
    // Lists of all goto statements and labels in the curent function.
    Node *gotos;
    Node *labels;
    // Current "goto" and "continue" jump targets.
    char *brk_label;
    char *cont_label;
    // Points to a node representing a switch if we are parsing
    // a switch statement. Otherwise, NULL.
    Node *current_switch;
    Obj *builtin_alloca;
    Obj *builtin_setjmp;
    Obj *builtin_longjmp;

    StringArray include_paths;
    StringArray system_include_paths;  // System header search paths for <...>

    // Code generation state
    int label_counter;          // For generating unique labels
    int local_offset;           // Current local variable offset

#ifndef MAX_CALLS
#define MAX_CALLS 1024
#endif
    struct {
        long long *location; // Location in text segment to patch
        Obj *function;       // Function to call
    } call_patches[MAX_CALLS];
    int num_call_patches;

    // Function address patches for function pointers
    struct {
        long long *location; // Location of IMM operand to patch with function address
        Obj *function;       // Function whose address to use
    } func_addr_patches[MAX_CALLS];
    int num_func_addr_patches;

#ifndef MAX_LABELS
#define MAX_LABELS 256
#endif
    LabelEntry label_table[MAX_LABELS];
    int num_labels;
    GotoPatch goto_patches[MAX_LABELS];
    int num_goto_patches;

    // Inline assembly callback
    JCCAsmCallback asm_callback;  // User-provided callback for asm statements
    void *asm_user_data;               // User-provided context for callback

    // Foreign Function Interface (FFI)
    ForeignFunc *ffi_table;            // Registry of foreign C functions
    int ffi_count;                     // Number of registered functions
    int ffi_capacity;                  // Capacity of ffi_table array

    // Current function being compiled (for VLA cleanup)
    Obj *current_codegen_fn;

    // Struct/union return buffer (copy-before-return approach)
    char *return_buffer;               // Buffer for struct/union returns
    int return_buffer_size;            // Size of return buffer

    // Linked programs for extern offset propagation
    Obj **link_progs;                 // Array of original program lists
    int link_prog_count;               // Number of programs

    // Error handling (setjmp/longjmp for exception-like behavior)
    jmp_buf *error_jmp_buf;            // Jump buffer for error handling (NULL = use exit())
    char *error_message;               // Last error message (when using longjmp)
};

/*!
 @function cc_init
 @abstract Initialize an JCC instance.
 @discussion The caller should allocate an `JCC` struct (usually on the
             stack) and pass its pointer to this function. This sets up
             memory segments, default include paths, and other runtime
             defaults.
 @param vm Pointer to an uninitialized JCC struct to initialize.
 @param enable_debugger Non-zero to enable the built-in debugger.
*/
void cc_init(JCC *vm, bool enable_debugger);

/*!
 @function cc_destroy
 @abstract Free resources owned by an JCC instance.
 @discussion Does not free the `JCC` struct itself; the caller is
             responsible for the memory of the struct if it was
             dynamically allocated.
 @param vm The JCC instance to destroy.
*/
void cc_destroy(JCC *vm);

/*!
 @function cc_include
 @abstract Add a directory to the compiler's header search paths.
 @discussion This adds the path to the list of directories searched for
             "..." includes (quote includes).
 @param vm The JCC instance.
 @param path Filesystem path to add to include search.
*/
void cc_include(JCC *vm, const char *path);

/*!
 @function cc_system_include
 @abstract Add a directory to the compiler's system header search paths.
 @discussion This adds the path to the list of directories searched for
             <...> includes (angle bracket includes). System include paths
             are searched after regular include paths for "..." includes.
 @param vm The JCC instance.
 @param path Filesystem path to add to system include search.
*/
void cc_system_include(JCC *vm, const char *path);

/*!
 @function cc_define
 @abstract Define or override a preprocessor macro for the given VM.
 @param vm The JCC instance.
 @param name Macro identifier (NUL-terminated).
 @param buf Macro replacement text (NUL-terminated).
*/
void cc_define(JCC *vm, char *name, char *buf);

/*!
 @function cc_undef
 @abstract Remove a preprocessor macro definition from the VM.
 @param vm The JCC instance.
 @param name Macro identifier to remove.
*/
void cc_undef(JCC *vm, char *name);

/*!
 @function cc_set_asm_callback
 @abstract Register a callback invoked for `asm("...")` statements.
 @param vm The JCC instance.
 @param callback Callback function pointer, or NULL to unregister.
 @param user_data Optional user context pointer passed to the callback.
*/
void cc_set_asm_callback(JCC *vm, JCCAsmCallback callback, void *user_data);

/*!
 @function cc_register_cfunc
 @abstract Register a native C function to be callable from VM code via FFI.
 @param vm The JCC instance.
 @param name Function name (must match declarations in C source).
 @param func_ptr Pointer to the native C function.
 @param num_args Number of arguments the function expects.
 @param returns_double 1 if function returns double, 0 if returns long long.
 @discussion Registered functions can be called from C code compiled to VM
             bytecode. The CALLF instruction handles argument marshalling.
             All integer types are passed/returned as long long, floats as double.
*/
void cc_register_cfunc(JCC *vm, const char *name, void *func_ptr, int num_args, int returns_double);

/*!
 @function cc_register_variadic_cfunc
 @abstract Register a variadic native C function to be callable from VM code via FFI.
 @param vm The JCC instance.
 @param name Function name (must match declarations in C source).
 @param func_ptr Pointer to the native C variadic function.
 @param num_fixed_args Number of fixed arguments before the ... (e.g., printf has 1: format string).
 @param returns_double 1 if function returns double, 0 if returns long long.
 @discussion This function is only available when JCC_HAS_FFI is defined. When libffi
             is not available, use fixed-argument wrappers instead. Variadic functions
             accept a variable number of arguments after the fixed arguments.
             Example: printf has 1 fixed arg (format), fprintf has 2 (stream, format).
*/
void cc_register_variadic_cfunc(JCC *vm, const char *name, void *func_ptr, int num_fixed_args, int returns_double);

/*!
 @function cc_load_stdlib
 @abstract Register all standard library functions available via FFI.
 @param vm The JCC instance.
 @discussion Automatically registers 50+ standard library functions including:
             - Memory: malloc, free, calloc, realloc, memcpy, memmove, memset, memcmp
             - String: strlen, strcpy, strncpy, strcat, strcmp, strncmp, strchr, strstr
             - I/O: puts, putchar, getchar, fopen, fclose, fread, fwrite, fgetc, fputc
             - Math: sin, cos, tan, sqrt, pow, exp, log, floor, ceil, fabs
             - Conversion: atoi, atol, atof, strtol, strtod
             - System: exit, abort, system, open, close, read, write

             This function is automatically called by cc_init(), but can be called
             manually if you want to reset the FFI registry or initialize it separately.
*/
void cc_load_stdlib(JCC *vm);

/*!
 @function cc_dlsym
 @abstract Update an existing registered FFI function's pointer by name.
 @param vm The JCC instance.
 @param name Function name to update.
 @param func_ptr New function pointer to assign.
 @param num_args Expected number of arguments (must match registered function).
 @param returns_double Expected return type (must match registered function).
 @return 0 on success, -1 on error (function not found or signature mismatch).
 @discussion This function is useful for updating function pointers after loading
             a dynamic library, or for redirecting calls to different implementations.
             The function must already be registered via cc_register_cfunc or
             cc_register_variadic_cfunc.
*/
int cc_dlsym(JCC *vm, const char *name, void *func_ptr, int num_args, int returns_double);

/*!
 @function cc_dlopen
 @abstract Load a dynamic library and resolve all registered FFI functions.
 @param vm The JCC instance.
 @param lib_path Path to the dynamic library (.so, .dylib, .dll) or NULL for default libraries.
 @return 0 on success, -1 on error.
 @discussion This function opens a dynamic library and attempts to resolve all currently
             registered FFI functions. Functions that cannot be resolved will print warnings
             but won't fail the entire operation. If lib_path is NULL, the function searches
             in default system libraries.

             Platform-specific behavior:
             - Unix: Uses dlopen/dlsym to load .so/.dylib files
             - Windows: Uses LoadLibrary/GetProcAddress to load .dll files

             The library handle is not closed after loading to keep function pointers valid.
*/
int cc_dlopen(JCC *vm, const char *lib_path);

/*!
 @function cc_load_libc
 @abstract Load the platform's standard C library and resolve FFI functions.
 @param vm The JCC instance.
 @return 0 on success, -1 on error.
 @discussion This function automatically detects and loads the correct C library for
             the current platform:
             - macOS: /usr/lib/libSystem.dylib
             - Linux: /lib64/libc.so.6 (or /lib/libc.so.6 on 32-bit)
             - FreeBSD: /lib/libc.so.7
             - Windows: msvcrt.dll
             This is useful when you want to load stdlib functions dynamically instead
             of registering them with explicit function pointers.
*/
int cc_load_libc(JCC *vm);

/*!
 @function cc_preprocess
 @abstract Run the preprocessor on a C source file and return a token stream.
 @param vm The JCC instance.
 @param path Path to the source file to preprocess.
 @return Head of the token stream (linked Token list). Caller owns tokens.
*/
Token *cc_preprocess(JCC *vm, const char *path);

/*!
 @function cc_parse
 @abstract Parse a preprocessed token stream into an AST and produce
           a linked list of top-level Obj declarations.
 @param vm The JCC instance.
 @param tok Head of the preprocessed token stream.
 @return Linked list of top-level Obj representing globals and functions.
*/
Obj *cc_parse(JCC *vm, Token *tok);

/*!
 @function cc_parse_expr
 @abstract Parse a single C expression from token stream.
 @param vm The JCC instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse.
 @return AST node representing the parsed expression.
*/
Node *cc_parse_expr(JCC *vm, Token **rest, Token *tok);

/*!
 @function cc_parse_assign
 @abstract Parse an assignment expression from token stream (stops at commas).
 @discussion Used for parsing function arguments and other contexts where commas
             are separators rather than operators.
 @param vm The JCC instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse.
 @return AST node representing the parsed assignment expression.
*/
Node *cc_parse_assign(JCC *vm, Token **rest, Token *tok);

/*!
 @function cc_parse_stmt
 @abstract Parse a single C statement from token stream.
 @param vm The JCC instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse.
 @return AST node representing the parsed statement.
*/
Node *cc_parse_stmt(JCC *vm, Token **rest, Token *tok);

/*!
 @function cc_parse_compound_stmt
 @abstract Parse a compound statement (block) from token stream.
 @param vm The JCC instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse (should be opening brace).
 @return AST node representing the parsed compound statement.
*/
Node *cc_parse_compound_stmt(JCC *vm, Token **rest, Token *tok);

/*!
 @function cc_link_progs
 @abstract Link multiple parsed programs (Obj lists) into a single program.
 @discussion Takes an array of Obj* programs and combines them into one
             linked list. This allows multiple source files to be compiled
             together into a single program. The function handles duplicate
             definitions by preferring definitions over declarations.
 @param vm The JCC instance.
 @param progs Array of Obj* programs (linked lists from cc_parse).
 @param count Number of programs in the array.
 @return A single merged Obj* linked list containing all objects.
*/
Obj *cc_link_progs(JCC *vm, Obj **progs, int count);

/*!
 @function cc_compile
 @abstract Compile the parsed program (Obj list) into VM bytecode.
 @param vm The JCC instance.
 @param prog Linked list of top-level Obj returned by cc_parse.
*/
void cc_compile(JCC *vm, Obj *prog);

/*!
 @function cc_run
 @abstract Execute the compiled program within the VM.
 @param vm The JCC instance containing compiled bytecode.
 @param argc Argument count to pass to the program's main().
 @param argv Argument vector (NUL-terminated array of strings).
 @return Program exit code returned by main().
*/
int cc_run(JCC *vm, int argc, char **argv);

/*!
 @function cc_print_tokens
 @abstract Print a token stream to stdout (useful for debugging the
           preprocessor and tokenizer).
 @param tok Head of the token stream to print.
*/
void cc_print_tokens(Token *tok);

/*!
 @function cc_output_json
 @abstract Output C header declarations as JSON for FFI wrapper generation.
 @discussion Serializes function signatures, struct/union definitions, enum
             declarations, and global variables from the parsed AST to JSON
             format. The output includes full type information with recursive
             expansion of pointers, arrays, and aggregate types. Storage class
             specifiers (static, extern) are included. Function bodies are not
             serialized - only signatures.

             The JSON output format is:
             {
               "functions": [...],
               "structs": [...],
               "unions": [...],
               "enums": [...],
               "variables": [...]
             }
 @param f Output file stream (e.g., stdout or file opened with fopen).
 @param prog Head of the parsed AST object list (from cc_parse).
*/
void cc_output_json(FILE *f, Obj *prog);

/*!
 @function cc_save_bytecode
 @abstract Save compiled bytecode to a file for later execution.
 @discussion Serializes the text segment, data segment, and necessary
             metadata to a binary file. The file can be loaded and executed
             by cc_load_bytecode().
 @param vm The JCC instance containing compiled bytecode.
 @param path Output file path.
 @return 0 on success, -1 on error.
*/
int cc_save_bytecode(JCC *vm, const char *path);

/*!
 @function cc_load_bytecode
 @abstract Load compiled bytecode from a file.
 @discussion Deserializes bytecode previously saved with cc_save_bytecode()
             and prepares the VM for execution with cc_run().
 @param vm The JCC instance to load bytecode into.
 @param path Input file path.
 @return 0 on success, -1 on error.
*/
int cc_load_bytecode(JCC *vm, const char *path);

/*!
 @function cc_add_breakpoint
 @abstract Add a breakpoint at a specific program counter address.
 @param vm The JCC instance.
 @param pc Program counter address where breakpoint should be set.
 @return Breakpoint index, or -1 if failed (too many breakpoints).
*/
int cc_add_breakpoint(JCC *vm, long long *pc);

/*!
 @function cc_remove_breakpoint
 @abstract Remove a breakpoint by index.
 @param vm The JCC instance.
 @param index Breakpoint index to remove.
*/
void cc_remove_breakpoint(JCC *vm, int index);

/*!
 @function cc_debug_repl
 @abstract Enter interactive debugger REPL (Read-Eval-Print Loop).
 @param vm The JCC instance.
 @discussion Provides an interactive shell for debugging with commands like:
             - break/b: Set breakpoint
             - continue/c: Continue execution
             - step/s: Single step
             - next/n: Step over
             - finish/f: Step out
             - print/p: Print registers
             - stack/st: Print stack
             - help/h: Show help
*/
void cc_debug_repl(JCC *vm);

#ifdef __cplusplus
}
#endif
#endif
