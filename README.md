# Custom Real-Time Operating System (RTOS)

A lightweight, preemptive real-time operating system implemented in C using POSIX ucontext for context switching. This RTOS provides essential multitasking capabilities with priority-based scheduling, synchronization primitives, and inter-task communication.

## 🚀 Features

### Core RTOS Features
- **Preemptive Multitasking**: Tasks are automatically preempted based on priority and time slicing (10ms quantum)
- **Priority-based Scheduling**: 4 priority levels (HIGH, NORMAL, LOW, IDLE) with priority queues
- **Context Switching**: Efficient context switching using POSIX ucontext library
- **Task Management**: Create, terminate, delay, and yield tasks dynamically
- **Stack Safety**: Stack canary protection with overflow detection (0xDEADBEEF magic number)

### Synchronization Primitives
- **Semaphores**: Binary and counting semaphores for resource synchronization
- **Message Queues**: FIFO message passing between tasks with blocking send/receive
- **Critical Sections**: Signal masking for atomic operations

### Safety & Reliability
- **Signal-Safe Context Switching**: Async-signal-safe timer handlers
- **Parameter Validation**: Comprehensive input validation across all APIs
- **Resource Cleanup**: Automatic cleanup of terminated tasks
- **Error Handling**: Graceful handling of edge cases and resource limits

## 📋 System Requirements

- **Operating System**: Linux (uses POSIX signals and ucontext)
- **Compiler**: GCC with C99 support
- **Dependencies**: Standard C library, POSIX.1-2001

## 🛠️ Building and Running

### Compilation
```bash
# Compile a basic RTOS application
gcc your_app.c rtos.c -o your_app

# Compile with warnings enabled (recommended)
gcc -Wall -Wextra your_app.c rtos.c -o your_app
```

### Quick Start Example
```c
#include "rtos.h"
#include <stdio.h>

void task1(void) {
    for (int i = 0; i < 5; i++) {
        printf("Task 1: %d\n", i);
        rtos_task_delay(100);
    }
}

void task2(void) {
    for (int i = 0; i < 3; i++) {
        printf("Task 2: %d\n", i);
        rtos_task_delay(150);
    }
}

int main() {
    rtos_task_create_with_priority(task1, PRIORITY_NORMAL);
    rtos_task_create_with_priority(task2, PRIORITY_HIGH);
    
    rtos_start(); // Start the RTOS scheduler
    return 0;
}
```

## 📚 API Reference

### Task Management

#### `int rtos_task_create(void (*task_function)(void))`
Creates a new task with default priority (PRIORITY_NORMAL).
- **Returns**: Task ID on success, -1 on failure

#### `int rtos_task_create_with_priority(void (*task_function)(void), uint32_t priority)`
Creates a new task with specified priority.
- **Parameters**: 
  - `task_function`: Function pointer to task entry point
  - `priority`: Task priority (PRIORITY_HIGH, PRIORITY_NORMAL, PRIORITY_LOW, PRIORITY_IDLE)
- **Returns**: Task ID on success, -1 on failure

#### `void rtos_task_yield(void)`
Voluntarily yields CPU to other tasks.

#### `void rtos_task_delay(uint32_t ticks)`
Puts current task to sleep for specified ticks.
- **Parameters**: `ticks`: Number of ticks to sleep

#### `void rtos_start(void)`
Starts the RTOS scheduler. This function does not return under normal operation.

### Semaphores

#### `void rtos_sem_init(rtos_semaphore_t* sem, int initial_value)`
Initializes a semaphore.
- **Parameters**:
  - `sem`: Pointer to semaphore structure
  - `initial_value`: Initial semaphore count

#### `void rtos_sem_wait(rtos_semaphore_t* sem)`
Waits on (decrements) a semaphore. Blocks if semaphore count is 0.

#### `void rtos_sem_post(rtos_semaphore_t* sem)`
Posts to (increments) a semaphore. Wakes up waiting tasks.

### Message Queues

#### `rtos_queue_t* rtos_queue_create(int msg_size, int capacity)`
Creates a new message queue.
- **Parameters**:
  - `msg_size`: Size of each message in bytes
  - `capacity`: Maximum number of messages in queue
- **Returns**: Queue pointer on success, NULL on failure

