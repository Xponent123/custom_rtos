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


