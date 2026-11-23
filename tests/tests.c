/*
 * kernel/tests/tests.c -- Test harness (Minggu 3, PIC 2 / Rasya)
 *
 *  test_sync()      : mutex -> tidak ada lost update
 *  test_race()      : race condition tanpa mutex -> lost update
 *  test_deadlock()  : deadlock nyata + deteksi wait-for graph + pencegahan
 *  test_semaphore() : counting semaphore (producer / consumer)
 *  test_protect()   : proteksi hak akses + audit log + critical section
 */

#include "tests.h"
#include "task.h"
#include "sync.h"
#include "protect.h"
#include "thread.h"
#include "kernel.h"
#include "vga.h"
#include "memory.h"
#include "vfs.h"

#define TEST_ITER 5

/* ===================== 1. UJI MUTEX (aman) ===================== */

static mutex_t      test_mutex;
static volatile int safe_counter = 0;

static void worker_safe(void) {
    for (int i = 0; i < TEST_ITER; i++) {
        mutex_lock(&test_mutex);
        int tmp = safe_counter;
        task_yield();                  /* preempt di dalam critical section */
        safe_counter = tmp + 1;
        mutex_unlock(&test_mutex);
    }
}

void test_sync(void) {
    vga_set_color(0x0C);
    vga_puts("\n=== UJI MUTEX (aman) ===\n");
    vga_set_color(0x0F);
    vga_puts("2 task menambah penghitung yang sama, dilindungi mutex\n");

    mutex_init(&test_mutex);
    safe_counter = 0;
    task_reset();

    int a = task_create("counter-a", worker_safe);
    int b = task_create("counter-b", worker_safe);
    vga_puts("  task dibuat: A=");
    vga_put_int(a);
    vga_puts("  B=");
    vga_put_int(b);
    vga_puts("\n");

    task_run_until_blocked();

    vga_puts("  hasil    : ");
    vga_put_int(safe_counter);
    vga_puts("   ekspektasi: ");
    vga_put_int(2 * TEST_ITER);
    vga_puts("\n");
    if (safe_counter == 2 * TEST_ITER) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] tidak ada lost update, mutex bekerja\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] masih ada lost update\n");
    }
    vga_set_color(0x0F);
    task_reset();
}

/* ===================== 2. UJI RACE CONDITION ===================== */

static volatile int race_counter = 0;

static void worker_race(void) {
    for (int i = 0; i < TEST_ITER; i++) {
        int tmp = race_counter;        /* READ  */
        task_yield();                  /* preemption di tengah critical section */
        race_counter = tmp + 1;        /* WRITE */
    }
}

void test_race(void) {
    vga_set_color(0x0C);
    vga_puts("\n=== UJI RACE CONDITION ===\n");
    vga_set_color(0x0F);
    vga_puts("2 task akses data bersama TANPA mutex\n");

    race_counter = 0;
    task_reset();
    task_create("race-a", worker_race);
    task_create("race-b", worker_race);

    task_run_until_blocked();

    vga_puts("  hasil    : ");
    vga_put_int(race_counter);
    vga_puts("   ekspektasi: ");
    vga_put_int(2 * TEST_ITER);
    vga_puts("\n");
    if (race_counter < 2 * TEST_ITER) {
        vga_set_color(0x0A);
        vga_puts("  [TERBUKTI] race condition terjadi (lost update ");
        vga_put_int(2 * TEST_ITER - race_counter);
        vga_puts(")\n");
    } else {
        vga_set_color(0x0E);
        vga_puts("  [TIDAK TERBUKTI] jadwal eksekusi tidak interleaving\n");
    }
    vga_set_color(0x0F);
    vga_puts("  Bandingkan dengan perintah 'sync' (mutex)\n");
    task_reset();
}

/* ===================== 3. UJI DEADLOCK ===================== */

static mutex_t dl_m1, dl_m2;

