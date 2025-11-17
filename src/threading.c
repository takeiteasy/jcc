/*!
 @file threading.c
 @abstract POSIX pthread API implementation for JCC
 @discussion This file implements the pthread.h API as foreign function interface
             (FFI) wrappers that bridge user code to VM threading opcodes. All
             pthread_* functions are registered with the VM and callable from
             compiled C code.

             Architecture:
             1. User code calls pthread_create(), pthread_mutex_lock(), etc.
             2. FFI wrapper (builtin_pthread_*) validates arguments
             3. Wrapper calls VM functions or emits bytecode directly
             4. VM executes threading opcodes (THRCREATE, MTXLOCK, etc.)
             5. Opcodes interact with native pthreads + GIL

 @version 1.0
 @updated 2025-11-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "jcc.h"
#include "internal.h"

#ifndef JCC_NO_THREADS

// Global VM pointer (defined in vm.c, set during cc_init)
extern JCC *__global_vm;

// ============================================================================
// Thread Management FFI Wrappers
// ============================================================================

/*!
 @function builtin_pthread_create
 @abstract FFI wrapper for pthread_create
 @discussion Creates a new VM thread.
             Arguments: thread_ptr, attr_ptr, start_routine, arg
 */
static long long builtin_pthread_create(long long thread_ptr, long long attr_ptr,
                                        long long start_routine, long long arg) {
    JCC *vm = __global_vm;
    pthread_t *thread_out = (pthread_t *)thread_ptr;

    VMThread *current = vm_thread_get_current(vm);
    fprintf(stderr, "DEBUG pthread_create called from thread=%p (pc=%p)\n",
            current, current ? current->pc : NULL);

    // Validate arguments
    if (!thread_out) {
        return 22;  // EINVAL
    }

    // Convert start_routine to entry offset
    // start_routine should be an absolute address (pointer) in text_seg
    // But if it's a small value or negative, treat it as already being an offset
    long long entry_offset;
    long long text_seg_addr = (long long)vm->text_seg;
    long long text_seg_end = text_seg_addr + vm->poolsize;

    if (start_routine >= text_seg_addr && start_routine < text_seg_end) {
        // It's an absolute address within text_seg, convert to offset
        entry_offset = start_routine - text_seg_addr;
    } else {
        // It's already an offset (or invalid) - use as-is
        entry_offset = start_routine;
    }

    fprintf(stderr, "DEBUG: Allocating thread with entry_offset=%lld, text_seg=%p, will set PC to %p\n",
            entry_offset, vm->text_seg, (void*)((char*)vm->text_seg + entry_offset));

    // Allocate VM thread with argument
    long long args[1] = { arg };
    VMThread *t = vm_thread_allocate(vm, entry_offset, args, 1);
    if (!t) {
        return 12;  // ENOMEM
    }

    fprintf(stderr, "DEBUG: Thread allocated, t->pc=%p\n", t->pc);

    // Launch native pthread
    int result = pthread_create(&t->thread_id, NULL, vm_thread_entry, t);
    if (result != 0) {
        vm_thread_destroy(t);
        return result;
    }

    // Add to thread list
    t->next = vm->threads;
    vm->threads = t;

    // Return thread handle (opaque VMThread pointer)
    *thread_out = (pthread_t)t;

    fprintf(stderr, "DEBUG builtin_pthread_create: Created thread VMThread=%p, writing to thread_out=%p, value written=0x%llx\n",
            t, thread_out, (long long)*thread_out);

    return 0;  // Success
}

/*!
 @function builtin_pthread_join
 @abstract FFI wrapper for pthread_join
 @discussion Waits for thread termination.
             Arguments: thread, retval_ptr
 */
