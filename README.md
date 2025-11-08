# jcc

> [!WARNING]
> Work in progress, see [TODO](#todo)

`JCC` is a ~~C89~~/~~C99~~/C11* JIT C Compiler. The preprocessor/lexer/parser is taken from [chibicc](http://https://github.com/rui314/chibicc) and the VM was built off [c4](https://github.com/rswier/c4) and [write-a-C-interpreter](https://github.com/lotabout/write-a-C-interpreter). 

**\*** Most C11 features implemented, see [TODO](#todo) for features still missing

The goal of this project is correctness and safety. This is just a toy, and won't be 🚀🔥 **BLAZING FAST** 🔥🚀. I wouldn't recommend using this for anything important or in any production code. I'm not an expert so safety features may not be perfect. They will also have performance overhead (depends on the features enabled).

`JCC` is not just a JIT compiler. It also extends the C preprocessor with new features, see [Pragma Macros](#pragma-macros) for more details. I have lots of other ideas for more `#pragma` extensions too.

## Features

### Core C Language Support

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`, unary `-`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- Increment/decrement: `++`, `--` (both prefix and postfix)
- Ternary: `? :`
- Comma: `,`

### Control Flow

- `if`/`else`
- `for`, `while`, `do-while`
- `switch`/`case`/`default`
- `goto`/ labels
- `break`/`continue`

### Functions

- Function declarations and definitions
- Function calls (direct and indirect via function pointers)
- Recursion
- Parameters and return values
- Function pointers (declaration, assignment, indirect calls via `CALLI`)
- Variadic functions (`<stdarg.h>` support with `va_start`, `va_arg`, `va_end`)

### Data Types

- Basic types: `void`, `char`, `short`, `int`, `long`, `float`, `double`, `_Bool`
- Pointers (declaration, dereference `*`, address-of `&`, pointer arithmetic)
- Arrays (fixed-size, initialization, multidimensional, indexing)
- Array decay (in function parameters and expressions)
- Strings (literals with data segment storage)
- Structs (declaration, member access `.`, nested structs, initialization, flexible array members)
- Unions (declaration, member access, initialization)
- Enums (declaration, explicit values, in expressions and switches)
- Variable-length arrays (VLA with runtime allocation via VM heap)

### Storage Classes & Qualifiers

- `static` (static locals, static globals, static functions)
- `extern` (external linkage for multi-file projects)
- `const` (const-correctness in type system and codegen)
- `inline` (parsed and generated normally, behaves like static, no special optimization)
- `register` (accepted and ignored, no special optimization)
- `volatile` (accepted and ignored, VM doesn't optimize across statements)
- `restrict` (accepted and ignored, aliasing not tracked)

### Other Features

- `typedef` (all types including function pointers)
- `sizeof` operator (compile-time evaluation for all types)
- Forward declarations (incomplete types, self-referential structs)
- Floating-point arithmetic (double precision with full operations)
- Compound literals (scalars, arrays, structs)
- Statement expressions (GNU extension: `({ ... })`)
- Designated initializers (arrays and structs)
- Cast expressions (explicit type conversions)

### Preprocessor

- `#include` (with multiple search paths)
- `#define` (object-like and function-like macros)
- `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`
- `#undef`
- `#pragma once`
- Macro expansion and stringification
- String literal concatenation (adjacent strings automatically combined)

### Linker

- Multiple input files
- Symbol resolution
- Duplicate definition detection

### Foreign Function Interface (FFI)

- Direct calls to native C standard library via `CALLF` opcode
- Standard library functions supported (see [Standard Library Support](#standard-library-support))
- Custom function registration via `cc_register_cfunc()` (fixed-arg) and `cc_register_variadic_cfunc()` (variadic)
- **Optional libffi support** for true variadic foreign functions (`printf`, `scanf`, etc.)
  - Build with `make JCC_HAS_FFI=1` to enable libffi support
  - Automatically uses platform-specific calling conventions via libffi
  - Falls back to macro-based dispatch if libffi is not available
- Dual-mode operation: works with or without libffi (see [Variadic Foreign Functions](#variadic-foreign-functions))

### Inline Assembly

- `asm("...")` statements via callback mechanism
- Not executed in the VM (no-op by default)
- Callback can emit custom bytecode or perform logging

### GNU Extensions

- `({ ... })` statement expressions
- `typeof` operator (compile-time type inquiry for variables and expressions)
- `__attribute__((...))` for functions and variables (currently ignored, parsed only)

### C11 Extensions

- `_Generic` type-generic expressions (compile-time type selection)
- `_Alignof` operator (query type/variable alignment)
- `_Alignas` specifier (control variable alignment)
- `_Static_assert` (compile-time assertions with custom error messages)

### Threading + Atomics Support

> [!WARNING]
> Not supported (yet)

JCC is single-threaded and does not implement any threading or atomic operations yet.

## Pragma Macros

> [!WARNING]
> This is a very early feature and a wip. The API may change in the future and some features may not be fully implemented.

Pragma macros are a feature that allows you to define macros that are expanded at compile time. Create functions, structs, unions, enums, and variables at compile time. There is a comprehensive reflection API. A common problem in C is creating "enum_to_string" functions. Now you can write a single pragma macro that generates the function at compile time for *any* enum:

```TODO: Example```

### JSON Output

Create JSON files from header files containing all functions, structs, unions, enums, and variables. Useful for FFI wrapper generation?

```bash
$ ./jcc --json -o lib.json lib.h
```

```json
{
    "functions": [...],
    "structs": [...],
    "unions": [...],
    "enums": [...],
    "variables": [...]
}
```

## TODO

### C11 Features

- [ ] Anonymous structs/unions (Direct member access without intermediate name)
- [ ] Bitfields - Bit-level struct member access

### Clang + GNU Extensions 

- [ ] Universal character names - `\uXXXX` escapes in strings
- [ ] Blocks/closures - `^{}` (**important**)
- [ ] Zero-length arrays - `int arr[0];` as flexible array alternative
- [ ] Nested functions
- [ ] Labels as values - `&&label` and `goto *ptr;`
- [ ] Switch case ranges - `case 1 ... 5:`

### C23 Features (very limited support)
- [ ] #elifdef and #elifndef directives for cleaner conditional compilation
- [ ] #warning directive standardized
- [ ] #embed directive for embedding binary data directly
- [ ] `__VA_OPT__` for better variadic macro handling
- [ ] `[[deprecated]]` for marking deprecated code
    - [ ] Also support GNU `__attribute__((deprecated))` (and other matching attributes if possible)
- [ ] `[[nodiscard]]` for warning about ignored return values
- [ ] `[[maybe_unused]]` for suppressing unused warnings
- [ ] `[[noreturn]]` and `[[ _Noreturn]]` for non-returning functions
- [ ] `[[unsequenced]], [[reproducible]]` for function properties
- [ ] `[[fallthrough]]` for intentional switch fallthrough
- [ ] Binary integer literals (0b prefix)
- [ ] Digit separators with single quotes (e.g., 1’000’000)
- [ ] Empty initializer lists {}

### Memory safety features

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

### Advanced Pointer Tracking Features

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
  - CHKPA opcode validates pointer arithmetic results
  - Requires `--provenance-tracking` to be enabled
  - Checks that `ptr` stays within `[base, base+size]` after arithmetic
  - Detects out-of-bounds pointer computations before dereference
  - Prevents pointer escape from original object

### Future Memory Safety Features

- [ ] `--stack-instrumentation` flag for tracking stack variable lifetimes and accesses
- [ ] `--ffi-type-checking` flag for runtime type checking on FFI calls
- [ ] `--ffi-allow` and `--ffi-deny` flags for whitelisting and blacklisting functions to be exposed to the FFI

#### Example Usage

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

### Interactive Debugger

The JCC VM includes an interactive, source-level debugger for step-by-step program execution and inspection.

**Enable with:** `-g` or `--debug` flags

When enabled, the debugger provides a powerful GDB-like interface for controlling program flow and inspecting state.

### Features

- **Source-Level Debugging**: The debugger maps bytecode instructions to their original source code locations. When you step through the code, it displays the current file, line number, and the corresponding source line, providing a seamless debugging experience.
- **Advanced Breakpoints**: Set breakpoints using multiple formats:
    - By line number in the current file (`break 42`).
    - By file and line number (`break test.c:42`).
    - At the entry point of a function (`break main`).
    - At a raw bytecode offset (legacy support).
- **Conditional Breakpoints**: Set breakpoints that only trigger when a specific condition is met. The expression can use local and global variables, arithmetic, comparison, and logical operators.
    - Syntax: `break <location> if <expression>`
    - Example: `break 22 if x > 5`
- **Watchpoints (Data Breakpoints)**: Break execution when memory is read or written. Watchpoints can be set on variables by name or on raw memory addresses.
    - `watch <var|addr>`: Break on write.
    - `rwatch <addr>`: Break on read.
    - `awatch <addr>`: Break on read or write.
- **Execution Control**: Full control over program flow with commands to step into (`step`), step over (`next`), and step out of (`finish`) functions.
- **State Inspection**: Inspect VM registers, the call stack, and raw memory at any address. The debugger tracks local and global variable names, allowing them to be used in expressions.

#### Debugger Commands

| Command | Short | Description |
|---|---|---|
| `continue` | `c` | Continue execution until next breakpoint or watchpoint. |
| `step` | `s` | Single step, stepping into function calls. |
| `next` | `n` | Step over, executing function calls without stopping. |
| `finish` | `f` | Run until the current function returns. |
| `break <loc>` | `b <loc>` | Set a breakpoint at a specified location (`<line>`, `<file:line>`, `<func>`, or `<offset>`). |
| `break <loc> if <expr>` | `b <loc> if <expr>` | Set a conditional breakpoint. |
| `watch <var/addr>` | `w <var/addr>` | Set a watchpoint to break on writes to a variable or address. |
| `rwatch <addr>` | | Set a watchpoint to break on reads from an address. |
| `awatch <addr>` | | Set a watchpoint to break on reads or writes to an address. |
| `info watch` | | List all active watchpoints. |
| `delete <num>` | `d <num>` | Delete a breakpoint by its number. |
| `list` | `l` | List all breakpoints. |
| `registers` | `r` | Print all register values. |
| `stack [count]` | `st [count]` | Print the top `count` entries of the stack (default 10). |
| `disasm` | `dis` | Disassemble the current instruction. |
| `memory <addr>` | `m <addr>` | Inspect memory at a given address. |
| `help` | `h`, `?` | Show the help message. |
| `quit` | `q` | Exit the debugger and terminate the program. |

#### Example Debugging Session

```bash
$ ./jcc -g test_debugger_enhanced.c

========================================
    JCC Debugger
========================================
Starting at entry point...
Type 'help' for commands, 'c' to continue

(jcc-dbg) break 20            # Set breakpoint at line 20
Breakpoint #0 set at test_debugger_enhanced.c:20

(jcc-dbg) continue            # Run to breakpoint
Breakpoint #0 hit at test_debugger_enhanced.c:20
At test_debugger_enhanced.c:20
    20:     int x = 10;
0xc33400018 (offset 24): LEA -4

(jcc-dbg) watch x             # Watch for writes to variable 'x'
Watchpoint #0: watch x

(jcc-dbg) step                # Execute one instruction (the assignment to x)
Watchpoint #0 hit: write to x at 0x7ffeea28d3f8
Old value: 0
New value: 10
At test_debugger_enhanced.c:21
    21:     int y = factorial(4);

(jcc-dbg) stack 3             # Check the stack
=== Stack (top 3 entries) ===
  sp[0] = 0x000000000000000a  (10)
  ...

(jcc-dbg) continue            # Run to completion
```

### Quality-of-Life Features

- [ ] **Optimization passes** - Constant folding, dead code elimination
- [ ] **Better error messages** - Line numbers in runtime errors
- [ ] **Specify C versions** - `-std=c89`, `-std=c11` etc

## Building

```bash
make            # Build jcc compiler
make all        # Build everything (jcc, libjcc.dylib) and run tests

# Optional: Build with libffi support for true variadic foreign functions
make JCC_HAS_FFI=1
# or export JCC_HAS_FFI=1 && make
```

This produces:
- `jcc` - Full compiler executable (C source → bytecode → execute)
- `libjcc.dylib` - Shared library for embedding

### libffi Support

To enable true variadic foreign function support, build with libffi:

**macOS (Homebrew):**
```bash
brew install libffi
make JCC_HAS_FFI=1
```

**Linux:**
```bash
# Debian/Ubuntu
sudo apt-get install libffi-dev

# Fedora/RHEL
sudo dnf install libffi-devel

# Then build
make JCC_HAS_FFI=1
```

With libffi enabled:
- Variadic foreign functions (`printf`, `scanf`, `fprintf`, etc.) work with any number of arguments
- No need for macro-based dispatch
- Standard headers (`stdio.h`) use real variadic declarations
- Backward compatible: code written for non-libffi mode still works

### Compile to Bytecode

```bash
# Compile C source to bytecode file
./jcc -o program.bin program.c

# With multiple files
./jcc -o app.bin main.c utils.c helpers.c

# With preprocessor flags
./jcc -I./include -DDEBUG -o debug.bin main.c
```

### Bytecode File Format

Binary format (little-endian):
```
[Magic: "JCC\0" (4 bytes)]
[Version: 1 (4 bytes)]
[Text size: bytes (8 bytes)]
[Data size: bytes (8 bytes)]
[Main offset: instruction offset (8 bytes)]
[Text segment: bytecode instructions]
[Data segment: global variables and constants]
```

## Running Tests

All test files are located in the `tests/` directory. To run the complete test suite:

```bash
./run_tests.fish
```

This will run all test files and report:
- Total number of tests
- Number of passed tests
- Number of failed tests
- List of any failed tests

Individual tests can be run directly:

```bash
./jcc tests/test_simple.c
echo $status  # Check exit code
```

## Standard Library Support

JCC includes a **Foreign Function Interface (FFI)** that allows compiled C code to call native standard library functions directly, without reimplementing them as VM opcodes. The FFI is automatically initialized by `cc_init()` via `cc_load_stdlib()`, which registers most C standard library functions.

### Variadic Foreign Functions

JCC supports variadic foreign functions (`printf`, `scanf`, etc.) in **two modes**:

#### Mode 1: With libffi (Recommended)

Build with `make JCC_HAS_FFI=1` to enable true variadic support:

**How it works:**
1. Variadic functions are registered with `cc_register_variadic_cfunc()`
2. At call time, the actual argument count is passed to the `CALLF` opcode
3. libffi's `ffi_prep_cif_var()` prepares the call interface dynamically
4. Native function is called with correct platform-specific calling convention
5. **No limit** on number of arguments (platform-dependent, typically 100+)

**Advantages:**
- True C variadic functions - no macro magic
- Standard headers use real `...` syntax: `int printf(const char *fmt, ...);`
- Works with any number of arguments
- Fully portable (libffi handles all platforms)

#### Mode 2: Without libffi (Fallback)

When built without libffi, variadic functions use macro-based dispatch:

**How it works:**
1. `printf(...)` is a preprocessor macro that **counts arguments at compile time**
2. Macro expands to `printf0`, `printf1`, ... `printf16` based on argument count
3. Each `printfN` is a fixed-argument FFI function registered with the VM
4. The FFI function calls native `printf()` with the exact number of arguments

**Limitation:** Maximum **20 additional arguments** (format string counts as 1). This covers 99% of real-world usage and can be increased if needed.

**Why this mode exists:**
- Works without external dependencies
- Portable and simple
- Compile-time argument counting (no runtime overhead)
- Useful for embedded environments or when libffi is unavailable

### Registering Custom Functions

You can register additional native functions using `cc_register_cfunc()` (fixed-arg) or `cc_register_variadic_cfunc()` (variadic):

```c
// Example: Register a custom fixed-argument function
void my_native_func(int x, double y) {
    printf("Called with: %d, %f\n", x, y);
}

JCC vm;
cc_init(&vm);

// Register fixed-arg function: name, function pointer, arg count, returns_double
cc_register_cfunc(&vm, "my_native_func", (void*)my_native_func, 2, 0);

// Register variadic function (requires libffi, JCC_HAS_FFI=1)
#ifdef JCC_HAS_FFI
// Register: name, function pointer, num_fixed_args, returns_double
cc_register_variadic_cfunc(&vm, "my_printf", (void*)printf, 1, 0);
// num_fixed_args = 1 (format string is the fixed arg, rest are variadic)
#endif

// Now they can be called from C code compiled to VM bytecode
```

## LICENSE
```
jcc

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
```
