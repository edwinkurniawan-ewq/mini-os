/*
 * kernel/tests/tests.c -- Test harness (Minggu 3, PIC 2 / Rasya)
 *
 *  test_sync()      : kas_mutex -> saldo keuangan pribadi tetap akurat
 *  test_race()      : tanpa kas_mutex -> lost update saldo pribadi
 *  test_deadlock()  : konflik buku kas vs kas.dat + deteksi wait-for graph
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
#include "finance.h"

#define TEST_ITER 5

/* ==== 1. MUTEX PADA CATATAN KEUANGAN PRIBADI (WOWIT KAS) ============== */

void test_sync(void) {
    int saldo, count, expected = 2 * KAS_TEST_TOPUP;

    vga_set_color(0x0C);
    vga_puts("\n=== UJI MUTEX CATATAN KEUANGAN PRIBADI (sync) ===\n");
    vga_set_color(0x0F);
    vga_puts("Thread Catat Transaksi + Thread Auto-Backup menambah saldo pribadi\n");
    vga_puts("dengan kas_mutex -> tanpa lost update\n");

    finance_test_snapshot();
    finance_test_reset();
    task_reset();

    int a = task_create("CatatTransaksi", finance_catat_sync_worker);
    int b = task_create("AutoBackupVFS", finance_backup_sync_worker);
    vga_puts("  Thread Catat Transaksi = T");
    vga_put_int(a);
    vga_puts(" | Thread Auto-Backup VFS = T");
    vga_put_int(b);
    vga_puts("\n");

    task_run_until_blocked();

    saldo = finance_test_saldo();
    count = finance_test_count();
    vga_puts("  saldo pribadi : Rp");
    vga_put_int(saldo);
    vga_puts(" | ekspektasi Rp");
    vga_put_int(expected);
    vga_puts(" | transaksi: ");
    vga_put_int(count);
    vga_puts("\n");

    if (saldo == expected && count == 2) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] saldo pribadi akurat 100%, kas_mutex bekerja\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] saldo pribadi tidak akurat\n");
    }
    vga_set_color(0x0F);
    finance_test_restore();
    task_reset();
}

/* ==== 2. RACE CONDITION PADA CATATAN KEUANGAN PRIBADI ================= */

void test_race(void) {
    int saldo, count, expected = 2 * KAS_TEST_TOPUP;

    vga_set_color(0x0C);
    vga_puts("\n=== UJI RACE CONDITION KEUANGAN PRIBADI (race) ===\n");
    vga_set_color(0x0F);
    vga_puts("Thread Catat Transaksi + Thread Auto-Backup / Pemasukan Rutin\n");
    vga_puts("menambah saldo pribadi TANPA kas_mutex -> lost update\n");

    finance_test_snapshot();
    finance_test_reset();
    task_reset();
    task_create("CatatTransaksi", finance_catat_race_worker);
    task_create("AutoBackupVFS", finance_backup_race_worker);

    task_run_until_blocked();

    saldo = finance_test_saldo();
    count = finance_test_count();
    vga_puts("  saldo pribadi : Rp");
    vga_put_int(saldo);
    vga_puts(" | ekspektasi Rp");
    vga_put_int(expected);
    vga_puts(" | transaksi: ");
    vga_put_int(count);
    vga_puts("\n");

    if (saldo < expected) {
        vga_set_color(0x0A);
        vga_puts("  [TERBUKTI] lost update pada saldo pribadi: Rp");
        vga_put_int(expected - saldo);
        vga_puts("\n");
    } else {
        vga_set_color(0x0E);
        vga_puts("  [TIDAK TERBUKTI] jadwal eksekusi tidak interleaving\n");
    }
    vga_set_color(0x0F);
    vga_puts("  Bandingkan dengan perintah 'sync' (kas_mutex)\n");
    finance_test_restore();
    task_reset();
}

/* ==== 3. DEADLOCK DUA RESOURCE PADA KEUANGAN PRIBADI ================= */

