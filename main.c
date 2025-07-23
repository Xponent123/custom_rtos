    #include "rtos.h"
#include <stdio.h>

// Task state variables
static int task1_counter = 0;
static int task2_counter = 0;
static int task3_counter = 0;

// --- Demo Tasks ---
void task1(void) {
    printf("Task 1: Running! (count: %d)\n", ++task1_counter);
    
    if (task1_counter >= 5) {
        printf("Task 1: Finished\n");
        return; // Task finishes
    }
    
    // Sleep for 3 ticks then re-queue
    rtos_task_delay(3);
}

void task2(void) {
    printf("Task 2: Running! (count: %d)\n", ++task2_counter);
    
    if (task2_counter >= 3) {
        printf("Task 2: Finished\n");
        return; // Task finishes
    }
    
    // Sleep for 5 ticks then re-queue
    rtos_task_delay(5);
}

void task3(void) {
    printf("Task 3: Running! (count: %d)\n", ++task3_counter);
    
    if (task3_counter >= 4) {
        printf("Task 3: Finished\n");
        return; // Task finishes
    }
    
    // Sleep for 2 ticks then re-queue
    rtos_task_delay(2);
}

// --- Main Function ---
int main() {
    printf("Custom RTOS Demo - Cooperative Multitasking\n");
    printf("===========================================\n");
    
    // Create the tasks
    rtos_task_create(task1);
    rtos_task_create(task2);
    rtos_task_create(task3);
    
    // Start the RTOS
    rtos_start();
    
    printf("RTOS Demo Complete!\n");
    return 0;
}