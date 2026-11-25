#include "kernel.h"
#include "thread.h"
#include "task.h"
#include "sync.h"
#include "vga.h"

/* =========================================================================
 * DEMO PENGELOLAAN THREAD (CPMK 2) -- memakai TCB dan scheduler sungguhan.
 * Status yang dicetak selalu dibaca langsung dari tcb->state.
 * ========================================================================= */

static mutex_t demo_mutex;
static volatile int demo_a_done = 0;
static volatile int demo_b_done = 0;

void thread_init(void) {
    demo_a_done = 0;
    demo_b_done = 0;
}

static const char *demo_state_name(task_state_t st) {
    switch (st) {
        case TASK_NEW:        return "NEW";
        case TASK_READY:      return "READY";
        case TASK_RUNNING:    return "RUNNING";
        case TASK_WAITING:    return "WAITING";
        case TASK_TERMINATED: return "TERMINATED";
        default:              return "?";
    }
}

static void demo_print_state(int id, const char *note) {
    tcb_t *t = task_get(id);

    vga_puts("[Thread ");
    vga_put_int(id + 1);
    vga_puts("] ");
    vga_puts(t && t->name[0] ? t->name : "?");
    vga_puts(" -> ");
    vga_set_color(0x0E);
    vga_puts(t ? demo_state_name(t->state) : "?");
    vga_set_color(0x09);
    if (note) {
        vga_puts (" (");
        vga_puts (note);
        vga_putc (')');
    }
    vga_putc('\n');
}

static void demo_worker_a(void) {
    int id = task_current_id();

    demo_print_state(id, "tugas A: menghitung");
    task_yield();                       /* beri giliran ke Thread 2 */

    demo_print_state(id, "menunggu mutex dari Thread 2");
    mutex_lock(&demo_mutex);            /* benar-benar memblokir */
    demo_print_state(id, "mendapat mutex, lanjut");
    mutex_unlock(&demo_mutex);

    demo_a_done = 1;
    task_exit();
}

static void demo_worker_b(void) {
    int id = task_current_id();

    demo_print_state(id, "tugas B: membersihkan memori");
    mutex_lock(&demo_mutex);
    task_yield();                       /* Thread 1 kini menunggu mutex */

    demo_print_state(id, "memegang mutex");
    mutex_unlock(&demo_mutex);          /* bangunkan Thread 1 */

    demo_b_done = 1;
    task_exit();
}

void run_thread_demo(void) {
    vga_set_color(0x09);
    vga_puts("\n--- DEMO PENGELOLAAN THREAD (TCB + SCHEDULER NYATA) ---\n\n");

    mutex_init(&demo_mutex);
    demo_a_done = 0;
    demo_b_done = 0;
    task_reset();

    int a = task_create_pending("Task-1", demo_worker_a);
    int b = task_create_pending("Task-2", demo_worker_b);
    if (a < 0 || b < 0) {
        vga_puts("[!] TCB penuh, demo dibatalkan\n");
        task_reset();
        return;
    }

    /* NEW -> READY dibaca dari TCB, bukan string hardcode. */
    demo_print_state(a, "dibuat");
    demo_print_state(b, "dibuat");
    task_start(a);
    task_start(b);
    demo_print_state(a, "entrant antrean");
    demo_print_state(b, "entrant antrean");

    task_run_until_blocked();

    vga_puts("Status akhir (dibaca dari TCB):\n");
    demo_print_state(a, "selesai");
    demo_print_state(b, "selesai");
    vga_puts("Flag tugas: A=");
    vga_put_int(demo_a_done);
    vga_puts(" B=");
    vga_put_int(demo_b_done);
    vga_putc('\n');

    if (demo_a_done && demo_b_done) {
        vga_set_color(0x0A);
        vga_puts("[LULUS] kedua thread berjalan melalui scheduler\n");
        vga_set_color(0x09);
    }
    task_reset();
}

/* =========================================================================
 * KODE TAMBAHAN: SIMULASI DEADLOCK KASIR (perintah deadlock-sim)
 * ========================================================================= */

typedef struct {
    int locked;
    int owner_id;
} mini_mutex_t;

static mini_mutex_t mtx_brankas = {0, 0};
static mini_mutex_t mtx_buku    = {0, 0};

static void try_lock(mini_mutex_t *mtx, int thread_id,
                     const char *resource_name) {
    vga_puts("   -> [Thread ");
    vga_put_int(thread_id);
    vga_puts("] Mencoba akses ");
    vga_puts(resource_name);
    vga_puts("...\n");

    if (mtx->locked == 1) {
        vga_set_color(0x0C);
        vga_puts("      [!] DITOLAK: Sedang dikunci oleh Thread ");
        vga_put_int(mtx->owner_id);
        vga_puts(" (Masuk state WAITING)\n");
        vga_set_color(0x09);
    } else {
        mtx->locked   = 1;
        mtx->owner_id = thread_id;
        vga_set_color(0x0A);
        vga_puts("      [v] BERHASIL: Kunci dipegang oleh Thread ");
        vga_put_int(thread_id);
        vga_puts("\n");
        vga_set_color(0x09);
    }
}

void run_deadlock_demo(void) {
    vga_puts("\n=== SIMULASI DEADLOCK (CPMK 5) ===\n");
    vga_puts("Kasus: 2 Kasir berebut Brankas (M1) dan Buku Kas (M2)\n\n");

    mtx_brankas.locked = 0;
    mtx_buku.locked    = 0;

    vga_set_color(0x0E);
    vga_puts("[SKENARIO 1: TERJADINYA DEADLOCK]\n");
    vga_set_color(0x09);
    try_lock(&mtx_brankas, 1, "Brankas (M1)");
    try_lock(&mtx_buku, 2, "Buku Kas (M2)");
    try_lock(&mtx_buku, 1, "Buku Kas (M2)");
    try_lock(&mtx_brankas, 2, "Brankas (M1)");

    vga_set_color(0x0C);
    vga_puts("\n[!!] FATAL ERROR: DEADLOCK TERJADI (Circular Wait) [!!]\n");
    vga_puts("     Kedua thread saling mengunci dan menunggu selamanya.\n\n");
    vga_set_color(0x09);

    vga_set_color(0x0E);
    vga_puts("[SKENARIO 2: SOLUSI (RESOURCE ORDERING)]\n");
    vga_set_color(0x09);
    vga_puts("Aturan Baru: Semua Thread HARUS antre Brankas (M1) dulu!\n");

    mtx_brankas.locked = 0;
    mtx_buku.locked    = 0;

    try_lock(&mtx_brankas, 1, "Brankas (M1)");
    try_lock(&mtx_brankas, 2, "Brankas (M1)");
    vga_puts("\n   (Karena Thread 2 menunggu, Thread 1 bisa lanjut)\n");
    try_lock(&mtx_buku, 1, "Buku Kas (M2)");
    vga_puts("\n   -> [Thread 1] Selesai transaksi! Melepas M1 & M2...\n");
    mtx_brankas.locked = 0;
    mtx_buku.locked    = 0;
    vga_puts("\n   (Sekarang Thread 2 bangun dan bisa masuk)\n");
    try_lock(&mtx_brankas, 2, "Brankas (M1)");
    try_lock(&mtx_buku, 2, "Buku Kas (M2)");

    vga_set_color(0x0A);
    vga_puts("\n[v] SISTEM AMAN: Deadlock berhasil dicegah!\n");
    vga_set_color(0x09);
}
