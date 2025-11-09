# jcc

> [!WARNING]
> Work in progress, see [TODO](#todo)

`JCC` is a ~~C89~~/~~C99~~/C11* JIT C Compiler. The preprocessor/lexer/parser is taken from [chibicc](http://https://github.com/rui314/chibicc) and the VM was built off [c4](https://github.com/rswier/c4) and [write-a-C-interpreter](https://github.com/lotabout/write-a-C-interpreter). 

**\*** Most C11 features implemented, see [TODO](#todo) for features still missing

The goal of this project is correctness and safety. This is just a toy, and won't be 🚀🔥 **BLAZING FAST** 🔥🚀. I wouldn't recommend using this for anything important or in any production code. I'm not an expert, so safety features may not be perfect and will have *minimal* to **significant** performance overhead (depending on which features are enabled).

`JCC` is not just a JIT compiler. It also extends the C preprocessor with new features, see [Pragma Macros](#pragma-macros) for more details. I have lots of other ideas for more `#pragma` extensions too.

## Features

### Debugger

The debugger is a GDB-like interface for controlling program flow and inspecting state. It is enabled with the `-g` or `--debug` flags. See [Debugger](./DEBUGGER.md) for more details.

### Memory Safety

There are lots of memory safety features available. See [SAFETY.md](./SAFETY.md) for more details.

- `--stack-canaries` **Stack overflow protection**
- `--heap-canaries` **Heap overflow protection**
- `--random-canaries` **Random stack canaries (prevents predictable bypass)**
- `--memory-poisoning` **Poison allocated/freed memory (0xCD/0xDD patterns)**
- `--memory-leak-detection` **Memory leak detection**
- `--uaf-detection` **Use-after-free detection**
- **Double-free detection** (always enabled with VM heap)
- `--bounds-checks` **Runtime array bounds checking**
- `--type-checks` **Runtime type checking on pointer dereferences**
- `--uninitialized-detection` **Uninitialized variable detection**
- `--overflow-checks` **Signed integer overflow detection**
- `--pointer-sanitizer` **Comprehensive pointer checking (convenience flag)**
- `--dangling-pointers` **Dangling stack pointer detection**
- `--alignment-checks` **Pointer alignment validation**
- `--provenance-tracking` **Pointer origin tracking**
- `--invalid-arithmetic` **Pointer arithmetic bounds checking**
- `--stack-instrumentation` **Stack variable lifetime and access tracking**
- `--format-string-checks` **Format string validation for printf-family functions**
- `--memory-tagging` **Temporal memory tagging (track pointer generation tags)**
- `--control-flow-integrity` **Control flow integrity (shadow stack for return address validation)**
- `--vm-heap` **Route all malloc/free through VM heap (enables memory safety features)**

> [!NOTE]
> **VM Heap Mode**: Most memory safety features require heap allocations to go through the VM's internal allocator (MALC/MFRE opcodes) rather than system malloc/free. The `--vm-heap` flag automatically intercepts malloc/free calls at compile time and routes them through the VM heap. Without this flag, memory safety checks only apply to code that directly uses VM heap opcodes. Use `--vm-heap` in combination with other safety flags for comprehensive protection.

### Pragma Macros

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
## Core C Language Support

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