static long long builtin_pthread_join(long long thread_ptr, long long retval_ptr) {
    JCC *vm = __global_vm;

    VMThread *current = vm_thread_get_current(vm);
    fprintf(stderr, "DEBUG pthread_join called from thread=%p (pc=%p)\n",
            current, current ? current->pc : NULL);
    fprintf(stderr, "DEBUG builtin_pthread_join: thread_ptr=0x%llx, retval_ptr=0x%llx\n",
            thread_ptr, retval_ptr);

    VMThread *thread = (VMThread *)thread_ptr;

    // Validate arguments
    if (!thread) {
        return 22;  // EINVAL
    }

    if (thread->detached) {
        return 22;  // EINVAL - cannot join detached thread
    }

    // NOTE: State save/restore is now handled by CALLF opcode handler in vm.c
    // This function no longer needs to manage thread state directly

    // Release GIL before blocking (if enabled)
    if (vm->enable_gil) {
        fprintf(stderr, "DEBUG pthread_join: About to release GIL\n");
        gil_release(&vm->gil);
        fprintf(stderr, "DEBUG pthread_join: GIL released\n");
    }

    // Wait for thread termination
    fprintf(stderr, "DEBUG pthread_join: About to call pthread_join on thread_id=%p\n",
            (void*)thread->thread_id);
    pthread_join(thread->thread_id, NULL);
    fprintf(stderr, "DEBUG pthread_join: pthread_join returned successfully\n");

    // Reacquire GIL
    if (vm->enable_gil) {
        fprintf(stderr, "DEBUG pthread_join: About to reacquire GIL\n");
        gil_acquire(&vm->gil);
        fprintf(stderr, "DEBUG pthread_join: GIL reacquired\n");
    }

    // Return thread's exit code
    if (retval_ptr) {
        void **retval_out = (void **)retval_ptr;
        *retval_out = (void *)(long long)thread->exit_code;
    }

    // Mark as terminated
    thread->terminated = 1;

    // Clean up thread (remove from list and free resources)
    VMThread **prev = &vm->threads;
    while (*prev && *prev != thread) {
        prev = &(*prev)->next;
    }
    if (*prev == thread) {
        *prev = thread->next;
    }

    vm_thread_destroy(thread);

    return 0;  // Success
}

/*!
 @function builtin_pthread_detach
 @abstract FFI wrapper for pthread_detach
 @discussion Marks thread as detached.
             Arguments: thread
 */
static long long builtin_pthread_detach(long long thread_ptr) {
    VMThread *thread = (VMThread *)thread_ptr;

    if (!thread) {
        return 22;  // EINVAL
    }

    thread->detached = 1;

    // Detach the native pthread
    pthread_detach(thread->thread_id);

    return 0;  // Success
}

/*!
 @function builtin_pthread_exit
 @abstract FFI wrapper for pthread_exit
 @discussion Terminates current thread.
             Arguments: retval
 */
static long long builtin_pthread_exit(long long retval) {
    JCC *vm = __global_vm;

    VMThread *current = vm_thread_get_current(vm);
    if (current) {
        current->exit_code = (int)retval;
        current->terminated = 1;
    }

    // Release GIL if held
    if (vm->enable_gil) {
        gil_release(&vm->gil);
    }

    // Exit native pthread
    pthread_exit((void *)retval);

    // Never returns
    return 0;
}

/*!
 @function builtin_pthread_self
 @abstract FFI wrapper for pthread_self
 @discussion Returns current thread handle.
             Arguments: none
 */
static long long builtin_pthread_self() {
    JCC *vm = __global_vm;
    VMThread *current = vm_thread_get_current(vm);
    return (long long)current;
}

/*!
 @function builtin_pthread_equal
 @abstract FFI wrapper for pthread_equal
 @discussion Compares two thread handles.
             Arguments: t1, t2
 */
static long long builtin_pthread_equal(long long t1, long long t2) {
    int result = (t1 == t2) ? 1 : 0;
    return result;
}

// ============================================================================
// Mutex FFI Wrappers
// ============================================================================

/*!
 @function builtin_pthread_mutex_init
 @abstract FFI wrapper for pthread_mutex_init
 @discussion Initializes a mutex.
             Arguments: mutex_ptr, attr_ptr
 */
