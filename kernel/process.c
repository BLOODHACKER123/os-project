#include "process.h"

static pcb_t process_table[MAX_PROCESSES];

static pcb_t *process_list = 0;
static pcb_t *current_process = 0;

static uint32_t next_pid = 1;


void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_TERMINATED;
        process_table[i].esp = 0;
        process_table[i].eip = 0;
        process_table[i].entry = 0;
        process_table[i].name = 0;
        process_table[i].next = 0;
    }

    process_list = 0;
    current_process = 0;
    next_pid = 1;
}


pcb_t *process_create(const char *name, void (*entry)(void)) {
    pcb_t *new_process = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == 0 ||
            process_table[i].state == PROCESS_TERMINATED) {

            new_process = &process_table[i];
            break;
        }
    }

    if (new_process == 0) {
        return 0;
    }

    new_process->pid = next_pid++;
    new_process->state = PROCESS_READY;

    new_process->entry = entry;
    new_process->name = name;

    new_process->eip = (uint32_t)entry;

    /*
 * Prepare the process stack so that POPAD + IRETD can
 * start this process as if it had previously been interrupted.
 */
uint32_t *stack =
    &new_process->stack[PROCESS_STACK_SIZE / sizeof(uint32_t)];

/*
 * IRETD frame
 *
 * IRETD will restore these values:
 *   EIP
 *   CS
 *   EFLAGS
 */
*(--stack) = 0x00000202;          /* EFLAGS: interrupts enabled */
*(--stack) = 0x00000008;          /* CS: kernel code segment */
*(--stack) = (uint32_t)entry;     /* EIP: process entry point */

/*
 * POPAD frame
 *
 * POPAD restores:
 *   EDI
 *   ESI
 *   EBP
 *   skips ESP
 *   EBX
 *   EDX
 *   ECX
 *   EAX
 *
 * We push them here in reverse order.
 */
*(--stack) = 0;   /* EAX */
*(--stack) = 0;   /* ECX */
*(--stack) = 0;   /* EDX */
*(--stack) = 0;   /* EBX */
*(--stack) = 0;   /* ESP placeholder - POPAD ignores this */
*(--stack) = 0;   /* EBP */
*(--stack) = 0;   /* ESI */
*(--stack) = 0;   /* EDI */

/* Save the prepared stack pointer in the PCB */

    new_process->esp = (uint32_t)stack;

    new_process->next = 0;

    if (process_list == 0) {
        process_list = new_process;
    } else {
        pcb_t *temp = process_list;

        while (temp->next != 0) {
            temp = temp->next;
        }

        temp->next = new_process;
    }

    return new_process;
}


pcb_t *process_current(void) {
    return current_process;
}

void process_set_current(pcb_t *process) {
    current_process = process;
}


pcb_t *process_get_list(void) {
    return process_list;
}


pcb_t *process_find(uint32_t pid) {
    pcb_t *process = process_list;

    while (process != 0) {
        if (process->pid == pid) {
            return process;
        }

        process = process->next;
    }

    return 0;
}


int process_kill(uint32_t pid) {
    pcb_t *process = process_find(pid);

    if (process == 0) {
        return -1;
    }

    process->state = PROCESS_TERMINATED;

    return 0;
}


const char *process_state_string(proc_state_t state) {
    switch (state) {
        case PROCESS_READY:
            return "READY";

        case PROCESS_RUNNING:
            return "RUNNING";

        case PROCESS_BLOCKED:
            return "BLOCKED";

        case PROCESS_TERMINATED:
            return "TERMINATED";

        default:
            return "UNKNOWN";
    }
}