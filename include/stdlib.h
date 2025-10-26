// Standard library function declarations for FFI
// This header provides declarations for C standard library functions
// that are registered in the VM's FFI and can be called directly

#ifndef __STDLIB_H
#define __STDLIB_H 

#include "stddef.h"

/* EXIT macros */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

// typedef unsigned int once_flag;
// extern void call_once(once_flag* flag, void (*func)(void));

extern double atof(const char* nptr);
extern int atoi(const char* nptr);
extern long int atol(const char* nptr);
extern long long int atoll(const char* nptr);
// extern int strfromd(char* s, size_t n, const char* format, double fp);
// extern int strfromf(char* s, size_t n, const char* format, float fp);
// extern int strfroml(char* s, size_t n, const char* format, long double fp);
extern double strtod(const char* nptr, char** endptr);
extern float strtof(const char* nptr, char** endptr);
extern long double strtold(const char* nptr, char** endptr);
extern long int strtol(const char* nptr, char** endptr, int base);
extern long long int strtoll(const char* nptr, char** endptr, int base);
extern unsigned long int strtoul(const char* nptr, char** endptr, int base);
extern unsigned long long int strtoull(const char* nptr, char** endptr, int base);

extern int rand(void);
extern void srand(unsigned int seed);
// extern void* aligned_alloc(size_t alignment, size_t size);
extern void* calloc(size_t nmemb, size_t size);
extern void free(void* ptr);
// extern void free_sized(void* ptr, size_t size);
// extern void free_aligned_sized(void* ptr, size_t alignment, size_t size);
extern void* malloc(size_t size);
extern void* realloc(void* ptr, size_t size);
extern void abort(void);
extern int atexit(void (*func)(void));
// extern int at_quick_exit(void (*func)(void));
extern void exit(int status);
// extern void _Exit(int status);
extern char* getenv(const char* name);
extern void quick_exit(int status);
extern int system(const char* string);
extern int posix_memalign(void** memptr, size_t alignment, size_t size);
extern void* bsearch(const void* key, void* base, size_t nmemb, size_t size, int (*compar)(const void* , const void* ));
extern void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void* , const void* ));
extern int abs(int j);
extern long int labs(long int j);
extern long long int llabs(long long int j);

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

extern div_t div(int numer, int denom);
extern ldiv_t ldiv(long int numer, long int denom);
extern lldiv_t lldiv(long long int numer, long long int denom);

typedef int wchar_t; // Temporary definition for wchar_t

extern int mblen(const char* s, size_t n);
extern int mbtowc(wchar_t* pwc, const char* s, size_t n);
extern int wctomb(char* s, wchar_t wc);

extern size_t mbstowcs(wchar_t* pwcs, const char* s, size_t n);
extern size_t wcstombs(char* s, const wchar_t* pwcs, size_t n);
extern size_t memalignment(const void* p);

#endif
