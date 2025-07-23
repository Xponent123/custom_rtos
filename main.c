#include "rtos.h"
#include <stdio.h>

void normal_task() { while(1) { printf("N"); fflush(stdout); rtos_task_delay(100); } }
void terminating_task() {
    printf("\nT: I am a task that will run once and finish.\n");
    fflush(stdout);
    // This task will now return, and uc_link should return control to the scheduler.
}

int main() {
    printf("--- Running Test 2: Task Termination ---\n");
    rtos_task_create(normal_task);
    rtos_task_create(terminating_task);
    rtos_start();
    return 0;
}