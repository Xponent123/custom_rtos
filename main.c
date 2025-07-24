#include "rtos.h"
#include <stdio.h>

void normal_task(void) {
    int i = 0;
    while(1) {
        printf("Normal Task: Still running... (%d)\n", i++);
        fflush(stdout);
        rtos_task_delay(200); // Delay for 200 ticks
    }
}

void misbehaving_task(void) {
    printf("Misbehaving Task: I will never yield!\n");
    fflush(stdout);
    // This infinite loop would freeze a cooperative scheduler.
    while(1) {
        // Burning CPU cycles...
     
    }
}

int main() {
    printf("--- Running Preemption Test ---\n");

    rtos_task_create(normal_task);
    rtos_task_create(misbehaving_task);

    rtos_start();

    return 0;
}