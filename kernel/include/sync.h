#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

/* Panjang antrean penunggu maksimum per primitive */
#define SYNC_MAX_WAITERS 8

/* Mutex: kepemilikan eksklusif + antrean penunggu FIFO */
typedef struct {
    volatile int locked;
    int          owner_id;                 /* id task pemilik; -1 = bebas */
    int          waiters[SYNC_MAX_WAITERS];
    int          head, tail, nwait;
} mutex_t;

/* Counting semaphore */
typedef struct {
    volatile int value;
    int          waiters[SYNC_MAX_WAITERS];
    int          head, tail, nwait;
} semaphore_t;

/* ---- Mutex -------------------------------------------------------------- */
void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);            /* memblokir task jika sedang dipegang */
int  mutex_trylock(mutex_t *m);         /* 1 = berhasil, 0 = sedang dipegang */
int  mutex_unlock(mutex_t *m);          /* 1 = sukses, 0 = bukan pemilik */
int  mutex_is_locked(mutex_t *m);
int  mutex_owner(mutex_t *m);
int  mutex_waiters(mutex_t *m);

/* ---- Counting semaphore ------------------------------------------------- */
void semaphore_init(semaphore_t *s, int initial);
void semaphore_wait(semaphore_t *s);     /* memblokir task jika tidak ada token */
int  semaphore_trywait(semaphore_t *s);  /* 1 = token diambil, 0 = tidak ada */
void semaphore_signal(semaphore_t *s);
int  semaphore_value(semaphore_t *s);
int  semaphore_waiters(semaphore_t *s);

/* ---- Deteksi deadlock (wait-for graph) ---------------------------------- */
/* 1 = siklus deadlock terdeteksi, 0 = tidak ada deadlock */
int  sync_detect_deadlock(void);
void sync_print_deadlock(void);

void sync_init(void);

#endif /* SYNC_H */
