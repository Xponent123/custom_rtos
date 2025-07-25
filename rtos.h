#ifndef RTOS_H
#define RTOS_H

#define _XOPEN_SOURCE // Required for ucontext.h

#include <stdint.h>
#include <ucontext.h> // The standard context switching library

// --- Constants ---
#define MAX_TASKS 10
#define STACK_SIZE 32768 // 32KB stack
#define STACK_CANARY 0xDEADBEEF // Stack overflow detection magic number

// Task priorities (lower number = higher priority)
#define PRIORITY_HIGH 0
#define PRIORITY_NORMAL 1
#define PRIORITY_LOW 2
#define PRIORITY_IDLE 3

// --- Type Definitions ---
typedef enum
{
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_TERMINATED,
    TASK_BLOCKED
} TaskState;

typedef struct TCB
{
    uint32_t id;
    uint32_t priority;         // Task priority (lower = higher priority)
    TaskState state;
    ucontext_t context;        // The correct struct for holding a context
    uint8_t stack[STACK_SIZE]; // Stack memory for the task; used by ucontext to store the task's execution stack
    uint32_t stack_canary;     // Stack overflow detection
    uint32_t delay_ticks;
    struct TCB *next;
} TCB;

typedef struct semaphore{
    int value;
    TCB* wait_queue;
}rtos_semaphore_t; 
typedef struct qu{
    void* buffer;
    int head;
    int tail;
    int count;
    int capacity;
    int msg_size;
    rtos_semaphore_t mutex;
    rtos_semaphore_t sem_full;
    rtos_semaphore_t sem_empty;
} rtos_queue_t;
// --- Public API ---
// extern TCB* current_task;
int rtos_task_create(void (*task_function)(void));
int rtos_task_create_with_priority(void (*task_function)(void), uint32_t priority);
void rtos_task_yield(void);
void rtos_task_delay(uint32_t ticks);
void rtos_start(void);


void rtos_sem_init(rtos_semaphore_t* sem, int initial_value); 
void rtos_sem_wait(rtos_semaphore_t* sem);                     
void rtos_sem_post(rtos_semaphore_t* sem);  

rtos_queue_t* rtos_queue_create(int msg_size, int capacity);
void rtos_queue_send(rtos_queue_t* queue, const void* msg);
void rtos_queue_receive(rtos_queue_t* queue, void* msg);
void rtos_queue_delete(rtos_queue_t* queue);
#endif // RTOS_H