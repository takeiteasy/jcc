/*!
 @header pthread.h
 @abstract POSIX threads API for JCC
 @discussion This header provides a subset of the POSIX pthread API for JCC.
             The implementation is backed by JCC VM opcodes that interact with
             native pthreads. This allows compiled C code to use standard
             pthread functions for multi-threaded programming.

             Key features:
             - Thread creation and management (create, join, exit, self, detach)
             - Mutex synchronization (init, lock, unlock, trylock, destroy)
             - Condition variables (init, wait, signal, broadcast, destroy)
             - Static initializers (PTHREAD_MUTEX_INITIALIZER, etc.)
             - POSIX-compliant return codes (0 = success, errno on failure)

             Current limitations:
             - Thread attributes are ignored (always use defaults)
             - Mutex/condvar attributes are ignored (always use defaults)
             - No priority inheritance or robust mutexes
             - No timed waits (timedwait, timedjoin)
             - No read-write locks, barriers, or spinlocks

 @version 1.0
 @updated 2025-11-16
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include "stddef.h"

/*!
 @typedef pthread_t
 @abstract Opaque thread identifier
 @discussion This is a handle to a VM thread. Internally it points to a
             VMThread structure, but users should treat it as opaque.
 */
typedef void* pthread_t;

/*!
 @typedef pthread_attr_t
 @abstract Thread attributes (currently unused)
 @discussion Attributes are accepted but ignored. All threads use default
             stack size and scheduling policy.
 */
typedef void* pthread_attr_t;

/*!
 @typedef pthread_mutex_t
 @abstract Mutex synchronization primitive
 @discussion Storage for mutex state. Can be statically initialized with
             PTHREAD_MUTEX_INITIALIZER or dynamically with pthread_mutex_init().
 */
typedef struct {
    long long _opaque[8];  // Enough space for pthread_mutex_t internals
} pthread_mutex_t;

/*!
 @typedef pthread_mutexattr_t
 @abstract Mutex attributes (currently unused)
 */
typedef void* pthread_mutexattr_t;

/*!
 @typedef pthread_cond_t
 @abstract Condition variable for thread synchronization
 @discussion Used for wait/signal patterns. Can be statically initialized
             with PTHREAD_COND_INITIALIZER or dynamically with pthread_cond_init().
 */
typedef struct {
    long long _opaque[8];  // Enough space for pthread_cond_t internals
} pthread_cond_t;

/*!
 @typedef pthread_condattr_t
 @abstract Condition variable attributes (currently unused)
 */
typedef void* pthread_condattr_t;

// ============================================================================
// Static Initializers
// ============================================================================

/*!
 @define PTHREAD_MUTEX_INITIALIZER
 @abstract Static initializer for mutex
 @discussion Allows mutex to be initialized at compile time:
             pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
 */
#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0, 0, 0, 0, 0, 0 }

/*!
 @define PTHREAD_COND_INITIALIZER
 @abstract Static initializer for condition variable
 */
#define PTHREAD_COND_INITIALIZER { 0, 0, 0, 0, 0, 0, 0, 0 }

// ============================================================================
// Thread Management Functions
// ============================================================================

/*!
 @function pthread_create
 @abstract Create a new thread
 @param thread Output parameter receiving the thread handle
 @param attr Thread attributes (currently ignored, pass NULL)
 @param start_routine Function to execute in new thread (pass as void*)
 @param arg Argument to pass to start_routine
 @return 0 on success, error number on failure
 @discussion Creates a new VM thread that executes start_routine(arg).
             The new thread has its own stack and TLS segment.
             Note: start_routine should be a function pointer of type void*(*)(void*)
 */
int pthread_create(pthread_t *thread, void *attr, void *start_routine, void *arg);

/*!
 @function pthread_join
 @abstract Wait for thread to terminate
 @param thread Thread to wait for
 @param retval Output parameter receiving thread's return value (can be NULL)
 @return 0 on success, error number on failure
 @discussion Blocks until the specified thread terminates. The thread must
             not be detached. After join, the thread resources are freed.
 */
int pthread_join(pthread_t thread, void **retval);

/*!
 @function pthread_detach
 @abstract Detach a thread
 @param thread Thread to detach
 @return 0 on success, error number on failure
 @discussion Marks thread as detached so resources are automatically freed
             when it terminates. Cannot join a detached thread.
 */
int pthread_detach(pthread_t thread);

