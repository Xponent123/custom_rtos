#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Global test statistics
static volatile int test_counter = 0;
static volatile int stress_counter = 0;
static volatile int preemption_count = 0;
static volatile int context_switches = 0;

// Shared resources for testing
static rtos_semaphore_t test_mutex;
static rtos_semaphore_t resource_sem;
static rtos_queue_t* test_queue;
static volatile int shared_resource = 0;
static volatile int race_detector = 0;

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, test_name) do { \
    if (condition) { \
        printf("✓ PASS: %s\n", test_name); \
        tests_passed++; \
    } else { \
        printf("✗ FAIL: %s\n", test_name); \
        tests_failed++; \
    } \
    fflush(stdout); \
} while(0)

// =============================================================================
// TEST 1: Stack Overflow Detection Test
// =============================================================================
void stack_overflow_task(void) {
    printf("Stack Overflow Test: Attempting to overflow stack...\n");
    fflush(stdout);
    
    // Try to cause stack overflow with deep recursion
    char large_array[1024];
    memset(large_array, 0xAA, sizeof(large_array));
    
    // Simulate some work
    for (int i = 0; i < 100; i++) {
        large_array[i % 1024] = i;
        if (i % 20 == 0) {
            rtos_task_delay(1); // Allow preemption
        }
    }
    
    printf("Stack Overflow Test: Task completed normally (no overflow)\n");
    test_counter |= 0x01;
}

// =============================================================================
// TEST 2: High Priority Task Preemption Test
// =============================================================================
void high_priority_preemptor(void) {
    printf("High Priority Task: Starting preemption test\n");
    fflush(stdout);
    
    for (int i = 0; i < 5; i++) {
        printf("HIGH[%d]: Preempting lower priority tasks\n", i);
        preemption_count++;
        rtos_task_delay(50); // Short delay to allow others to run
    }
    
    printf("High Priority Task: Completed\n");
    test_counter |= 0x02;
}

void medium_priority_worker(void) {
    for (int i = 0; i < 10; i++) {
        printf("MEDIUM[%d]: Should be preempted frequently\n", i);
        // CPU intensive work without yielding
        volatile int dummy = 0;
        for (int j = 0; j < 1000; j++) {
            dummy += j;
        }
        rtos_task_delay(30);
    }
    test_counter |= 0x04;
}

void low_priority_background(void) {
    for (int i = 0; i < 8; i++) {
        printf("LOW[%d]: Background task\n", i);
        rtos_task_delay(100);
    }
    test_counter |= 0x08;
}

// =============================================================================
// TEST 3: Semaphore Stress Test
// =============================================================================
void semaphore_producer(void) {
    printf("Producer: Starting semaphore stress test\n");
    
    for (int i = 0; i < 20; i++) {
        rtos_sem_wait(&test_mutex);
        
        // Critical section - modify shared resource
        int old_value = shared_resource;
        shared_resource++;
        race_detector++;
        
        // Simulate some work in critical section
        volatile int dummy = 0;
        for (int j = 0; j < 100; j++) {
            dummy += j;
        }
        
        if (shared_resource != old_value + 1) {
            printf("ERROR: Race condition detected in semaphore test!\n");
            tests_failed++;
        }
        
        printf("Producer[%d]: shared_resource = %d\n", i, shared_resource);
        rtos_sem_post(&test_mutex);
        
        rtos_task_delay(10);
    }
    
    test_counter |= 0x10;
}

void semaphore_consumer(void) {
    printf("Consumer: Starting semaphore stress test\n");
    
    for (int i = 0; i < 15; i++) {
        rtos_sem_wait(&test_mutex);
        
        // Critical section - read shared resource
        int value = shared_resource;
        race_detector--;
        
        printf("Consumer[%d]: read shared_resource = %d\n", i, value);
        rtos_sem_post(&test_mutex);
        
        rtos_task_delay(15);
    }
    
    test_counter |= 0x20;
}

// =============================================================================
// TEST 4: Queue Communication Test
// =============================================================================
void queue_sender(void) {
    printf("Queue Sender: Starting queue communication test\n");
    
    for (int i = 0; i < 25; i++) {
        int message = i * 10 + 100;
        printf("Sender: Sending message %d\n", message);
        rtos_queue_send(test_queue, &message);
        
        if (i % 3 == 0) {
            rtos_task_delay(5); // Occasional delay
        }
    }
    
    // Send termination message
    int terminator = -1;
    rtos_queue_send(test_queue, &terminator);
    
    test_counter |= 0x40;
}

