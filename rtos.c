#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Global Variables ---
static TCB tasks[MAX_TASKS];
TCB* current_task = NULL;
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

int rtos_task_create(void (*task_function)(void)) {
    // 1. Check if the task limit has been reached
    if (num_tasks >= MAX_TASKS) {
        fprintf(stderr, "Error: Maximum number of tasks reached.\n");
        return -1; // Return an error code
    }

    TCB* new_tcb = &tasks[num_tasks];
    
    // 2. Check the return value of getcontext()
    if (getcontext(&new_tcb->context) == -1) {
        perror("getcontext failed"); // perror prints our message + the system error
        return -1;
    }

    // --- Stack Setup (Stays the same) ---
    new_tcb->context.uc_stack.ss_sp = new_tcb->stack;
    new_tcb->context.uc_stack.ss_size = STACK_SIZE;
    new_tcb->context.uc_link = &main_context;

    // --- Context Creation (Stays the same) ---
    makecontext(&new_tcb->context, (void (*)(void))task_function, 0);

    // --- Final TCB Initialization ---
    new_tcb->id = num_tasks++;
    new_tcb->state = TASK_READY;
    new_tcb->delay_ticks = 0;
    
    enqueue(&ready_queue, new_tcb);
    printf("Task %d created.\n", new_tcb->id);
    fflush(stdout);
    
    return 0; // Return 0 for success
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

// Add these three functions to the end of your rtos.c file

void rtos_sem_init(rtos_semaphore_t* sem, int initial_value) {
    sem->value = initial_value;
    sem->wait_queue = NULL;
}

void rtos_sem_wait(rtos_semaphore_t* sem) {
    // If the semaphore value is positive, decrement it and continue.
    if (sem->value > 0) {
        sem->value--;
        return;
    }

    // If the semaphore value is zero, the task must block.
    TCB* waiting_task = current_task;
    waiting_task->state = TASK_BLOCKED;
    enqueue(&sem->wait_queue, waiting_task);

    // Switch context back to the scheduler to run another task.
    swapcontext(&waiting_task->context, &main_context);
}

void rtos_sem_post(rtos_semaphore_t* sem) {
    // Increment the semaphore value.
    sem->value++;

    // If there are tasks waiting, unblock one.
    if (sem->wait_queue != NULL) {
        TCB* unblocked_task = dequeue(&sem->wait_queue);
        unblocked_task->state = TASK_READY;
        enqueue(&ready_queue, unblocked_task);
    }
}