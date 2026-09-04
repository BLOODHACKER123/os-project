#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_STACK_SIZE 4096
#define MAX_PROCESSES 16

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} proc_state_t;

typedef struct pcb {
    uint32_t pid;

    proc_state_t state;

    uint32_t esp;
    uint32_t eip;

    uint32_t stack[PROCESS_STACK_SIZE / sizeof(uint32_t)];

    void (*entry)(void);

    const char *name;

    struct pcb *next;
} pcb_t;


/* Initialize the process subsystem */
void process_init(void);

/* Create a new process */
pcb_t *process_create(const char *name, void (*entry)(void));

/* Return the currently running process */
pcb_t *process_current(void);

/* Set the process that is currently running */
void process_set_current(pcb_t *process);

/* Return the first process in the process list */
pcb_t *process_get_list(void);

/* Find a process using its PID */
pcb_t *process_find(uint32_t pid);

/* Mark a process as terminated */
int process_kill(uint32_t pid);

/* Convert process state to printable text */
const char *process_state_string(proc_state_t state);

#endif