#include "sync.h"
#include "kernel.h"

void sync_init(void) {
    vga_puts("[SYNC] Sync primitives ready\n");
}

void mutex_init(mutex_t *m) {
    m->locked   = 0;
    m->owner_id = -1;
}

void mutex_lock(mutex_t *m) {
    /* ponytail: busy-spin — replace with blocking sleep_on() in Week 2 */
    while (m->locked) { __asm__ volatile ("pause"); }
    m->locked = 1;
}

void mutex_unlock(mutex_t *m) {
    m->locked   = 0;
    m->owner_id = -1;
}

void semaphore_init(semaphore_t *s, int initial) {
    s->count = initial;
}

void semaphore_wait(semaphore_t *s) {
    /* ponytail: busy-spin — replace with blocking in Week 2 */
    while (s->count <= 0) { __asm__ volatile ("pause"); }
    s->count--;
}

void semaphore_signal(semaphore_t *s) {
    s->count++;
}