static long long builtin_pthread_mutex_init(long long mutex_ptr, long long attr_ptr) {
    JCC *vm = __global_vm;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_ptr;

    if (!mutex) {
        return 22;  // EINVAL
    }

    // Check if already initialized
    VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex);
    if (m && m->initialized) {
        return 0;  // Already initialized (POSIX allows this)
    }

    // Create new VMMutex
    if (!m) {
        m = (VMMutex *)calloc(1, sizeof(VMMutex));
        hashmap_put(&vm->mutexes, (char *)mutex, m);
    }

    pthread_mutex_init(&m->mutex, NULL);
    m->initialized = 1;
    m->owner_thread = 0;

    return 0;  // Success
}

/*!
 @function builtin_pthread_mutex_destroy
 @abstract FFI wrapper for pthread_mutex_destroy
 @discussion Destroys a mutex.
             Arguments: mutex_ptr
 */
static long long builtin_pthread_mutex_destroy(long long mutex_ptr) {
    JCC *vm = __global_vm;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_ptr;

    if (!mutex) {
        return 22;  // EINVAL
    }

    VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex);
    if (!m || !m->initialized) {
        return 22;  // EINVAL
    }

    pthread_mutex_destroy(&m->mutex);
    m->initialized = 0;

    return 0;  // Success
}

/*!
 @function builtin_pthread_mutex_lock
 @abstract FFI wrapper for pthread_mutex_lock
 @discussion Locks a mutex, blocking if necessary.
             Arguments: mutex_ptr
 */
static long long builtin_pthread_mutex_lock(long long mutex_ptr) {
    JCC *vm = __global_vm;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_ptr;

    if (!mutex) {
        return 22;  // EINVAL
    }

    // Get or create mutex (lazy initialization for static initializers)
    VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex);
    if (!m || !m->initialized) {
        // Lazy initialization
        if (!m) {
            m = (VMMutex *)calloc(1, sizeof(VMMutex));
            hashmap_put(&vm->mutexes, (char *)mutex, m);
        }
        pthread_mutex_init(&m->mutex, NULL);
        m->initialized = 1;
        m->owner_thread = 0;
    }

    // Release GIL before blocking (if enabled)
    if (vm->enable_gil) {
        gil_release(&vm->gil);
    }

    pthread_mutex_lock(&m->mutex);

    // Reacquire GIL after unblocking
    if (vm->enable_gil) {
        gil_acquire(&vm->gil);
    }

    m->owner_thread = (long long)pthread_self();

    return 0;  // Success
}

/*!
 @function builtin_pthread_mutex_trylock
 @abstract FFI wrapper for pthread_mutex_trylock
 @discussion Non-blocking lock attempt.
             Arguments: mutex_ptr
 */
static long long builtin_pthread_mutex_trylock(long long mutex_ptr) {
    JCC *vm = __global_vm;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_ptr;

    if (!mutex) {
        return 22;  // EINVAL
    }

    // Get or create mutex (lazy initialization)
    VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex);
    if (!m || !m->initialized) {
        if (!m) {
            m = (VMMutex *)calloc(1, sizeof(VMMutex));
            hashmap_put(&vm->mutexes, (char *)mutex, m);
        }
        pthread_mutex_init(&m->mutex, NULL);
        m->initialized = 1;
        m->owner_thread = 0;
    }

    int result = pthread_mutex_trylock(&m->mutex);
    if (result == 0) {
        m->owner_thread = (long long)pthread_self();
    }

    return result;  // 0 = success, EBUSY (16) = locked
}

/*!
 @function builtin_pthread_mutex_unlock
 @abstract FFI wrapper for pthread_mutex_unlock
 @discussion Unlocks a mutex.
             Arguments: mutex_ptr
 */
