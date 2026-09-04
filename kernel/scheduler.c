#include "scheduler.h"
#include "process.h"

void scheduler_init(void) {
    process_set_current(0);
}

uint32_t scheduler_tick(uint32_t current_esp) {
    pcb_t *current = process_current();

    if (current != 0) {
        current->esp = current_esp;

        if (current->state == PROCESS_RUNNING) {
            current->state = PROCESS_READY;
        }
    }

    pcb_t *next;

    if (current != 0 && current->next != 0) {
        next = current->next;
    } else {
        next = process_get_list();
    }

    pcb_t *start = next;

    while (next != 0) {
        if (next->state == PROCESS_READY) {
            next->state = PROCESS_RUNNING;
            process_set_current(next);

            return next->esp;
        }

        next = next->next;

        if (next == 0) {
            next = process_get_list();
        }

        if (next == start) {
            break;
        }
    }

    if (current != 0 &&
        current->state != PROCESS_TERMINATED &&
        current->state != PROCESS_BLOCKED) {

        current->state = PROCESS_RUNNING;
        process_set_current(current);

        return current_esp;
    }

    return current_esp;
}