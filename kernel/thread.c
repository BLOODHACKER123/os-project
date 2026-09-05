#include "thread.h"

static thread_t thread_table[MAX_THREADS];

static thread_t *thread_list = 0;
static thread_t *current_thread = 0;

static uint32_t next_tid = 1;


void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].tid = 0;
        thread_table[i].state = THREAD_TERMINATED;

        thread_table[i].esp = 0;
        thread_table[i].eip = 0;

        thread_table[i].entry = 0;
        thread_table[i].name = 0;

        thread_table[i].owner_pid = 0;
        thread_table[i].next = 0;
    }

    thread_list = 0;
    current_thread = 0;
    next_tid = 1;
}


thread_t *thread_create(
    const char *name,
    void (*entry)(void),
    uint32_t owner_pid
) {
    thread_t *new_thread = 0;

    /*
     * Find an unused thread-table entry.
     */
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].tid == 0 ||
            thread_table[i].state == THREAD_TERMINATED) {

            new_thread = &thread_table[i];
            break;
        }
    }

    if (new_thread == 0) {
        return 0;
    }

    new_thread->tid = next_tid++;
    new_thread->state = THREAD_READY;

    new_thread->entry = entry;
    new_thread->name = name;
    new_thread->owner_pid = owner_pid;

    new_thread->eip = (uint32_t)entry;

    

    uint32_t *stack =
        &new_thread->stack[THREAD_STACK_SIZE / sizeof(uint32_t)];

    /* IRETD frame */
    *(--stack) = 0x00000202;          /* EFLAGS */
    *(--stack) = 0x00000008;          /* CS */
    *(--stack) = (uint32_t)entry;     /* EIP */

    /* POPAD frame */
    *(--stack) = 0;                   /* EAX */
    *(--stack) = 0;                   /* ECX */
    *(--stack) = 0;                   /* EDX */
    *(--stack) = 0;                   /* EBX */
    *(--stack) = 0;                   /* ESP placeholder */
    *(--stack) = 0;                   /* EBP */
    *(--stack) = 0;                   /* ESI */
    *(--stack) = 0;                   /* EDI */

    new_thread->esp = (uint32_t)stack;
    new_thread->next = 0;

    /*
     * Add thread to the end of the linked list.
     */
    if (thread_list == 0) {
        thread_list = new_thread;
    } else {
        thread_t *temp = thread_list;

        while (temp->next != 0) {
            temp = temp->next;
        }

        temp->next = new_thread;
    }

    return new_thread;
}


thread_t *thread_current(void) {
    return current_thread;
}

void thread_set_current(thread_t *thread) {
    current_thread = thread;
}



thread_t *thread_get_list(void) {
    return thread_list;
}


thread_t *thread_find(uint32_t tid) {
    thread_t *thread = thread_list;

    while (thread != 0) {
        if (thread->tid == tid) {
            return thread;
        }

        thread = thread->next;
    }

    return 0;
}


const char *thread_state_string(thread_state_t state) {
    switch (state) {
        case THREAD_READY:
            return "READY";

        case THREAD_RUNNING:
            return "RUNNING";

        case THREAD_BLOCKED:
            return "BLOCKED";

        case THREAD_TERMINATED:
            return "TERMINATED";

        default:
            return "UNKNOWN";
    }
}