#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

// Definisi siklus hidup thread (Sesuai syarat CPMK 2)
typedef enum {
    THREAD_NEW = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
} thread_state_t;

// Struktur Data Thread Control Block (TCB)
typedef struct {
    int id;
    thread_state_t state;
    void (*task)(void); // Pointer fungsi ke tugas yang akan dijalankan
} tcb_t;

void thread_init(void);
void run_thread_demo(void);

#endif