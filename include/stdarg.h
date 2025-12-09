/*
 * stdarg.h - Variable Argument Lists
 * JCC C Compiler - Standard C Library Header
 * 
 * Implements C99/C11 variable argument list support for the JCC VM.
 * 
 * Stack Layout for Variadic Functions:
 *   [argN]         ← sp (extra variadic args)
 *   ...
 *   [arg(fixed+1)] ← First variadic argument
 *   [arg(fixed)]   ← Last fixed argument
 *   [arg1]
 *   [arg0]
 *   [ret_addr]
 *   [old_bp]       ← bp (Base Pointer)
 *   [va_area]      ← bp - 1 (136-byte buffer storing variadic args)
 *   [locals...]
 * 
 * The va_area is a 136-byte buffer that holds copies of all variadic arguments.
 * This buffer is automatically allocated by the compiler for variadic functions.
 * 
 * Implementation:
 * - va_list is a pointer (char*) into the va_area buffer
 * - va_start initializes it to point to the start of va_area
 * - va_arg retrieves the value and advances the pointer
 * - va_end is a no-op (included for compatibility)
 * - va_copy copies the pointer (shallow copy)
 */

#ifndef _STDARG_H
#define _STDARG_H

/* 
 * va_list type: pointer to variable argument list
 * Implemented as char* to allow pointer arithmetic
 */
typedef char *va_list;

/*
 * va_start(ap, last) - Initialize va_list
 * @ap:   va_list to initialize
 * @last: name of the last fixed parameter before '...'
 * 
 * Sets ap to point to the first variadic argument on the stack.
 * In the VM, all arguments are 8 bytes (long long), so we add 8 bytes
 * to the address of the last fixed parameter.
 */
#define va_start(ap, last) ((ap) = (char *)((long long *)&(last) - 1))

/*
 * va_arg(ap, type) - Retrieve next argument
 * @ap:   va_list to read from
 * @type: type of the argument to retrieve
 * 
 * Returns the current argument cast to 'type' and advances ap.
 * All arguments are stored as 8-byte values (VM uses 8-byte integers).
 * 
 * Note: The VM stores all integers and pointers as 8 bytes (long long).
 * Floating-point values (double/float) are also stored as 8 bytes.
 * 
 * For floating-point types, we use pointer casting to reinterpret the bit
 * pattern from the stack as a double. The stack stores doubles as 8-byte
 * bit patterns (same memory layout as long long), and we cast the pointer
 * to double* before dereferencing to get the correct IEEE 754 value.
 * 
 * The __jcc_types_compatible_p check allows compile-time type dispatch:
 * - For double/float: cast pointer to double* before dereferencing
 * - For other types: cast directly (works for int, long, pointers, etc.)
 */
#define va_arg(ap, type) \
    (__jcc_types_compatible_p(type, double) || \
     __jcc_types_compatible_p(type, float) \
        ? (*(double *)((ap) -= 8, (ap) + 8)) \
        : (*(type *)((ap) -= 8, (ap) + 8)))

/*
 * va_end(ap) - Cleanup va_list
 * @ap: va_list to cleanup
 * 
 * No-op in this implementation. Included for C standard compliance.
 * The va_area buffer is automatically deallocated when the function returns.
 */
#define va_end(ap) ((void)0)

/*
 * va_copy(dest, src) - Copy va_list (C99)
 * @dest: destination va_list
 * @src:  source va_list
 * 
 * Creates a copy of src in dest. Both can then be used independently.
 */
#define va_copy(dest, src) ((dest) = (src))

#endif /* _STDARG_H */
