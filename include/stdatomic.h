#ifndef _STDATOMIC_H
#define _STDATOMIC_H

/*
 * C11 Atomic Operations Header
 * JCC Compiler Implementation
 *
 * Provides C11 atomic types and operations for lock-free concurrent programming.
 * Current implementation provides single-threaded correctness; multi-threaded
 * atomicity will be enhanced in Phase 2 with compiler intrinsics.
 */

// Memory order enumeration (C11 7.17.3)
typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

// Atomic flag type (C11 7.17.1)
typedef struct {
    int _Value;
} atomic_flag;

// Atomic type definitions (C11 7.17.6)
// Note: In JCC, _Atomic is a type qualifier recognized by the parser
typedef _Atomic char atomic_char;
typedef _Atomic signed char atomic_schar;
typedef _Atomic unsigned char atomic_uchar;
typedef _Atomic short atomic_short;
typedef _Atomic unsigned short atomic_ushort;
typedef _Atomic int atomic_int;
typedef _Atomic unsigned int atomic_uint;
typedef _Atomic long atomic_long;
typedef _Atomic unsigned long atomic_ulong;
typedef _Atomic long long atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;

// Atomic pointer type
#define _Atomic_ptr(T) _Atomic T*
typedef _Atomic void* atomic_ptr;

// Atomic type initialization macros (C11 7.17.2.1)
#define ATOMIC_VAR_INIT(value) (value)

// Atomic flag initialization (C11 7.17.8)
#define ATOMIC_FLAG_INIT { 0 }

/*
 * Atomic Operations
 *
 * These macros map C11 atomic operations to JCC's internal __jcc_atomic_*
 * builtin functions. The memory_order parameter is currently ignored but
 * included for C11 compatibility.
 */

// atomic_store: Store value to atomic object (C11 7.17.7.2)
#define atomic_store(obj, desired) \
    atomic_store_explicit(obj, desired, memory_order_seq_cst)

#define atomic_store_explicit(obj, desired, order) \
    (*(obj) = (desired))

// atomic_load: Load value from atomic object (C11 7.17.7.1)
#define atomic_load(obj) \
    atomic_load_explicit(obj, memory_order_seq_cst)

#define atomic_load_explicit(obj, order) \
    (*(obj))

// atomic_exchange: Atomically replace value (C11 7.17.7.3)
#define atomic_exchange(obj, desired) \
    atomic_exchange_explicit(obj, desired, memory_order_seq_cst)

#define atomic_exchange_explicit(obj, desired, order) \
    __jcc_atomic_exchange(obj, desired)

// atomic_compare_exchange_strong: Compare and swap (C11 7.17.7.4)
#define atomic_compare_exchange_strong(obj, expected, desired) \
    atomic_compare_exchange_strong_explicit(obj, expected, desired, \
                                            memory_order_seq_cst, \
                                            memory_order_seq_cst)

#define atomic_compare_exchange_strong_explicit(obj, expected, desired, succ, fail) \
    __jcc_compare_and_swap(obj, expected, desired)

// atomic_compare_exchange_weak: Compare and swap (may spuriously fail)
// Note: JCC doesn't distinguish between strong and weak CAS
#define atomic_compare_exchange_weak(obj, expected, desired) \
    atomic_compare_exchange_weak_explicit(obj, expected, desired, \
                                          memory_order_seq_cst, \
                                          memory_order_seq_cst)

#define atomic_compare_exchange_weak_explicit(obj, expected, desired, succ, fail) \
    __jcc_compare_and_swap(obj, expected, desired)

/*
 * Atomic fetch-and-modify operations (C11 7.17.7.5)
 *
 * These operations atomically perform: old = *obj; *obj = old OP operand; return old;
 * Currently implemented using CAS loops (desugared by parser for compound assignments).
 */

// Helper macro for fetch operations using CAS loop
#define _ATOMIC_FETCH_OP(obj, operand, op) ({ \
    __typeof__(*(obj)) _old, _new; \
    do { \
        _old = *(obj); \
        _new = _old op (operand); \
    } while (!__jcc_compare_and_swap(obj, &_old, _new)); \
    _old; \
})

