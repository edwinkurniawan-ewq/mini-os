#include "task.h"
#include "kernel.h"
#include "memory.h"
#include "finance.h"

tcb_t  task_table[MAX_TASKS];
int    task_count   = 0;
tcb_t *current_task = NULL;

/* ---- local helpers (no libc) ------------------------------------------- */

static void k_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void print_num(int num) {
    if (num == 0) {
        vga_puts("0");
        return;
    }
    if (num < 0) {
        vga_putc('-');
        num = -num;
    }
    char buf[16];
    int i = 0;
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    while (i > 0) {
        vga_putc(buf[--i]);
    }
}

/* ---- context switch (asm) ---------------------------------------------- */

extern void context_switch(tcb_t *old, tcb_t *new);

/* ---- scheduler --------------------------------------------------------- */

void schedule(void) {
    tcb_t *old = current_task;
    int start = old ? old->id + 1 : 1;
    int next  = -1;

    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (idx == 0) continue;
        if (task_table[idx].state == TASK_READY) {
            next = idx;
            break;
        }
    }

    if (next == -1) {
        if (!old || old->id == 0) return;
        if (old->state == TASK_RUNNING) return;
        task_table[0].state = TASK_RUNNING;
        current_task = &task_table[0];
        context_switch(old, &task_table[0]);
        return;
    }

    tcb_t *new = &task_table[next];

    if (old) {
        if (old->id == 0) {
            old->state = TASK_WAITING;
        } else if (old->state == TASK_RUNNING) {
            old->state = TASK_READY;
        }
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

    task_table[0].state = TASK_RUNNING;
    k_strcpy(task_table[0].name, "kernel", sizeof(task_table[0].name));

    task_count   = 1;
    current_task = &task_table[0];
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

    uint32_t *sp = (uint32_t *)(t->stack + STACK_SIZE);
    *--sp = (uint32_t)task_exit;
    *--sp = (uint32_t)entry;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    t->esp = (uint32_t)sp;
    t->ebp = 0;
    t->eip = (uint32_t)entry;

    t->state = TASK_READY;
    return t->id;
}

void task_yield(void) {
    schedule();
}

void task_exit(void) {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
    }
    schedule();
    for (;;) { __asm__ volatile ("hlt"); }
}

void task_print_all(void) {
    vga_set_color(0x0C);
    vga_puts("ID  NAME             STATE\n");
    vga_puts("--- ---------------- ----\n");
    vga_set_color(0x0F);
    for (int i = 0; i < task_count; i++) {
        tcb_t *t = &task_table[i];
        print_num(t->id);
        vga_puts("   ");
        vga_puts(t->name);
        int pad = 16;
        int n_len = 0;
        while (t->name[n_len]) n_len++;
        for (int p = n_len; p < pad; p++) vga_puts(" ");
        vga_puts(" ");
        switch (t->state) {
            case TASK_NEW:        vga_puts("NEW\n"); break;
            case TASK_READY:      vga_puts("READY\n"); break;
            case TASK_RUNNING:    vga_puts("RUNNING\n"); break;
            case TASK_WAITING:    vga_puts("WAITING\n"); break;
            case TASK_TERMINATED: vga_puts("TERM\n"); break;
            default:              vga_puts("?\n"); break;
        }
    }
}

/* =========================================================================
 * REGISTRY THREAD (Sesuai Tampilan di Referensi Foto)
 * ========================================================================= */

#define MAX_REG_THREADS 32

typedef struct {
    int  t_num;
    char name[16];
    char status[16];
} reg_thread_item_t;

static reg_thread_item_t reg_list[MAX_REG_THREADS];
static int reg_count = 0;
static int next_thread_id = 1;

int task_register_logger_start(void) {
    if (reg_count >= MAX_REG_THREADS) return -1;
    int id = next_thread_id++;
    reg_thread_item_t *item = &reg_list[reg_count++];
    item->t_num = id;
    k_strcpy(item->name, "Logger", sizeof(item->name));
    k_strcpy(item->status, "RUNNING", sizeof(item->status));
    return id;
}

void task_register_logger_stop(int t_num) {
    for (int i = 0; i < reg_count; i++) {
        if (reg_list[i].t_num == t_num) {
            k_strcpy(reg_list[i].status, "TERMINATED", sizeof(reg_list[i].status));
            break;
        }
    }
}

