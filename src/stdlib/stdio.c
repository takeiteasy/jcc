// stdio.h stdlib function registration
#include "../jcc.h"
#include <stdarg.h>

// Standard stream getters (since we can't easily register global pointers)
static FILE* __jcc_stdin(void) { return stdin; }
static FILE* __jcc_stdout(void) { return stdout; }
static FILE* __jcc_stderr(void) { return stderr; }

// Wrapper functions for stdio to ensure correct ABI conversion
long long wrap_fread(long long ptr, long long size, long long nmemb, long long stream) {
    return (long long)fread((void *)ptr, (size_t)size, (size_t)nmemb, (FILE *)stream);
}

long long wrap_fwrite(long long ptr, long long size, long long nmemb, long long stream) {
    return (long long)fwrite((const void *)ptr, (size_t)size, (size_t)nmemb, (FILE *)stream);
}

#ifndef JCC_HAS_FFI
// Variadic wrapper generation (only when libffi is not available)

// Special case for 0 arguments
static long long wrap_printf0(const char *fmt) {
    return (long long)printf("%s", fmt);
}
static long long wrap_sprintf0(char *str, const char *fmt) {
    return (long long)sprintf(str, "%s", fmt);
}
static long long wrap_fprintf0(FILE *stream, const char *fmt) {
    return (long long)fprintf(stream, "%s", fmt);
}
static long long wrap_scanf0(const char *fmt) {
    return (long long)scanf("%s", (char *)fmt);
}
static long long wrap_sscanf0(const char *str, const char *fmt) {
    return (long long)sscanf(str, "%s", (char *)fmt);
}
static long long wrap_fscanf0(FILE *stream, const char *fmt) {
    return (long long)fscanf(stream, "%s", (char *)fmt);
}
static long long wrap_snprintf0(char *str, long long size, const char *fmt) {
    return (long long)snprintf(str, (size_t)size, "%s", fmt);
}

// X-macro list: (N, declarations, calls)
#define WRAPPER_LIST \
    X(1, long long a1, a1) \
    X(2, long long a1 X_COMMA long long a2, a1 X_COMMA a2) \
    X(3, long long a1 X_COMMA long long a2 X_COMMA long long a3, a1 X_COMMA a2 X_COMMA a3) \
    X(4, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4) \
    X(5, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5) \
    X(6, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6) \
    X(7, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7) \
    X(8, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8) \
    X(9, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9) \
    X(10, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10) \
    X(11, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10 X_COMMA long long a11, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10 X_COMMA a11) \
    X(12, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10 X_COMMA long long a11 X_COMMA long long a12, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10 X_COMMA a11 X_COMMA a12) \
    X(13, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10 X_COMMA long long a11 X_COMMA long long a12 X_COMMA long long a13, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10 X_COMMA a11 X_COMMA a12 X_COMMA a13) \
    X(14, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10 X_COMMA long long a11 X_COMMA long long a12 X_COMMA long long a13 X_COMMA long long a14, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10 X_COMMA a11 X_COMMA a12 X_COMMA a13 X_COMMA a14) \
    X(15, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10 X_COMMA long long a11 X_COMMA long long a12 X_COMMA long long a13 X_COMMA long long a14 X_COMMA long long a15, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10 X_COMMA a11 X_COMMA a12 X_COMMA a13 X_COMMA a14 X_COMMA a15) \
    X(16, long long a1 X_COMMA long long a2 X_COMMA long long a3 X_COMMA long long a4 X_COMMA long long a5 X_COMMA long long a6 X_COMMA long long a7 X_COMMA long long a8 X_COMMA long long a9 X_COMMA long long a10 X_COMMA long long a11 X_COMMA long long a12 X_COMMA long long a13 X_COMMA long long a14 X_COMMA long long a15 X_COMMA long long a16, a1 X_COMMA a2 X_COMMA a3 X_COMMA a4 X_COMMA a5 X_COMMA a6 X_COMMA a7 X_COMMA a8 X_COMMA a9 X_COMMA a10 X_COMMA a11 X_COMMA a12 X_COMMA a13 X_COMMA a14 X_COMMA a15 X_COMMA a16)

// Generate printf wrappers
#define X_COMMA ,
#define X(N, DECL, CALL) \
    static long long wrap_printf##N(const char *fmt, DECL) { \
        return (long long)printf(fmt, CALL); \
    }
WRAPPER_LIST
#undef X

// Generate sprintf wrappers
#define X(N, DECL, CALL) \
    static long long wrap_sprintf##N(char *str, const char *fmt, DECL) { \
        return (long long)sprintf(str, fmt, CALL); \
    }
WRAPPER_LIST
#undef X

// Generate fprintf wrappers
#define X(N, DECL, CALL) \
    static long long wrap_fprintf##N(FILE *stream, const char *fmt, DECL) { \
        return (long long)fprintf(stream, fmt, CALL); \
    }