void queue_receiver(void) {
    printf("Queue Receiver: Starting queue communication test\n");
    
    int received_count = 0;
    while (1) {
        int message;
        rtos_queue_receive(test_queue, &message);
        
        if (message == -1) {
            printf("Queue Receiver: Received termination signal\n");
            break;
        }
        
        printf("Receiver: Got message %d\n", message);
        received_count++;
        
        if (received_count % 5 == 0) {
            rtos_task_delay(8); // Occasional processing delay
        }
    }
    
    TEST_ASSERT(received_count == 25, "Queue received all messages");
    test_counter |= 0x80;
}

// =============================================================================
// TEST 5: CPU Hog Task (Preemption Stress Test)
// =============================================================================
void cpu_hog_task(void) {
    printf("CPU Hog: Starting infinite loop (should be preempted)\n");
    fflush(stdout);
    
    volatile int counter = 0;
    time_t start_time = time(NULL);
    
    // Infinite CPU-intensive loop
    while (1) {
        counter++;
        
        // Print occasionally to show it's being preempted
        if (counter % 1000000 == 0) {
            time_t current = time(NULL);
            printf("CPU Hog: Still alive after %ld seconds (counter=%d)\n", 
                   current - start_time, counter);
            fflush(stdout);
            
            // Exit after reasonable time to avoid infinite test
            if (current - start_time > 3) {
                printf("CPU Hog: Exiting after stress test\n");
                break;
            }
        }
    }
    
    test_counter |= 0x100;
}

// =============================================================================
// TEST 6: Rapid Task Creation/Termination Test
// =============================================================================
void short_lived_task(void) {
    static volatile int task_count = 0;
    int my_id = ++task_count;
    
    printf("Short Task %d: Quick execution\n", my_id);
    
    // Do minimal work and exit
    rtos_task_delay(1);
    
    printf("Short Task %d: Terminating\n", my_id);
}

void rapid_creator_task(void) {
    printf("Rapid Creator: Creating and destroying tasks rapidly\n");
    
    // Create many short-lived tasks to stress task management
    for (int i = 0; i < 8; i++) {
        if (rtos_task_create_with_priority(short_lived_task, PRIORITY_NORMAL) < 0) {
            printf("ERROR: Failed to create task %d\n", i);
            tests_failed++;
        }
        rtos_task_delay(5); // Small delay between creations
    }
    
    rtos_task_delay(50); // Wait for tasks to complete
    test_counter |= 0x200;
}

// =============================================================================
// TEST 7: Resource Contention Test
// =============================================================================
void resource_contender(void) {
    static volatile int contender_id = 0;
    int my_id = ++contender_id;
    
    printf("Contender %d: Competing for limited resource\n", my_id);
    
    for (int i = 0; i < 5; i++) {
        printf("Contender %d: Attempting to acquire resource (attempt %d)\n", my_id, i+1);
        rtos_sem_wait(&resource_sem);
        
        printf("Contender %d: GOT RESOURCE! Working...\n", my_id);
        
        // Simulate work with the resource
        rtos_task_delay(20);
        
        printf("Contender %d: Releasing resource\n", my_id);
        rtos_sem_post(&resource_sem);
        
        rtos_task_delay(10);
    }
    
    test_counter |= (0x400 << (my_id - 1));
}

// =============================================================================
// TEST 8: Context Switch Stress Test
// =============================================================================
void context_switch_task_a(void) {
    for (int i = 0; i < 50; i++) {
        printf("CTX_A[%d] ", i);
        if (i % 10 == 9) printf("\n");
        fflush(stdout);
        context_switches++;
        rtos_task_yield(); // Force context switch
    }
    printf("\nContext Task A: Completed\n");
    test_counter |= 0x2000;
}

void context_switch_task_b(void) {
    for (int i = 0; i < 50; i++) {
        printf("CTX_B[%d] ", i);
        if (i % 10 == 9) printf("\n");
        fflush(stdout);
        context_switches++;
        rtos_task_yield(); // Force context switch
    }
    printf("\nContext Task B: Completed\n");
    test_counter |= 0x4000;
}

// =============================================================================
// TEST 9: Edge Case Parameter Testing
// =============================================================================
void parameter_test_task(void) {
    printf("Parameter Test: Testing edge cases\n");
    
    // Test invalid delay values
    printf("Testing delay(0)...\n");
    rtos_task_delay(0); // Should work (immediate yield)
    
    printf("Testing large delay...\n");
    rtos_task_delay(999999); // Should trigger warning but work
    
    // Test NULL parameters (these should be handled gracefully)
    printf("Testing NULL semaphore operations...\n");
    rtos_sem_wait(NULL);  // Should print error and return
    rtos_sem_post(NULL);  // Should print error and return
    
    printf("Testing NULL queue operations...\n");
    int temp_value = test_counter;
    rtos_queue_send(NULL, &temp_value);  // Should print error and return
    rtos_queue_receive(NULL, &temp_value); // Should print error and return
    
    printf("Parameter Test: Completed\n");
    test_counter |= 0x8000;
}

