#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>

/*
 * Simple kernel mutex.
 *
 * locked = 0 -> mutex is available
 * locked = 1 -> mutex is owned
 */
typedef struct {
    volatile uint32_t locked;
} mutex_t;


/* Initialize an unlocked mutex. */
void mutex_init(mutex_t *mutex);


/*
 * Acquire the mutex.
 *
 * For our first version this will use atomic locking.
 * We will improve it to blocking behavior afterward.
 */
void mutex_lock(mutex_t *mutex);


/* Release the mutex. */
void mutex_unlock(mutex_t *mutex);

#endif