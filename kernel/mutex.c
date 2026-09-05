#include "mutex.h"

/*
 * Atomically exchange *ptr with value.
 *
 * xchg with a memory operand is atomic on x86,
 * so two threads cannot successfully acquire
 * the mutex at the same time.
 */
static inline uint32_t atomic_exchange(
    volatile uint32_t *ptr,
    uint32_t value
) {
    __asm__ __volatile__(
        "xchg %0, %1"
        : "+r"(value), "+m"(*ptr)
        :
        : "memory"
    );

    return value;
}


void mutex_init(mutex_t *mutex) {
    mutex->locked = 0;
}


void mutex_lock(mutex_t *mutex) {

    while (atomic_exchange(&mutex->locked, 1) != 0) {
        /*
         * Another thread owns the mutex.
         * For now, spin until it becomes free.
         */
    }
}


void mutex_unlock(mutex_t *mutex) {

    /*
     * Release the mutex.
     */
    __asm__ __volatile__(
        "" ::: "memory"
    );

    mutex->locked = 0;
}