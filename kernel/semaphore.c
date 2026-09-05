#include "semaphore.h"

/*
 * Initialize the semaphore.
 */
void semaphore_init(semaphore_t *sem, int32_t initial_value) {
    sem->value = initial_value;
}


/*
 * Wait until a resource is available,
 * then atomically decrement the semaphore.
 */
void semaphore_wait(semaphore_t *sem) {
    while (1) {
        int32_t value = sem->value;

        /*
         * Nothing is currently available.
         */
        if (value <= 0) {
            continue;
        }

        /*
         * cmpxchg:
         *
         * Compare sem->value with 'value'.
         * If equal, replace it with value - 1.
         *
         * This prevents two threads from acquiring
         * the same semaphore resource.
         */
        unsigned char success;

        __asm__ __volatile__(
            "lock; cmpxchgl %3, %1\n\t"
            "sete %0"
            : "=q"(success),
              "+m"(sem->value),
              "+a"(value)
            : "r"(value - 1)
            : "memory"
        );

        if (success) {
            return;
        }
    }
}


/*
 * Return one resource to the semaphore.
 */
void semaphore_signal(semaphore_t *sem) {
    __asm__ __volatile__(
        "lock; incl %0"
        : "+m"(sem->value)
        :
        : "memory"
    );
}