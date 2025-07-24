#include "rtos.h"
#include <stdio.h>

// The shared resource
static int g_shared_counter = 0;
// The semaphore to protect the shared resource
static rtos_semaphore_t g_counter_sem;

void race_condition_task(void) {
    while(1) {
        // "Wait" for the semaphore before entering the critical section
        rtos_sem_wait(&g_counter_sem);

        // --- Start of Critical Section ---
        int local_value = g_shared_counter;
        // We can even yield now, and the data is still safe!
        rtos_task_yield();
        local_value++;
        g_shared_counter = local_value;
        // --- End of Critical Section ---

        // "Post" the semaphore to release the lock
        rtos_sem_post(&g_counter_sem);

        printf("Task %d updated counter to: %d\n", current_task->id, g_shared_counter);
        fflush(stdout);
        rtos_task_delay(200);
    }
}

int main() {
    printf("--- Running Race Condition Test (FIXED) ---\n");

    // Initialize the semaphore with a value of 1 (making it a mutex)
    rtos_sem_init(&g_counter_sem, 1);

    rtos_task_create(race_condition_task);
    rtos_task_create(race_condition_task);

    rtos_start();
    return 0;
}