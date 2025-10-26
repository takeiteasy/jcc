/*
 * stdio.h - Standard I/O functions for JCC VM
 * 
 * This header provides variadic stdio functions via macros that dispatch to 
 * fixed-argument variants based on argument count. Supports printf, sprintf,
 * fprintf, scanf, sscanf, and fscanf with 0-16 additional arguments.
 */

#ifndef __STDIO_H
#define __STDIO_H

#include "stdarg.h"
#include "stddef.h"

// FILE type (opaque pointer to host FILE)
typedef void FILE;

// Standard streams (accessed via getter functions)
extern FILE* __jcc_stdin(void);
extern FILE* __jcc_stdout(void);
extern FILE* __jcc_stderr(void);
#define stdin __jcc_stdin()
#define stdout __jcc_stdout()
#define stderr __jcc_stderr()

/* Types and macros from the C standard */
typedef long fpos_t;

/* Buffering and limits */
#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

#ifndef EOF
#define EOF (-1)
#endif

#ifndef FILENAME_MAX
#define FILENAME_MAX 4096
#endif

#ifndef FOPEN_MAX
#define FOPEN_MAX 20
#endif

#ifndef L_tmpnam
#define L_tmpnam 20
#endif

#ifndef TMP_MAX
#define TMP_MAX 238328
#endif

/* Seek constants */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* setvbuf modes */
#ifndef _IOFBF
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#endif

