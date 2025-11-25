# jcc

`JCC` is a **J**IT **C**11 **C**ompiler. The preprocessor/lexer/parser is built from [chibicc](http://https://github.com/rui314/chibicc) and the VM was built from [c4](https://github.com/rswier/c4) and [write-a-C-interpreter](https://github.com/lotabout/write-a-C-interpreter).

## About

The goal of this project is correctness and safety. This is just a toy, and won't be 🚀🔥 **BLAZING FAST** 🔥🚀. I wouldn't recommend using this for anything important or in any production code. I'm not an expert, so safety features may not be perfect and will have *minimal* to **significant** performance overhead (depending on which features are enabled).

`JCC` is not just a JIT compiler. It also extends the C preprocessor with new features, see [Pragma Macros](#pragma-macros) for more details. I have lots of other ideas for more `#pragma` extensions too.

**Status**: All C11 language features are supported (minus `<threads.h>` and `_Thread_local`, see [Thread Support](#threading-and-atomics-support)). There is also partial C23 support (mostly parser/preprocessor features).

See [here](https://takeiteasy.github.io/jcc) for some basic documentation on the API.

## Features

### Debugger

The debugger is a GDB-like interface for controlling program flow and inspecting state. It is enabled with the `-g` or `--debug` flags. See [DEBUGGER.md](./DEBUGGER.md) for more details.

### Memory Safety

There are lots of memory safety features available. See [SAFETY.md](./SAFETY.md) for more details.

#### Safety Levels

JCC provides preset safety levels that automatically enable appropriate combinations of safety features. These make it easy to choose the right balance between safety and performance without needing to remember individual flags. These overhead percentages are just guesses by the way. Pure speculation. Source? I made them up.

**Level 0: None** (`-0` or `--safety=none`)
- No safety checks
- Maximum performance
- Use when safety has been validated or performance is critical

**Level 1: Basic** (`-1` or `--safety=basic`)
- Essential low-overhead checks (~5-10% overhead)
- Enables: Stack/heap canaries, memory leak detection, overflow checks, format string checks, VM heap
- Recommended for: Production code with minimal performance impact
- Detects: Stack/heap overflows, memory leaks, integer overflow, format string bugs

**Level 2: Standard** (`-2` or `--safety=standard`)
- Comprehensive development safety (~20-40% overhead)
- Enables: All of Level 1 + pointer sanitizer, uninitialized detection, memory poisoning
- Recommended for: Development and testing (default recommended level)
- Detects: All of Level 1 + use-after-free, bounds violations, type confusion, uninitialized variables

**Level 3: Maximum** (`-3` or `--safety=max`)
- All safety features for deep debugging (~60-100%+ overhead)
- Enables: All safety features including CFI, temporal tagging, dangling pointers, alignment checks, provenance tracking
- Recommended for: Debugging hard-to-find memory bugs
- Detects: Everything possible

**Usage Examples:**
```bash
# No safety checks (maximum performance)
./jcc -0 program.c

# Basic safety (recommended for production)
./jcc -1 program.c
./jcc --safety=basic program.c

# Standard safety (recommended for development)
./jcc -2 program.c
./jcc --safety=standard program.c

# Maximum safety (for debugging)
./jcc -3 program.c
./jcc --safety=max program.c

# Combine preset with additional flags (additive)
./jcc -2 --memory-tagging program.c  # Standard + temporal tagging
```

#### Individual Safety Flags

All individual safety flags can be used standalone or combined with safety levels. When combined, the preset level provides a baseline and individual flags enable additional features.

- `--stack-canaries` **Stack overflow protection**
- `--heap-canaries` **Heap overflow protection**
- `--random-canaries` **Random stack canaries (prevents predictable bypass)**
- `--memory-poisoning` **Poison allocated/freed memory (0xCD/0xDD patterns)**
- `--memory-leak-detection` **Memory leak detection**
- `--uaf-detection` **Use-after-free detection**
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

> **VM Heap Mode**: Most memory safety features require heap allocations to go through the VM's internal allocator (MALC/MFRE opcodes) rather than system malloc/free. The `--vm-heap` flag automatically intercepts malloc/free calls at compile time and routes them through the VM heap. Without this flag, memory safety checks only apply to code that directly uses VM heap opcodes. Use `--vm-heap` in combination with other safety flags for comprehensive protection. While `--vm-heap` is enable, double free protection checks are done automatically.

### Pragma Macros

> This is a very early feature and a wip. The API may change in the future and some features may not be fully implemented.

Pragma macros are a feature that allows you to define macros that are expanded at compile time. Create functions, structs, unions, enums, and variables at compile time. There is a comprehensive reflection API. A common problem in C is creating "enum_to_string" functions. Now you can write a single pragma macro that generates the function at compile time for *any* enum:

```TODO: Example```

### URL Header Imports

JCC supports including header files directly from URLs, enabling remote header distribution and easier dependency management. This feature is **optional** and requires building with libcurl support.

**Key Features:**
- Include headers from `http://` or `https://` URLs
- Automatic caching in platform temp directory for offline access
- HTTPS certificate verification for security
- Manual cache control (persistent until cleared)

**Syntax:**
```c
// Both angle bracket and quoted syntax work for URLs
#include <https://raw.githubusercontent.com/user/repo/main/header.h>
#include "https://example.com/lib/myheader.h"
```

**Build with URL Support:**
```bash
# Install libcurl (if not already installed)
# macOS:
brew install curl

# Linux (Debian/Ubuntu):
sudo apt-get install libcurl4-openssl-dev

# Linux (Fedora/RHEL):
sudo dnf install libcurl-devel

# Build JCC with URL support
make JCC_HAS_CURL=1
```

**Cache Management:**
```bash
# Downloaded headers are cached in system temp directory
# Default locations:
#   Linux/macOS: $TMPDIR/jcc_cache or /tmp/jcc_cache
#   Windows: %TEMP%/jcc_cache
# Cache persists across runs for offline access

# Clear cache
./jcc --url-cache-clear program.c

# Use custom cache directory
./jcc --url-cache-dir /path/to/cache program.c
```

**Security Considerations:**
- Always use HTTPS URLs to prevent man-in-the-middle attacks
- SSL certificates are verified by default
- Downloaded files are size-limited (10MB max)
- Network timeout is 30 seconds
- HTTP URLs will work but are not recommended

**Example:**
```c
// Include a header from GitHub
#include <https://raw.githubusercontent.com/nothings/stb/master/stb_sprintf.h>

int main() {
    char buffer[100];
    stbsp_sprintf(buffer, "Hello from URL include!");
    return 0;
}
```

**Limitations:**
- Relative includes in URL-fetched headers are **not supported** - they must use absolute URLs or local filesystem paths
- Requires network connectivity on first include (cached afterward)
- Works with both `<...>` and `"..."` syntax

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

#### Source Map JSON Export

JCC now supports exporting detailed source maps in JSON format, which map bytecode locations to source code positions including file, line, and column information. This is useful for debugging tools, IDE integration, and error reporting.

**Features:**
- Maps bytecode PC offsets to source locations
- Includes column number tracking for precise positioning
- UTF-8 aware column calculation
- JSON format for easy integration with external tools

**Using the API:**
```c
#include "jcc.h"

JCC vm;
cc_init(&vm, JCC_ENABLE_DEBUGGER);  // Source maps require debugger flag
Token *tok = cc_preprocess(&vm, "program.c");
Obj *prog = cc_parse(&vm, tok);
cc_compile(&vm, prog);

// Export source map to JSON file
FILE *f = fopen("sourcemap.json", "w");
cc_output_source_map_json(&vm, f);
fclose(f);

cc_destroy(&vm);
```

**JSON Format:**
```json
{
  "version": 3,
  "mappings": [
    {"pc": 8, "file": "program.c", "line": 3, "col": 5, "end_col": 8},
    {"pc": 16, "file": "program.c", "line": 4, "col": 9, "end_col": 14},
    ...
  ]
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

- `if`, `else`
- `for`, `while`, `do-while`
- `switch`, `case`, `default`
- `goto label`, `goto *expr;`, `&&label`
- `break`, `continue`

### Functions

- Function declarations and definitions
- Function calls (direct and indirect via function pointers)
- Recursion
- Parameters and return values
- Function pointers (declaration, assignment, indirect calls)
- Variadic functions (`<stdarg.h>` support with `va_start`, `va_arg`, `va_end`)

### Data Types

- Basic types: `void`, `char`, `short`, `int`, `long`, `float`, `double`, `_Bool`
- Pointers (declaration, dereference `*`, address-of `&`, pointer arithmetic)
- Arrays (fixed-size, initialization, multidimensional, indexing)
- Array decay (in function parameters and expressions)
- Zero-length arrays (`int arr[0];` GNU extension for flexible array members)
- Strings (literals with data segment storage)
- Structs (declaration, member access `.`, nested structs, initialization, flexible array members, anonymous structs/unions)
- Bitfields (bit-level struct member access with `: width` syntax, signed/unsigned, read-modify-write operations)
- Unions (declaration, member access, initialization, anonymous unions)
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
- Designated initializers (arrays and structs)
- Cast expressions (explicit type conversions)

### Preprocessor

- `#include` (with intelligent search path handling)
  - **Standard library headers** (`<stdio.h>`, `<stdlib.h>`, etc.) automatically use JCC's `./include` directory
  - **User headers** with `"quotes"` search `-I` paths
  - **Non-standard angle-bracket headers** require `--isystem` paths (system headers not searched by default)
  - Full paths work directly
  - No `-I./include` flag needed - added automatically
- `#define` (object-like and function-like macros)
- `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`
- `#elifdef`, `#elifndef`
- `#undef`
- `#pragma once`
- `#error`, `#warning`
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
#- **Optional libffi support** for true variadic foreign functions (`printf`, `scanf`, etc.)
  - Build with `make JCC_HAS_FFI=1` to enable libffi support
  - Automatically uses platform-specific calling conventions via libffi
  - Falls back to macro-based dispatch if libffi is not available
- Dual-mode operation: works with or without libffi (see [Variadic Foreign Functions](#variadic-foreign-functions))

### Inline Assembly

- `asm("...")` statements via callback mechanism
- Not executed in the VM (no-op by default)
- Callback can emit custom bytecode or perform logging

### C11, C23, and GNU Extensions

- Statement expressions `({...})`
- `typeof` operator (compile-time type inquiry for variables and expressions)
- `typeof_unqual` - typeof that removes type qualifiers
- `__attribute__((...))` for functions and variables (currently ignored, parsed only)
- `[[...]]` - C23 attributes, like GNU attributes, are parsed but ignored
- Labels as values `&&label` - Get the address of a label for computed goto
- Computed goto `goto *expr;` - Jump to an address computed at runtime
- Switch case ranges `case 1 ... 5:` - Match ranges of values in switch statements
- Zero-length arrays `int arr[0];` - Flexible array member alternative
- Universal character names `\uXXXX`, `\UXXXXXXXX` - Unicode escapes in strings
- `_Generic` type-generic expressions (compile-time type selection)
- `_Alignof` operator (query type/variable alignment)
- `_Alignas` specifier (control variable alignment)
- `static_asset` and `_Static_assert` (compile-time assertions with custom error messages)
- `Bitfields` - Bit-level struct member access (e.g., `unsigned int flags : 3;`)
- `Anonymous structs/unions` - Direct member access without intermediate name
- Binary literals (0b10101010)
- Digit seperates with single quotes (1'000'000)

### Threading + Atomics Support

> Not supported (yet)

JCC is single-threaded and does not implement any threading or atomic operations yet. Registering `pthread` as a dynamic library amd loading the functions may work but is untested.

## TODO

### Language features

- Blocks/closures (`^{}`) - Complex feature requiring variable capture and closure runtime
- Nested functions - Requires static chain or trampolines for accessing parent scope
- `#embed` directive for embedding binary data directly
- `__VA_OPT__` for better variadic macro handling

### JCC features

- Hot reloading for FFI functions + compiled functions
- Support for pthread + internal thread support
  - Thread safety features (race condition checks, etc)
- Refactor VM from single accumulator (ax/fx) to multiple register
- Optimisation pass for bytecode
  - Peephole, code folding, dead code elimination, etc
- Extend `#pragma macro`, support `#pragma derive`
  - Rust like `derive` directive using pragma macros
- Built in test suite `#pragma test(suite)`
  - When in `test` mode, replace `main()` with a test runner (generated at compile time)
  - Setup/teardown phase
- UFCS (universal function call syntax)
  - Extend parser to support `5.sqrt`

## Building

```bash
make jcc        # Build jcc compiler
make all        # Build everything (jcc, libjcc.dylib) and run tests

# Optional: Build with libffi support for true variadic foreign functions
make JCC_HAS_FFI=1

# Optional: Build with libcurl support for URL header imports
make JCC_HAS_CURL=1
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

### libcurl Support (URL Header Imports)

To enable URL-based `#include` directives, build with libcurl:

**macOS (Homebrew):**
```bash
brew install curl
make JCC_HAS_CURL=1
```

**Linux:**
```bash
# Debian/Ubuntu
sudo apt-get install libcurl4-openssl-dev

# Fedora/RHEL
sudo dnf install libcurl-devel

# Then build
make JCC_HAS_CURL=1
```

With libcurl enabled:
- Include header files directly from HTTP/HTTPS URLs
- Automatic caching in platform temp directory
- HTTPS certificate verification for secure downloads
- See [URL Header Imports](#url-header-imports) section for detailed usage

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
./run_tests
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

JCC includes a **Foreign Function Interface (FFI)** that allows compiled C code to call native standard library functions directly, without reimplementing them as VM opcodes. The FFI uses a **header-based lazy loading** system that automatically registers functions when their corresponding standard headers are included.

### Header-Based Registration

Functions are registered on-demand during preprocessing when you `#include` standard headers:

- `#include <stdio.h>` → Registers `printf`, `fopen`, `fread`, etc. (~50 functions)
- `#include <stdlib.h>` → Registers `malloc`, `atoi`, `rand`, etc. (~40 functions)
- `#include <string.h>` → Registers `strlen`, `strcmp`, `memcpy`, etc. (~25 functions)
- `#include <math.h>` → Registers `sin`, `cos`, `sqrt`, `pow`, etc. (~180 functions)
- `#include <ctype.h>` → Registers `isalpha`, `isdigit`, `tolower`, etc. (~11 functions)
- `#include <time.h>` → Registers `time`, `clock`, `strftime`, etc. (~14 functions)

**Benefits:**
- **Smaller FFI table:** Only loads functions you actually use (~70% reduction for typical programs)
- **Faster initialization:** Functions registered during preprocessing, not at runtime
- **Backward compatible:** Programs calling `cc_load_stdlib()` directly still work (registers all functions)

**Implementation:** The preprocessor calls `register_stdlib_for_header()` in `include_file()` when a standard header is encountered. Each header has a dedicated registration function in `src/stdlib/*.c`.

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
cc_init(&vm, 0);  // 0 for no flags, or JCC_ENABLE_DEBUGGER etc.

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