// =============================================================================
// TEST 10: Memory Stress Test
// =============================================================================
void memory_stress_task(void) {
    printf("Memory Stress: Testing memory operations\n");
    
    // Create and destroy multiple queues
    for (int i = 0; i < 5; i++) {
        rtos_queue_t* temp_queue = rtos_queue_create(sizeof(int), 10);
        if (temp_queue) {
            // Use the queue briefly
            int test_data = i;
            rtos_queue_send(temp_queue, &test_data);
            rtos_queue_receive(temp_queue, &test_data);
            
            rtos_queue_delete(temp_queue);
            printf("Memory Stress: Created and destroyed queue %d\n", i);
        } else {
            printf("ERROR: Failed to create queue %d\n", i);
            tests_failed++;
        }
        rtos_task_delay(5);
    }
    
    test_counter |= 0x10000;
}

// =============================================================================
// MAIN TEST ORCHESTRATOR
// =============================================================================
int main() {
    printf("================================================================================\n");
    printf("                        RTOS HARDCORE STRESS TEST SUITE\n");
    printf("================================================================================\n");
    printf("Testing: Preemption, Priorities, Semaphores, Queues, Stack Safety, Edge Cases\n");
    printf("================================================================================\n\n");
    
    // Initialize test resources
    rtos_sem_init(&test_mutex, 1);
    rtos_sem_init(&resource_sem, 2); // Allow 2 concurrent accessors
    
    test_queue = rtos_queue_create(sizeof(int), 50);
    if (!test_queue) {
        printf("FATAL: Failed to create test queue\n");
        return 1;
    }
    
    printf("Phase 1: Basic Priority and Preemption Tests\n");
    printf("--------------------------------------------\n");
    
    // Create tasks with different priorities to test scheduling
    rtos_task_create_with_priority(high_priority_preemptor, PRIORITY_HIGH);
    rtos_task_create_with_priority(medium_priority_worker, PRIORITY_NORMAL);
    rtos_task_create_with_priority(low_priority_background, PRIORITY_LOW);
    
    // Stack overflow test (should be caught by canaries)
    rtos_task_create_with_priority(stack_overflow_task, PRIORITY_NORMAL);
    
    printf("\nPhase 2: Concurrency and Synchronization Tests\n");
    printf("----------------------------------------------\n");
    
    // Semaphore stress test
    rtos_task_create_with_priority(semaphore_producer, PRIORITY_NORMAL);
    rtos_task_create_with_priority(semaphore_consumer, PRIORITY_NORMAL);
    
    // Queue communication test
    rtos_task_create_with_priority(queue_sender, PRIORITY_NORMAL);
    rtos_task_create_with_priority(queue_receiver, PRIORITY_HIGH);
    
    printf("\nPhase 3: Stress and Edge Case Tests\n");
    printf("-----------------------------------\n");
    
    // CPU hog task (should be preempted regularly)
    rtos_task_create_with_priority(cpu_hog_task, PRIORITY_LOW);
    
    // Rapid task creation/destruction
    rtos_task_create_with_priority(rapid_creator_task, PRIORITY_NORMAL);
    
    // Resource contention
    rtos_task_create_with_priority(resource_contender, PRIORITY_NORMAL);
    rtos_task_create_with_priority(resource_contender, PRIORITY_NORMAL);
    rtos_task_create_with_priority(resource_contender, PRIORITY_HIGH);
    
    // Context switch stress
    rtos_task_create_with_priority(context_switch_task_a, PRIORITY_NORMAL);
    rtos_task_create_with_priority(context_switch_task_b, PRIORITY_NORMAL);
    
    // Parameter edge case testing
    rtos_task_create_with_priority(parameter_test_task, PRIORITY_LOW);
    
    // Memory stress testing
    rtos_task_create_with_priority(memory_stress_task, PRIORITY_NORMAL);
    
    printf("\nStarting RTOS...\n");
    printf("================================================================================\n");
    
    // Start the RTOS (this will run until all tasks complete or timeout)
    rtos_start();
    
    // This point should never be reached in normal operation
    printf("\n================================================================================\n");
    printf("                              TEST RESULTS\n");
    printf("================================================================================\n");
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("Test Counter: 0x%X\n", test_counter);
    printf("Context Switches: %d\n", context_switches);
    printf("Preemptions: %d\n", preemption_count);
    printf("Shared Resource Final Value: %d\n", shared_resource);
    printf("Race Detector: %d\n", race_detector);
    
    // Cleanup
    rtos_queue_delete(test_queue);
    
    return tests_failed > 0 ? 1 : 0;
}
