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

/* Thread Control Block */
typedef struct {
    uint32_t     esp;              /* saved stack pointer (context switch) */
    uint32_t     ebp;              /* saved base pointer */
    uint32_t     eip;              /* instruction pointer at creation */
    task_state_t state;
    int          id;
    char         name[16];
    uint8_t      stack[STACK_SIZE];
} tcb_t;

void  task_init(void);
int   task_create(const char *name, void (*entry)(void));
void  task_yield(void);
void  task_exit(void);
void  schedule(void);
void  task_print_all(void);

/* Fungsi tracking thread sesuai referensi foto */
int   task_register_logger_start(void);
void  task_register_logger_stop(int t_num);
void  task_print_threads(void);
void  task_run_laporan(void);

#endif /* TASK_H */