void test_deadlock(void) {
    int stuck;

    vga_set_color(0x0C);
    vga_puts("\n=== UJI DEADLOCK CATATAN KEUANGAN PRIBADI (WOWIT KAS) ===\n");
    vga_set_color(0x0F);
    vga_puts("Kunci 1: mutex_buku_kas (tabel transaksi pribadi di RAM)\n");
    vga_puts("Kunci 2: mutex_file_vfs  (baca/tulis kas.dat di RAMDisk)\n");

    finance_test_snapshot();

    /* --- skenario 1: urutan kunci berlawanan -> siklus nyata --- */
    vga_puts("\n[1] Thread Catat Transaksi: buku_kas -> file_vfs\n");
    vga_puts("    Thread Auto-Backup VFS: file_vfs -> buku_kas\n");
    finance_deadlock_prepare();
    task_reset();
    task_create("CatatTransaksi", finance_deadlock_catat_unsafe);
    task_create("AutoBackupVFS", finance_deadlock_backup_unsafe);

    stuck = task_run_until_blocked();
    vga_puts("  thread macet (WAITING): ");
    vga_put_int(stuck);
    vga_puts("\n");

    if (sync_detect_deadlock()) {
        vga_set_color(0x0C);
        vga_puts("  [TERDETEKSI] tabrakan Thread Catat Transaksi & Thread Auto-Backup VFS\n");
        vga_puts("  pada kedua resource keuangan pribadi (wait-for graph):\n");
        vga_set_color(0x0F);
        sync_print_deadlock();
    } else {
        vga_set_color(0x0E);
        vga_puts("  [TIDAK TERDETEKSI] tidak ada siklus wait-for\n");
        vga_set_color(0x0F);
    }
    task_reset();

    /* --- skenario 2: pemulihan dengan resource ordering --- */
    vga_puts("\n[2] Resource ordering: kedua thread kunci buku_kas lalu file_vfs\n");
    finance_deadlock_prepare();
    task_reset();
    task_create("CatatTransaksi", finance_deadlock_catat_ordered);
    task_create("AutoBackupVFS", finance_deadlock_backup_ordered);
    task_run_until_blocked();
    if (sync_detect_deadlock()) {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] siklus masih terjadi\n");
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] resource ordering memutus siklus, kedua thread selesai\n");
        vga_set_color(0x0F);
    }
    task_reset();

    /* --- skenario 3: pemulihan alternatif dengan trylock + backoff --- */
    vga_puts("\n[3] Trylock + backoff: gagal kunci -> mundur, tidak menunggu\n");
    finance_deadlock_prepare();
    task_reset();
    task_create("CatatTransaksi", finance_deadlock_catat_trylock);
    task_create("AutoBackupVFS", finance_deadlock_backup_trylock);
    task_run_until_blocked();
    if (sync_detect_deadlock()) {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] siklus masih terjadi\n");
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] trylock + backoff mencegah deadlock\n");
        vga_set_color(0x0F);
    }
    task_reset();
    finance_test_restore();
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
    int audit_before = protect_denied_count();
    int denied, allowed_kernel;

    vga_set_color(0x0C);
    vga_puts("\n=== UJI PROTEKSI HAK AKSES ===\n");
    vga_set_color(0x0F);

    vga_puts("Mode awal: ");
    vga_puts(saved_mode == MODE_KERNEL ? "KERNEL" : "USER");
    vga_puts("\nIzin awal:\n");
    protect_print_perms();

    /*
     * Uji pembatasan harus dilakukan di MODE_USER. Di MODE_KERNEL,
     * protect_check_perm() selalu mengembalikan 1 (privilege penuh), sehingga
     * percobaan "izin hapus off" tidak akan pernah menampilkan AKSES DITOLAK.
     */
    protect_set_mode(MODE_USER);
    protect_set_perm_by_name("hapus", 0);
    vga_puts("Setelah 'izin hapus off' (MODE_USER):\n");
    protect_print_perms();

    vga_puts("Coba hapus transaksi di USER  : ");
    if (protect_require(PERM_HAPUS, "hapus transaksi")) {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] hapus masih diizinkan\n");
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] AKSES DITOLAK, izin hapus OFF ditegakkan\n");
        vga_set_color(0x0F);
    }

    vga_puts("Coba lihat transaksi di USER  : ");
    if (protect_require(PERM_LIHAT, "lihat transaksi")) {
        vga_puts("[LULUS] diizinkan\n");
    } else {
        vga_puts("[DITOLAK] diblokir proteksi\n");
    }

    /* Mode KERNEL tetap punya akses penuh walau izin USER dimatikan. */
    protect_set_mode(MODE_KERNEL);
    allowed_kernel = protect_check_perm(PERM_HAPUS);
    vga_puts("Mode KERNEL: hapus = ");
    vga_puts(allowed_kernel ? "diizinkan (privilege penuh)" : "ditolak");
    vga_puts("\n");

    /* Kembali ke mode USER untuk membuktikan penolakan masih aktif. */
    protect_set_mode(MODE_USER);
    protect_require(PERM_HAPUS, "hapus lagi (verifikasi audit)");
    protect_set_mode(saved_mode);

    denied = protect_denied_count() - audit_before;
    vga_puts("Penolakan tercatat di audit  : ");
    vga_put_int(denied);
    vga_puts("\n");
    if (denied > 0 && !allowed_kernel) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] audit log bertambah & kernel privilege benar\n");
        vga_set_color(0x0F);
    }

    /* Bagian kritis bersarang: IF dipulihkan persis seperti semula. */
    protect_enter_critical();
    protect_enter_critical();
    protect_exit_critical();
    protect_exit_critical();
    vga_puts("Critical section (nested) selesai, interrupt flag dipulihkan\n");

    vga_puts("Audit log:\n");
    protect_print_audit();

    /* Pulihkan izin & mode sesuai kondisi pemanggil. */
    protect_set_perm_by_name("hapus", 1);
    protect_set_mode(saved_mode);
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

