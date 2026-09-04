#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/* Initialize the round-robin scheduler. */
void scheduler_init(void);

/*
 * Called from the timer interrupt.
 *
 * current_esp is the stack pointer of the process/context
 * that was interrupted.
 *
 * Returns the stack pointer that should be restored.
 */
uint32_t scheduler_tick(uint32_t current_esp);

#endif