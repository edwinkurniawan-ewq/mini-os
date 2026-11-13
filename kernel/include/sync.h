#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

/* Mutex */
typedef struct {
    volatile int locked;
    int          owner_id;  /* TCB id of current lock holder; -1 = free */
} mutex_t;

/* Counting semaphore */
typedef struct {
    volatile int count;
} semaphore_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

void semaphore_init(semaphore_t *s, int initial);
void semaphore_wait(semaphore_t *s);
void semaphore_signal(semaphore_t *s);

void sync_init(void);

#endif /* SYNC_H */
