#include "task.h"
#include "kernel.h"
#include "vga.h"
#include "memory.h"
#include "finance.h"
#include "timer.h"
#include "irq.h"
#include "idt.h"
#include "sync.h"
#include "rr.h"

tcb_t  task_table[MAX_TASKS];
int    task_count   = 0;
tcb_t *current_task = NULL;

/* Jejak context switch (dipakai perintah 'preempt-test'). */
#define TRACE_LEN 64
static uint8_t sched_trace[TRACE_LEN];
static int     sched_trace_pos = 0;

/* ---- local helpers (no libc) ------------------------------------------- */

static void k_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static inline void yield_interrupt(void) {
    __asm__ volatile ("int $0x30");
}

/* ---- context switch (asm) ---------------------------------------------- */

extern void context_switch(tcb_t *old, tcb_t *new);

/* ---- scheduler --------------------------------------------------------- */

void schedule(void) {
    uint32_t flags = irq_save_disable();   /* atomik: pilih task + switch */
    tcb_t *old = current_task;
    int start = old ? old->id + 1 : 1;
    int next  = -1;

    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (idx == 0) continue;
        if (idx < task_count && task_table[idx].state == TASK_READY) {
            next = idx;
            break;
        }
    }

    if (next == -1) {
        if (!old || old->id == 0) goto done;
        if (old->state == TASK_RUNNING) goto done;
        task_table[0].state = TASK_RUNNING;
        current_task = &task_table[0];
        sched_trace[sched_trace_pos % TRACE_LEN] = 0;  /* kembali ke T0 */
        sched_trace_pos++;
        context_switch(old, &task_table[0]);
        goto done;
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

    sched_trace[sched_trace_pos % TRACE_LEN] = (uint8_t)new->id;
    sched_trace_pos++;

    if (old != new) {
        context_switch(old, new);
    }

done:
    /* Flag milik task yang sedang berjalan dipulihkan di task tersebut. */
    irq_restore(flags);
}

void task_clear_trace(void) {
    uint32_t flags = irq_save_disable();
    sched_trace_pos = 0;
    for (int i = 0; i < TRACE_LEN; i++) sched_trace[i] = 0;
    irq_restore(flags);
}

void task_print_trace(void) {
    uint32_t flags = irq_save_disable();
    int n = sched_trace_pos < TRACE_LEN ? sched_trace_pos : TRACE_LEN;
    int start = sched_trace_pos <= TRACE_LEN ? 0 : sched_trace_pos % TRACE_LEN;
    irq_restore(flags);

    vga_set_color(0x0E);
    vga_puts("Jejak context switch (T0=kernel, T1/T2=worker):\n");
    vga_set_color(0x0F);
    for (int i = 0; i < n; i++) {
        vga_puts("T");
        vga_put_int((int)sched_trace[(start + i) % TRACE_LEN]);
        vga_putc(' ');
        if ((i + 1) % 16 == 0) vga_putc('\n');
    }
    vga_putc('\n');
}

/* ---- public API --------------------------------------------------------- */

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_table[i].state      = TASK_TERMINATED;
        task_table[i].id         = i;
        task_table[i].name[0]    = '\0';
        task_table[i].esp        = 0;
        task_table[i].ebp        = 0;
        task_table[i].eip        = 0;
        task_table[i].start_tick = timer_get_ticks();
        task_table[i].cpu_ticks  = 0;
        task_table[i].wait_res   = NULL;
        task_table[i].wait_kind  = WAIT_NONE;
    }

    task_table[0].state = TASK_RUNNING;
    k_strcpy(task_table[0].name, "kernel", (int)sizeof(task_table[0].name));

    task_count   = 1;
    current_task = &task_table[0];
    task_clear_trace();
}

