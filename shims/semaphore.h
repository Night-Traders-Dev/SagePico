/* Shim for semaphore.h - not available on baremetal RP2350 */
#ifndef SHIM_SEMAPHORE_H
#define SHIM_SEMAPHORE_H

typedef struct { int count; } sem_t;

static inline int sem_init(sem_t* sem, int pshared, unsigned int value) {
    (void)pshared;
    if (!sem) return -1;
    sem->count = (int)value;
    return 0;
}
static inline int sem_wait(sem_t* sem) {
    if (!sem) return -1;
    while (sem->count <= 0) { /* spin */ }
    sem->count--;
    return 0;
}
static inline int sem_post(sem_t* sem) {
    if (!sem) return -1;
    sem->count++;
    return 0;
}
static inline int sem_trywait(sem_t* sem) {
    if (!sem || sem->count <= 0) return -1;
    sem->count--;
    return 0;
}

#endif
