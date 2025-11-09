# JCC Memory Safety Features

JCC includes a suite of powerful memory safety features designed to detect common C programming errors at runtime. These features can be enabled individually or together to provide comprehensive protection against bugs like buffer overflows, use-after-free, and type confusion.

## Memory safety features

- [x] `--stack-canaries` **Stack overflow protection**
  - Places canary values (0xDEADBEEFCAFEBABE) on the stack between saved base pointer and local variables
  - Validates canary on function return (LEV instruction)
  - Detects stack buffer overflows with detailed error reporting including PC offset
- [x] `--heap-canaries` **Heap overflow protection**
  - Front canary in AllocHeader (before user data)
  - Rear canary after user data
  - Both canaries (0xCAFEBABEDEADBEEF) validated on free()
  - Detects heap buffer over/underflows with allocation site information
- [x] `--memory-leak-detection` **Memory leak detection**
  - Tracks all VM heap allocations in a linked list
  - Removes from list on free()
  - Reports all unfreed allocations at program exit
  - Shows address, size, and PC offset of allocation site for each leak
- [x] `--uaf-detection` **Use-after-free detection**
  - Marks freed blocks instead of reusing them
  - Increments generation counter on each free
  - CHKP opcode checks if accessed pointer has been freed
  - Reports UAF with allocation details and generation number
- [x] `--bounds-checks` **Runtime array bounds checking**
  - Tracks requested vs allocated sizes for all heap allocations
  - CHKP opcode validates pointer is within allocated region
  - Checks against originally requested size (not rounded allocation)
  - Detects out-of-bounds array accesses with offset information
- [x] `--type-checks` **Runtime type checking on pointer dereferences**
  - Tracks allocation type information in heap headers
  - CHKT opcode validates pointer type matches expected type on dereference
  - Detects type confusion bugs (e.g., casting `int*` to `float*`)
  - Only checks heap allocations (stack types not tracked at runtime)
  - Skips checks for `void*` and generic pointers (universal pointers)
- [x] `--uninitialized-detection` **Uninitialized variable detection**
  - Tracks initialization state of stack variables using HashMap
  - MARKI opcode marks variables as initialized after assignment
  - CHKI opcode validates variable is initialized before read
  - Detects use of uninitialized local variables with stack offset info
  - HashMap key: BP address + offset for per-function-call tracking
- [x] `--pointer-sanitizer` **Comprehensive pointer checking (convenience flag)**
  - Enables `--bounds-checks`, `--uaf-detection`, and `--type-checks` together
  - Provides comprehensive pointer safety in a single flag
  - Recommended for development and testing

## Advanced Pointer Tracking Features

- [x] `--dangling-pointers` **Dangling stack pointer detection**
  - Tracks all stack pointer creations via MARKA opcode
  - Invalidates pointers when function returns (LEV instruction)
  - CHKP validates pointer hasn't been invalidated before dereference
  - Detects use-after-return bugs (e.g., returning `&local_var`)
  - HashMap tracks: pointer value → {BP, stack offset, size}
- [x] `--alignment-checks` **Pointer alignment validation**
  - CHKA opcode validates pointer alignment before dereference
  - Checks that `pointer % type_size == 0`
  - Detects misaligned memory access (e.g., `int*` at odd address)
  - Only checks types larger than 1 byte
- [x] `--provenance-tracking` **Pointer origin tracking**
  - Tracks pointer provenance: HEAP, STACK, or GLOBAL
  - MARKP opcode records origin when pointers are created
  - Automatically tracks heap allocations in MALC opcode
  - HashMap stores: pointer → {origin_type, base, size}
  - Enables validation of pointer operations within original object bounds
- [x] `--invalid-arithmetic` **Pointer arithmetic bounds checking**
  - Requires `--provenance-tracking` to be enabled
  - Checks that `ptr` stays within `[base, base+size]` after arithmetic
  - Detects out-of-bounds pointer computations before dereference
  - Prevents pointer escape from original object
- [x] `--stack-instrumentation` **Stack variable lifetime and access tracking**
  - Tracks all stack variable lifetimes with full block-level scoping
  - SCOPEIN/SCOPEOUT opcodes mark scope entry/exit for each `{ }` block
  - CHKL opcode validates variable is alive before access
  - MARKR/MARKW opcodes track read/write counts for each variable
  - Detects use-after-scope and use-after-return bugs
  - Stack overflow detection: tracks high water mark, warns at 90% threshold
  - Integrated with `--dangling-pointers` for comprehensive temporal safety
  - Use `--stack-errors` flag to enable runtime errors (vs logging only)
  - Use `cc_print_stack_report()` API to print access statistics

## FFI Safety Features

- [x] `--ffi-allow=func1,func2` **FFI function whitelist**
  - Comma-separated list of allowed FFI function names
  - When allow list is non-empty, only listed functions can be called via FFI
  - Enforced at runtime during CALLF instruction execution
  - Use with `cc_ffi_allow()` API for programmatic configuration