int task_create_pending(const char *name, void (*entry)(void)) {
    if (task_count >= MAX_TASKS) {
        vga_puts("[TASK] ERROR: task table full\n");
        return -1;
    }

    tcb_t *t = &task_table[task_count];
    t->id    = task_count++;
    t->state = TASK_NEW;
    t->wait_res  = NULL;
    t->wait_kind = WAIT_NONE;
    k_strcpy(t->name, name, (int)sizeof(t->name));

    /*
     * Susun frame agar context_switch() bisa me-RET ke _isr_restore, lalu
     * epilogue isr.asm melakukan popa + iretd masuk ke entry (ring0, IF=1).
     *
     * Layout dari t->esp (alamat terendah ke tertinggi):
     *   edi/esi/ebp/ebx  -> 4 register yang di-pop context_switch
     *   &_isr_restore    -> target ret context_switch
     *   gs/fs/es/ds      -> pop oleh _isr_restore
     *   edi..eax         -> popa (esp dummy dilewati)
     *   int_no/err_code  -> add esp, 8
     *   eip/cs/eflags    -> iretd (CPU tidak menumpuk SS/ESP untuk ring0)
     *   &task_exit       -> ret otomatis saat entry function selesai
     */
    extern uint32_t isr_restore;      /* dari boot/isr.asm */

    uint32_t *sp = (uint32_t *)(t->stack + STACK_SIZE);
    /* Return address untuk entry(): jika fungsi worker return tanpa memanggil
     * task_exit(), ia tetap di-terminasi dengan benar (tidak jatuh ke alamat
     * acak di atas stack). */
    *--sp = (uint32_t)&task_exit;
    *--sp = 0x202;                    /* eflags: IF=1 */
    *--sp = 0x08;                     /* cs kernel code */
    *--sp = (uint32_t)entry;          /* eip */
    *--sp = 0;                        /* err_code */
    *--sp = 0;                        /* int_no */
    *--sp = 0;                        /* eax */
    *--sp = 0;                        /* ecx */
    *--sp = 0;                        /* edx */
    *--sp = 0;                        /* ebx */
    *--sp = 0;                        /* esp (diabaikan popa) */
    *--sp = 0;                        /* ebp */
    *--sp = 0;                        /* esi */
    *--sp = 0;                        /* edi */
    *--sp = 0x10;                     /* ds */
    *--sp = 0x10;                     /* es */
    *--sp = 0x10;                     /* fs */
    *--sp = 0x10;                     /* gs */
    *--sp = (uint32_t)&isr_restore;   /* ret target setelah pop 4 regs */
    *--sp = 0;                        /* ebx (context_switch) */
    *--sp = 0;                        /* ebp (context_switch) */
    *--sp = 0;                        /* esi (context_switch) */
    *--sp = 0;                        /* edi (context_switch) */

    t->esp = (uint32_t)sp;
    t->ebp = 0;
    t->eip = (uint32_t)entry;
    t->start_tick = timer_get_ticks();
    t->cpu_ticks  = 0;

    return t->id;                     /* masih TASK_NEW */
}

void task_start(int id) {
    uint32_t flags = irq_save_disable();
    if (id > 0 && id < task_count && task_table[id].state == TASK_NEW) {
        task_table[id].state = TASK_READY;
    }
    irq_restore(flags);
}

int task_create(const char *name, void (*entry)(void)) {
    int id = task_create_pending(name, entry);
    if (id >= 0) task_start(id);
    return id;
}

void task_yield(void) {
    yield_interrupt();
}

void task_exit(void) {
    if (current_task) {
        current_task->state     = TASK_TERMINATED;
        current_task->wait_res  = NULL;
        current_task->wait_kind = WAIT_NONE;
    }
    yield_interrupt();
    for (;;) { __asm__ volatile ("hlt"); }
}

/* ---- primitif blocking -------------------------------------------------- */

int task_current_id(void) {
    return current_task ? current_task->id : 0;
}

void task_block_on(void *res, wait_kind_t kind) {
    if (!current_task) return;

    uint32_t flags = irq_save_disable();
    current_task->wait_res  = res;
    current_task->wait_kind = kind;
    current_task->state     = TASK_WAITING;
    irq_restore(flags);

    yield_interrupt();
}

void task_wake(int id) {
    if (id <= 0 || id >= task_count) return;

    uint32_t flags = irq_save_disable();
    tcb_t *t = &task_table[id];
    if (t->state == TASK_WAITING) {
        t->state      = TASK_READY;
        t->wait_res   = NULL;
        t->wait_kind  = WAIT_NONE;
    }
    irq_restore(flags);
}

int task_ready_count(void) {
    int n = 0;
    for (int i = 1; i < task_count; i++) {
        if (task_table[i].state == TASK_READY) n++;
    }
    return n;
}

int task_waiting_count(void) {
    int n = 0;
    for (int i = 1; i < task_count; i++) {
        if (task_table[i].state == TASK_WAITING) n++;
    }
    return n;
}

int task_run_until_blocked(void) {
    while (task_ready_count() > 0) {
        yield_interrupt();
    }
    return task_waiting_count();
}

void task_reset(void) {
    uint32_t flags = irq_save_disable();
    for (int i = 1; i < task_count; i++) {
        task_table[i].state      = TASK_TERMINATED;
        task_table[i].wait_res   = NULL;
        task_table[i].wait_kind  = WAIT_NONE;
        task_table[i].name[0]    = '\0';
        task_table[i].start_tick = timer_get_ticks();
        task_table[i].cpu_ticks  = 0;
    }
    task_count            = 1;
    task_table[0].state   = TASK_RUNNING;
    current_task          = &task_table[0];
    irq_restore(flags);
    task_clear_trace();
}

