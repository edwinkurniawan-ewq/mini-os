#include "task.h"
#include "kernel.h"

tcb_t  task_table[MAX_TASKS];
int    task_count   = 0;
tcb_t *current_task = NULL;

/* ---- local helpers (no libc) ------------------------------------------- */

static void k_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int k_strlen(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}

static void k_itoa(int n, char *buf, int bufsz) {
    if (bufsz < 2) return;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12]; int i = 0;
    while (n > 0 && i < (int)sizeof(tmp)) { tmp[i++] = '0' + (n % 10); n /= 10; }
    if (i >= bufsz) { buf[0] = '?'; buf[1] = '\0'; return; }
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

static const char *state_str(task_state_t s) {
    switch (s) {
        case TASK_NEW:        return "NEW";
        case TASK_READY:      return "READY";
        case TASK_RUNNING:    return "RUNNING";
        case TASK_WAITING:    return "WAITING";
        case TASK_TERMINATED: return "TERM";
        default:              return "?";
    }
}

/* ---- context switch (asm) ---------------------------------------------- */

extern void context_switch(tcb_t *old, tcb_t *new);

/* ---- scheduler --------------------------------------------------------- */

void schedule(void) {
    tcb_t *old = current_task;
    int start = old ? old->id + 1 : 1;
    int next  = -1;

    /* Round-robin among user tasks (id >= 1), skip kernel task 0 */
    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (idx == 0) continue;
        if (task_table[idx].state == TASK_READY) {
            next = idx;
            break;
        }
    }

    if (next == -1) {
        /* No other ready user task */
        if (!old || old->id == 0) return;     /* kernel running, nothing to do */
        /* User task: if still RUNNING (yielding), keep it running —
           no other task to switch to. If TERMINATED (task_exit), switch to kernel. */
        if (old->state == TASK_RUNNING)
            return;                            /* keep current task running */
        task_table[0].state = TASK_RUNNING;
        current_task = &task_table[0];
        context_switch(old, &task_table[0]);
        return;
    }

    tcb_t *new = &task_table[next];

    if (old) {
        if (old->id == 0) {
            /* Kernel yielding -> mark WAITING (excluded from round-robin) */
            old->state = TASK_WAITING;
        } else if (old->state == TASK_RUNNING) {
            old->state = TASK_READY;
        }
        /* TERMINATED (from task_exit) -> leave as-is */
    }

    new->state = TASK_RUNNING;
    current_task = new;

    if (old != new)
        context_switch(old, new);
}

/* ---- public API --------------------------------------------------------- */

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_table[i].state   = TASK_TERMINATED;
        task_table[i].id      = i;
        task_table[i].name[0] = '\0';
        task_table[i].esp     = 0;
        task_table[i].ebp     = 0;
        task_table[i].eip     = 0;
    }

    /* Task 0 = kernel (current context) -- ESP set on first context_switch */
    task_table[0].state = TASK_RUNNING;
    k_strcpy(task_table[0].name, "kernel", sizeof(task_table[0].name));

    task_count   = 1;
    current_task = &task_table[0];

    vga_puts("[TASK] Task manager initialized (max=16)\n");
}

int task_create(const char *name, void (*entry)(void)) {
    if (task_count >= MAX_TASKS) {
        vga_puts("[TASK] ERROR: task table full\n");
        return -1;
    }
    tcb_t *t = &task_table[task_count];
    t->id    = task_count++;
    t->state = TASK_NEW;
    k_strcpy(t->name, name, sizeof(t->name));

    /*
     * Initial stack -- matches context_switch restore sequence:
     *   pop edi, pop esi, pop ebx, pop ebp, ret
     *
     * Stack (low->high):  EDI(0) | ESI(0) | EBX(0) | EBP(0) | entry | task_exit
     *
     * context_switch's RET pops `entry` -> thread starts.
     * If entry() returns, its RET pops `task_exit` -> cleanup.
     */
    uint32_t *sp = (uint32_t *)(t->stack + STACK_SIZE);
    *--sp = (uint32_t)task_exit;   /* return addr if entry() returns */
    *--sp = (uint32_t)entry;       /* return addr for context_switch RET */
    *--sp = 0;                     /* EBP */
    *--sp = 0;                     /* EBX */
    *--sp = 0;                     /* ESI */
    *--sp = 0;                     /* EDI */
    t->esp = (uint32_t)sp;
    t->ebp = 0;
    t->eip = (uint32_t)entry;

    t->state = TASK_READY;
    vga_puts("[TASK] Created '"); vga_puts(t->name); vga_puts("' -> READY\n");
    return t->id;
}

void task_yield(void) {
    schedule();
}

void task_exit(void) {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
        vga_puts("[TASK] '"); vga_puts(current_task->name); vga_puts("' terminated\n");
    }
    schedule();               /* switch to next task -- never returns */
    for (;;) { __asm__ volatile ("hlt"); }   /* safety: shouldn't reach */
}

void task_print_all(void) {
    char buf[8];
    vga_set_color(0x0B);
    vga_puts("ID  NAME             STATE\n");
    vga_puts("--- ---------------- ----\n");
    vga_set_color(0x0F);
    for (int i = 0; i < task_count; i++) {
        tcb_t *t = &task_table[i];
        k_itoa(t->id, buf, sizeof(buf));
        vga_puts(buf); vga_puts("   ");
        vga_puts(t->name);
        int pad = 16 - k_strlen(t->name);
        for (int p = 0; p < pad; p++) vga_puts(" ");
        vga_puts(" "); vga_puts(state_str(t->state)); vga_puts("\n");
    }
}
