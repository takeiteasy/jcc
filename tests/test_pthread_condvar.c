// pthread condition variable test - signal/wait pattern
#include <pthread.h>

int ready = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* waiter_thread(void* arg) {
    pthread_mutex_lock(&lock);

    // Wait for signal
    while (!ready) {
        pthread_cond_wait(&cond, &lock);
    }

    // Ready flag should be set now
    if (ready != 1) {
        pthread_mutex_unlock(&lock);
        return (void*)1;  // Failed - ready not set
    }

    pthread_mutex_unlock(&lock);
    return (void*)0;  // Success
}

int main() {
    pthread_t waiter;
    void *result;

    // Create waiter thread (will block on condition variable)
    pthread_create(&waiter, NULL, (void*)waiter_thread, NULL);

    // Give thread time to start waiting (crude synchronization)
    // In real code, use a ready flag or barrier
    for (volatile int i = 0; i < 1000000; i++);

    // Signal the waiting thread
    pthread_mutex_lock(&lock);
    ready = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);

    // Wait for thread to finish
    pthread_join(waiter, &result);

    if ((long long)result != 0) return 1;  // Waiter thread failed

    // Test pthread_cond_broadcast with multiple waiters
    ready = 0;
    pthread_t waiters[3];

    for (int i = 0; i < 3; i++) {
        pthread_create(&waiters[i], NULL, (void*)waiter_thread, NULL);
    }

    // Give threads time to start waiting
    for (volatile int i = 0; i < 1000000; i++);

    // Broadcast to all waiters
    pthread_mutex_lock(&lock);
    ready = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);

    // All threads should wake up
    for (int i = 0; i < 3; i++) {
        pthread_join(waiters[i], &result);
        if ((long long)result != 0) return 2;  // Waiter failed
    }

    // Test destroy
    int err = pthread_cond_destroy(&cond);
    if (err != 0) return 3;  // Destroy failed

    err = pthread_mutex_destroy(&lock);
    if (err != 0) return 4;  // Destroy failed

    return 0;  // Success
}
