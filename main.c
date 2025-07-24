#include "rtos.h"
#include <stdio.h>

// A global pointer for our message queue
static rtos_queue_t* g_message_queue;

// The Producer Task: Creates messages and sends them to the queue.
void producer_task(void) {
    int message_to_send = 0;
    while (1) {
        printf("Producer: Sending message %d\n", message_to_send);
        fflush(stdout);

        // Send the message to the queue.
        // This will block if the queue is full.
        rtos_queue_send(g_message_queue, &message_to_send);

        message_to_send++;

        // Wait before producing the next message
        rtos_task_delay(500);
    }
}

// The Consumer Task: Waits for messages and processes them.
void consumer_task(void) {
    int received_message;
    while (1) {
        printf("Consumer: Waiting for a message...\n");
        fflush(stdout);

        // Receive a message from the queue.
        // This will block if the queue is empty.
        rtos_queue_receive(g_message_queue, &received_message);

        printf("Consumer: Received message %d\n", received_message);
        fflush(stdout);
    }
}

int main() {
    printf("--- Running Message Queue Test ---\n");

    // Create a queue that can hold 5 messages of type int.
    g_message_queue = rtos_queue_create(sizeof(int), 5);

    if (g_message_queue == NULL) {
        fprintf(stderr, "Failed to create message queue.\n");
        return -1;
    }

    // Create the producer and consumer tasks
    rtos_task_create(producer_task);
    rtos_task_create(consumer_task);

    // Start the RTOS
    rtos_start();

    // The program should not reach here.
    // In a real application, you might delete the queue on exit.
    rtos_queue_delete(g_message_queue);

    return 0;
}