int task_total(void) {
    return task_count;
}

tcb_t *task_get(int id) {
    if (id < 0 || id >= task_count) return NULL;
    return &task_table[id];
}

void task_print_all(void) {
    vga_set_color(0x0C);
    vga_puts("ID  NAME             STATE\n");
    vga_puts("--- ---------------- ----\n");
    vga_set_color(0x0F);
    for (int i = 0; i < task_count; i++) {
        tcb_t *t = &task_table[i];
        vga_put_int(t->id);
        vga_puts("   ");
        vga_puts(t->name);
        int n_len = 0;
        while (t->name[n_len]) n_len++;
        for (int p = n_len; p < 16; p++) vga_puts(" ");
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
 * REGISTRY THREAD (dipakai perintah 'threads')
 * ========================================================================= */

#define MAX_REG_THREADS 32

typedef struct {
    int  t_num;
    int  tcb_id;
    char name[16];
    char status[16];
} reg_thread_item_t;

static reg_thread_item_t reg_list[MAX_REG_THREADS];
static int reg_count = 0;
static int next_thread_id = 1;

static int reg_add(const char *name, int tcb_id) {
    if (reg_count >= MAX_REG_THREADS) return -1;
    int id = next_thread_id++;
    reg_thread_item_t *item = &reg_list[reg_count++];
    item->t_num  = id;
    item->tcb_id = tcb_id;
    k_strcpy(item->name, name, (int)sizeof(item->name));
    k_strcpy(item->status, "NEW", (int)sizeof(item->status));
    return id;
}

int task_register_logger_start(void) {
    return reg_add("Logger", -1);
}

void task_register_logger_stop(int t_num) {
    for (int i = 0; i < reg_count; i++) {
        if (reg_list[i].t_num == t_num) {
            k_strcpy(reg_list[i].status, "TERMINATED",
                     (int)sizeof(reg_list[i].status));
            break;
        }
    }
}

static const char *state_label(task_state_t st) {
    switch (st) {
        case TASK_NEW:        return "NEW";
        case TASK_READY:      return "READY";
        case TASK_RUNNING:    return "RUNNING";
        case TASK_WAITING:    return "WAITING";
        case TASK_TERMINATED: return "TERMINATED";
        default:              return "?";
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
        const char *status = reg_list[i].status;
        tcb_t *t = task_get(reg_list[i].tcb_id);
        if (t) status = state_label(t->state);

        vga_puts("T");
        vga_put_int(reg_list[i].t_num);
        vga_puts("   ");
        vga_puts(reg_list[i].name);
        int n_len = 0;
        while (reg_list[i].name[n_len]) n_len++;
        for (int p = n_len; p < 9; p++) vga_puts(" ");
        vga_puts(status);
        vga_puts("\n");
    }
}

/* =========================================================================
 * UJI PREEMPTION NYATA (tanpa task_yield manual)
 * ========================================================================= */

static volatile uint32_t preempt_count_a = 0;
static volatile uint32_t preempt_count_b = 0;

/* Loop sengaja dibuat cukup panjang agar beberapa quantum habis dan
 * preemption oleh IRQ0 benar-benar teramati (bukan sekadar task_exit). */
#define PREEMPT_WORK 2000000u

static void preempt_worker_a(void) {
    for (volatile uint32_t i = 0; i < PREEMPT_WORK; i++) {
        preempt_count_a++;
    }
    task_exit();
}

static void preempt_worker_b(void) {
    for (volatile uint32_t i = 0; i < PREEMPT_WORK; i++) {
        preempt_count_b++;
    }
    task_exit();
}

void task_preempt_demo(void) {
    vga_set_color(0x0C);
    vga_puts("\n=== UJI PREEMPTION TIMER IRQ0 (tanpa task_yield) ===\n");
    vga_set_color(0x0F);

    preempt_count_a = 0;
    preempt_count_b = 0;
    task_reset();
    task_clear_trace();

    int a = task_create("cpu-a", preempt_worker_a);
    int b = task_create("cpu-b", preempt_worker_b);
    vga_puts("Thread CPU-bound dibuat: A=T");
    vga_put_int(a);
    vga_puts(" B=T");
    vga_put_int(b);
    vga_puts(" | quantum=");
    vga_put_int((int)rr_get_quantum());
    vga_puts(" tick\n");

    task_run_until_blocked();

    vga_puts("  iterasi A=");
    vga_put_int((int)preempt_count_a);
    vga_puts("  iterasi B=");
    vga_put_int((int)preempt_count_b);
    vga_puts("\n");

    tcb_t *ta = task_get(a);
    tcb_t *tb = task_get(b);
    if (ta && tb && preempt_count_a > 0 && preempt_count_b > 0 &&
        ta->state == TASK_TERMINATED && tb->state == TASK_TERMINATED) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] IRQ0 memotong kedua thread bergantian (preemptive)\n");
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] preemption tidak teramati\n");
        vga_set_color(0x0F);
    }
    task_print_trace();
    vga_puts("  total context switch: ");
    vga_put_int(sched_trace_pos);
    vga_puts("\n");
    task_reset();
}

