// Basic pthread_create and pthread_join test
#include <pthread.h>

int shared_value = 0;

void* thread_func(void* arg) {
    int val = (int)(long long)arg;
    shared_value = val;
    return (void*)(long long)(val * 2);
}

int main() {
    pthread_t thread;
    void *retval;

    // Test pthread_create
    int result = pthread_create(&thread, NULL, thread_func, (void*)42);
    if (result != 0) return 1;  // Failed to create thread

    // Test pthread_join
    result = pthread_join(thread, &retval);
    if (result != 0) return 2;  // Failed to join

    // Verify shared_value was written by thread
    if (shared_value != 42) return 3;  // Thread didn't execute

    // Verify return value
    if ((long long)retval != 84) return 4;  // Wrong return value

    // Test pthread_self
    pthread_t self = pthread_self();
    if (self == 0) return 5;  // pthread_self failed

    // Test pthread_equal
    if (!pthread_equal(self, self)) return 6;  // Equal failed
    if (pthread_equal(self, thread)) return 7;  // Should not be equal

    return 0;  // Success
}