/*!
 @function pthread_exit
 @abstract Exit current thread
 @param retval Return value for pthread_join (can be NULL)
 @discussion Terminates the calling thread and makes retval available to
             any thread that joins. Never returns.
 */
void pthread_exit(void *retval);

/*!
 @function pthread_self
 @abstract Get current thread handle
 @return Handle to the calling thread
 */
pthread_t pthread_self(void);

/*!
 @function pthread_equal
 @abstract Compare two thread handles
 @param t1 First thread handle
 @param t2 Second thread handle
 @return Non-zero if equal, 0 if different
 */
int pthread_equal(pthread_t t1, pthread_t t2);

// ============================================================================
// Mutex Functions
// ============================================================================

/*!
 @function pthread_mutex_init
 @abstract Initialize a mutex
 @param mutex Mutex to initialize
 @param attr Mutex attributes (currently ignored, pass NULL)
 @return 0 on success, error number on failure
 @discussion Initializes a mutex for use. Can also use static initializer
             PTHREAD_MUTEX_INITIALIZER instead.
 */
int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);

/*!
 @function pthread_mutex_destroy
 @abstract Destroy a mutex
 @param mutex Mutex to destroy
 @return 0 on success, EINVAL if not initialized
 @discussion Destroys a mutex. Must not be locked when destroyed.
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex);

/*!
 @function pthread_mutex_lock
 @abstract Lock a mutex
 @param mutex Mutex to lock
 @return 0 on success, error number on failure
 @discussion Blocks until the mutex is acquired. If the GIL is enabled,
             it is released during blocking to allow other threads to run.
 */
int pthread_mutex_lock(pthread_mutex_t *mutex);

/*!
 @function pthread_mutex_trylock
 @abstract Attempt to lock a mutex without blocking
 @param mutex Mutex to lock
 @return 0 if locked, EBUSY if already locked, other error on failure
 @discussion Non-blocking version of pthread_mutex_lock.
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex);

/*!
 @function pthread_mutex_unlock
 @abstract Unlock a mutex
 @param mutex Mutex to unlock
 @return 0 on success, EINVAL if not initialized
 @discussion Unlocks a mutex previously locked by this thread.
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex);

// ============================================================================
// Condition Variable Functions
// ============================================================================

/*!
 @function pthread_cond_init
 @abstract Initialize a condition variable
 @param cond Condition variable to initialize
 @param attr Condvar attributes (currently ignored, pass NULL)
 @return 0 on success, error number on failure
 @discussion Initializes a condition variable. Can also use static initializer
             PTHREAD_COND_INITIALIZER instead.
 */
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *attr);

/*!
 @function pthread_cond_destroy
 @abstract Destroy a condition variable
 @param cond Condition variable to destroy
 @return 0 on success, EINVAL if not initialized
 @discussion Destroys a condition variable. No threads should be waiting on it.
 */
int pthread_cond_destroy(pthread_cond_t *cond);

/*!
 @function pthread_cond_wait
 @abstract Wait on a condition variable
 @param cond Condition variable to wait on
 @param mutex Mutex that must be locked by the caller
 @return 0 on success, error number on failure
 @discussion Atomically unlocks mutex and waits on cond. When signaled,
             atomically reacquires mutex before returning. The mutex must
             be locked when calling this function.
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/*!
 @function pthread_cond_signal
 @abstract Wake one thread waiting on a condition variable
 @param cond Condition variable to signal
 @return 0 on success, error number on failure
 @discussion Wakes at least one thread waiting on cond. If no threads are
             waiting, this has no effect.
 */
int pthread_cond_signal(pthread_cond_t *cond);

/*!
 @function pthread_cond_broadcast
 @abstract Wake all threads waiting on a condition variable
 @param cond Condition variable to broadcast
 @return 0 on success, error number on failure
 @discussion Wakes all threads waiting on cond. If no threads are waiting,
             this has no effect.
 */
int pthread_cond_broadcast(pthread_cond_t *cond);

// ============================================================================
// Error Codes
// ============================================================================

// Standard POSIX error codes (from errno.h)
#ifndef EINVAL
#define EINVAL  22  // Invalid argument
#endif

#ifndef EBUSY
#define EBUSY   16  // Resource busy
#endif

#ifndef ENOMEM
#define ENOMEM  12  // Out of memory
#endif

#ifndef EAGAIN
#define EAGAIN  11  // Try again
#endif

#ifndef EDEADLK
#define EDEADLK 35  // Deadlock avoided
#endif

#endif // _PTHREAD_H
