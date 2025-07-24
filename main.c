#include "rtos.h"
#include <stdio.h>

void persistent_task(void) {
    int i = 0;
    while(1) {
        printf("Persistent task is running (%d)...\n", i++);
        fflush(stdout);
        rtos_task_delay(200);
    }
}

void terminating_task(void) {
    printf("Terminating task: I run only once and then I'm done.\n");
    fflush(stdout);
    // This task will now return, triggering the exit handler.
}

int main() {
    printf("--- Running Task Termination Test ---\n");

    rtos_task_create(persistent_task);
    rtos_task_create(terminating_task);

    rtos_start();
    return 0;
}