static void dl_task1(void) {
    mutex_lock(&dl_m1);
    vga_puts("  T1 ambil M1 (brankas)\n");
    task_yield();
    vga_puts("  T1 minta M2 (buku kas) ...\n");
    mutex_lock(&dl_m2);                 /* memblokir */
    vga_puts("  T1 dapat M2\n");
    mutex_unlock(&dl_m2);
    mutex_unlock(&dl_m1);
}

static void dl_task2(void) {
    mutex_lock(&dl_m2);
    vga_puts("  T2 ambil M2 (buku kas)\n");
    task_yield();
    vga_puts("  T2 minta M1 (brankas) ...\n");
    mutex_lock(&dl_m1);                 /* memblokir */
    vga_puts("  T2 dapat M1\n");
    mutex_unlock(&dl_m1);
    mutex_unlock(&dl_m2);
}

/* Pencegahan 1: resource ordering (semua task ambil M1 lalu M2) */
static void ord_task1(void) {
    mutex_lock(&dl_m1);
    vga_puts("  T1 ambil M1 (urutan baku)\n");
    task_yield();
    mutex_lock(&dl_m2);
    vga_puts("  T1 ambil M2 (urutan baku)\n");
    mutex_unlock(&dl_m2);
    mutex_unlock(&dl_m1);
}

static void ord_task2(void) {
    vga_puts("  T2 minta M1 (urutan baku) ...\n");
    mutex_lock(&dl_m1);                 /* menunggu, tidak deadlock */
    vga_puts("  T2 dapat M1\n");
    mutex_unlock(&dl_m1);
}

/* Pencegahan 2: trylock + backoff (tidak pernah menunggu) */
static void try_task1(void) {
    mutex_lock(&dl_m1);
    vga_puts("  T1 ambil M1\n");
    task_yield();
    mutex_lock(&dl_m2);
    vga_puts("  T1 ambil M2\n");
    mutex_unlock(&dl_m2);
    mutex_unlock(&dl_m1);
}

static void try_task2(void) {
    if (!mutex_trylock(&dl_m1)) {
        vga_puts("  T2 trylock M1 gagal -> backoff, tidak menunggu\n");
        return;
    }
    vga_puts("  T2 dapat M1\n");
    mutex_unlock(&dl_m1);
}

void test_deadlock(void) {
    vga_set_color(0x0C);
    vga_puts("\n=== UJI DEADLOCK ===\n");
    vga_set_color(0x0F);

    /* --- skenario 1: deadlock terjadi --- */
    vga_puts("[1] Dua task mengambil dua mutex dengan urutan terbalik\n");
    mutex_init(&dl_m1);
    mutex_init(&dl_m2);
    task_reset();
    task_create("dead-t1", dl_task1);
    task_create("dead-t2", dl_task2);

    int stuck = task_run_until_blocked();
    vga_puts("  taskmacet (WAITING): ");
    vga_put_int(stuck);
    vga_puts("\n");

    if (sync_detect_deadlock()) {
        vga_set_color(0x0C);
        vga_puts("  [TERDETEKSI] deadlock terjadi:\n");
        vga_set_color(0x0F);
        sync_print_deadlock();
    } else {
        vga_set_color(0x0E);
        vga_puts("  [TIDAK TERDETEKSI] tidak ada siklus wait-for\n");
        vga_set_color(0x0F);
    }
    task_reset();

    /* --- skenario 2: pencegahan dengan resource ordering --- */
    vga_puts("\n[2] Pencegahan: semua task mengikuti urutan sumber daya (M1 -> M2)\n");
    mutex_init(&dl_m1);
    mutex_init(&dl_m2);
    task_reset();
    task_create("ord-t1", ord_task1);
    task_create("ord-t2", ord_task2);
    task_run_until_blocked();
    if (sync_detect_deadlock()) {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] deadlock masih terjadi\n");
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] deadlock dicegah, semua task selesai\n");
        vga_set_color(0x0F);
    }
    task_reset();

    /* --- skenario 3: pencegahan dengan trylock + backoff --- */
    vga_puts("\n[3] Pencegahan: trylock + backoff (tidak pernah menunggu)\n");
    mutex_init(&dl_m1);
    mutex_init(&dl_m2);
    task_reset();
    task_create("try-t1", try_task1);
    task_create("try-t2", try_task2);
    task_run_until_blocked();
    if (sync_detect_deadlock()) {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] deadlock masih terjadi\n");
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] deadlock dicegah, semua task selesai\n");
        vga_set_color(0x0F);
    }
    task_reset();
}

