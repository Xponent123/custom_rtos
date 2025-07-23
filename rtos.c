#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

static TCB tasks[MAX_TASKS];
static int num_tasks = 0;

static TCB *ready_queue = NULL;
static TCB *sleep_queue = NULL;
static TCB *current_task = NULL;

// Global tick counter
uint32_t system_tick = 0;

// --- Queue Helpers ---
static void enqueue(TCB **queue, TCB *tcb)
{
    tcb->next = NULL;
    if (*queue == NULL)
    {
        *queue = tcb;
    }
    else
    {
        TCB *cur = *queue;
        while (cur->next != NULL)
            cur = cur->next;
        cur->next = tcb;
    }
}

static TCB *dequeue(TCB **queue)
{
    if (*queue == NULL)
        return NULL;
    TCB *tcb = *queue;
    *queue = tcb->next;
    tcb->next = NULL;
    return tcb;
}

// --- Scheduler ---
static void schedule(void)
{
    while (1)
    {
        system_tick++;
        
        // Wake sleeping tasks
        TCB *curr = sleep_queue;
        TCB *prev = NULL;
        while (curr != NULL)
        {
            if (system_tick >= curr->wake_time)
            {
                TCB *next = curr->next;
                if (prev == NULL){
                    sleep_queue = next;
                }
                else{
                    prev->next = next;
                }

                curr->state = TASK_READY;
                enqueue(&ready_queue, curr);
                curr = next;
            }
            else
            {
                prev = curr;
                curr = curr->next;
            }
        }

        // Dispatch ready task
        TCB *next_task = dequeue(&ready_queue);
        if (next_task)
        {
            current_task = next_task;
            current_task->state = TASK_RUNNING;
            
            // Execute one iteration of the task
            current_task->task_fn();
            
            // If task didn't yield or delay, it's probably finished
            if (current_task && current_task->state == TASK_RUNNING) {
                printf("Task %d finished.\n", current_task->id);
                current_task->state = TASK_FINISHED;
                current_task = NULL;
            }
        }
        else
        {
            // No ready tasks - check if we have sleeping tasks
            if (sleep_queue == NULL)
            {
                printf("All tasks completed. Exiting...\n");
                break;
            }
            // Continue to next tick to check sleeping tasks
        }
    }
}

// --- Public API ---

void rtos_task_create(void (*task_fn)(void))
{
    if (num_tasks >= MAX_TASKS){
        printf("No more tasks can be added.\n");
        return;
    }

    TCB *tcb = &tasks[num_tasks];
    tcb->id = num_tasks;
    tcb->state = TASK_READY;
    tcb->wake_time = 0;
    tcb->task_fn = task_fn;
    tcb->next = NULL;

    enqueue(&ready_queue, tcb);
    printf("Task %d created.\n", tcb->id);
    num_tasks++;
}

void rtos_start(void)
{
    printf("Starting RTOS...\n");
    schedule();
}

void rtos_task_yield(void)
{
    if (current_task)
    {
        current_task->state = TASK_READY;
        enqueue(&ready_queue, current_task);
        current_task = NULL;
    }
}

void rtos_task_delay(uint32_t ticks)
{
    if (current_task)
    {
        if (ticks == 0)
        {
            rtos_task_yield();
            return;
        }

        current_task->wake_time = system_tick + ticks;
        current_task->state = TASK_SLEEPING;
        enqueue(&sleep_queue, current_task);
        current_task = NULL;
    }
}