WRAPPER_LIST
#undef X

// Generate scanf wrappers
#define X(N, DECL, CALL) \
    static long long wrap_scanf##N(const char *fmt, DECL) { \
        return (long long)scanf(fmt, CALL); \
    }
WRAPPER_LIST
#undef X

// Generate sscanf wrappers
#define X(N, DECL, CALL) \
    static long long wrap_sscanf##N(const char *str, const char *fmt, DECL) { \
        return (long long)sscanf(str, fmt, CALL); \
    }
WRAPPER_LIST
#undef X

// Generate fscanf wrappers
#define X(N, DECL, CALL) \
    static long long wrap_fscanf##N(FILE *stream, const char *fmt, DECL) { \
        return (long long)fscanf(stream, fmt, CALL); \
    }
WRAPPER_LIST
#undef X

// Generate snprintf wrappers
#define X(N, DECL, CALL) \
    static long long wrap_snprintf##N(char *str, long long size, const char *fmt, DECL) { \
        return (long long)snprintf(str, (size_t)size, fmt, CALL); \
    }
WRAPPER_LIST
#undef X
#undef X_COMMA
#undef WRAPPER_LIST

// V* variants (format functions that take va_list)
static long long wrap_vprintf(const char *fmt, long long va_ptr) {
    return (long long)vprintf(fmt, *(va_list*)va_ptr);
}

static long long wrap_vsprintf(char *str, const char *fmt, long long va_ptr) {
    return (long long)vsprintf(str, fmt, *(va_list*)va_ptr);
}

static long long wrap_vsnprintf(char *str, long long size, const char *fmt, long long va_ptr) {
    return (long long)vsnprintf(str, (size_t)size, fmt, *(va_list*)va_ptr);
}

static long long wrap_vfprintf(FILE *stream, const char *fmt, long long va_ptr) {
    return (long long)vfprintf(stream, fmt, *(va_list*)va_ptr);
}

static long long wrap_vscanf(const char *fmt, long long va_ptr) {
    return (long long)vscanf(fmt, *(va_list*)va_ptr);
}

static long long wrap_vsscanf(const char *str, const char *fmt, long long va_ptr) {
    return (long long)vsscanf(str, fmt, *(va_list*)va_ptr);
}

static long long wrap_vfscanf(FILE *stream, const char *fmt, long long va_ptr) {
    return (long long)vfscanf(stream, fmt, *(va_list*)va_ptr);
}

