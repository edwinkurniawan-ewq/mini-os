#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS   16
#define STACK_SIZE  4096

typedef enum {
    TASK_NEW        = 0,
    TASK_READY      = 1,
    TASK_RUNNING    = 2,
    TASK_WAITING    = 3,
    TASK_TERMINATED = 4
} task_state_t;

/* Jenis sumber daya yang sedang ditunggu task (dipakai deteksi deadlock) */
typedef enum {
    WAIT_NONE  = 0,
    WAIT_MUTEX = 1,
    WAIT_SEM   = 2
} wait_kind_t;

/* Thread Control Block */
typedef struct {
    uint32_t     esp;              /* saved stack pointer (context switch) */
    uint32_t     ebp;              /* saved base pointer */
    uint32_t     eip;              /* instruction pointer at creation */
    uint32_t     start_tick;       /* tick saat task dibuat */
    uint32_t     cpu_ticks;        /* akumulasi tick eksekusi (dari timer/scheduler) */
    task_state_t state;
    int          id;
    char         name[16];
    void        *wait_res;         /* mutex/semaphore yang sedang ditunggu */
    wait_kind_t  wait_kind;
    uint8_t      stack[STACK_SIZE];
} tcb_t;

void  task_init(void);
int   task_create(const char *name, void (*entry)(void));
int   task_create_pending(const char *name, void (*entry)(void)); /* NEW */
void  task_start(int id);                      /* NEW -> READY */
void  task_yield(void);
void  task_exit(void);
void  schedule(void);
void  task_print_all(void);
void  task_preempt_demo(void);
void  task_clear_trace(void);
void  task_print_trace(void);

/* --- primitif blocking (Minggu 3: sinkronisasi) ------------------------- */
int   task_current_id(void);
void  task_block_on(void *res, wait_kind_t kind);  /* state WAITING + yield */
void  task_wake(int id);                          /* WAITING -> READY */
int   task_ready_count(void);
int   task_waiting_count(void);
int   task_run_until_blocked(void);   /* jalankan task READY sampai macet/selesai */
void  task_reset(void);                /* bersihkan task hasil uji coba */
int   task_total(void);
tcb_t *task_get(int id);

/* Fungsi tracking thread sesuai referensi foto */
int   task_register_logger_start(void);
void  task_register_logger_stop(int t_num);
void  task_print_threads(void);
void  task_run_laporan(void);

#endif /* TASK_H */
