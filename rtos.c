#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

// --- Global Variables ---
static TCB tasks[MAX_TASKS];
static TCB* current_task = NULL;
static int num_tasks = 0;
static ucontext_t scheduler_context;
static sigset_t signal_mask;

// Queues
static TCB* ready_queue = NULL;
static TCB* sleep_queue = NULL;

// --- Critical Section Helpers ---
static void enter_critical_section() {
    sigprocmask(SIG_BLOCK, &signal_mask, NULL);
}

static void exit_critical_section() {
    sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
}

// --- Helper Functions (enqueue, dequeue) ---
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
// --- Signal Handler for Preemption ---
static void timer_handler(int signum) {
    rtos_task_yield();
}

// --- Public API ---

void rtos_start(void) {
    // --- Timer and Signal Setup for Preemption ---
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &timer_handler;
    sigaction(SIGALRM, &sa, NULL);

    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGALRM);

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000; // 10ms time slice
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);
    // --- End of Setup ---

    printf("Starting Preemptive RTOS...\n");
    fflush(stdout);

    // Main scheduler loop
    while(1) {
        enter_critical_section();
        // Process sleep queue
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
        exit_critical_section();

        // If a task is ready, run it
        if (ready_queue != NULL) {
            enter_critical_section();
            current_task = dequeue(&ready_queue);
            current_task->state = TASK_RUNNING;
            swapcontext(&scheduler_context, &current_task->context);
            exit_critical_section();
        }
    }
}

int rtos_task_create(void (*task_function)(void)) {
    enter_critical_section();
    if (num_tasks >= MAX_TASKS) {
        fprintf(stderr, "Error: Max tasks reached.\n");
        exit_critical_section();
        return -1;
    }

    TCB* new_tcb = &tasks[num_tasks];
    if (getcontext(&new_tcb->context) == -1) {
        perror("getcontext failed");
        exit_critical_section();
        return -1;
    }

    new_tcb->context.uc_stack.ss_sp = new_tcb->stack;
    new_tcb->context.uc_stack.ss_size = STACK_SIZE;
    new_tcb->context.uc_link = &scheduler_context;

    makecontext(&new_tcb->context, (void (*)(void))task_function, 0);

    new_tcb->id = num_tasks++;
    new_tcb->state = TASK_READY;
    new_tcb->delay_ticks = 0;
    enqueue(&ready_queue, new_tcb);

    printf("Task %d created.\n", new_tcb->id);
    fflush(stdout);
    
    exit_critical_section();
    return 0;
}

void rtos_task_yield(void) {
    enter_critical_section();
    TCB* yielding_task = current_task;
    yielding_task->state = TASK_READY;
    enqueue(&ready_queue, yielding_task);
    swapcontext(&yielding_task->context, &scheduler_context);
    exit_critical_section();
}

void rtos_task_delay(uint32_t ticks) {
    enter_critical_section();
    if (ticks == 0) {
        exit_critical_section();
        rtos_task_yield();
        return;
    }
    TCB* delaying_task = current_task;
    delaying_task->delay_ticks = ticks;
    delaying_task->state = TASK_SLEEPING;
    enqueue(&sleep_queue, delaying_task);
    swapcontext(&delaying_task->context, &scheduler_context);
    exit_critical_section();
}

void rtos_sem_init(rtos_semaphore_t* sem, int initial_value) {
    sem->value = initial_value;
    sem->wait_queue = NULL;
}

void rtos_sem_wait(rtos_semaphore_t* sem) {
    enter_critical_section();
    if (sem->value > 0) {
        sem->value--;
        exit_critical_section();
        return;
    }
    TCB* waiting_task = current_task;
    waiting_task->state = TASK_BLOCKED;
    enqueue(&sem->wait_queue, waiting_task);
    swapcontext(&waiting_task->context, &scheduler_context);
    exit_critical_section();
}

void rtos_sem_post(rtos_semaphore_t* sem) {
    enter_critical_section();
    if (sem->wait_queue != NULL) {
        TCB* unblocked_task = dequeue(&sem->wait_queue);
        unblocked_task->state = TASK_READY;
        enqueue(&ready_queue, unblocked_task);
    } else {
        sem->value++;
    }
    exit_critical_section();
}

rtos_queue_t* rtos_queue_create(int msg_size, int capacity) {
    // Since malloc can fail, it's not ideal inside a critical section,
    // but for this project it's acceptable.
    enter_critical_section();
    rtos_queue_t* queue = malloc(sizeof(rtos_queue_t));
    if (queue == NULL) { exit_critical_section(); return NULL; }

    queue->buffer = malloc(capacity * msg_size);
    if (queue->buffer == NULL) {
        free(queue);
        exit_critical_section();
        return NULL;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->capacity = capacity;
    queue->msg_size = msg_size;

    rtos_sem_init(&queue->mutex, 1);
    rtos_sem_init(&queue->sem_full, capacity);
    rtos_sem_init(&queue->sem_empty, 0);
    
    exit_critical_section();
    return queue;
}

void rtos_queue_delete(rtos_queue_t* queue) {
    enter_critical_section();
    if (queue == NULL) { exit_critical_section(); return; }
    free(queue->buffer);
    free(queue);
    exit_critical_section();
}

void rtos_queue_send(rtos_queue_t* queue, const void* msg) {
    rtos_sem_wait(&queue->sem_full);
    rtos_sem_wait(&queue->mutex);

    void* target_addr = (uint8_t*)queue->buffer + (queue->tail * queue->msg_size);
    memcpy(target_addr, msg, queue->msg_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    rtos_sem_post(&queue->mutex);
    rtos_sem_post(&queue->sem_empty);
}

void rtos_queue_receive(rtos_queue_t* queue, void* msg) {
    rtos_sem_wait(&queue->sem_empty);
    rtos_sem_wait(&queue->mutex);

    void* source_addr = (uint8_t*)queue->buffer + (queue->head * queue->msg_size);
    memcpy(msg, source_addr, queue->msg_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    rtos_sem_post(&queue->mutex);
    rtos_sem_post(&queue->sem_full);
}