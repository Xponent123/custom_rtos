#include "rtos.h"
#include <stdio.h>

void high_priority_task(void) {
    for(int i = 0; i < 5; i++) {
        printf("HIGH priority task running (%d)...\n", i);
        fflush(stdout);
        rtos_task_delay(100);
    }
    printf("High priority task finished!\n");
}

void normal_priority_task(void) {
    for(int i = 0; i < 5; i++) {
        printf("NORMAL priority task running (%d)...\n", i);
        fflush(stdout);
        rtos_task_delay(150);
    }
    printf("Normal priority task finished!\n");
}

void low_priority_task(void) {
    for(int i = 0; i < 5; i++) {
        printf("LOW priority task running (%d)...\n", i);
        fflush(stdout);
        rtos_task_delay(200);
    }
    printf("Low priority task finished!\n");
}

int main() {
    printf("--- Priority Scheduling Test ---\n");

    // Create tasks with different priorities
    rtos_task_create_with_priority(low_priority_task, PRIORITY_LOW);
    rtos_task_create_with_priority(normal_priority_task, PRIORITY_NORMAL);  
    rtos_task_create_with_priority(high_priority_task, PRIORITY_HIGH);

    rtos_start();
    return 0;
}