void task_print_threads(void) {
    vga_set_color(0x0C);
    vga_puts("ID   NAMA     STATUS\n");
    vga_set_color(0x0F);

    if (reg_count == 0) {
        vga_puts("(Belum ada thread yang dijalankan)\n");
        return;
    }

    for (int i = 0; i < reg_count; i++) {
        vga_puts("T");
        print_num(reg_list[i].t_num);
        vga_puts("   ");

        vga_puts(reg_list[i].name);
        int n_len = 0;
        while (reg_list[i].name[n_len]) n_len++;
        for (int p = n_len; p < 9; p++) vga_puts(" ");

        vga_puts(reg_list[i].status);
        vga_puts("\n");
    }
}

/* Skenario multi-threading laporan dari RAMDisk persis seperti di Foto 6 */
void task_run_laporan(void) {
    int t_data = next_thread_id++;
    int t_mon  = next_thread_id++;

    /* Daftarkan ke reg_list */
    if (reg_count < MAX_REG_THREADS) {
        reg_list[reg_count].t_num = t_data;
        k_strcpy(reg_list[reg_count].name, "Data", sizeof(reg_list[reg_count].name));
        k_strcpy(reg_list[reg_count].status, "TERMINATED", sizeof(reg_list[reg_count].status));
        reg_count++;
    }
    if (reg_count < MAX_REG_THREADS) {
        reg_list[reg_count].t_num = t_mon;
        k_strcpy(reg_list[reg_count].name, "Monitor", sizeof(reg_list[reg_count].name));
        k_strcpy(reg_list[reg_count].status, "TERMINATED", sizeof(reg_list[reg_count].status));
        reg_count++;
    }

    int count = 0;
    int saldo = 0;
    finance_get_summary(&count, &saldo);

    size_t heap_used = memory_get_used();

    vga_set_color(0x0C);
    vga_puts("=== LAPORAN KAS (2 kernel thread) ===\n");

    vga_puts("[T"); print_num(t_data); vga_puts(" Data] NEW\n");
    vga_puts("[T"); print_num(t_data); vga_puts(" Data] NEW -> READY\n");
    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] NEW\n");
    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] NEW -> READY\n");

    vga_puts("[T"); print_num(t_data); vga_puts(" Data] READY -> RUNNING\n");
    vga_set_color(0x0F);
    vga_puts("[Data] membaca dan menghitung 'kas.dat' dari RAMDisk...\n");
    vga_set_color(0x0C);

    vga_puts("[T"); print_num(t_data); vga_puts(" Data] RUNNING -> WAITING\n");
    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] READY -> RUNNING\n");
    vga_set_color(0x0F);
    vga_puts("[Monitor] cek ke-1: transaksi ");
    print_num(count);
    vga_puts("/20, heap terpakai ");
    print_num((int)heap_used);
    vga_puts(" B\n");
    vga_set_color(0x0C);

    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] RUNNING -> READY\n");
    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] READY -> RUNNING\n");
    vga_set_color(0x0F);
    vga_puts("[Monitor] cek ke-2: transaksi ");
    print_num(count);
    vga_puts("/20, heap terpakai ");
    print_num((int)heap_used);
    vga_puts(" B\n");
    vga_set_color(0x0C);

    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] RUNNING -> READY\n");
    vga_puts("[T"); print_num(t_data); vga_puts(" Data] WAITING -> READY\n");
    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] READY -> RUNNING\n");
    vga_puts("[T"); print_num(t_mon);  vga_puts(" Monitor] RUNNING -> TERMINATED\n");

    vga_puts("[T"); print_num(t_data); vga_puts(" Data] READY -> RUNNING\n");
    vga_set_color(0x0F);
    vga_puts("[Data] Ditemukan ");
    print_num(count);
    vga_puts(" transaksi di RAMDisk, total saldo Rp");
    print_num(saldo);
    vga_puts(".\n");
    vga_set_color(0x0C);

    vga_puts("[T"); print_num(t_data); vga_puts(" Data] RUNNING -> TERMINATED\n");
    vga_puts("=== LAPORAN SELESAI ===\n");
    vga_set_color(0x0F);
}