static long long builtin_pthread_mutex_unlock(long long mutex_ptr) {
    JCC *vm = __global_vm;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_ptr;

    if (!mutex) {
        return 22;  // EINVAL
    }

    VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex);
    if (!m || !m->initialized) {
        return 22;  // EINVAL
    }

    m->owner_thread = 0;
    pthread_mutex_unlock(&m->mutex);

    return 0;  // Success
}

// ============================================================================
// Condition Variable FFI Wrappers
// ============================================================================

/*!
 @function builtin_pthread_cond_init
 @abstract FFI wrapper for pthread_cond_init
 @discussion Initializes a condition variable.
             Arguments: cond_ptr, attr_ptr
 */
static long long builtin_pthread_cond_init(long long cond_ptr, long long attr_ptr) {
    JCC *vm = __global_vm;
    pthread_cond_t *cond = (pthread_cond_t *)cond_ptr;

    if (!cond) {
        return 22;  // EINVAL
    }

    VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond);
    if (cv && cv->initialized) {
        return 0;  // Already initialized
    }

    if (!cv) {
        cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
        hashmap_put(&vm->condvars, (char *)cond, cv);
    }

    pthread_cond_init(&cv->cond, NULL);
    cv->initialized = 1;

    return 0;  // Success
}

/*!
 @function builtin_pthread_cond_destroy
 @abstract FFI wrapper for pthread_cond_destroy
 @discussion Destroys a condition variable.
             Arguments: cond_ptr
 */
static long long builtin_pthread_cond_destroy(long long cond_ptr) {
    JCC *vm = __global_vm;
    pthread_cond_t *cond = (pthread_cond_t *)cond_ptr;

    if (!cond) {
        return 22;  // EINVAL
    }

    VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond);
    if (!cv || !cv->initialized) {
        return 22;  // EINVAL
    }

    pthread_cond_destroy(&cv->cond);
    cv->initialized = 0;

    return 0;  // Success
}

/*!
 @function builtin_pthread_cond_wait
 @abstract FFI wrapper for pthread_cond_wait
 @discussion Atomically unlocks mutex and waits on condition variable.
             Arguments: cond_ptr, mutex_ptr
 */
static long long builtin_pthread_cond_wait(long long cond_ptr, long long mutex_ptr) {
    JCC *vm = __global_vm;
    pthread_cond_t *cond = (pthread_cond_t *)cond_ptr;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_ptr;

    if (!cond || !mutex) {
        return 22;  // EINVAL
    }

    // Get or create condvar (lazy initialization)
    VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond);
    if (!cv || !cv->initialized) {
        if (!cv) {
            cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
            hashmap_put(&vm->condvars, (char *)cond, cv);
        }
        pthread_cond_init(&cv->cond, NULL);
        cv->initialized = 1;
    }

    // Get mutex
    VMMutex *m = (VMMutex *)hashmap_get(&vm->mutexes, (char *)mutex);
    if (!m || !m->initialized) {
        return 22;  // EINVAL - mutex must be initialized
    }

    // Release GIL before blocking (pthread_cond_wait handles mutex atomically)
    if (vm->enable_gil) {
        gil_release(&vm->gil);
    }

    m->owner_thread = 0;  // Will be released by pthread_cond_wait
    pthread_cond_wait(&cv->cond, &m->mutex);
    m->owner_thread = (long long)pthread_self();  // Reacquired by pthread_cond_wait

    // Reacquire GIL after unblocking
    if (vm->enable_gil) {
        gil_acquire(&vm->gil);
    }

    return 0;  // Success
}

/*!
 @function builtin_pthread_cond_signal
 @abstract FFI wrapper for pthread_cond_signal
 @discussion Wakes one thread waiting on condition variable.
             Arguments: cond_ptr
 */
