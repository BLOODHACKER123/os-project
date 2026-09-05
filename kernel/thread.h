#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

#define MAX_THREADS 32
#define THREAD_STACK_SIZE 4096

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct thread {
    uint32_t tid;

    thread_state_t state;

    uint32_t esp;
    uint32_t eip;

    uint32_t stack[THREAD_STACK_SIZE / sizeof(uint32_t)];

    void (*entry)(void);

    const char *name;

    uint32_t owner_pid;

    struct thread *next;
} thread_t;


/* Initialize thread subsystem */
void thread_init(void);

/* Create a new thread */
thread_t *thread_create(
    const char *name,
    void (*entry)(void),
    uint32_t owner_pid
);


/* Return the currently running thread */
thread_t *thread_current(void);

/* Set the currently running thread */
void thread_set_current(thread_t *thread);


/* Return first thread */
thread_t *thread_get_list(void);

/* Find thread by TID */
thread_t *thread_find(uint32_t tid);

/* Convert thread state to printable text */
const char *thread_state_string(thread_state_t state);

#endif