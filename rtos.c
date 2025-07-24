#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE  // For usleep on some systems
#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

// --- Global Variables ---
static TCB tasks[MAX_TASKS];
static TCB *current_task = NULL;
static int num_tasks = 0;
static ucontext_t scheduler_context;
static sigset_t signal_mask;
static volatile int in_critical_section = 0;
static volatile int yield_requested = 0;

// Queues
static TCB *ready_queue = NULL;
static TCB *sleep_queue = NULL;

// --- Critical Section Helpers ---
static void enter_critical_section()
{
    in_critical_section = 1;
    sigprocmask(SIG_BLOCK, &signal_mask, NULL);
}

static void exit_critical_section()
{
    in_critical_section = 0;
    sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
}

// --- Task Cleanup Functions ---
static void cleanup_terminated_task(TCB *tcb)
{
    // Clear sensitive data
    memset(&tcb->context, 0, sizeof(ucontext_t));
    
    // Clear stack (security measure)
    memset(tcb->stack, 0xDE, STACK_SIZE); // 0xDE = "dead" pattern
    
    // Reset TCB fields
    tcb->state = TASK_TERMINATED;
    tcb->delay_ticks = 0;
    tcb->next = NULL;
    tcb->stack_canary = 0; // Invalidate canary
    
    printf("Task %d resources cleaned up\n", tcb->id);
}

// --- Stack Canary Functions ---
static void setup_stack_canary(TCB *tcb)
{
    tcb->stack_canary = STACK_CANARY;
    // Place canary at the beginning of stack (lowest address)
    uint32_t *stack_guard = (uint32_t *)tcb->stack;
    *stack_guard = STACK_CANARY;
}

static int check_stack_canary(TCB *tcb)
{
    if (tcb->stack_canary != STACK_CANARY) {
        return 0; // TCB canary corrupted
    }
    
    // Check stack guard canary
    uint32_t *stack_guard = (uint32_t *)tcb->stack;
    if (*stack_guard != STACK_CANARY) {
        return 0; // Stack canary corrupted
    }
    
    return 1; // OK
}

// --- Helper Functions (enqueue, dequeue) ---
static void enqueue(TCB **queue, TCB *tcb)
{
    tcb->next = NULL;
    if (*queue == NULL)
    {
        *queue = tcb;
    }
    else
    {
        TCB *current = *queue;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = tcb;
    }
}

// Priority-based enqueue for ready queue (higher priority first)
static void enqueue_priority(TCB **queue, TCB *tcb)
{
    tcb->next = NULL;
    
    // If queue is empty or tcb has higher priority than head
    if (*queue == NULL || tcb->priority < (*queue)->priority)
    {
        tcb->next = *queue;
        *queue = tcb;
        return;
    }
    
    // Find the correct position to insert
    TCB *current = *queue;
    while (current->next != NULL && current->next->priority <= tcb->priority)
    {
        current = current->next;
    }
    
    tcb->next = current->next;
    current->next = tcb;
}

static TCB *dequeue(TCB **queue)
{
    if (*queue == NULL)
    {
        return NULL;
    }
    TCB *tcb = *queue;
    *queue = (*queue)->next;
    tcb->next = NULL;
    return tcb;
}
// --- Signal Handler for Preemption ---
static void timer_handler(int signum)
{
    // SAFE: Only set flag - no complex operations in signal handler
    // This is async-signal-safe and prevents race conditions
    if (current_task != NULL && !in_critical_section) {
        yield_requested = 1;
    }
}

// --- Public API ---

void rtos_start(void)
{
    // --- Timer and Signal Setup for Preemption ---
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &timer_handler;
    // Use SA_RESTART to automatically restart interrupted system calls
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGALRM);

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000; // 10ms time slice for more responsive preemption
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);
    // --- End of Setup ---

    printf("Starting RTOS with preemptive scheduling...\n");
    fflush(stdout);

    // Main scheduler loop
    while (1)
    {
        enter_critical_section();
        // Process sleep queue
        if (sleep_queue != NULL)
        {
            TCB *current = sleep_queue;
            TCB *prev = NULL;
            while (current != NULL)
            {
                current->delay_ticks--;
                if (current->delay_ticks == 0)
                {
                    TCB *ready_task = current;
                    TCB *next = current->next;
                    if (prev == NULL)
                    {
                        sleep_queue = next;
                    }
                    else
                    {
                        prev->next = next;
                    }
                    ready_task->state = TASK_READY;
                    enqueue_priority(&ready_queue, ready_task); // Use priority queue
                    current = next;
                }
                else
                {
                    prev = current;
                    current = current->next;
                }
            }
        }
        exit_critical_section();

        // If a task is ready, run it
        if (ready_queue != NULL)
        {
            enter_critical_section();
            current_task = dequeue(&ready_queue);
            current_task->state = TASK_RUNNING;
            yield_requested = 0; // Reset yield flag
            exit_critical_section();
            
            // Run the task
            swapcontext(&scheduler_context, &current_task->context);
            
            enter_critical_section();
            // Check if task terminated or was preempted
            if (current_task->state == TASK_RUNNING) {
                // Check for stack overflow FIRST
                if (!check_stack_canary(current_task)) {
                    printf("FATAL: Stack overflow detected in Task %d!\n", current_task->id);
                    fflush(stdout);
                    current_task->state = TASK_TERMINATED;
                    exit_critical_section();
                    exit(1); // Fatal error - terminate program
                }
                
                if (yield_requested) {
                    // Task was preempted by timer, put it back in ready queue
                    current_task->state = TASK_READY;
                    enqueue_priority(&ready_queue, current_task); // Use priority queue
                    yield_requested = 0;
                    printf("Task %d preempted by timer\n", current_task->id);
                } else {
                    // Task terminated normally - clean up resources
                    cleanup_terminated_task(current_task);
                    printf("Task %d has terminated.\n", current_task->id);
                    fflush(stdout);
                }
            }
            // Note: If state is not TASK_RUNNING, task called yield or delay voluntarily
            exit_critical_section();
        }
        else
        {
            // No tasks ready, small sleep to prevent busy waiting
            usleep(1000); // 1ms
        }
    }
}