/* =========================================================================
 * LAPORAN KAS OLEH 2 THREAD NYATA (dibaca dari TCB)
 * ========================================================================= */

static mutex_t   lap_mutex;
static volatile int lap_count = 0;
static volatile int lap_saldo = 0;

static void lap_print_state(int id, const char *extra) {
    tcb_t *t = task_get(id);
    vga_puts("[T");
    vga_put_int(id);
    vga_puts(" ");
    vga_puts(t && t->name[0] ? t->name : "?");
    vga_puts("] ");
    vga_puts(t ? state_label(t->state) : "?");
    if (extra) {
        vga_puts(" ");
        vga_puts(extra);
    }
    vga_puts("\n");
}

static void laporan_data_task(void) {
    int id = task_current_id();

    lap_print_state(id, "membaca kas.dat dari RAMDisk");
    {
        int c = 0, s = 0;
        finance_get_summary(&c, &s);
        lap_count = c;
        lap_saldo = s;
    }

    /* Beri kesempatan Monitor mengambil mutex, lalu blokir nyata. */
    task_yield();
    lap_print_state(id, "menunggu mutex laporan");
    mutex_lock(&lap_mutex);
    lap_print_state(id, "mempunyai mutex laporan");
    mutex_unlock(&lap_mutex);

    lap_print_state(id, "selesai menghitung");
    task_exit();
}

static void laporan_monitor_task(void) {
    int id = task_current_id();

    lap_print_state(id, "mulai monitor");
    mutex_lock(&lap_mutex);
    lap_print_state(id, "memegang mutex (cek 1 & 2)");
    vga_set_color(0x0F);
    vga_puts("[Monitor] transaksi ");
    vga_put_int(lap_count);
    vga_puts(" | saldo Rp");
    vga_put_int(lap_saldo);
    vga_puts(" | heap ");
    vga_put_int((int)memory_get_used());
    vga_puts(" B\n");
    mutex_unlock(&lap_mutex);
    lap_print_state(id, "selesai");
    task_exit();
}

void task_run_laporan(void) {
    int t_data, t_mon;

    mutex_init(&lap_mutex);
    lap_count = 0;
    lap_saldo = 0;
    task_reset();

    vga_set_color(0x0C);
    vga_puts("=== LAPORAN KAS (2 kernel thread nyata) ===\n");
    vga_set_color(0x0F);

    t_data = task_create_pending("Data", laporan_data_task);
    t_mon  = task_create_pending("Monitor", laporan_monitor_task);
    if (t_data < 0 || t_mon < 0) {
        task_reset();
        return;
    }
    reg_add("Data", t_data);
    reg_add("Monitor", t_mon);

    lap_print_state(t_data, "NEW");
    lap_print_state(t_mon, "NEW");
    task_start(t_data);
    task_start(t_mon);
    tcb_t *d = task_get(t_data);
    tcb_t *m = task_get(t_mon);
    vga_puts("[T");
    vga_put_int(t_data);
    vga_puts(" Data] ");
    vga_puts(d ? state_label(d->state) : "?");
    vga_puts(" -> READY\n");
    vga_puts("[T");
    vga_put_int(t_mon);
    vga_puts(" Monitor] ");
    vga_puts(m ? state_label(m->state) : "?");
    vga_puts(" -> READY\n");

    task_run_until_blocked();

    d = task_get(t_data);
    m = task_get(t_mon);
    vga_puts("Status akhir dari TCB: T");
    vga_put_int(t_data);
    vga_puts("=");
    vga_puts(d ? state_label(d->state) : "?");
    vga_puts(" T");
    vga_put_int(t_mon);
    vga_puts("=");
    vga_puts(m ? state_label(m->state) : "?");
    vga_puts("\n=== LAPORAN SELESAI ===\n");

    /* Simpan status akhir ke registry sebelum task_reset() menghapus TCB,
     * supaya perintah 'threads' tetap menampilkan hasil yang benar. */
    for (int i = 0; i < reg_count; i++) {
        if (reg_list[i].tcb_id > 0) {
            tcb_t *t = task_get(reg_list[i].tcb_id);
            if (t) {
                k_strcpy(reg_list[i].status, state_label(t->state),
                         (int)sizeof(reg_list[i].status));
            }
        }
    }
    task_reset();
}
