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
        case TASK_TERMINATED: return "TERMINATED";
        default:              return "UNKNOWN";
    }
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
    task_count   = 0;
    current_task = NULL;
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

    /* Initial stack frame: entry address pushed as return EIP */
    uint32_t *sp = (uint32_t *)(t->stack + STACK_SIZE);
    *--sp  = (uint32_t)entry;
    t->esp = (uint32_t)sp;
    t->ebp = (uint32_t)sp;
    t->eip = (uint32_t)entry;

    t->state = TASK_READY;
    vga_puts("[TASK] Created '"); vga_puts(t->name); vga_puts("' -> READY\n");
    return t->id;
}

void task_yield(void) {
    /* ponytail: real context_switch() + scheduler added Week 2 */
}

void task_print_all(void) {
    char buf[8];
    vga_set_color(0x0B);
    vga_puts("ID  NAME             STATE\n");
    vga_puts("--- ---------------- ----------\n");
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
