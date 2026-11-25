/*
 * kernel/sync/sync.c -- Primitif sinkronisasi (Minggu 3, PIC 2 / Rasya)
 *
 * Mutex     : kepemilikan eksklusif + antrean penunggu FIFO + handoff ownership
 *             saat unlock (tidak ada barging saat mutex dilepas).
 * Semaphore : counting semaphore dengan token; wait memblokir task.
 * Deadlock  : deteksi via wait-for graph (A menunggu mutex milik B, B
 *             menunggu mutex milik A  ->  siklus).
 */

#include "sync.h"
#include "task.h"
#include "kernel.h"
#include "vga.h"
#include "irq.h"

#define SYNC_MAX_MUTEX 8

static mutex_t *mutex_reg[SYNC_MAX_MUTEX];
static int      mutex_reg_count = 0;

static int deadlock_chain[MAX_TASKS];
static int deadlock_len = 0;

/* ---- Antrean penunggu (FIFO circular) ---------------------------------- */

static void waitq_push(int *q, int *tail, int *n, int id) {
    if (*n >= SYNC_MAX_WAITERS) return;
    q[*tail] = id;
    *tail = (*tail + 1) % SYNC_MAX_WAITERS;
    (*n)++;
}

static int waitq_pop(int *q, int *head, int *n) {
    if (*n <= 0) return -1;
    int id = q[*head];
    *head = (*head + 1) % SYNC_MAX_WAITERS;
    (*n)--;
    return id;
}

/* ---- Mutex -------------------------------------------------------------- */

void mutex_init(mutex_t *m) {
    m->locked   = 0;
    m->owner_id = -1;
    m->head = m->tail = m->nwait = 0;

    /* daftarkan sekali saja untuk keperluan deteksi deadlock */
    for (int i = 0; i < mutex_reg_count; i++) {
        if (mutex_reg[i] == m) return;
    }
    if (mutex_reg_count < SYNC_MAX_MUTEX) {
        mutex_reg[mutex_reg_count++] = m;
    }
}

void mutex_lock(mutex_t *m) {
    uint32_t flags = irq_save_disable();
    int me = task_current_id();

    if (!m->locked) {
        m->locked   = 1;
        m->owner_id = me;
        irq_restore(flags);
        return;
    }

    /* Antrean + perubahan status WAITING harus satu blok atomik terhadap
     * IRQ0, jika tidak thread bisa "terbangun" sebelum sempat diblokir. */
    waitq_push(m->waiters, &m->tail, &m->nwait, me);
    task_block_on(m, WAIT_MUTEX);
    /* Ownership sudah dipindahkan mutex_unlock ke task ini. */
    irq_restore(flags);
}

int mutex_trylock(mutex_t *m) {
    uint32_t flags = irq_save_disable();
    int ok = 0;

    if (!m->locked) {
        m->locked   = 1;
        m->owner_id = task_current_id();
        ok = 1;
    }
    irq_restore(flags);
    return ok;
}

int mutex_unlock(mutex_t *m) {
    uint32_t flags = irq_save_disable();
    int ok = 0;

    if (m->locked && m->owner_id == task_current_id()) {
        int w = waitq_pop(m->waiters, &m->head, &m->nwait);
        if (w >= 0) {
            /* Handoff: lock tidak pernah "bebas" di antara dua task */
            m->owner_id = w;
            task_wake(w);
        } else {
            m->locked   = 0;
            m->owner_id = -1;
        }
        ok = 1;
    }
    irq_restore(flags);
    return ok;
}

int mutex_is_locked(mutex_t *m) {
    return m->locked;
}

int mutex_owner(mutex_t *m) {
    return m->owner_id;
}

int mutex_waiters(mutex_t *m) {
    return m->nwait;
}

/* ---- Counting semaphore ------------------------------------------------- */

void semaphore_init(semaphore_t *s, int initial) {
    s->value = initial;
    s->head = s->tail = s->nwait = 0;
}

void semaphore_wait(semaphore_t *s) {
    for (;;) {
        uint32_t flags = irq_save_disable();

        if (s->value > 0) {
            s->value--;
            irq_restore(flags);
            return;
        }

        waitq_push(s->waiters, &s->tail, &s->nwait, task_current_id());
        task_block_on(s, WAIT_SEM);
        irq_restore(flags);
    }
}

int semaphore_trywait(semaphore_t *s) {
    uint32_t flags = irq_save_disable();
    int ok = 0;

    if (s->value > 0) {
        s->value--;
        ok = 1;
    }
    irq_restore(flags);
    return ok;
}

void semaphore_signal(semaphore_t *s) {
    uint32_t flags = irq_save_disable();

    s->value++;
    int w = waitq_pop(s->waiters, &s->head, &s->nwait);
    if (w >= 0) task_wake(w);

    irq_restore(flags);
}

int semaphore_value(semaphore_t *s) {
    return s->value;
}

int semaphore_waiters(semaphore_t *s) {
    return s->nwait;
}

/* ---- Deteksi deadlock: wait-for graph ----------------------------------- */

int sync_detect_deadlock(void) {
    int total = task_total();
    deadlock_len = 0;

    for (int start = 1; start < total; start++) {
        int chain[MAX_TASKS];
        int len = 0;
        int cur = start;

        while (cur > 0 && cur < total && len < MAX_TASKS) {
            int seen = 0;
            for (int k = 0; k < len; k++) {
                if (chain[k] == cur) { seen = 1; break; }
            }
            if (seen) {                       /* siklus tertutup -> deadlock */
                for (int k = 0; k < len; k++) deadlock_chain[k] = chain[k];
                deadlock_len = len;
                return 1;
            }
            chain[len++] = cur;

            tcb_t *t = task_get(cur);
            if (!t || t->state != TASK_WAITING) break;
            if (t->wait_kind != WAIT_MUTEX || !t->wait_res) break;

            mutex_t *m = (mutex_t *)t->wait_res;
            if (m->owner_id <= 0) break;       /* tanpa pemilik: bukan deadlock */
            cur = m->owner_id;
        }
    }
    return 0;
}

void sync_print_deadlock(void) {
    vga_set_color(0x0C);
    vga_puts("[DEADLOCK] Siklus wait-for: ");
    for (int i = 0; i < deadlock_len; i++) {
        vga_puts("T");
        vga_put_int(deadlock_chain[i]);
        vga_puts(" -> ");
    }
    if (deadlock_len > 0) {
        vga_puts("T");
        vga_put_int(deadlock_chain[0]);
    }
    vga_puts("\n");
    vga_set_color(0x0F);
}

void sync_init(void) {
    mutex_reg_count = 0;
    deadlock_len    = 0;
    vga_puts("[SYNC] Mutex + Semaphore + deteksi deadlock siap\n");
}