/* ===================== 4. UJI COUNTING SEMAPHORE ===================== */

static semaphore_t  sem_empty, sem_full;
static volatile int prod_buffer = 0;
static volatile int prod_count = 0, cons_count = 0;

static void producer_task(void) {
    for (int i = 0; i < TEST_ITER; i++) {
        semaphore_wait(&sem_empty);     /* slot buffer kosong */
        prod_buffer = i;
        prod_count++;
        vga_puts("  [Prod] barang ");
        vga_put_int(i);
        vga_puts(" masuk buffer\n");
        semaphore_signal(&sem_full);    /* ada barang siap diambil */
    }
}

static void consumer_task(void) {
    for (int i = 0; i < TEST_ITER; i++) {
        semaphore_wait(&sem_full);      /* barang tersedia */
        int item = prod_buffer;
        cons_count++;
        vga_puts("  [Cons] barang ");
        vga_put_int(item);
        vga_puts(" diterima\n");
        semaphore_signal(&sem_empty);   /* slot buffer kosong lagi */
    }
}

void test_semaphore(void) {
    vga_set_color(0x0C);
    vga_puts("\n=== UJI COUNTING SEMAPHORE (producer/consumer) ===\n");
    vga_set_color(0x0F);

    semaphore_init(&sem_empty, 1);     /* 1 slot buffer kosong */
    semaphore_init(&sem_full, 0);      /* belum ada barang */
    prod_count = cons_count = 0;
    task_reset();
    task_create("producer", producer_task);
    task_create("consumer", consumer_task);

    task_run_until_blocked();

    vga_puts("  produced=");
    vga_put_int(prod_count);
    vga_puts("  consumed=");
    vga_put_int(cons_count);
    vga_puts("\n");
    if (prod_count == TEST_ITER && cons_count == TEST_ITER) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] semua barang diproduksi & dikonsumsi\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] ada barang yang tertinggal\n");
    }
    vga_set_color(0x0F);
    task_reset();
}

/* ===================== 5. UJI PROTEKSI HAK AKSES ===================== */

void test_protect(void) {
    os_mode_t saved_mode = protect_get_mode();

    vga_set_color(0x0C);
    vga_puts("\n=== UJI PROTEKSI HAK AKSES ===\n");
    vga_set_color(0x0F);

    vga_puts("Mode awal: ");
    vga_puts(saved_mode == MODE_KERNEL ? "KERNEL" : "USER");
    vga_puts("\nIzin awal:\n");
    protect_print_perms();

    /* matikan satu izin, lalu coba akses */
    protect_set_perm_by_name("hapus", 0);
    vga_puts("Setelah 'izin hapus off':\n");
    protect_print_perms();

    if (protect_require(PERM_HAPUS, "hapus transaksi")) {
        vga_puts("  [LULUS] hapus diizinkan\n");
    } else {
        vga_puts("  [DITOLAK] hapus diblokir proteksi\n");
    }
    if (protect_require(PERM_LIHAT, "lihat transaksi")) {
        vga_puts("  [LULUS] lihat diizinkan\n");
    } else {
        vga_puts("  [DITOLAK] lihat diblokir proteksi\n");
    }

    /* mode KERNEL: akses penuh tanpa perlu izin per fitur */
    protect_set_mode(MODE_KERNEL);
    vga_puts("Mode KERNEL: hapus = ");
    vga_puts(protect_check_perm(PERM_HAPUS) ? "diizinkan (privilege penuh)" : "ditolak");
    vga_puts("\n");
    protect_set_mode(saved_mode);  /* kembalikan mode sesuai pemanggil */

    /* bagian kritis */
    protect_enter_critical();
    protect_enter_critical();
    protect_exit_critical();
    protect_exit_critical();
    vga_puts("Critical section (nested) selesai, tidak ada interupsi)\n");

    vga_puts("Audit log:\n");
    protect_print_audit();

    /* pulihkan izin */
    protect_set_perm_by_name("hapus", 1);
}

