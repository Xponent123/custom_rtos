#ifndef RTOS_H
#define RTOS_H

#include <stdint.h>
#include <setjmp.h>

// --- Constants ---
#define MAX_TASKS 10
#define STACK_SIZE 1024 // Simulated stack size

// --- Task State ---
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_FINISHED
} TaskState;

// --- Task Control Block ---
typedef struct TCB {
    uint32_t id;
    TaskState state;
    uint8_t stack[STACK_SIZE];
    uint32_t wake_time;
    struct TCB* next;
    void (*task_fn)(void);  // Function pointer to task
} TCB;

// --- Public API ---
void rtos_start(void);
void rtos_task_create(void (*task_function)(void));
void rtos_task_yield(void);
void rtos_task_delay(uint32_t ticks);

// --- Global Variables ---
extern uint32_t system_tick;

#endif // RTOS_H