#define atomic_fetch_add(obj, operand) \
    atomic_fetch_add_explicit(obj, operand, memory_order_seq_cst)

#define atomic_fetch_add_explicit(obj, operand, order) \
    _ATOMIC_FETCH_OP(obj, operand, +)

#define atomic_fetch_sub(obj, operand) \
    atomic_fetch_sub_explicit(obj, operand, memory_order_seq_cst)

#define atomic_fetch_sub_explicit(obj, operand, order) \
    _ATOMIC_FETCH_OP(obj, operand, -)

#define atomic_fetch_or(obj, operand) \
    atomic_fetch_or_explicit(obj, operand, memory_order_seq_cst)

#define atomic_fetch_or_explicit(obj, operand, order) \
    _ATOMIC_FETCH_OP(obj, operand, |)

#define atomic_fetch_xor(obj, operand) \
    atomic_fetch_xor_explicit(obj, operand, memory_order_seq_cst)

#define atomic_fetch_xor_explicit(obj, operand, order) \
    _ATOMIC_FETCH_OP(obj, operand, ^)

#define atomic_fetch_and(obj, operand) \
    atomic_fetch_and_explicit(obj, operand, memory_order_seq_cst)

#define atomic_fetch_and_explicit(obj, operand, order) \
    _ATOMIC_FETCH_OP(obj, operand, &)

/*
 * Memory fences (C11 7.17.4)
 *
 * Currently no-ops; will be implemented with memory barriers in Phase 2.
 */

#define atomic_thread_fence(order) \
    ((void)0)

#define atomic_signal_fence(order) \
    ((void)0)

/*
 * Atomic flag operations (C11 7.17.8)
 *
 * atomic_flag is the only lock-free atomic type guaranteed by C11.
 */

// Test-and-set: atomically set flag to 1 and return previous value
#define atomic_flag_test_and_set(obj) \
    atomic_flag_test_and_set_explicit(obj, memory_order_seq_cst)

#define atomic_flag_test_and_set_explicit(obj, order) \
    __jcc_atomic_exchange(&(obj)->_Value, 1)

// Clear: atomically set flag to 0
#define atomic_flag_clear(obj) \
    atomic_flag_clear_explicit(obj, memory_order_seq_cst)

#define atomic_flag_clear_explicit(obj, order) \
    ((obj)->_Value = 0)

/*
 * Lock-free property query (C11 7.17.5)
 *
 * Returns whether the atomic type is lock-free.
 * JCC uses CAS which is lock-free on most architectures.
 */

#define ATOMIC_BOOL_LOCK_FREE 2
#define ATOMIC_CHAR_LOCK_FREE 2
#define ATOMIC_CHAR16_T_LOCK_FREE 2
#define ATOMIC_CHAR32_T_LOCK_FREE 2
#define ATOMIC_WCHAR_T_LOCK_FREE 2
#define ATOMIC_SHORT_LOCK_FREE 2
#define ATOMIC_INT_LOCK_FREE 2
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#define ATOMIC_POINTER_LOCK_FREE 2

// Query lock-free property at runtime
#define atomic_is_lock_free(obj) 1

/*
 * Additional JCC-specific notes:
 *
 * 1. Current implementation provides atomicity for single-threaded execution
 *    and correctness guarantees. True multi-threaded atomicity requires
 *    compiler intrinsics (Phase 2).
 *
 * 2. All atomic operations currently operate on 64-bit values (long long).
 *    Sized atomic operations for 8/16/32-bit values will be added in Phase 2.
 *
 * 3. Memory ordering parameters are accepted but ignored. All operations
 *    currently use sequential consistency semantics.
 *
 * 4. Compound assignments on _Atomic types are automatically transformed
 *    to CAS retry loops by the JCC parser:
 *
 *    _Atomic int x = 0;
 *    x += 5;  // Transformed to: do { old=x; new=old+5; } while(!CAS(&x,&old,new));
 *
 * 5. For advanced users, low-level builtins are available:
 *    - int __jcc_compare_and_swap(void *addr, void *expected_ptr, long new_val)
 *    - long __jcc_atomic_exchange(void *addr, long new_val)
 */

#endif // _STDATOMIC_H
