#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>

/*
 * Counting semaphore.
 *
 * value > 0  -> resource available
 * value == 0 -> caller must wait
 */
typedef struct {
    volatile int32_t value;
} semaphore_t;


/* Initialize semaphore with starting count */
void semaphore_init(semaphore_t *sem, int32_t initial_value);


/*
 * Wait / acquire.
 *
 * Decrements the semaphore when a resource is available.
 * If value is zero, this first version will wait.
 */
void semaphore_wait(semaphore_t *sem);


/*
 * Signal / release.
 *
 * Returns one resource to the semaphore.
 */
void semaphore_signal(semaphore_t *sem);

#endif