#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Global Variables ---
static TCB tasks[MAX_TASKS];
static TCB* current_task = NULL;
static int num_tasks = 0;
static ucontext_t main_context; // Context for the scheduler itself

// Queues
static TCB* ready_queue = NULL;
static TCB* sleep_queue = NULL;

// --- Helper Functions (enqueue, dequeue) ---
// (These remain the same as before)
static void enqueue(TCB** queue, TCB* tcb) {
    tcb->next = NULL;
    if (*queue == NULL) {
        *queue = tcb;
    } else {
        TCB* current = *queue;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = tcb;
    }
}

static TCB* dequeue(TCB** queue) {
    if (*queue == NULL) {
        return NULL;
    }
    TCB* tcb = *queue;
    *queue = (*queue)->next;
    tcb->next = NULL;
    return tcb;
}
// --- Scheduler ---
static void schedule(void) {
    // Get the next task to run
    TCB* next_task = dequeue(&ready_queue);
    
    if (next_task == NULL) {
        // In a real system, you might go to an idle task.
        // For now, this case shouldn't be hit if the idle loop is correct.
        return; 
    }

    current_task = next_task;
    current_task->state = TASK_RUNNING;
    
    // Swap from the main scheduler context to the task's context
    swapcontext(&main_context, &current_task->context);
}

static void idle_loop(void) {
    while(1) {
        // Process the sleep queue
        if (sleep_queue != NULL) {
            TCB* current = sleep_queue;
            TCB* prev = NULL;
            while(current != NULL) {
                current->delay_ticks--;
                if (current->delay_ticks == 0) {
                    TCB* ready_task = current;
                    TCB* next = current->next;
                    if (prev == NULL) { sleep_queue = next; }
                    else { prev->next = next; }
                    ready_task->state = TASK_READY;
                    enqueue(&ready_queue, ready_task);
                    current = next;
                } else {
                    prev = current;
                    current = current->next;
                }
            }
        }
        
        // If there's a task ready, schedule it
        if (ready_queue != NULL) {
            schedule();
        }
    }
}


// --- Public API ---

void rtos_start(void) {
    printf("Starting RTOS...\n");
    // The idle_loop will now be our main scheduler loop
    idle_loop();
}

void rtos_task_create(void (*task_function)(void)) {
    if (num_tasks >= MAX_TASKS) return;

    TCB* new_tcb = &tasks[num_tasks];
    new_tcb->id = num_tasks++;
    new_tcb->state = TASK_READY;
    new_tcb->delay_ticks = 0;

    // Get a starting context to modify
    getcontext(&new_tcb->context);

    // --- Definitive Stack Setup ---

    // The user-provided stack alignment fix was excellent, but let's refine it slightly
    // for maximum clarity and correctness with makecontext.
    
    // 1. Set the stack pointer (ss_sp) to the beginning (lowest address) of our buffer.
    //    makecontext() expects this convention.
    new_tcb->context.uc_stack.ss_sp = new_tcb->stack;

    // 2. Set the stack size. We must ensure the buffer size is a multiple of 16 for alignment.
    //    We can simply guarantee this by making our STACK_SIZE constant a multiple of 16.
    //    Let's assume STACK_SIZE is 8192 (which is a multiple of 16).
    new_tcb->context.uc_stack.ss_size = STACK_SIZE;

    // --- End of Fix ---

    // Set the context to resume when this task's function returns
    new_tcb->context.uc_link = &main_context;

    // Prepare the context to execute the desired task function
    makecontext(&new_tcb->context, (void (*)(void))task_function, 0);

    enqueue(&ready_queue, new_tcb);
    printf("Task %d created.\n", new_tcb->id);
    fflush(stdout);
}

void rtos_task_yield(void) {
    TCB* yielding_task = current_task;
    yielding_task->state = TASK_READY;
    enqueue(&ready_queue, yielding_task);
    
    // Swap back to the scheduler's main context
    swapcontext(&yielding_task->context, &main_context);
}

void rtos_task_delay(uint32_t ticks) {
    TCB* delaying_task = current_task;
    if (ticks == 0) {
        rtos_task_yield();
        return;
    }
    
    delaying_task->delay_ticks = ticks;
    delaying_task->state = TASK_SLEEPING;
    enqueue(&sleep_queue, delaying_task);

    // Swap back to the scheduler's main context
    swapcontext(&delaying_task->context, &main_context);
}