#### `void rtos_queue_send(rtos_queue_t* queue, const void* msg)`
Sends a message to the queue. Blocks if queue is full.

#### `void rtos_queue_receive(rtos_queue_t* queue, void* msg)`
Receives a message from the queue. Blocks if queue is empty.

#### `void rtos_queue_delete(rtos_queue_t* queue)`
Deletes a message queue and frees associated memory.

## 🏗️ Architecture

### Task Control Block (TCB)
Each task is represented by a Task Control Block containing:
- Task ID and priority
- Execution state (READY, RUNNING, SLEEPING, TERMINATED, BLOCKED)
- ucontext for context switching
- 32KB stack with canary protection
- Delay counter for sleeping tasks

### Scheduler
- **Algorithm**: Priority-based round-robin within each priority level
- **Preemption**: 10ms time slices using SIGALRM
- **Priority Queues**: Separate queues for each priority level
- **Context Switching**: Signal-safe implementation using ucontext

### Memory Management
- **Static Allocation**: Fixed-size task table (10 tasks maximum)
- **Dynamic Allocation**: Message queues use malloc/free
- **Stack Protection**: Canary values detect stack overflow

## 🧪 Testing

The project includes a comprehensive stress test suite (`stress_test.c`) that validates:

- **Priority Scheduling**: High priority tasks preempt lower priority ones
- **Semaphore Synchronization**: Producer-consumer patterns with race condition detection
- **Message Queues**: Inter-task communication under stress
- **Stack Safety**: Stack overflow detection and prevention
- **Context Switching**: Rapid context switches between tasks
- **Resource Management**: Task creation/destruction and resource cleanup
- **Edge Cases**: Parameter validation and error handling

### Running Tests
```bash
# Compile and run the stress test
gcc stress_test.c rtos.c -o stress_test
./stress_test

# Run with timeout to prevent infinite execution
timeout 30 ./stress_test
```

## 📊 Performance Characteristics

- **Context Switch Time**: ~1-5µs (depending on hardware)
- **Maximum Tasks**: 10 concurrent tasks
- **Stack Size**: 32KB per task
- **Time Slice**: 10ms quantum
- **Memory Footprint**: ~330KB for 10 tasks + queue overhead

## ⚠️ Limitations

- **Platform Dependency**: Linux-only (uses POSIX signals and ucontext)
- **Task Limit**: Maximum 10 concurrent tasks
- **No Dynamic Priority**: Task priorities are fixed at creation
- **No SMP Support**: Single-core scheduling only
- **Signal Constraints**: Uses SIGALRM for timing

## 🔧 Configuration

Key constants can be modified in `rtos.h`:

```c
#define MAX_TASKS 10          // Maximum number of tasks
#define STACK_SIZE 32768      // Stack size per task (32KB)
#define STACK_CANARY 0xDEADBEEF // Stack overflow detection

// Priority levels (lower number = higher priority)
#define PRIORITY_HIGH 0
#define PRIORITY_NORMAL 1
#define PRIORITY_LOW 2
#define PRIORITY_IDLE 3
```

Timer quantum can be adjusted in `rtos.c`:
```c
timer.it_interval.tv_usec = 10000; // 10ms time slice
```

## 🐛 Debugging Tips

1. **Stack Overflow**: Check for "Stack canary corrupted" messages
2. **Deadlocks**: Use semaphore debugging prints
3. **Resource Leaks**: Monitor task creation/termination
4. **Priority Inversion**: Verify task priorities are set correctly
5. **Signal Issues**: Ensure no other signal handlers conflict with SIGALRM

## 📈 Future Enhancements

- [ ] Dynamic priority adjustment
- [ ] SMP (multi-core) support
- [ ] Real-time clock integration
- [ ] Power management hooks
- [ ] Memory protection units
- [ ] File system integration
- [ ] Network stack
- [ ] Device driver framework

## 📄 License

This project is open source. Feel free to use, modify, and distribute according to your needs.

## 🤝 Contributing

Contributions are welcome! Please ensure:
- Code follows existing style conventions
- All tests pass
- New features include appropriate tests
- Documentation is updated

## 📞 Support

For questions, issues, or contributions, please refer to the project documentation or create an issue in the repository.

---

**Note**: This RTOS is designed for educational and embedded system purposes. For production systems, consider additional safety features and extensive testing.
