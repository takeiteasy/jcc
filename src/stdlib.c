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

#include "jcc.h"
#include "internal.h"
#include <stdarg.h>

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
// Note: va_list is passed as a pointer (long long) from VM

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
void register_variadic_wrappers(JCC *vm) {
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

// Wrapper functions for stdlib to ensure correct ABI conversion
// These convert between VM types (long long) and actual C types (size_t, etc.)
long long wrap_strlen(long long s) {
    return (long long)strlen((const char *)s);
}

long long wrap_strcmp(long long s1, long long s2) {
    return (long long)strcmp((const char *)s1, (const char *)s2);
}

long long wrap_strncmp(long long s1, long long s2, long long n) {
    return (long long)strncmp((const char *)s1, (const char *)s2, (size_t)n);
}

long long wrap_memcmp(long long s1, long long s2, long long n) {
    return (long long)memcmp((const void *)s1, (const void *)s2, (size_t)n);
}

long long wrap_fread(long long ptr, long long size, long long nmemb, long long stream) {
    return (long long)fread((void *)ptr, (size_t)size, (size_t)nmemb, (FILE *)stream);
}

long long wrap_fwrite(long long ptr, long long size, long long nmemb, long long stream) {
    return (long long)fwrite((const void *)ptr, (size_t)size, (size_t)nmemb, (FILE *)stream);
}

// long long wrap_read(long long fd, long long buf, long long count) {
//     return (long long)read((int)fd, (void *)buf, (size_t)count);
// }

// long long wrap_write(long long fd, long long buf, long long count) {
//     return (long long)write((int)fd, (const void *)buf, (size_t)count);
// }

// Standard stream getters (since we can't easily register global pointers)
static FILE* __jcc_stdin(void) { return stdin; }
static FILE* __jcc_stdout(void) { return stdout; }
static FILE* __jcc_stderr(void) { return stderr; }

// Floating-point jccs used by include/math.h macros.
// Implemented similarly to the standard stream getters: simple wrappers
// that produce the appropriate special values for the VM C headers.
static double __jcc_huge_val(void) {
    /* Produce a positive infinity for double */
    return 1.0 / 0.0;
}

static float __jcc_inff(void) {
    /* Produce a positive infinity for float */
    return (float)(1.0 / 0.0);
}

static float __jcc_nanf(const char *s) {
    (void)s; /* ignore payload string, return a quiet NaN */
    return 0.0f / 0.0f;
}

/* jcc checks for NaN and Inf provided as extern linkable functions. */
static int __jcc_isnan(double x) {
    return x != x;
}

static int __jcc_isinf(double x) {
    return (x == (1.0 / 0.0)) || (x == -(1.0 / 0.0));
}

void cc_load_stdlib(JCC *vm) {
    // Register standard library functions for FFI
    // Note: All integer types are passed/returned as long long in the VM
    // Pointers are also passed as long long

    // ctype.h functions
    cc_register_cfunc(vm, "isalnum", (void*)isalnum, 1, 0);
    cc_register_cfunc(vm, "isalpha", (void*)isalpha, 1, 0);
    cc_register_cfunc(vm, "isdigit", (void*)isdigit, 1, 0);
    cc_register_cfunc(vm, "islower", (void*)islower, 1, 0);
    cc_register_cfunc(vm, "isprint", (void*)isprint, 1, 0);
    cc_register_cfunc(vm, "ispunct", (void*)ispunct, 1, 0);
    cc_register_cfunc(vm, "isspace", (void*)isspace, 1, 0);
    cc_register_cfunc(vm, "isupper", (void*)isupper, 1, 0);
    cc_register_cfunc(vm, "isxdigit", (void*)isxdigit, 1, 0);
    cc_register_cfunc(vm, "tolower", (void*)tolower, 1, 0);
    cc_register_cfunc(vm, "toupper", (void*)toupper, 1, 0);
    
    // math.h functions
    cc_register_cfunc(vm, "__jcc_huge_val", (void*)__jcc_huge_val, 0, 1);
    cc_register_cfunc(vm, "__jcc_inff", (void*)__jcc_inff, 0, 1);
    cc_register_cfunc(vm, "__jcc_nanf", (void*)__jcc_nanf, 1, 1);
    cc_register_cfunc(vm, "__jcc_isnan", (void*)__jcc_isnan, 1, 0);
    cc_register_cfunc(vm, "__jcc_isinf", (void*)__jcc_isinf, 1, 0);
    cc_register_cfunc(vm, "fabs", (void*)fabs, 1, 1);
    cc_register_cfunc(vm, "fabsf", (void*)fabsf, 1, 0);
    cc_register_cfunc(vm, "fabsl", (void*)fabsl, 1, 1);
    cc_register_cfunc(vm, "fmod", (void*)fmod, 2, 1);
    cc_register_cfunc(vm, "fmodf", (void*)fmodf, 2, 0);
    cc_register_cfunc(vm, "fmodl", (void*)fmodl, 2, 1);
    cc_register_cfunc(vm, "remainder", (void*)remainder, 2, 1);
    cc_register_cfunc(vm, "remainderf", (void*)remainderf, 2, 0);
    cc_register_cfunc(vm, "remainderl", (void*)remainderl, 2, 1);
    cc_register_cfunc(vm, "remquo", (void*)remquo, 3, 0);
    cc_register_cfunc(vm, "remquof", (void*)remquof, 3, 0);
    cc_register_cfunc(vm, "remquol", (void*)remquol, 3, 0);
    cc_register_cfunc(vm, "fma", (void*)fma, 3, 1);
    cc_register_cfunc(vm, "fmaf", (void*)fmaf, 3, 0);
    cc_register_cfunc(vm, "fmal", (void*)fmal, 3, 1);
    cc_register_cfunc(vm, "fmax", (void*)fmax, 2, 1);
    cc_register_cfunc(vm, "fmaxf", (void*)fmaxf, 2, 0);
    cc_register_cfunc(vm, "fmaxl", (void*)fmaxl, 2, 1);
    cc_register_cfunc(vm, "fmin", (void*)fmin, 2, 1);
    cc_register_cfunc(vm, "fminf", (void*)fminf, 2, 0);
    cc_register_cfunc(vm, "fminl", (void*)fminl, 2, 1);
    cc_register_cfunc(vm, "fdim", (void*)fdim, 2, 1);
    cc_register_cfunc(vm, "fdimf", (void*)fdimf, 2, 0);
    cc_register_cfunc(vm, "fdiml", (void*)fdiml, 2, 1);
    cc_register_cfunc(vm, "nan", (void*)nan, 1, 1);
    cc_register_cfunc(vm, "nanf", (void*)nanf, 1, 0);
    cc_register_cfunc(vm, "nanl", (void*)nanl, 1, 1);
    cc_register_cfunc(vm, "exp", (void*)exp, 1, 1);
    cc_register_cfunc(vm, "expf", (void*)expf, 1, 0);
    cc_register_cfunc(vm, "expl", (void*)expl, 1, 1);
    cc_register_cfunc(vm, "exp", (void*)exp, 1, 1);
    cc_register_cfunc(vm, "expf", (void*)expf, 1, 0);
    cc_register_cfunc(vm, "expl", (void*)expl, 1, 1);
    cc_register_cfunc(vm, "exp2", (void*)exp2, 1, 1);
    cc_register_cfunc(vm, "exp2f", (void*)exp2f, 1, 0);
    cc_register_cfunc(vm, "exp2l", (void*)exp2l, 1, 1);
    cc_register_cfunc(vm, "expm1", (void*)expm1, 1, 1);
    cc_register_cfunc(vm, "expm1f", (void*)expm1f, 1, 0);
    cc_register_cfunc(vm, "expm1l", (void*)expm1l, 1, 1);
    cc_register_cfunc(vm, "log", (void*)log, 1, 1);
    cc_register_cfunc(vm, "logf", (void*)logf, 1, 0);
    cc_register_cfunc(vm, "logl", (void*)logl, 1, 1);
    cc_register_cfunc(vm, "log10", (void*)log10, 1, 1);
    cc_register_cfunc(vm, "log10f", (void*)log10f, 1, 0);
    cc_register_cfunc(vm, "log10l", (void*)log10l, 1, 1);
    cc_register_cfunc(vm, "log2", (void*)log2, 1, 1);
    cc_register_cfunc(vm, "log2f", (void*)log2f, 1, 0);
    cc_register_cfunc(vm, "log2l", (void*)log2l, 1, 1);
    cc_register_cfunc(vm, "log1p", (void*)log1p, 1, 1);
    cc_register_cfunc(vm, "log1pf", (void*)log1pf, 1, 0);
    cc_register_cfunc(vm, "log1pl", (void*)log1pl, 1, 1);
    cc_register_cfunc(vm, "pow", (void*)pow, 2, 1);
    cc_register_cfunc(vm, "powf", (void*)powf, 2, 0);
    cc_register_cfunc(vm, "powl", (void*)powl, 2, 1);
    cc_register_cfunc(vm, "sqrt", (void*)sqrt, 1, 1);
    cc_register_cfunc(vm, "sqrtf", (void*)sqrtf, 1, 0);
    cc_register_cfunc(vm, "sqrtl", (void*)sqrtl, 1, 1);
    cc_register_cfunc(vm, "cbrt", (void*)cbrt, 1, 1);
    cc_register_cfunc(vm, "cbrtf", (void*)cbrtf, 1, 0);
    cc_register_cfunc(vm, "cbrtl", (void*)cbrtl, 1, 1);
    cc_register_cfunc(vm, "hypot", (void*)hypot, 2, 1);
    cc_register_cfunc(vm, "hypotf", (void*)hypotf, 2, 0);
    cc_register_cfunc(vm, "hypotl", (void*)hypotl, 2, 1);
    cc_register_cfunc(vm, "sin", (void*)sin, 1, 1);
    cc_register_cfunc(vm, "sinf", (void*)sinf, 1, 0);
    cc_register_cfunc(vm, "sinl", (void*)sinl, 1, 1);
    cc_register_cfunc(vm, "cos", (void*)cos, 1, 1);
    cc_register_cfunc(vm, "cosf", (void*)cosf, 1, 0);
    cc_register_cfunc(vm, "cosl", (void*)cosl, 1, 1);
    cc_register_cfunc(vm, "tan", (void*)tan, 1, 1);
    cc_register_cfunc(vm, "tanf", (void*)tanf, 1, 0);
    cc_register_cfunc(vm, "tanl", (void*)tanl, 1, 1);
    cc_register_cfunc(vm, "asin", (void*)asin, 1, 1);
    cc_register_cfunc(vm, "asinf", (void*)asinf, 1, 0);
    cc_register_cfunc(vm, "asinl", (void*)asinl, 1, 1);
    cc_register_cfunc(vm, "acos", (void*)acos, 1, 1);
    cc_register_cfunc(vm, "acosf", (void*)acosf, 1, 0);
    cc_register_cfunc(vm, "acosl", (void*)acosl, 1, 1);
    cc_register_cfunc(vm, "atan", (void*)atan, 1, 1);
    cc_register_cfunc(vm, "atanf", (void*)atanf, 1, 0);
    cc_register_cfunc(vm, "atanl", (void*)atanl, 1, 1);
    cc_register_cfunc(vm, "atan2", (void*)atan2, 2, 1);
    cc_register_cfunc(vm, "atan2f", (void*)atan2f, 2, 0);
    cc_register_cfunc(vm, "atan2l", (void*)atan2l, 2, 1);
    cc_register_cfunc(vm, "sinh", (void*)sinh, 1, 1);
    cc_register_cfunc(vm, "sinhf", (void*)sinhf, 1, 0);
    cc_register_cfunc(vm, "sinhl", (void*)sinhl, 1, 1);
    cc_register_cfunc(vm, "cosh", (void*)cosh, 1, 1);
    cc_register_cfunc(vm, "coshf", (void*)coshf, 1, 0);
    cc_register_cfunc(vm, "coshl", (void*)coshl, 1, 1);
    cc_register_cfunc(vm, "tanh", (void*)tanh, 1, 1);
    cc_register_cfunc(vm, "tanhf", (void*)tanhf, 1, 0);
    cc_register_cfunc(vm, "tanhl", (void*)tanhl, 1, 1);
    cc_register_cfunc(vm, "asinh", (void*)asinh, 1, 1);
    cc_register_cfunc(vm, "asinhf", (void*)asinhf, 1, 0);
    cc_register_cfunc(vm, "asinhl", (void*)asinhl, 1, 1);
    cc_register_cfunc(vm, "erf", (void*)erf, 1, 1);
    cc_register_cfunc(vm, "erff", (void*)erff, 1, 0);
    cc_register_cfunc(vm, "erfl", (void*)erfl, 1, 1);
    cc_register_cfunc(vm, "erfc", (void*)erfc, 1, 1);
    cc_register_cfunc(vm, "erfcf", (void*)erfcf, 1, 0);
    cc_register_cfunc(vm, "erfcf", (void*)erfcf, 1, 0);
    cc_register_cfunc(vm, "erfcl", (void*)erfcl, 1, 1);
    cc_register_cfunc(vm, "tgamma", (void*)tgamma, 1, 1);
    cc_register_cfunc(vm, "tgammaf", (void*)tgammaf, 1, 0);
    cc_register_cfunc(vm, "tgammal", (void*)tgammal, 1, 1);
    cc_register_cfunc(vm, "lgamma", (void*)lgamma, 1, 1);
    cc_register_cfunc(vm, "lgammaf", (void*)lgammaf, 1, 0);
    cc_register_cfunc(vm, "lgammal", (void*)lgammal, 1, 1);
    cc_register_cfunc(vm, "ceil", (void*)ceil, 1, 1);
    cc_register_cfunc(vm, "ceilf", (void*)ceilf, 1, 0);
    cc_register_cfunc(vm, "ceill", (void*)ceill, 1, 1);
    cc_register_cfunc(vm, "floor", (void*)floor, 1, 1);
    cc_register_cfunc(vm, "floorf", (void*)floorf, 1, 0);
    cc_register_cfunc(vm, "floorl", (void*)floorl, 1, 1);
    cc_register_cfunc(vm, "trunc", (void*)trunc, 1, 1);
    cc_register_cfunc(vm, "truncf", (void*)truncf, 1, 0);
    cc_register_cfunc(vm, "truncl", (void*)truncl, 1, 1);
    cc_register_cfunc(vm, "round", (void*)round, 1, 1);
    cc_register_cfunc(vm, "roundf", (void*)roundf, 1, 0);
    cc_register_cfunc(vm, "roundl", (void*)roundl, 1, 1);
    cc_register_cfunc(vm, "lround", (void*)lround, 1, 0);
    cc_register_cfunc(vm, "lroundf", (void*)lroundf, 1, 0);
    cc_register_cfunc(vm, "lroundl", (void*)lroundl, 1, 0);
    cc_register_cfunc(vm, "llround", (void*)llround, 1, 0);
    cc_register_cfunc(vm, "llroundf", (void*)llroundf, 1, 0);
    cc_register_cfunc(vm, "llroundl", (void*)llroundl, 1, 0);
    cc_register_cfunc(vm, "nearbyint", (void*)nearbyint, 1, 1);
    cc_register_cfunc(vm, "nearbyintf", (void*)nearbyintf, 1, 0);
    cc_register_cfunc(vm, "nearbyintl", (void*)nearbyintl, 1, 1);
    cc_register_cfunc(vm, "rint", (void*)rint, 1, 1);
    cc_register_cfunc(vm, "rintf", (void*)rintf, 1, 0);
    cc_register_cfunc(vm, "rintl", (void*)rintl, 1, 1);
    cc_register_cfunc(vm, "lrint", (void*)lrint, 1, 0);
    cc_register_cfunc(vm, "lrintf", (void*)lrintf, 1, 0);
    cc_register_cfunc(vm, "lrintl", (void*)lrintl, 1, 0);
    cc_register_cfunc(vm, "llrint", (void*)llrint, 1, 0);
    cc_register_cfunc(vm, "llrintf", (void*)llrintf, 1, 0);
    cc_register_cfunc(vm, "llrintl", (void*)llrintl, 1, 0);
    cc_register_cfunc(vm, "frexp", (void*)frexp, 3, 0);
    cc_register_cfunc(vm, "frexpf", (void*)frexpf, 3, 0);
    cc_register_cfunc(vm, "frexpl", (void*)frexpl, 3, 0);
    cc_register_cfunc(vm, "ldexp", (void*)ldexp, 2, 1);
    cc_register_cfunc(vm, "ldexpf", (void*)ldexpf, 2, 0);
    cc_register_cfunc(vm, "ldexpl", (void*)ldexpl, 2, 1);
    cc_register_cfunc(vm, "modf", (void*)modf, 2, 0);
    cc_register_cfunc(vm, "modff", (void*)modff, 2, 0);
    cc_register_cfunc(vm, "modfl", (void*)modfl, 2, 0);
    cc_register_cfunc(vm, "modf", (void*)modf, 2, 0);
    cc_register_cfunc(vm, "modff", (void*)modff, 2, 0);
    cc_register_cfunc(vm, "modfl", (void*)modfl, 2, 0);
    cc_register_cfunc(vm, "scalbn", (void*)scalbn, 2, 1);
    cc_register_cfunc(vm, "scalbnf", (void*)scalbnf, 2, 0);
    cc_register_cfunc(vm, "scalbnl", (void*)scalbnl, 2, 1);
    cc_register_cfunc(vm, "scalbln", (void*)scalbln, 2, 1);
    cc_register_cfunc(vm, "scalblnf", (void*)scalblnf, 2, 0);
    cc_register_cfunc(vm, "scalblnl", (void*)scalblnl, 2, 1);
    cc_register_cfunc(vm, "ilogb", (void*)ilogb, 1, 1);
    cc_register_cfunc(vm, "ilogbf", (void*)ilogbf, 1, 0);
    cc_register_cfunc(vm, "ilogbl", (void*)ilogbl, 1, 1);
    cc_register_cfunc(vm, "logb", (void*)logb, 1, 1);
    cc_register_cfunc(vm, "logbf", (void*)logbf, 1, 0);
    cc_register_cfunc(vm, "logbl", (void*)logbl, 1, 1);
    cc_register_cfunc(vm, "nextafter", (void*)nextafter, 2, 1);
    cc_register_cfunc(vm, "nextafterf", (void*)nextafterf, 2, 0);
    cc_register_cfunc(vm, "nextafterl", (void*)nextafterl, 2, 1);
    cc_register_cfunc(vm, "nexttoward", (void*)nexttoward, 2, 1);
    cc_register_cfunc(vm, "nexttowardf", (void*)nexttowardf, 2, 0);
    cc_register_cfunc(vm, "nexttowardl", (void*)nexttowardl, 2, 1);
    cc_register_cfunc(vm, "copysign", (void*)copysign, 2, 1);
    cc_register_cfunc(vm, "copysignf", (void*)copysignf, 2, 0);
    cc_register_cfunc(vm, "copysignl", (void*)copysignl, 2, 1);

    // stdio.h functions
    // Standard streams (use getter functions since can't directly export globals)
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
    cc_register_cfunc(vm, "fgetc", (void*)fgetc, 1, 0);
    cc_register_cfunc(vm, "fputc", (void*)fputc, 2, 0);
    cc_register_cfunc(vm, "fgets", (void*)fgets, 3, 0);
    cc_register_cfunc(vm, "fputs", (void*)fputs, 2, 0);
    cc_register_cfunc(vm, "getc", (void*)getc, 1, 0);
    cc_register_cfunc(vm, "putc", (void*)putc, 2, 0);
    cc_register_cfunc(vm, "getchar", (void*)getchar, 0, 0);
    cc_register_cfunc(vm, "putchar", (void*)putchar, 1, 0);
    cc_register_cfunc(vm, "puts", (void*)puts, 1, 0);
    cc_register_cfunc(vm, "ungetc", (void*)ungetc, 2, 0);
    cc_register_cfunc(vm, "fread", (void*)wrap_fread, 4, 0);
    cc_register_cfunc(vm, "fwrite", (void*)wrap_fwrite, 4, 0);
    cc_register_cfunc(vm, "fputc", (void*)fputc, 2, 0);
    cc_register_cfunc(vm, "fgetpos", (void*)fgetpos, 2, 0);
    cc_register_cfunc(vm, "fsetpos", (void*)fsetpos, 2, 0);
    cc_register_cfunc(vm, "fseek", (void*)fseek, 3, 0);
    cc_register_cfunc(vm, "ftell", (void*)ftell, 1, 0);
    cc_register_cfunc(vm, "rewind", (void*)rewind, 1, 0);
    cc_register_cfunc(vm, "clearerr", (void*)clearerr, 1, 0);
    cc_register_cfunc(vm, "feof", (void*)feof, 1, 0);
    cc_register_cfunc(vm, "ferror", (void*)ferror, 1, 0);
    cc_register_cfunc(vm, "perror", (void*)perror, 1, 0);

    // stdlib.h functions
    // cc_register_cfunc(vm, "call_once", (void*)call_once, 2, 0);
    cc_register_cfunc(vm, "atof", (void*)atof, 1, 1);
    cc_register_cfunc(vm, "atof", (void*)atof, 1, 1);
    cc_register_cfunc(vm, "atoi", (void*)atoi, 1, 1);
    cc_register_cfunc(vm, "atoll", (void*)atoll, 1, 0);
    // cc_register_cfunc(vm, "strfromd", (void*)strfromd, 4, 0);
    // cc_register_cfunc(vm, "strfromf", (void*)strfromf, 4, 0);
    // cc_register_cfunc(vm, "strfroml", (void*)strfroml, 4, 0);
    cc_register_cfunc(vm, "strtod", (void*)strtod, 2, 1);
    cc_register_cfunc(vm, "strtof", (void*)strtof, 2, 0);
    cc_register_cfunc(vm, "strtold", (void*)strtold, 2, 1);
    cc_register_cfunc(vm, "strtol", (void*)strtol, 3, 0);
    cc_register_cfunc(vm, "strtoll", (void*)strtoll, 3, 0);
    cc_register_cfunc(vm, "strtoul", (void*)strtoul, 3, 0);
    cc_register_cfunc(vm, "strtoull", (void*)strtoull, 3, 0);
    cc_register_cfunc(vm, "rand", (void*)rand, 0, 0);
    cc_register_cfunc(vm, "srand", (void*)srand, 1, 0);
    // cc_register_cfunc(vm, "aligned_alloc", (void*)aligned_alloc, 2, 0);
    cc_register_cfunc(vm, "calloc", (void*)calloc, 2, 0);
    cc_register_cfunc(vm, "free", (void*)free, 1, 0);
    // cc_register_cfunc(vm, "free_sized", (void*)free_sized, 2, 0);
    // cc_register_cfunc(vm, "free_sized_alloc", (void*)free_sized_alloc, 2, 0);
    cc_register_cfunc(vm, "malloc", (void*)malloc, 1, 0);
    cc_register_cfunc(vm, "realloc", (void*)realloc, 2, 0);
    cc_register_cfunc(vm, "abort", (void*)abort, 0, 0);
    cc_register_cfunc(vm, "exit", (void*)exit, 1, 0);
    cc_register_cfunc(vm, "_Exit", (void*)_Exit, 1, 0);
    cc_register_cfunc(vm, "atexit", (void*)atexit, 1, 0);
    // cc_register_cfunc(vm, "at_quick_exit", (void*)at_quick_exit, 1, 0);
    cc_register_cfunc(vm, "getenv", (void*)getenv, 1, 0);
    cc_register_cfunc(vm, "system", (void*)system, 1, 0);
    cc_register_cfunc(vm, "posix_memalign", (void*)posix_memalign, 3, 0);
    cc_register_cfunc(vm, "bsearch", (void*)bsearch, 4, 0);
    cc_register_cfunc(vm, "qsort", (void*)qsort, 3, 0);
    cc_register_cfunc(vm, "abs", (void*)abs, 1, 1);
    cc_register_cfunc(vm, "labs", (void*)labs, 1, 1);
    cc_register_cfunc(vm, "llabs", (void*)llabs, 1, 1);
    cc_register_cfunc(vm, "div", (void*)div, 2, 0);
    cc_register_cfunc(vm, "ldiv", (void*)ldiv, 2, 0);
    cc_register_cfunc(vm, "lldiv", (void*)lldiv, 2, 0);
    cc_register_cfunc(vm, "mblen", (void*)mblen, 2, 0);
    cc_register_cfunc(vm, "mbtowc", (void*)mbtowc, 3, 0);
    cc_register_cfunc(vm, "wctomb", (void*)wctomb, 2, 0);
    cc_register_cfunc(vm, "mbstowcs", (void*)mbstowcs, 3, 0);
    cc_register_cfunc(vm, "wcstombs", (void*)wcstombs, 3, 0);
    // cc_register_cfunc(vm, "memalignment", (void*)memalignment, 1, 0);

    // string.h functions
    cc_register_cfunc(vm, "memcpy", (void*)memcpy, 3, 0);
    cc_register_cfunc(vm, "memmove", (void*)memmove, 3, 0);
    cc_register_cfunc(vm, "memset", (void*)memset, 3, 0);
    cc_register_cfunc(vm, "memcmp", (void*)wrap_memcmp, 3, 0);

    cc_register_cfunc(vm, "strlen", (void*)wrap_strlen, 1, 0);
    cc_register_cfunc(vm, "strcmp", (void*)wrap_strcmp, 2, 0);
    cc_register_cfunc(vm, "strncmp", (void*)wrap_strncmp, 3, 0);
    cc_register_cfunc(vm, "strcpy", (void*)strcpy, 2, 0);
    cc_register_cfunc(vm, "strncpy", (void*)strncpy, 3, 0);
    cc_register_cfunc(vm, "strcat", (void*)strcat, 2, 0);
    cc_register_cfunc(vm, "strncat", (void*)strncat, 3, 0);
    cc_register_cfunc(vm, "strcmp", (void*)strcmp, 2, 0);
    cc_register_cfunc(vm, "strncmp", (void*)strncmp, 3, 0);
    cc_register_cfunc(vm, "strchr", (void*)strchr, 2, 0);
    cc_register_cfunc(vm, "strrchr", (void*)strrchr, 2, 0);
    cc_register_cfunc(vm, "strstr", (void*)strstr, 2, 0);
    cc_register_cfunc(vm, "strxfrm", (void*)strxfrm, 3, 0);
    cc_register_cfunc(vm, "strerror", (void*)strerror, 1, 0);
    // cc_register_cfunc(vm, "strcpy_s", (void*)strcpy_s, 2, 0);
    // cc_register_cfunc(vm, "strncpy_s", (void*)strncpy_s, 3, 0);
    // cc_register_cfunc(vm, "strcat_s", (void*)strcat_s, 2, 0);
    // cc_register_cfunc(vm, "strncat_s", (void*)strncat_s, 3, 0);
    cc_register_cfunc(vm, "strdup", (void*)strdup, 1, 0);
    cc_register_cfunc(vm, "strndup", (void*)strndup, 2, 0);
    // cc_register_cfunc(vm, "strnlen_s", (void*)strnlen_s, 2, 0);
    // cc_register_cfunc(vm, "strndup_s", (void*)strndup_s, 3, 0);
    // cc_register_cfunc(vm, "memset_explicit", (void*)memset_explicit, 3, 0);
    // cc_register_cfunc(vm, "memset_s", (void*)memset_s, 3, 0);
    // cc_register_cfunc(vm, "memcpy_s", (void*)memcpy_s, 3, 0);
    // cc_register_cfunc(vm, "memmove_s", (void*)memmove_s, 3, 0);
    cc_register_cfunc(vm, "memccpy", (void*)memccpy, 4, 0);
    // cc_register_cfunc(vm, "strerror_s", (void*)strerror_s, 2, 0);
    // cc_register_cfunc(vm, "strerrorlen_s", (void*)strerrorlen_s, 1, 0);

    // time.h functions
    cc_register_cfunc(vm, "clock", (void*)clock, 0, 0);
    cc_register_cfunc(vm, "difftime", (void*)difftime, 2, 1);
    cc_register_cfunc(vm, "mktime", (void*)mktime, 1, 0);
    // cc_register_cfunc(vm, "timegm", (void*)timegm, 1, 0);
    cc_register_cfunc(vm, "time", (void*)time, 1, 0);
    // cc_register_cfunc(vm, "timespec_get", (void*)timespec_get, 2, 0);
    // cc_register_cfunc(vm, "timespec_getres", (void*)timespec_getres, 2, 0);
    cc_register_cfunc(vm, "asctime", (void*)asctime, 1, 0);
    cc_register_cfunc(vm, "ctime", (void*)ctime, 1, 0);
    cc_register_cfunc(vm, "gmtime", (void*)gmtime, 1, 0);
    cc_register_cfunc(vm, "gmtime_r", (void*)gmtime_r, 2, 0);
    cc_register_cfunc(vm, "localtime", (void*)localtime, 1, 0);
    cc_register_cfunc(vm, "localtime_r", (void*)localtime_r, 2, 0);
    cc_register_cfunc(vm, "strftime", (void*)strftime, 4, 0);
}