- [x] `--ffi-deny=func1,func2` **FFI function blacklist**
  - Comma-separated list of denied FFI function names
  - Prevents specific functions from being called via FFI
  - Only checked when allow list is empty
  - Use with `cc_ffi_deny()` API for programmatic configuration
- [x] `--disable-ffi` **Disable all FFI calls**
  - Completely blocks all foreign function calls at runtime
  - Overrides both allow and deny lists
  - Useful for sandboxing untrusted code
- [x] `--ffi-errors-fatal` **Make FFI errors abort execution**
  - By default, FFI safety violations print warnings and skip the call
  - With this flag, violations cause the program to abort with exit code 1
  - Provides strict enforcement mode for production environments
- [x] `--ffi-type-checking` **Runtime type validation on FFI calls**
  - Validates argument counts match registered FFI function signatures
  - Checks argument types are compatible with expected parameter types
  - Detects type mismatches at compile time before calling native code
  - Supports variadic functions (only checks fixed parameters)
  - Uses lenient compatibility rules (void* with any pointer, all integers, all floats)

## Example Usage

### Use-after-free
```c
// test_uaf.c - Use-after-free example
void *malloc(unsigned long size);
void free(void *ptr);

int main() {
    int *ptr = (int *)malloc(sizeof(int) * 10);
    ptr[0] = 42;
    free(ptr);
    int value = ptr[0];  // Use after free!
    return value;
}
```

```bash
$ ./jcc --uaf-detection test_uaf.c

========== USE-AFTER-FREE DETECTED ==========
Attempted to access freed memory
Address:     0x7f3640028
Size:        40 bytes
Allocated at PC offset: 15
Generation:  1 (freed)
Current PC:  0x7f34002b0 (offset: 86)
============================================
```

### Bounds Checking
```c
// test_bounds.c - Bounds checking example
void *malloc(unsigned long size);

int main() {
    char *arr = (char *)malloc(10);
    char c = arr[10];  // Out of bounds!
    return c;
}
```

```bash
$ ./jcc --bounds-checks test_bounds.c

========== ARRAY BOUNDS ERROR ==========
Pointer is outside allocated region
Address:       0x8c564003a
Base:          0x8c5640030
Offset:        10 bytes
Requested size: 10 bytes
Allocated size: 16 bytes (rounded)
Allocated at PC offset: 11
Current PC:    0x8c5400140 (offset: 40)
=========================================
```

### Type Checking
```c
// test_type_check.c - Type checking example
void *malloc(unsigned long size);

int main() {
    int *int_ptr = (int *)malloc(sizeof(int) * 10);
    int_ptr[0] = 42;

    // Type confusion: treating int* as float*
    float *float_ptr = (float *)int_ptr;
    float value = *float_ptr;  // Type mismatch!
    return (int)value;
}
```

```bash
$ ./jcc --type-checks test_type_check.c

========== TYPE MISMATCH DETECTED ==========
Pointer type mismatch on dereference
Address:       0x7f8640028
Expected type: float
Actual type:   int
Allocated at PC offset: 15
Current PC:    0x7f84002d8 (offset: 52)
============================================
```

### Uninitialized Variables
```c
// test_uninit.c - Uninitialized variable example

int main() {
    int x;
    int y = 10;
    int z = x + y;  // Reading uninitialized variable x!
    return z;
}
```

```bash
$ ./jcc --uninitialized-detection test_uninit.c

========== UNINITIALIZED VARIABLE READ ==========
Attempted to read uninitialized variable
Stack offset: -1
Address:      0x7ffee4b3f8
BP:           0x7ffee4b400
PC:           0x7ffe400120 (offset: 32)
================================================
```

### Dangling Pointers
```c
// test_dangling_pointer.c - Dangling stack pointer example

int *get_local_address() {
    int x = 42;
    return &x;  // Return address of local variable (dangling pointer!)
}

int main() {
    int *ptr = get_local_address();
    int value = *ptr;  // Dereference dangling pointer!
    return value;
}
```

```bash
$ ./jcc --dangling-pointers test_dangling_pointer.c

========== DANGLING STACK POINTER ==========
Attempted to dereference invalidated stack pointer
Address:       0x92e5fffb0
Original BP:   invalidated (function has returned)
Stack offset:  -1
Size:          4 bytes
Current PC:    0x92e800080 (offset: 16)
==========================================
```

### Pointer Alignment
```c
// test_alignment.c - Pointer alignment example

void *malloc(unsigned long size);

int main() {
    char *buffer = (char *)malloc(16);

    // Create a misaligned int pointer (offset by 1 byte)
    int *misaligned = (int *)(buffer + 1);

    int value = *misaligned;  // Alignment error!
    return value;
}
```

