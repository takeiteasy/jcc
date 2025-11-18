// math.h stdlib function registration
#include "../jcc.h"
#include <math.h>

// Special value helpers
static double __jcc_huge_val(void) {
    double huge = HUGE_VAL;
    return huge;
}

static float __jcc_inff(void) {
    float inf = INFINITY;
    return inf;
}

static float __jcc_nanf(const char *s) {
    (void)s;  // Unused parameter
    return NAN;
}

static int __jcc_isnan(double x) {
    return isnan(x);
}

static int __jcc_isinf(double x) {
    return isinf(x);
}

// Register all math.h functions
void register_math_functions(JCC *vm) {
    // Special values
    cc_register_cfunc(vm, "__jcc_huge_val", (void*)__jcc_huge_val, 0, 1);
    cc_register_cfunc(vm, "__jcc_inff", (void*)__jcc_inff, 0, 1);
    cc_register_cfunc(vm, "__jcc_nanf", (void*)__jcc_nanf, 1, 1);
    cc_register_cfunc(vm, "__jcc_isnan", (void*)__jcc_isnan, 1, 0);
    cc_register_cfunc(vm, "__jcc_isinf", (void*)__jcc_isinf, 1, 0);

    // Basic operations
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

    // Exponential/logarithmic
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

    // Trigonometric
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

    // Hyperbolic
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

    // Special functions
    cc_register_cfunc(vm, "erf", (void*)erf, 1, 1);
    cc_register_cfunc(vm, "erff", (void*)erff, 1, 0);
    cc_register_cfunc(vm, "erfl", (void*)erfl, 1, 1);
    cc_register_cfunc(vm, "erfc", (void*)erfc, 1, 1);
    cc_register_cfunc(vm, "erfcf", (void*)erfcf, 1, 0);
    cc_register_cfunc(vm, "erfcl", (void*)erfcl, 1, 1);
    cc_register_cfunc(vm, "tgamma", (void*)tgamma, 1, 1);
    cc_register_cfunc(vm, "tgammaf", (void*)tgammaf, 1, 0);
    cc_register_cfunc(vm, "tgammal", (void*)tgammal, 1, 1);
    cc_register_cfunc(vm, "lgamma", (void*)lgamma, 1, 1);
    cc_register_cfunc(vm, "lgammaf", (void*)lgammaf, 1, 0);
    cc_register_cfunc(vm, "lgammal", (void*)lgammal, 1, 1);

    // Rounding
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

    // Manipulation
    cc_register_cfunc(vm, "frexp", (void*)frexp, 3, 0);
    cc_register_cfunc(vm, "frexpf", (void*)frexpf, 3, 0);
    cc_register_cfunc(vm, "frexpl", (void*)frexpl, 3, 0);
    cc_register_cfunc(vm, "ldexp", (void*)ldexp, 2, 1);
    cc_register_cfunc(vm, "ldexpf", (void*)ldexpf, 2, 0);
    cc_register_cfunc(vm, "ldexpl", (void*)ldexpl, 2, 1);
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
}