/* ===================== modul PIC lain ===================== */

void test_tasks(void) {
    vga_puts("[TEST] Uji coba Task & Scheduler...\n");
    run_thread_demo();
}

void test_sched(void) {
    vga_puts("[TEST] Uji coba Thread Lifecycle (CPMK 2):\n");
    run_thread_demo();
}

void test_deadlock_sim(void) {
    run_deadlock_demo();
}

void test_malloc(void) {
    size_t total = memory_get_total();
    size_t before = memory_get_used();
    size_t during, after;
    uint8_t *a = (uint8_t *)kmalloc(64);
    uint8_t *b = (uint8_t *)kmalloc(128);
    int ok = (a != NULL && b != NULL);

    vga_set_color(0x0C);
    vga_puts("\n=== UJI ALOKASI MEMORI (kmalloc/kfree) ===\n");
    vga_set_color(0x0F);

    if (a != NULL) {
        for (int i = 0; i < 64; i++) a[i] = (uint8_t)(i & 0xFF);
        for (int i = 0; i < 64; i++) {
            if (a[i] != (uint8_t)(i & 0xFF)) ok = 0;
        }
    }
    if (b != NULL) {
        for (int i = 0; i < 128; i++) b[i] = (uint8_t)((i * 3) & 0xFF);
        for (int i = 0; i < 128; i++) {
            if (b[i] != (uint8_t)((i * 3) & 0xFF)) ok = 0;
        }
    }

    during = memory_get_used();
    kfree(a);
    kfree(b);
    after = memory_get_used();

    vga_puts("  heap total : ");
    vga_put_int((int)total);
    vga_puts(" B | sebelum ");
    vga_put_int((int)before);
    vga_puts(" B | saat alokasi ");
    vga_put_int((int)during);
    vga_puts(" B | sesudah ");
    vga_put_int((int)after);
    vga_puts(" B\n");

    if (ok && during > before && after == before) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] alokasi, penulisan, dan pembebasan memori benar\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] alokasi/kfree tidak konsisten\n");
    }
    vga_set_color(0x0F);
}

void test_vfs(void) {
    char payload[16];
    char hasil[16];
    int ok;
    int written, read_bytes;

    for (int i = 0; i < 16; i++) payload[i] = (char)('A' + i);

    vga_set_color(0x0C);
    vga_puts("\n=== UJI VFS RAMDISK ===\n");
    vga_set_color(0x0F);

    written = vfs_write("diag.dat", payload, sizeof(payload));
    read_bytes = vfs_read("diag.dat", hasil, sizeof(hasil));
    ok = (written == 0 && read_bytes == (int)sizeof(payload));
    for (int i = 0; ok && i < 16; i++) {
        if (hasil[i] != payload[i]) ok = 0;
    }

    vga_puts("  write=");
    vga_put_int(written);
    vga_puts(" read=");
    vga_put_int(read_bytes);
    vga_puts(" byte | used=");
    vga_put_int((int)vfs_get_used_bytes());
    vga_puts("/");
    vga_put_int((int)vfs_get_total_bytes());
    vga_puts(" B | file=");
    vga_put_int(vfs_get_used_files());
    vga_puts("\n");

    if (ok) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] data RAMDisk terbaca persis seperti yang ditulis\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] isi RAMDisk tidak cocok\n");
    }
    vga_set_color(0x0F);
    vfs_delete("diag.dat");
}