// Registration helper macros
#define REGISTER_FUNC(name, n, base_argc) \
    cc_register_cfunc(vm, #name #n, (void*)wrap_##name##n, base_argc + n, 0)

#define REGISTER_ALL_VARIANTS(name, base_argc) \
    REGISTER_FUNC(name, 0, base_argc); \
    REGISTER_FUNC(name, 1, base_argc); \
    REGISTER_FUNC(name, 2, base_argc); \
    REGISTER_FUNC(name, 3, base_argc); \
    REGISTER_FUNC(name, 4, base_argc); \
    REGISTER_FUNC(name, 5, base_argc); \
    REGISTER_FUNC(name, 6, base_argc); \
    REGISTER_FUNC(name, 7, base_argc); \
    REGISTER_FUNC(name, 8, base_argc); \
    REGISTER_FUNC(name, 9, base_argc); \
    REGISTER_FUNC(name, 10, base_argc); \
    REGISTER_FUNC(name, 11, base_argc); \
    REGISTER_FUNC(name, 12, base_argc); \
    REGISTER_FUNC(name, 13, base_argc); \
    REGISTER_FUNC(name, 14, base_argc); \
    REGISTER_FUNC(name, 15, base_argc); \
    REGISTER_FUNC(name, 16, base_argc)

// Register all variadic function variants
static void register_variadic_wrappers(JCC *vm) {
    // Printf variants (format + 0-16 args) - base_argc = 1 (format)
    REGISTER_ALL_VARIANTS(printf, 1);

    // Sprintf variants (buffer + format + 0-16 args) - base_argc = 2
    REGISTER_ALL_VARIANTS(sprintf, 2);

    // Fprintf variants (stream + format + 0-16 args) - base_argc = 2
    REGISTER_ALL_VARIANTS(fprintf, 2);

    // Scanf variants (format + 0-16 pointer args) - base_argc = 1
    REGISTER_ALL_VARIANTS(scanf, 1);

    // Sscanf variants (string + format + 0-16 pointer args) - base_argc = 2
    REGISTER_ALL_VARIANTS(sscanf, 2);

    // Fscanf variants (stream + format + 0-16 pointer args) - base_argc = 2
    REGISTER_ALL_VARIANTS(fscanf, 2);

    // Snprintf variants (buffer + size + format + 0-16 args) - base_argc = 3
    REGISTER_ALL_VARIANTS(snprintf, 3);

    // V* variants (format + va_list pointer)
    cc_register_cfunc(vm, "vprintf", (void*)wrap_vprintf, 2, 0);
    cc_register_cfunc(vm, "vsprintf", (void*)wrap_vsprintf, 3, 0);
    cc_register_cfunc(vm, "vsnprintf", (void*)wrap_vsnprintf, 4, 0);
    cc_register_cfunc(vm, "vfprintf", (void*)wrap_vfprintf, 3, 0);
    cc_register_cfunc(vm, "vscanf", (void*)wrap_vscanf, 2, 0);
    cc_register_cfunc(vm, "vsscanf", (void*)wrap_vsscanf, 3, 0);
    cc_register_cfunc(vm, "vfscanf", (void*)wrap_vfscanf, 3, 0);
}
#endif // !JCC_HAS_FFI

// Register all stdio.h functions
void register_stdio_functions(JCC *vm) {
    // Standard streams
    cc_register_cfunc(vm, "__jcc_stdin", (void*)__jcc_stdin, 0, 0);
    cc_register_cfunc(vm, "__jcc_stdout", (void*)__jcc_stdout, 0, 0);
    cc_register_cfunc(vm, "__jcc_stderr", (void*)__jcc_stderr, 0, 0);

#ifdef JCC_HAS_FFI
    // libffi is available - register true variadic functions
    cc_register_variadic_cfunc(vm, "printf", (void*)printf, 1, 0);
    cc_register_variadic_cfunc(vm, "fprintf", (void*)fprintf, 2, 0);
    cc_register_variadic_cfunc(vm, "sprintf", (void*)sprintf, 2, 0);
    cc_register_variadic_cfunc(vm, "snprintf", (void*)snprintf, 3, 0);
    cc_register_variadic_cfunc(vm, "scanf", (void*)scanf, 1, 0);
    cc_register_variadic_cfunc(vm, "sscanf", (void*)sscanf, 2, 0);
    cc_register_variadic_cfunc(vm, "fscanf", (void*)fscanf, 2, 0);
#else
    // libffi not available - use fixed-argument wrapper functions
    register_variadic_wrappers(vm);
#endif

    // File operations
    cc_register_cfunc(vm, "remove", (void*)remove, 1, 0);
    cc_register_cfunc(vm, "rename", (void*)rename, 2, 0);
    cc_register_cfunc(vm, "tmpfile", (void*)tmpfile, 0, 0);
    cc_register_cfunc(vm, "tmpnam", (void*)tmpnam, 1, 0);
    cc_register_cfunc(vm, "fclose", (void*)fclose, 1, 0);
    cc_register_cfunc(vm, "fflush", (void*)fflush, 1, 0);
    cc_register_cfunc(vm, "fopen", (void*)fopen, 2, 0);
    cc_register_cfunc(vm, "freopen", (void*)freopen, 3, 0);
    cc_register_cfunc(vm, "setbuf", (void*)setbuf, 2, 0);
    cc_register_cfunc(vm, "setvbuf", (void*)setvbuf, 3, 0);

    // Character I/O
    cc_register_cfunc(vm, "fgetc", (void*)fgetc, 1, 0);
    cc_register_cfunc(vm, "fputc", (void*)fputc, 2, 0);
    cc_register_cfunc(vm, "getc", (void*)getc, 1, 0);
    cc_register_cfunc(vm, "putc", (void*)putc, 2, 0);
    cc_register_cfunc(vm, "getchar", (void*)getchar, 0, 0);
    cc_register_cfunc(vm, "putchar", (void*)putchar, 1, 0);
    cc_register_cfunc(vm, "ungetc", (void*)ungetc, 2, 0);

    // String I/O
    cc_register_cfunc(vm, "fgets", (void*)fgets, 3, 0);
    cc_register_cfunc(vm, "fputs", (void*)fputs, 2, 0);
    cc_register_cfunc(vm, "puts", (void*)puts, 1, 0);

    // Binary I/O
    cc_register_cfunc(vm, "fread", (void*)wrap_fread, 4, 0);
    cc_register_cfunc(vm, "fwrite", (void*)wrap_fwrite, 4, 0);

    // Positioning
    cc_register_cfunc(vm, "fgetpos", (void*)fgetpos, 2, 0);
    cc_register_cfunc(vm, "fsetpos", (void*)fsetpos, 2, 0);
    cc_register_cfunc(vm, "fseek", (void*)fseek, 3, 0);
    cc_register_cfunc(vm, "ftell", (void*)ftell, 1, 0);
    cc_register_cfunc(vm, "rewind", (void*)rewind, 1, 0);

    // Error handling
    cc_register_cfunc(vm, "clearerr", (void*)clearerr, 1, 0);
    cc_register_cfunc(vm, "feof", (void*)feof, 1, 0);
    cc_register_cfunc(vm, "ferror", (void*)ferror, 1, 0);
    cc_register_cfunc(vm, "perror", (void*)perror, 1, 0);
}