void test_malloc(void) {
    size_t total = memory_get_total();
    size_t before = memory_get_used();
    size_t during, after;
    uint8_t *a = (uint8_t *)kmalloc(64);
    uint8_t *b = (uint8_t *)kmalloc(128);
    int ok = (a != NULL && b != NULL);
    int oom_ok, coalesce_ok;

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
        vga_set_color(0x0F);
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] alokasi/kfree tidak konsisten\n");
        vga_set_color(0x0F);
    }

    /* ---- Skenario OOM: alokasikan 1 KB blok hingga kmalloc() NULL -------- */
    {
        void *blocks[128];
        int n = 0;
        void *big;
        size_t after_free, before_big;

        for (int i = 0; i < 128; i++) {
            blocks[i] = kmalloc(1024);
            if (blocks[i] == NULL) break;
            n++;
        }

        vga_puts("  OOM: berhasil alokasi ");
        vga_put_int(n);
        vga_puts(" blok 1 KB, berikutnya = ");
        vga_puts(blocks[n < 128 ? n : 127] == NULL ? "NULL" : "masih ada");
        vga_puts("\n");

        oom_ok = (n > 0 && n < 128 &&
                  blocks[n < 128 ? n : 127] == NULL);

        /* Bebaskan setengah bagian AKHIR secara berurutan agar blok-blok
         * kosong bersebelahan dan dapat digabungkan (coalescing). */
        for (int i = n / 2; i < n; i++) kfree(blocks[i]);
        after_free = memory_get_used();
        before_big = memory_get_free();

        big = kmalloc(16 * 1024);
        coalesce_ok = (big != NULL);
        vga_puts("  setelah free ");
        vga_put_int(n - n / 2);
        vga_puts(" blok, free=");
        vga_put_int((int)before_big);
        vga_puts(" B -> alokasi 16 KB = ");
        vga_puts(coalesce_ok ? "BERHASIL" : "GAGAL");
        vga_puts("\n");

        if (big != NULL) {
            for (int i = 0; i < (16 * 1024); i += 512) {
                ((uint8_t *)big)[i] = 0x5A;
            }
            kfree(big);
        }
        for (int i = 0; i < n / 2; i++) kfree(blocks[i]);

        if (oom_ok && coalesce_ok && memory_get_used() == before) {
            vga_set_color(0x0A);
            vga_puts("  [LULUS] OOM aman (NULL), kernel hidup, coalescing bekerja\n");
            vga_set_color(0x0F);
        } else {
            vga_set_color(0x0C);
            vga_puts("  [GAGAL] skenario OOM/coalescing tidak konsisten\n");
            vga_set_color(0x0F);
        }
        (void)after_free;
    }
}