int rtos_task_create_with_priority(void (*task_function)(void), uint32_t priority)
{
    // Parameter validation
    if (task_function == NULL) {
        fprintf(stderr, "Error: task_function cannot be NULL\n");
        return -1;
    }
    
    if (priority > PRIORITY_IDLE) {
        fprintf(stderr, "Error: Invalid priority %u (max is %d)\n", priority, PRIORITY_IDLE);
        return -1;
    }
    
    enter_critical_section();
    if (num_tasks >= MAX_TASKS)
    {
        fprintf(stderr, "Error: Max tasks reached (%d/%d)\n", num_tasks, MAX_TASKS);
        exit_critical_section();
        return -1;
    }

    TCB *new_tcb = &tasks[num_tasks];
    
    // Initialize and setup stack canaries FIRST
    setup_stack_canary(new_tcb);
    
    if (getcontext(&new_tcb->context) == -1)
    {
        perror("getcontext failed");
        exit_critical_section();
        return -1;
    }

    // Properly align stack to 16-byte boundary
    // Reserve space for canary at start of stack
    uintptr_t stack_start = (uintptr_t)new_tcb->stack + sizeof(uint32_t);
    uintptr_t aligned_stack_start = (stack_start + 15) & ~15UL;
    size_t alignment_offset = aligned_stack_start - (uintptr_t)new_tcb->stack;
    size_t aligned_stack_size = STACK_SIZE - alignment_offset;
    
    // Ensure minimum stack size after alignment
    if (aligned_stack_size < 4096) {
        fprintf(stderr, "Error: Stack too small after alignment for task %d\n", num_tasks);
        exit_critical_section();
        return -1;
    }

    new_tcb->context.uc_stack.ss_sp = (void*)aligned_stack_start;
    new_tcb->context.uc_stack.ss_size = aligned_stack_size;
    new_tcb->context.uc_link = &scheduler_context;

    makecontext(&new_tcb->context, (void (*)(void))task_function, 0);

    new_tcb->id = num_tasks++;
    new_tcb->priority = priority;
    new_tcb->state = TASK_READY;
    new_tcb->delay_ticks = 0;
    enqueue_priority(&ready_queue, new_tcb); // Use priority queue

    printf("Task %d created with priority %u\n", new_tcb->id, priority);
    fflush(stdout);

    exit_critical_section();
    return new_tcb->id; // Return task ID instead of 0
}

int rtos_task_create(void (*task_function)(void))
{
    // Default to normal priority
    return rtos_task_create_with_priority(task_function, PRIORITY_NORMAL);
}

void rtos_task_yield(void)
{
    if (current_task == NULL) {
        fprintf(stderr, "Error: No current task to yield\n");
        return; // Safety check
    }
    
    enter_critical_section();
    TCB *yielding_task = current_task;
    yielding_task->state = TASK_READY;
    enqueue_priority(&ready_queue, yielding_task); // Use priority queue
    
    // Context switch with signals blocked
    swapcontext(&yielding_task->context, &scheduler_context);
    exit_critical_section();
}

void rtos_task_delay(uint32_t ticks)
{
    if (current_task == NULL) {
        fprintf(stderr, "Error: No current task to delay\n");
        return; // Safety check
    }
    
    // Validate ticks parameter
    if (ticks > 100000) { // Reasonable upper limit
        fprintf(stderr, "Warning: Very large delay requested (%u ticks)\n", ticks);
    }
    
    enter_critical_section();
    if (ticks == 0)
    {
        exit_critical_section();
        rtos_task_yield();
        return;
    }
    TCB *delaying_task = current_task;
    delaying_task->delay_ticks = ticks;
    delaying_task->state = TASK_SLEEPING;
    enqueue(&sleep_queue, delaying_task);
    
    // Context switch with signals blocked
    swapcontext(&delaying_task->context, &scheduler_context);
    exit_critical_section();
}

