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


// V* variants (format functions that take va_list) - still need wrappers for these
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

// Register all stdio.h functions
void register_stdio_functions(JCC *vm) {
    // Standard streams
    cc_register_cfunc(vm, "__jcc_stdin", (void*)__jcc_stdin, 0, 0);
    cc_register_cfunc(vm, "__jcc_stdout", (void*)__jcc_stdout, 0, 0);
    cc_register_cfunc(vm, "__jcc_stderr", (void*)__jcc_stderr, 0, 0);

    // Variadic printf/scanf family - now registering directly as variadic
    // (works with ARM64 inline assembly FFI without needing libffi)
    cc_register_variadic_cfunc(vm, "printf", (void*)printf, 1, 0);
    cc_register_variadic_cfunc(vm, "fprintf", (void*)fprintf, 2, 0);
    cc_register_variadic_cfunc(vm, "sprintf", (void*)sprintf, 2, 0);
    cc_register_variadic_cfunc(vm, "snprintf", (void*)snprintf, 3, 0);
    cc_register_variadic_cfunc(vm, "scanf", (void*)scanf, 1, 0);
    cc_register_variadic_cfunc(vm, "sscanf", (void*)sscanf, 2, 0);
    cc_register_variadic_cfunc(vm, "fscanf", (void*)fscanf, 2, 0);
    
    // V* variants still need wrappers to handle va_list pointer conversion
    cc_register_cfunc(vm, "vprintf", (void*)wrap_vprintf, 2, 0);
    cc_register_cfunc(vm, "vsprintf", (void*)wrap_vsprintf, 3, 0);
    cc_register_cfunc(vm, "vsnprintf", (void*)wrap_vsnprintf, 4, 0);
    cc_register_cfunc(vm, "vfprintf", (void*)wrap_vfprintf, 3, 0);
    cc_register_cfunc(vm, "vscanf", (void*)wrap_vscanf, 2, 0);
    cc_register_cfunc(vm, "vsscanf", (void*)wrap_vsscanf, 3, 0);
    cc_register_cfunc(vm, "vfscanf", (void*)wrap_vfscanf, 3, 0);


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
