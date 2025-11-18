// stdlib.h stdlib function registration
#include "../jcc.h"

// Register all stdlib.h functions
void register_stdlib_functions(JCC *vm) {
    // Conversion functions
    cc_register_cfunc(vm, "atof", (void*)atof, 1, 1);
    cc_register_cfunc(vm, "atoi", (void*)atoi, 1, 1);
    cc_register_cfunc(vm, "atoll", (void*)atoll, 1, 0);
    cc_register_cfunc(vm, "strtod", (void*)strtod, 2, 1);
    cc_register_cfunc(vm, "strtof", (void*)strtof, 2, 0);
    cc_register_cfunc(vm, "strtold", (void*)strtold, 2, 1);
    cc_register_cfunc(vm, "strtol", (void*)strtol, 3, 0);
    cc_register_cfunc(vm, "strtoll", (void*)strtoll, 3, 0);
    cc_register_cfunc(vm, "strtoul", (void*)strtoul, 3, 0);
    cc_register_cfunc(vm, "strtoull", (void*)strtoull, 3, 0);

    // Random number generation
    cc_register_cfunc(vm, "rand", (void*)rand, 0, 0);
    cc_register_cfunc(vm, "srand", (void*)srand, 1, 0);

    // Memory allocation functions
    cc_register_cfunc(vm, "calloc", (void*)calloc, 2, 0);
    cc_register_cfunc(vm, "free", (void*)free, 1, 0);
    cc_register_cfunc(vm, "malloc", (void*)malloc, 1, 0);
    cc_register_cfunc(vm, "realloc", (void*)realloc, 2, 0);
    cc_register_cfunc(vm, "posix_memalign", (void*)posix_memalign, 3, 0);

    // Process control
    cc_register_cfunc(vm, "abort", (void*)abort, 0, 0);
    cc_register_cfunc(vm, "exit", (void*)exit, 1, 0);
    cc_register_cfunc(vm, "_Exit", (void*)_Exit, 1, 0);
    cc_register_cfunc(vm, "atexit", (void*)atexit, 1, 0);

    // Environment
    cc_register_cfunc(vm, "getenv", (void*)getenv, 1, 0);
    cc_register_cfunc(vm, "system", (void*)system, 1, 0);

    // Search and sort
    cc_register_cfunc(vm, "bsearch", (void*)bsearch, 4, 0);
    cc_register_cfunc(vm, "qsort", (void*)qsort, 3, 0);

    // Integer arithmetic
    cc_register_cfunc(vm, "abs", (void*)abs, 1, 1);
    cc_register_cfunc(vm, "labs", (void*)labs, 1, 1);
    cc_register_cfunc(vm, "llabs", (void*)llabs, 1, 1);
    cc_register_cfunc(vm, "div", (void*)div, 2, 0);
    cc_register_cfunc(vm, "ldiv", (void*)ldiv, 2, 0);
    cc_register_cfunc(vm, "lldiv", (void*)lldiv, 2, 0);

    // Multibyte/wide character conversion
    cc_register_cfunc(vm, "mblen", (void*)mblen, 2, 0);
    cc_register_cfunc(vm, "mbtowc", (void*)mbtowc, 3, 0);
    cc_register_cfunc(vm, "wctomb", (void*)wctomb, 2, 0);
    cc_register_cfunc(vm, "mbstowcs", (void*)mbstowcs, 3, 0);
    cc_register_cfunc(vm, "wcstombs", (void*)wcstombs, 3, 0);
}