void rtos_sem_init(rtos_semaphore_t *sem, int initial_value)
{
    // Parameter validation
    if (sem == NULL) {
        fprintf(stderr, "Error: Semaphore pointer cannot be NULL\n");
        return;
    }
    
    if (initial_value < 0) {
        fprintf(stderr, "Error: Invalid semaphore initial value %d\n", initial_value);
        return;
    }
    
    enter_critical_section();
    sem->value = initial_value;
    sem->wait_queue = NULL;
    exit_critical_section();
}

void rtos_sem_wait(rtos_semaphore_t *sem)
{
    // Parameter validation
    if (sem == NULL) {
        fprintf(stderr, "Error: Semaphore pointer cannot be NULL\n");
        return;
    }
    
    if (current_task == NULL) {
        fprintf(stderr, "Error: No current task for semaphore wait\n");
        return;
    }
    
    enter_critical_section();
    if (sem->value > 0)
    {
        sem->value--;
        exit_critical_section();
        return;
    }
    
    // Need to block - add to wait queue and context switch
    TCB *waiting_task = current_task;
    waiting_task->state = TASK_BLOCKED;
    enqueue(&sem->wait_queue, waiting_task);
    
    // Context switch while in critical section (signals blocked)
    swapcontext(&waiting_task->context, &scheduler_context);
    exit_critical_section();
}

void rtos_sem_post(rtos_semaphore_t *sem)
{
    // Parameter validation
    if (sem == NULL) {
        fprintf(stderr, "Error: Semaphore pointer cannot be NULL\n");
        return;
    }
    
    enter_critical_section();
    if (sem->wait_queue != NULL)
    {
        TCB *unblocked_task = dequeue(&sem->wait_queue);
        unblocked_task->state = TASK_READY;
        enqueue_priority(&ready_queue, unblocked_task); // Use priority queue
    }
    else
    {
        sem->value++;
        // Prevent semaphore value overflow
        if (sem->value > 1000) {
            fprintf(stderr, "Warning: Semaphore value very high (%d)\n", sem->value);
        }
    }
    exit_critical_section();
}

rtos_queue_t *rtos_queue_create(int msg_size, int capacity)
{
    // Parameter validation
    if (msg_size <= 0 || msg_size > 65536) {
        fprintf(stderr, "Error: Invalid message size %d (must be 1-65536)\n", msg_size);
        return NULL;
    }
    
    if (capacity <= 0 || capacity > 1000) {
        fprintf(stderr, "Error: Invalid capacity %d (must be 1-1000)\n", capacity);
        return NULL;
    }
    
    // Check for potential overflow
    size_t total_size = (size_t)msg_size * capacity;
    if (total_size > SIZE_MAX / 2) {
        fprintf(stderr, "Error: Queue size too large (%zu bytes)\n", total_size);
        return NULL;
    }
    
    // Allocate memory OUTSIDE critical section to avoid blocking
    rtos_queue_t *queue = malloc(sizeof(rtos_queue_t));
    if (queue == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate queue structure\n");
        return NULL;
    }

    queue->buffer = malloc(total_size);
    if (queue->buffer == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate queue buffer (%zu bytes)\n", total_size);
        free(queue);
        return NULL;
    }

    // Now enter critical section for initialization
    enter_critical_section();
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

void rtos_queue_delete(rtos_queue_t *queue)
{
    enter_critical_section();
    if (queue == NULL)
    {
        exit_critical_section();
        return;
    }
    free(queue->buffer);
    free(queue);
    exit_critical_section();
}

void rtos_queue_send(rtos_queue_t *queue, const void *msg)
{
    // Parameter validation
    if (queue == NULL) {
        fprintf(stderr, "Error: Queue pointer cannot be NULL\n");
        return;
    }
    
    if (msg == NULL) {
        fprintf(stderr, "Error: Message pointer cannot be NULL\n");
        return;
    }
    
    rtos_sem_wait(&queue->sem_full);
    rtos_sem_wait(&queue->mutex);

    void *target_addr = (uint8_t *)queue->buffer + (queue->tail * queue->msg_size);
    memcpy(target_addr, msg, queue->msg_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    rtos_sem_post(&queue->mutex);
    rtos_sem_post(&queue->sem_empty);
}

void rtos_queue_receive(rtos_queue_t *queue, void *msg)
{
    // Parameter validation
    if (queue == NULL) {
        fprintf(stderr, "Error: Queue pointer cannot be NULL\n");
        return;
    }
    
    if (msg == NULL) {
        fprintf(stderr, "Error: Message buffer cannot be NULL\n");
        return;
    }
    
    rtos_sem_wait(&queue->sem_empty);
    rtos_sem_wait(&queue->mutex);

    void *source_addr = (uint8_t *)queue->buffer + (queue->head * queue->msg_size);
    memcpy(msg, source_addr, queue->msg_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    rtos_sem_post(&queue->mutex);
    rtos_sem_post(&queue->sem_full);
}

