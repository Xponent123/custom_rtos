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