```bash
$ ./jcc --alignment-checks test_alignment.c

========== ALIGNMENT ERROR ==========
Pointer is misaligned for type
Address:       0x100c8eac1
Type size:     4 bytes
Required alignment: 4 bytes
Current PC:    0xb98800148 (offset: 41)
=====================================
```

### FFI Deny List
```c
// test_ffi_deny.c - FFI deny list example

int printf0(const char *fmt);

int main() {
    printf0("Attempting to call blocked function\n");
    return 0;
}
```

```bash
$ ./jcc --ffi-deny=printf0 test_ffi_deny.c

========== FFI SAFETY ERROR ==========
Error type: FFI Access Denied
Function:   printf0
Details:    Function in deny list
PC offset:  12
======================================
```

### FFI Allow List
```c
// test_ffi_allow.c - FFI allow list example

int printf0(const char *fmt);
void *malloc(unsigned long size);

int main() {
    printf0("This call is allowed\n");
    malloc(100);  // This will be blocked
    return 0;
}
```

```bash
$ ./jcc --ffi-allow=printf0 test_ffi_allow.c

This call is allowed

========== FFI SAFETY ERROR ==========
Error type: FFI Access Denied
Function:   malloc
Details:    Function not in allow list
PC offset:  25
======================================
```

### Disable All FFI
```c
// test_disable_ffi.c - Disable all FFI calls

int printf0(const char *fmt);

int main() {
    printf0("This FFI call will be blocked\n");
    return 0;
}
```

```bash
$ ./jcc --disable-ffi test_disable_ffi.c

========== FFI SAFETY ERROR ==========
Error type: FFI Disabled
Function:   printf0
Details:    All FFI calls are disabled via --disable-ffi
PC offset:  12
======================================
```

### Fatal FFI Errors
```bash
# Default behavior: warnings only
$ ./jcc --ffi-deny=printf0 test.c
<FFI error printed, program continues, returns 0>

# Fatal mode: abort on error
$ ./jcc --ffi-deny=printf0 --ffi-errors-fatal test.c
<FFI error printed, program aborts with exit code 1>
```

### FFI Type Checking - Argument Count Mismatch
```c
// test_ffi_arg_count.c - Wrong number of arguments
int strcmp(const char *s1, const char *s2);

int main() {
    // strcmp expects 2 arguments, but only 1 provided
    int result = strcmp("hello");
    return result;
}
```

```bash
$ ./jcc --ffi-type-checking test_ffi_arg_count.c

test_ffi_arg_count.c:6: FFI function 'strcmp': argument count mismatch (expected 2, got 1)
    int result = strcmp("hello");
                 ^
```

### FFI Type Checking - Type Mismatch
```c
// test_ffi_type_mismatch.c - Wrong argument type
unsigned long strlen(const char *s);

int main() {
    int not_a_string = 42;
    // strlen expects char*, but we're passing int
    unsigned long len = strlen(not_a_string);
    return len;
}
```

```bash
$ ./jcc --ffi-type-checking test_ffi_type_mismatch.c

test_ffi_type_mismatch.c:7: FFI function 'strlen': argument 1 type mismatch
  Expected: char*
  Actual:   int
    unsigned long len = strlen(not_a_string);
                               ^
```

### FFI Type Checking - Valid Calls
```c
// test_ffi_valid.c - All types match correctly
void *malloc(unsigned long size);
void free(void *ptr);
int strcmp(const char *s1, const char *s2);

int main() {
    void *ptr = malloc(100);  // OK: unsigned long argument
    int cmp = strcmp("a", "b");  // OK: two char* arguments
    free(ptr);  // OK: void* argument
    return 0;
}
```

```bash
$ ./jcc --ffi-type-checking test_ffi_valid.c
<Program compiles and runs successfully>
```

## API Usage

The FFI safety features can also be controlled programmatically using the JCC API:

```c
#include "jcc.h"

int main() {
    JCC vm;
    cc_init(&vm, false);

    // Configure FFI safety
    cc_ffi_allow(&vm, "malloc");
    cc_ffi_allow(&vm, "free");
    cc_ffi_deny(&vm, "system");
    cc_ffi_deny(&vm, "exec");

    vm.disable_all_ffi = 0;           // Allow FFI (with restrictions)
    vm.ffi_errors_fatal = 1;          // Make errors fatal
    vm.enable_ffi_type_checking = 1;  // Enable type checking

    // Compile and run code...
    Token *tok = cc_preprocess(&vm, "program.c");
    Obj *prog = cc_parse(&vm, tok);
    cc_compile(&vm, prog);
    int exit_code = cc_run(&vm, argc, argv);

    // Clean up allow/deny lists
    cc_ffi_clear_allow_list(&vm);
    cc_ffi_clear_deny_list(&vm);

    cc_destroy(&vm);
    return exit_code;
}
```
