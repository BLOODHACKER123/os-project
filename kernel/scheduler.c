#include "scheduler.h"
#include "process.h"
#include "thread.h"

/*
 * The scheduler needs to know what kind of execution context
 * is currently using the CPU.
 */
typedef enum {
    SCHED_CONTEXT_PROCESS,
    SCHED_CONTEXT_THREAD
} sched_context_t;

static sched_context_t current_context = SCHED_CONTEXT_PROCESS;


/*
 * Find the first READY process starting from 'start'.
 * This does NOT wrap around the process list.
 */
static pcb_t *find_ready_process(pcb_t *start) {
    pcb_t *process = start;

    while (process != 0) {
        if (process->state == PROCESS_READY) {
            return process;
        }

        process = process->next;
    }

    return 0;
}


/*
 * Find the first READY thread starting from 'start'.
 * This does NOT wrap around the thread list.
 */
static thread_t *find_ready_thread(thread_t *start) {
    thread_t *thread = start;

    while (thread != 0) {
        if (thread->state == THREAD_READY) {
            return thread;
        }

        thread = thread->next;
    }

    return 0;
}


void scheduler_init(void) {
    process_set_current(0);
    thread_set_current(0);

    current_context = SCHED_CONTEXT_PROCESS;
}


uint32_t scheduler_tick(uint32_t current_esp) {

    /*
     * =========================================================
     * CASE 1: A PROCESS is currently running
     * =========================================================
     */
    if (current_context == SCHED_CONTEXT_PROCESS) {

        pcb_t *current_process = process_current();

        /*
         * Save the current process CPU context.
         */
        if (current_process != 0) {
            current_process->esp = current_esp;

            if (current_process->state == PROCESS_RUNNING) {
                current_process->state = PROCESS_READY;
            }
        }


        /*
         * First try the next process in the process list.
         */
        pcb_t *next_process = 0;

        if (current_process != 0) {
            next_process =
                find_ready_process(current_process->next);
        } else {
            next_process =
                find_ready_process(process_get_list());
        }


        if (next_process != 0) {
            next_process->state = PROCESS_RUNNING;

            process_set_current(next_process);
            thread_set_current(0);

            current_context = SCHED_CONTEXT_PROCESS;

            return next_process->esp;
        }


        /*
         * No more READY processes after the current one.
         *
         * Now give kernel threads a chance to run.
         */
        thread_t *next_thread =
            find_ready_thread(thread_get_list());

        if (next_thread != 0) {
            next_thread->state = THREAD_RUNNING;

            thread_set_current(next_thread);
            process_set_current(0);

            current_context = SCHED_CONTEXT_THREAD;

            return next_thread->esp;
        }


        /*
         * No threads are runnable.
         * Wrap around to the beginning of the process list.
         */
        next_process =
            find_ready_process(process_get_list());

        if (next_process != 0) {
            next_process->state = PROCESS_RUNNING;

            process_set_current(next_process);
            thread_set_current(0);

            current_context = SCHED_CONTEXT_PROCESS;

            return next_process->esp;
        }


        /*
         * Nothing else can run.
         * Continue the current execution context if possible.
         */
        if (current_process != 0 &&
            current_process->state != PROCESS_TERMINATED &&
            current_process->state != PROCESS_BLOCKED) {

            current_process->state = PROCESS_RUNNING;

            process_set_current(current_process);
            thread_set_current(0);

            current_context = SCHED_CONTEXT_PROCESS;

            return current_esp;
        }

        return current_esp;
    }


    /*
     * =========================================================
     * CASE 2: A THREAD is currently running
     * =========================================================
     */
    thread_t *current_thread = thread_current();

    /*
     * Save the current thread CPU context.
     */
    if (current_thread != 0) {
        current_thread->esp = current_esp;

        if (current_thread->state == THREAD_RUNNING) {
            current_thread->state = THREAD_READY;
        }
    }


    /*
     * First try the next thread.
     */
    thread_t *next_thread = 0;

    if (current_thread != 0) {
        next_thread =
            find_ready_thread(current_thread->next);
    } else {
        next_thread =
            find_ready_thread(thread_get_list());
    }


    if (next_thread != 0) {
        next_thread->state = THREAD_RUNNING;

        thread_set_current(next_thread);
        process_set_current(0);

        current_context = SCHED_CONTEXT_THREAD;

        return next_thread->esp;
    }


    /*
     * No more READY threads.
     *
     * Go back to the beginning of the process list.
     */
    pcb_t *next_process =
        find_ready_process(process_get_list());

    if (next_process != 0) {
        next_process->state = PROCESS_RUNNING;

        process_set_current(next_process);
        thread_set_current(0);

        current_context = SCHED_CONTEXT_PROCESS;

        return next_process->esp;
    }


    /*
     * No processes runnable.
     * Wrap around to the beginning of the thread list.
     */
    next_thread =
        find_ready_thread(thread_get_list());

    if (next_thread != 0) {
        next_thread->state = THREAD_RUNNING;

        thread_set_current(next_thread);
        process_set_current(0);

        current_context = SCHED_CONTEXT_THREAD;

        return next_thread->esp;
    }


    /*
     * Nothing else can run.
     */
    if (current_thread != 0 &&
        current_thread->state != THREAD_TERMINATED &&
        current_thread->state != THREAD_BLOCKED) {

        current_thread->state = THREAD_RUNNING;

        thread_set_current(current_thread);
        process_set_current(0);

        current_context = SCHED_CONTEXT_THREAD;

        return current_esp;
    }

    return current_esp;
}