void test_vfs(void) {
    char payload[16];
    char updated[16];
    char hasil[16];
    int ok_create, ok_read1, ok_update, ok_read2, ok_delete_read;
    int written, read_bytes;

    for (int i = 0; i < 16; i++) {
        payload[i] = (char)('A' + i);
        updated[i] = (char)('0' + (i % 10));
    }

    vga_set_color(0x0C);
    vga_puts("\n=== UJI VFS RAMDISK (CREATE-READ-UPDATE-DELETE) ===\n");
    vga_set_color(0x0F);

    /* 1. CREATE */
    written = vfs_write("diag.dat", payload, sizeof(payload));
    ok_create = (written == 0 && vfs_exists("diag.dat") &&
                 vfs_get_size("diag.dat") == sizeof(payload));
    vga_puts("  1. create  -> ");
    vga_puts(ok_create ? "OK" : "GAGAL");
    vga_puts(" (size=");
    vga_put_int((int)vfs_get_size("diag.dat"));
    vga_puts(" B)\n");

    /* 2. READ (verifikasi isi awal) */
    read_bytes = vfs_read("diag.dat", hasil, sizeof(hasil));
    ok_read1 = (read_bytes == (int)sizeof(payload));
    for (int i = 0; ok_read1 && i < 16; i++) {
        if (hasil[i] != payload[i]) ok_read1 = 0;
    }
    vga_puts("  2. read    -> ");
    vga_puts(ok_read1 ? "OK (isi=A..P)" : "GAGAL");
    vga_puts(" (");
    vga_put_int(read_bytes);
    vga_puts(" B)\n");

    /* 3. UPDATE: tulis ulang nama yang sama dengan isi berbeda */
    written = vfs_write("diag.dat", updated, sizeof(updated));
    ok_update = (written == 0 && vfs_get_size("diag.dat") == sizeof(updated));
    vga_puts("  3. update  -> ");
    vga_puts(ok_update ? "OK" : "GAGAL");
    vga_puts(" (size=");
    vga_put_int((int)vfs_get_size("diag.dat"));
    vga_puts(" B)\n");

    /* 4. READ ULANG: isi harus berubah, bukan isi lama */
    for (int i = 0; i < 16; i++) hasil[i] = 0;
    read_bytes = vfs_read("diag.dat", hasil, sizeof(hasil));
    ok_read2 = (read_bytes == (int)sizeof(updated));
    for (int i = 0; ok_read2 && i < 16; i++) {
        if (hasil[i] != updated[i]) ok_read2 = 0;
    }
    vga_puts("  4. re-read  -> ");
    vga_puts(ok_read2 ? "OK (isi berubah, bukan A..P)" : "GAGAL");
    vga_puts("\n");

    /* 5. DELETE + 6. READ harus gagal */
    written = vfs_delete("diag.dat");
    read_bytes = vfs_read("diag.dat", hasil, sizeof(hasil));
    ok_delete_read = (written == 0 && !vfs_exists("diag.dat") && read_bytes < 0);
    vga_puts("  5. delete  -> ");
    vga_puts(written == 0 ? "OK" : "GAGAL");
    vga_puts(" | 6. read after delete -> ");
    vga_puts(read_bytes < 0 ? "GAGAL (tidak ditemukan, sesuai harapan)"
                            : "MASIH ADA (bug)");
    vga_puts("\n");

    vga_puts("  used=");
    vga_put_int((int)vfs_get_used_bytes());
    vga_puts("/");
    vga_put_int((int)vfs_get_total_bytes());
    vga_puts(" B | file=");
    vga_put_int(vfs_get_used_files());
    vga_puts("\n");

    if (ok_create && ok_read1 && ok_update && ok_read2 && ok_delete_read) {
        vga_set_color(0x0A);
        vga_puts("  [LULUS] siklus CRUD RAMDisk lengkap & terverifikasi\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("  [GAGAL] siklus CRUD RAMDisk tidak konsisten\n");
    }
    vga_set_color(0x0F);

    /* Pastikan berkas uji tidak tertinggal walaupun ada langkah gagal. */
    if (vfs_exists("diag.dat")) vfs_delete("diag.dat");
}
