// pthread_mutex test - race-free counter increment
#include <pthread.h>

int counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void* increment_thread(void* arg) {
    int iterations = (int)(long long)arg;

    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

int main() {
    pthread_t t1, t2;

    // Test static initializer by using lock immediately
    pthread_mutex_lock(&lock);
    counter = 0;
    pthread_mutex_unlock(&lock);

    if (counter != 0) return 1;  // Initial value wrong

    // Create two threads that increment counter 100 times each
    pthread_create(&t1, NULL, (void*)increment_thread, (void*)100);
    pthread_create(&t2, NULL, (void*)increment_thread, (void*)100);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // With proper locking, counter should be exactly 200
    if (counter != 200) return 2;  // Race condition detected!

    // Test pthread_mutex_trylock
    int result = pthread_mutex_trylock(&lock);
    if (result != 0) return 3;  // Should succeed (lock is free)

    // Try to lock again (should fail - already locked)
    result = pthread_mutex_trylock(&lock);
    if (result == 0) return 4;  // Should have failed (EBUSY)

    pthread_mutex_unlock(&lock);

    // Test pthread_mutex_destroy
    result = pthread_mutex_destroy(&lock);
    if (result != 0) return 5;  // Destroy failed

    return 0;  // Success
}