// Printf variants (format + 0-16 args)
extern int printf0(char *fmt);
extern int printf1(char *fmt, long a1);
extern int printf2(char *fmt, long a1, long a2);
extern int printf3(char *fmt, long a1, long a2, long a3);
extern int printf4(char *fmt, long a1, long a2, long a3, long a4);
extern int printf5(char *fmt, long a1, long a2, long a3, long a4, long a5);
extern int printf6(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6);
extern int printf7(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
extern int printf8(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8);
extern int printf9(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9);
extern int printf10(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10);
extern int printf11(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11);
extern int printf12(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12);
extern int printf13(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13);
extern int printf14(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14);
extern int printf15(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15);
extern int printf16(char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15, long a16);

// Sprintf variants (buffer + format + 0-16 args)
extern int sprintf0(char *str, char *fmt);
extern int sprintf1(char *str, char *fmt, long a1);
extern int sprintf2(char *str, char *fmt, long a1, long a2);
extern int sprintf3(char *str, char *fmt, long a1, long a2, long a3);
extern int sprintf4(char *str, char *fmt, long a1, long a2, long a3, long a4);
extern int sprintf5(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5);
extern int sprintf6(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6);
extern int sprintf7(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
extern int sprintf8(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8);
extern int sprintf9(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9);
extern int sprintf10(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10);
extern int sprintf11(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11);
extern int sprintf12(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12);
extern int sprintf13(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13);
extern int sprintf14(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14);
extern int sprintf15(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15);
extern int sprintf16(char *str, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15, long a16);

// Fprintf variants (stream + format + 0-16 args)
extern int fprintf0(FILE *stream, char *fmt);
extern int fprintf1(FILE *stream, char *fmt, long a1);
extern int fprintf2(FILE *stream, char *fmt, long a1, long a2);
extern int fprintf3(FILE *stream, char *fmt, long a1, long a2, long a3);
extern int fprintf4(FILE *stream, char *fmt, long a1, long a2, long a3, long a4);
extern int fprintf5(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5);
extern int fprintf6(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6);
extern int fprintf7(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
extern int fprintf8(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8);
extern int fprintf9(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9);
extern int fprintf10(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10);
extern int fprintf11(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11);
extern int fprintf12(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12);
extern int fprintf13(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13);
extern int fprintf14(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14);
extern int fprintf15(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15);
extern int fprintf16(FILE *stream, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15, long a16);

// Scanf variants (format + 0-16 pointer args)
extern int scanf0(char *fmt);
extern int scanf1(char *fmt, long *a1);
extern int scanf2(char *fmt, long *a1, long *a2);
extern int scanf3(char *fmt, long *a1, long *a2, long *a3);
extern int scanf4(char *fmt, long *a1, long *a2, long *a3, long *a4);
extern int scanf5(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5);
extern int scanf6(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6);
extern int scanf7(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7);
extern int scanf8(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8);
extern int scanf9(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9);
extern int scanf10(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10);
extern int scanf11(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11);
extern int scanf12(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12);
extern int scanf13(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13);
extern int scanf14(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14);
extern int scanf15(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14, long *a15);
extern int scanf16(char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14, long *a15, long *a16);

// Sscanf variants (string + format + 0-16 pointer args)
extern int sscanf0(char *str, char *fmt);
extern int sscanf1(char *str, char *fmt, long *a1);
extern int sscanf2(char *str, char *fmt, long *a1, long *a2);
extern int sscanf3(char *str, char *fmt, long *a1, long *a2, long *a3);
extern int sscanf4(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4);
extern int sscanf5(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5);
extern int sscanf6(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6);
extern int sscanf7(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7);
extern int sscanf8(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8);
extern int sscanf9(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9);
extern int sscanf10(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10);
extern int sscanf11(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11);
extern int sscanf12(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12);
extern int sscanf13(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13);
extern int sscanf14(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14);
extern int sscanf15(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14, long *a15);
extern int sscanf16(char *str, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14, long *a15, long *a16);

// Fscanf variants (stream + format + 0-16 pointer args)
extern int fscanf0(FILE *stream, char *fmt);
extern int fscanf1(FILE *stream, char *fmt, long *a1);
extern int fscanf2(FILE *stream, char *fmt, long *a1, long *a2);
extern int fscanf3(FILE *stream, char *fmt, long *a1, long *a2, long *a3);
extern int fscanf4(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4);
extern int fscanf5(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5);
extern int fscanf6(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6);
extern int fscanf7(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7);
extern int fscanf8(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8);
extern int fscanf9(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9);
extern int fscanf10(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10);
extern int fscanf11(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11);
extern int fscanf12(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12);
extern int fscanf13(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13);
extern int fscanf14(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14);
extern int fscanf15(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14, long *a15);
extern int fscanf16(FILE *stream, char *fmt, long *a1, long *a2, long *a3, long *a4, long *a5, long *a6, long *a7, long *a8, long *a9, long *a10, long *a11, long *a12, long *a13, long *a14, long *a15, long *a16);

// Snprintf variants (buffer + size + format + 0-16 args)
extern int snprintf0(char *str, long size, char *fmt);
extern int snprintf1(char *str, long size, char *fmt, long a1);
extern int snprintf2(char *str, long size, char *fmt, long a1, long a2);
extern int snprintf3(char *str, long size, char *fmt, long a1, long a2, long a3);
extern int snprintf4(char *str, long size, char *fmt, long a1, long a2, long a3, long a4);
extern int snprintf5(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5);
extern int snprintf6(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6);
extern int snprintf7(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
extern int snprintf8(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8);
extern int snprintf9(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9);
extern int snprintf10(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10);
extern int snprintf11(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11);
extern int snprintf12(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12);
extern int snprintf13(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13);
extern int snprintf14(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14);
extern int snprintf15(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15);
extern int snprintf16(char *str, long size, char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9, long a10, long a11, long a12, long a13, long a14, long a15, long a16);

// V* variants (take va_list) - Note: va_list manipulation not fully supported in VM
// These are provided for completeness but may not work as expected
extern int vprintf(char *fmt, va_list ap);
extern int vsprintf(char *str, char *fmt, va_list ap);
extern int vsnprintf(char *str, long size, char *fmt, va_list ap);
extern int vfprintf(FILE *stream, char *fmt, va_list ap);
extern int vscanf(char *fmt, va_list ap);
extern int vsscanf(char *str, char *fmt, va_list ap);
extern int vfscanf(FILE *stream, char *fmt, va_list ap);

// Argument counting for printf/scanf (format + 0-16 extra args)
// Counts total args and subtracts 1 to get number of args after format
#define __PRINTF_NARG(...) __PRINTF_NARG_(__VA_ARGS__, __PRINTF_RSEQ())
#define __PRINTF_NARG_(...) __PRINTF_ARG_N(__VA_ARGS__)
#define __PRINTF_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,N,...) N
#define __PRINTF_RSEQ() 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

// Argument counting for sprintf/fprintf/sscanf/fscanf (buf/stream + format + 0-16 extra)
// Counts total args and subtracts 2 to get number of args after buf+format
#define __SPRINTF_NARG(...) __SPRINTF_NARG_(__VA_ARGS__, __SPRINTF_RSEQ())
#define __SPRINTF_NARG_(...) __SPRINTF_ARG_N(__VA_ARGS__)
#define __SPRINTF_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,N,...) N
#define __SPRINTF_RSEQ() 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

// Argument counting for snprintf (buf + size + format + 0-16 extra)
// Counts total args and subtracts 3 to get number of args after buf+size+format
#define __SNPRINTF_NARG(...) __SNPRINTF_NARG_(__VA_ARGS__, __SNPRINTF_RSEQ())
#define __SNPRINTF_NARG_(...) __SNPRINTF_ARG_N(__VA_ARGS__)
#define __SNPRINTF_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,N,...) N
#define __SNPRINTF_RSEQ() 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

// Macro concatenation helpers
#define __CONCAT(a,b) a##b
#define __PRINTF_DISPATCH(n) __CONCAT(printf, n)
#define __SPRINTF_DISPATCH(n) __CONCAT(sprintf, n)
#define __SNPRINTF_DISPATCH(n) __CONCAT(snprintf, n)
#define __FPRINTF_DISPATCH(n) __CONCAT(fprintf, n)
#define __SCANF_DISPATCH(n) __CONCAT(scanf, n)
#define __SSCANF_DISPATCH(n) __CONCAT(sscanf, n)
#define __FSCANF_DISPATCH(n) __CONCAT(fscanf, n)

// Main macros
#define printf(...) __PRINTF_DISPATCH(__PRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)
#define scanf(...) __SCANF_DISPATCH(__PRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)
#define sprintf(...) __SPRINTF_DISPATCH(__SPRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)
#define snprintf(...) __SNPRINTF_DISPATCH(__SNPRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)
#define fprintf(...) __FPRINTF_DISPATCH(__SPRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)
#define sscanf(...) __SSCANF_DISPATCH(__SPRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)
#define fscanf(...) __FSCANF_DISPATCH(__SPRINTF_NARG(__VA_ARGS__))(__VA_ARGS__)

extern int remove(const char* filename);
extern int rename(const char* old, const char* new);
extern FILE* tmpfile(void);
extern char* tmpnam(char* s);
extern int fclose(FILE* stream);
extern int fflush(FILE* stream);
extern FILE* fopen(const char* filename, const char* mode);
extern FILE* freopen(const char* filename, const char* mode, FILE* stream);
extern void setbuf(FILE* stream, char* buf);
extern int setvbuf(FILE* stream, char* buf, int mode, size_t size);
extern int fgetc(FILE* stream);
extern char* fgets(char* s, int n, FILE* stream);
extern int fputc(int c, FILE* stream);
extern int fputs(const char* s, FILE* stream);
extern int getc(FILE* stream);
extern int getchar(void);
extern int putc(int c, FILE* stream);
extern int putchar(int c);
extern int puts(const char* s);
extern int ungetc(int c, FILE* stream);
extern size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
extern size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
extern int fgetpos(FILE* stream, fpos_t* pos);
extern int fseek(FILE* stream, long int offset, int whence);
extern int fsetpos(FILE* stream, const fpos_t* pos);
extern long int ftell(FILE* stream);
extern void rewind(FILE* stream);
extern void clearerr(FILE* stream);
extern int feof(FILE* stream);
extern int ferror(FILE* stream);
extern void perror(const char* s);

#endif // __STDIO_H
