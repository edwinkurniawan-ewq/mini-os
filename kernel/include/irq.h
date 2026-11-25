#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/*
 * Simpan status interrupt flag (IF) lalu matikan interupsi.
 * Dipakai oleh scheduler, mutex, semaphore, dan critical section supaya
 * struktur data bersama tidak bisa diinterupsi oleh IRQ0/PIT.
 */
static inline uint32_t irq_save_disable(void) {
    uint32_t flags;
    __asm__ volatile ("pushfl\n\t"
                      "popl %0\n\t"
                      "cli"
                      : "=r" (flags)
                      :
                      : "memory");
    return flags;
}

/* Pulihkan EFLAGS (termasuk IF) persis seperti kondisi sebelum cli. */
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile ("pushl %0\n\t"
                      "popfl"
                      :
                      : "r" (flags)
                      : "memory");
}

#endif /* IRQ_H */
