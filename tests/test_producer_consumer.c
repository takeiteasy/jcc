// Producer-consumer pattern with bounded buffer
#include <pthread.h>

#define BUFFER_SIZE 5
#define ITEMS_TO_PRODUCE 20

int buffer[BUFFER_SIZE];
int count = 0;  // Number of items in buffer
int in = 0;     // Next position to write
int out = 0;    // Next position to read
int produced = 0;
int consumed = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

void* producer(void* arg) {
    for (int i = 0; i < ITEMS_TO_PRODUCE; i++) {
        pthread_mutex_lock(&mutex);

        // Wait while buffer is full
        while (count == BUFFER_SIZE) {
            pthread_cond_wait(&not_full, &mutex);
        }

        // Produce item
        buffer[in] = i;
        in = (in + 1) % BUFFER_SIZE;
        count++;
        produced++;

        // Signal that buffer is not empty
        pthread_cond_signal(&not_empty);

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

void* consumer(void* arg) {
    for (int i = 0; i < ITEMS_TO_PRODUCE; i++) {
        pthread_mutex_lock(&mutex);

        // Wait while buffer is empty
        while (count == 0) {
            pthread_cond_wait(&not_empty, &mutex);
        }

        // Consume item
        int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        count--;
        consumed++;

        // Verify item value (should be sequential)
        if (item != i) {
            pthread_mutex_unlock(&mutex);
            return (void*)1;  // Wrong item consumed!
        }

        // Signal that buffer is not full
        pthread_cond_signal(&not_full);

        pthread_mutex_unlock(&mutex);
    }

    return (void*)0;  // Success
}

int main() {
    pthread_t prod_thread, cons_thread;
    void *result;

    // Create producer and consumer
    pthread_create(&prod_thread, NULL, (void*)producer, NULL);
    pthread_create(&cons_thread, NULL, (void*)consumer, NULL);

    // Wait for both to finish
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, &result);

    // Check consumer result
    if ((long long)result != 0) return 1;  // Consumer failed

    // Verify all items were produced and consumed
    if (produced != ITEMS_TO_PRODUCE) return 2;  // Not all produced
    if (consumed != ITEMS_TO_PRODUCE) return 3;  // Not all consumed
    if (count != 0) return 4;  // Buffer not empty

    return 0;  // Success
}
