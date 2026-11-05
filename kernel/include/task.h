#ifndef TASK_H
#define TASK_H

#include <stdint.h>

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
void  task_yield(void);        /* cooperative yield — stub until Week 2 */
void  task_print_all(void);    /* CLI: list all threads & states */

extern tcb_t *current_task;
extern tcb_t  task_table[MAX_TASKS];
extern int    task_count;

#endif /* TASK_H */