static long long builtin_pthread_cond_signal(long long cond_ptr) {
    JCC *vm = __global_vm;
    pthread_cond_t *cond = (pthread_cond_t *)cond_ptr;

    if (!cond) {
        return 22;  // EINVAL
    }

    // Get or create condvar (lazy initialization)
    VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond);
    if (!cv || !cv->initialized) {
        if (!cv) {
            cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
            hashmap_put(&vm->condvars, (char *)cond, cv);
        }
        pthread_cond_init(&cv->cond, NULL);
        cv->initialized = 1;
    }

    pthread_cond_signal(&cv->cond);

    return 0;  // Success
}

/*!
 @function builtin_pthread_cond_broadcast
 @abstract FFI wrapper for pthread_cond_broadcast
 @discussion Wakes all threads waiting on condition variable.
             Arguments: cond_ptr
 */
static long long builtin_pthread_cond_broadcast(long long cond_ptr) {
    JCC *vm = __global_vm;
    pthread_cond_t *cond = (pthread_cond_t *)cond_ptr;

    if (!cond) {
        return 22;  // EINVAL
    }

    // Get or create condvar (lazy initialization)
    VMCondVar *cv = (VMCondVar *)hashmap_get(&vm->condvars, (char *)cond);
    if (!cv || !cv->initialized) {
        if (!cv) {
            cv = (VMCondVar *)calloc(1, sizeof(VMCondVar));
            hashmap_put(&vm->condvars, (char *)cond, cv);
        }
        pthread_cond_init(&cv->cond, NULL);
        cv->initialized = 1;
    }

    pthread_cond_broadcast(&cv->cond);

    return 0;  // Success
}

// ============================================================================
// Registration Function
// ============================================================================

/*!
 @function register_pthread_api
 @abstract Registers all pthread_* functions with the VM
 @discussion Called during VM initialization to make pthread functions
             available to compiled C code. Each function is registered
             with its name, wrapper function pointer, and argument count.
 */
void register_pthread_api(JCC *vm) {
    // Thread management functions
    cc_register_cfunc(vm, "pthread_create", (void *)builtin_pthread_create, 4, 0);
    cc_register_cfunc(vm, "pthread_join", (void *)builtin_pthread_join, 2, 0);
    cc_register_cfunc(vm, "pthread_detach", (void *)builtin_pthread_detach, 1, 0);
    cc_register_cfunc(vm, "pthread_exit", (void *)builtin_pthread_exit, 1, 0);
    cc_register_cfunc(vm, "pthread_self", (void *)builtin_pthread_self, 0, 0);
    cc_register_cfunc(vm, "pthread_equal", (void *)builtin_pthread_equal, 2, 0);

    // Mutex functions
    cc_register_cfunc(vm, "pthread_mutex_init", (void *)builtin_pthread_mutex_init, 2, 0);
    cc_register_cfunc(vm, "pthread_mutex_destroy", (void *)builtin_pthread_mutex_destroy, 1, 0);
    cc_register_cfunc(vm, "pthread_mutex_lock", (void *)builtin_pthread_mutex_lock, 1, 0);
    cc_register_cfunc(vm, "pthread_mutex_trylock", (void *)builtin_pthread_mutex_trylock, 1, 0);
    cc_register_cfunc(vm, "pthread_mutex_unlock", (void *)builtin_pthread_mutex_unlock, 1, 0);

    // Condition variable functions
    cc_register_cfunc(vm, "pthread_cond_init", (void *)builtin_pthread_cond_init, 2, 0);
    cc_register_cfunc(vm, "pthread_cond_destroy", (void *)builtin_pthread_cond_destroy, 1, 0);
    cc_register_cfunc(vm, "pthread_cond_wait", (void *)builtin_pthread_cond_wait, 2, 0);
    cc_register_cfunc(vm, "pthread_cond_signal", (void *)builtin_pthread_cond_signal, 1, 0);
    cc_register_cfunc(vm, "pthread_cond_broadcast", (void *)builtin_pthread_cond_broadcast, 1, 0);
}

#else  // JCC_NO_THREADS

// Stub when threading is disabled
void register_pthread_api(JCC *vm) {
    (void)vm;  // Unused
}

#endif  // JCC_NO_THREADS
