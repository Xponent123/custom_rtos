#include "rtos.h"
#include <stdio.h>

// Demo showing task yielding
static int priority_task_count = 0;
static int normal_task_count = 0;

void priority_task(void) {
    printf("Priority Task: Iteration %d\n", ++priority_task_count);
    
    if (priority_task_count >= 3) {
        printf("Priority Task: Completed\n");
        return;
    }
    
    // Yield to allow other tasks to run
    rtos_task_yield();
}

void normal_task(void) {
    printf("Normal Task: Iteration %d\n", ++normal_task_count);
    
    if (normal_task_count >= 5) {
        printf("Normal Task: Completed\n");
        return;
    }
    
    // Yield to allow other tasks to run
    rtos_task_yield();
}

void background_task(void) {
    static int count = 0;
    printf("Background Task: Working... %d\n", ++count);
    
    if (count >= 2) {
        printf("Background Task: Completed\n");
        return;
    }
    
    // Delay for a bit then continue
    rtos_task_delay(1);
}

int main() {
    printf("RTOS Demo - Task Yielding\n");
    printf("==========================\n");
    
    rtos_task_create(priority_task);
    rtos_task_create(normal_task);
    rtos_task_create(background_task);
    
    rtos_start();
    
    printf("Yield Demo Complete!\n");
    return 0;
}
