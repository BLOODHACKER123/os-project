#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

/*
 * Initialize the Interrupt Descriptor Table (IDT)
 * and prepare hardware interrupt handling.
 */
void interrupts_init(void);

/*
 * Enable CPU interrupts.
 * Equivalent to the x86 STI instruction.
 */
void interrupts_enable(void);

/*
 * Disable CPU interrupts.
 * Equivalent to the x86 CLI instruction.
 */
void interrupts_disable(void);

/* Configure the Programmable Interval Timer (PIT). */
void pit_init(uint32_t frequency);

uint32_t timer_get_ticks(void);

#endif