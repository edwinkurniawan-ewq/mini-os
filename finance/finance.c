#include "finance.h"
#include "kernel.h"
#include "protect.h"
#include "logbook.h"
#include "vfs.h"
#include "task.h"

#define MAX_TRANSAKSI 20

char keyboard_getchar(void);
void vga_putc(char c);
void vga_backspace(void);

typedef struct {
    int id;
    int aktif;       /* 1 = aktif, 0 = dihapus */
    char tipe;       /* '+' untuk masuk, '-' untuk keluar */
    int nominal;
    char ket[32];
} transaksi_t;

typedef struct {
    int count;
    transaksi_t list[MAX_TRANSAKSI];
} kas_store_t;

static transaksi_t buku_kas[MAX_TRANSAKSI];
static int jumlah_data = 0;
static int next_transaksi_id = 1;

/* ---- Kunci Catatan Keuangan Pribadi (WOWIT Kas) ------------------------- */
/* kas_mutex     : melindungi seluruh data transaksi & saldo pribadi.         */
/* mutex_buku_kas: resource 1, tabel transaksi keuangan pribadi di RAM.        */
/* mutex_file_vfs: resource 2, operasi kas.dat di RAMDisk VFS.                */
mutex_t kas_mutex;
mutex_t mutex_buku_kas;
mutex_t mutex_file_vfs;

/* Snapshot untuk test harness: skenario uji mengubah kas nyata lalu
 * mengembalikannya ke kondisi semula (termasuk isi kas.dat). */
static transaksi_t kas_snapshot[MAX_TRANSAKSI];
static int snapshot_count = 0;
static int snapshot_next_id = 1;
static int snapshot_valid = 0;

/* ---- Local helpers ------------------------------------------------------ */

static void f_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int f_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int hitung_aktif_unlocked(void);

static void print_int(int num) {
    if (num == 0) {
        vga_putc('0');
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

static void readline(char *buf, int maxlen) {
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putc('\n');
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                vga_backspace();
            }
        } else if (c >= 32 && c < 127 && len < maxlen - 1) {
            buf[len++] = c;
            vga_putc(c);
        }
    }
    buf[len] = '\0';
}

/* Parser nominal fleksibel: mendukung '50000', '+50000', '-20000', 'Rp 50.000', dll. */
static int parse_nominal_smart(const char *str, char *out_tipe) {
    while (*str == ' ') str++;

    if (*str == '+') {
        if (out_tipe) *out_tipe = '+';
        str++;
    } else if (*str == '-') {
        if (out_tipe) *out_tipe = '-';
        str++;
    }

    while (*str == ' ') str++;

    /* Lewati 'Rp' atau 'rp' jika ada */
    if ((str[0] == 'R' || str[0] == 'r') && (str[1] == 'P' || str[1] == 'p')) {
        str += 2;
        while (*str == ' ' || *str == '.') str++;
    }

    int res = 0;
    int found_digit = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            if (res > 99999999) return 0;   /* cegah overflow int (maks 999.999.999) */
            res = res * 10 + (*str - '0');
            found_digit = 1;
        } else if (*str == '.' || *str == ',' || *str == ' ') {
            /* Abaikan pemisah ribuan */
        } else {
            break;
        }
        str++;
    }

    return found_digit ? res : 0;
}

static int parse_int_str(const char *str) {
    while (*str == ' ') str++;
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

/* ---- Data Store & RAMDisk Sync ----------------------------------------- */

/* Pemanggil helper "_unlocked" harus memegang kas_mutex (atau, untuk
 * skenario deadlock, kedua mutex resource kas). */
static int hitung_saldo_unlocked(void) {
    int saldo = 0;
    for (int i = 0; i < jumlah_data; i++) {
        if (!buku_kas[i].aktif) continue;
        if (buku_kas[i].tipe == '+') saldo += buku_kas[i].nominal;
        else saldo -= buku_kas[i].nominal;
    }
    return saldo;
}

static int finance_save_to_ramdisk_unlocked(void) {
    kas_store_t store;
    store.count = jumlah_data;
    for (int i = 0; i < jumlah_data; i++) {
        store.list[i] = buku_kas[i];
    }
    return vfs_write("kas.dat", &store, sizeof(kas_store_t));
}

int finance_save_to_ramdisk(void) {
    int res;
    mutex_lock(&kas_mutex);
    res = finance_save_to_ramdisk_unlocked();
    mutex_unlock(&kas_mutex);
    return res;
}

static int finance_load_from_ramdisk_unlocked(void) {
    if (vfs_exists("kas.dat")) {
        kas_store_t store;
        int bytes = vfs_read("kas.dat", &store, sizeof(kas_store_t));
        if (bytes == (int)sizeof(kas_store_t)) {
            int count = store.count;
            if (count < 0) count = 0;
            if (count > MAX_TRANSAKSI) count = MAX_TRANSAKSI;

            jumlah_data = count;
            next_transaksi_id = 1;
            for (int i = 0; i < jumlah_data; i++) {
                buku_kas[i] = store.list[i];
                if (buku_kas[i].id >= next_transaksi_id) {
                    next_transaksi_id = buku_kas[i].id + 1;
                }
            }
            return 1;
        }
    }
    return 0;
}

int finance_load_from_ramdisk(void) {
    int res;
    mutex_lock(&kas_mutex);
    res = finance_load_from_ramdisk_unlocked();
    mutex_unlock(&kas_mutex);
    return res;
}

void finance_init(void) {
    mutex_init_named(&kas_mutex, "kas_mutex");
    mutex_init_named(&mutex_buku_kas, "mutex_buku_kas");
    mutex_init_named(&mutex_file_vfs, "mutex_file_vfs");

    for (int i = 0; i < MAX_TRANSAKSI; i++) {
        buku_kas[i].id = 0;
        buku_kas[i].aktif = 0;
        buku_kas[i].tipe = '+';
        buku_kas[i].nominal = 0;
        buku_kas[i].ket[0] = '\0';
    }
    jumlah_data = 0;
    next_transaksi_id = 1;

    finance_load_from_ramdisk_unlocked();
}

void finance_get_summary(int *out_count, int *out_saldo) {
    int c = 0;
    int s = 0;

    mutex_lock(&kas_mutex);
    /* Pastikan data tersinkronisasi dari RAMDisk */
    finance_load_from_ramdisk_unlocked();
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif) {
            c++;
            if (buku_kas[i].tipe == '+') s += buku_kas[i].nominal;
            else s -= buku_kas[i].nominal;
        }
    }
    mutex_unlock(&kas_mutex);

    if (out_count) *out_count = c;
    if (out_saldo) *out_saldo = s;
}

/* ---- Test harness pada data kas sungguhan ------------------------------- */

void finance_test_snapshot(void) {
    mutex_lock(&kas_mutex);
    for (int i = 0; i < MAX_TRANSAKSI; i++) {
        kas_snapshot[i] = buku_kas[i];
    }
    snapshot_count = jumlah_data;
    snapshot_next_id = next_transaksi_id;
    snapshot_valid = 1;
    mutex_unlock(&kas_mutex);
}

void finance_test_reset(void) {
    mutex_lock(&kas_mutex);
    for (int i = 0; i < MAX_TRANSAKSI; i++) {
        buku_kas[i].id = 0;
        buku_kas[i].aktif = 0;
        buku_kas[i].tipe = '+';
        buku_kas[i].nominal = 0;
        buku_kas[i].ket[0] = '\0';
    }
    jumlah_data = 0;
    next_transaksi_id = 1;
    mutex_unlock(&kas_mutex);
}

int finance_test_count(void) {
    int count;
    mutex_lock(&kas_mutex);
    count = hitung_aktif_unlocked();
    mutex_unlock(&kas_mutex);
    return count;
}

int finance_test_saldo(void) {
    int saldo;
    mutex_lock(&kas_mutex);
    saldo = hitung_saldo_unlocked();
    mutex_unlock(&kas_mutex);
    return saldo;
}

void finance_test_restore(void) {
    mutex_lock(&kas_mutex);
    if (snapshot_valid) {
        for (int i = 0; i < MAX_TRANSAKSI; i++) {
            buku_kas[i] = kas_snapshot[i];
        }
        jumlah_data = snapshot_count;
        next_transaksi_id = snapshot_next_id;
        snapshot_valid = 0;
        (void)finance_save_to_ramdisk_unlocked();
    }
    mutex_unlock(&kas_mutex);
}

/* Worker uji benar-benar menulis transaksi pemasukan ke buku kas pribadi.
 * Titik task_yield() sengaja berada di antara READ indeks dan WRITE transaksi:
 * tanpa kas_mutex kedua thread menimpa slot yang sama (lost update). */
static void kas_tambah_pemasukan_worker(int pakai_mutex, const char *keterangan) {
    if (pakai_mutex) mutex_lock(&kas_mutex);

    int idx = jumlah_data;              /* READ */
    task_yield();                       /* interleaving paksa */

    if (idx >= 0 && idx < MAX_TRANSAKSI) {
        buku_kas[idx].id = next_transaksi_id++;
        buku_kas[idx].aktif = 1;
        buku_kas[idx].tipe = '+';
        buku_kas[idx].nominal = KAS_TEST_TOPUP;
        f_strcpy(buku_kas[idx].ket, keterangan, sizeof(buku_kas[idx].ket));
        jumlah_data = idx + 1;          /* WRITE */
    }

    if (pakai_mutex) mutex_unlock(&kas_mutex);
    task_exit();
}

/* Thread Catat Transaksi dan Thread Auto-Backup VFS / Pemasukan Rutin:
 * dua sumber pemasukan pribadi yang menulis saldo yang sama. */
void finance_catat_race_worker(void) {
    kas_tambah_pemasukan_worker(0, "Catat");
}

void finance_backup_race_worker(void) {
    kas_tambah_pemasukan_worker(0, "Backup");
}

void finance_catat_sync_worker(void) {
    kas_tambah_pemasukan_worker(1, "Catat");
}

void finance_backup_sync_worker(void) {
    kas_tambah_pemasukan_worker(1, "Backup");
}

/* ---- Dua resource nyata pada Catatan Keuangan Pribadi (WOWIT Kas) ------- */

static void kas_tulis_transaksi_staging(void) {
    if (jumlah_data >= MAX_TRANSAKSI) return;

    int idx = jumlah_data++;
    buku_kas[idx].id = next_transaksi_id++;
    buku_kas[idx].aktif = 1;
    buku_kas[idx].tipe = '+';
    buku_kas[idx].nominal = KAS_TEST_TOPUP;
    f_strcpy(buku_kas[idx].ket, "Catat", sizeof(buku_kas[idx].ket));
}

/* Thread Auto-Backup VFS: cek apakah saldo RAM dan kas.dat sinkron. */
static void backup_cek_saldo(void) {
    kas_store_t store;
    int saldo_file = 0;
    int bytes = vfs_read("kas.dat", &store, sizeof(store));

    if (bytes == (int)sizeof(store)) {
        int count = store.count;
        if (count < 0) count = 0;
        if (count > MAX_TRANSAKSI) count = MAX_TRANSAKSI;
        for (int i = 0; i < count; i++) {
            if (!store.list[i].aktif) continue;
            if (store.list[i].tipe == '+') saldo_file += store.list[i].nominal;
            else saldo_file -= store.list[i].nominal;
        }
    }

    int saldo_ram = hitung_saldo_unlocked();
    vga_puts("  Thread Auto-Backup VFS: saldo RAM=");
    print_int(saldo_ram);
    vga_puts(" | saldo kas.dat=");
    print_int(saldo_file);
    vga_set_color(saldo_ram == saldo_file ? 0x0A : 0x0E);
    vga_puts(saldo_ram == saldo_file ? " -> COCOK\n" : " -> BEDA\n");
    vga_set_color(0x0F);
}

void finance_deadlock_prepare(void) {
    /* task_reset() mematikan task yang macet; kembalikan pemilik mutex. */
    mutex_init_named(&mutex_buku_kas, "mutex_buku_kas");
    mutex_init_named(&mutex_file_vfs, "mutex_file_vfs");
}

/* Skenario 1: urutan kunci berlawanan -> siklus nyata. */
void finance_deadlock_catat_unsafe(void) {
    vga_puts("  Thread Catat Transaksi: kunci mutex_buku_kas\n");
    mutex_lock(&mutex_buku_kas);
    kas_tulis_transaksi_staging();
    vga_puts("  Thread Catat Transaksi: minta mutex_file_vfs ...\n");
    task_yield();
    mutex_lock(&mutex_file_vfs);        /* menunggu Thread Auto-Backup VFS */
    vga_puts("  Thread Catat Transaksi: dapat mutex_file_vfs, simpan kas.dat\n");
    (void)finance_save_to_ramdisk_unlocked();
    mutex_unlock(&mutex_file_vfs);
    mutex_unlock(&mutex_buku_kas);
    task_exit();
}

void finance_deadlock_backup_unsafe(void) {
    vga_puts("  Thread Auto-Backup VFS: kunci mutex_file_vfs\n");
    mutex_lock(&mutex_file_vfs);
    vga_puts("  Thread Auto-Backup VFS: minta mutex_buku_kas ...\n");
    task_yield();
    mutex_lock(&mutex_buku_kas);        /* menunggu Thread Catat Transaksi */
    backup_cek_saldo();
    mutex_unlock(&mutex_buku_kas);
    mutex_unlock(&mutex_file_vfs);
    task_exit();
}

/* Skenario 2: resource ordering -- selalu buku_kas lalu file_vfs. */
void finance_deadlock_catat_ordered(void) {
    vga_puts("  Thread Catat Transaksi: [urutan baku] kunci mutex_buku_kas\n");
    mutex_lock(&mutex_buku_kas);
    kas_tulis_transaksi_staging();
    task_yield();
    vga_puts("  Thread Catat Transaksi: [urutan baku] kunci mutex_file_vfs\n");
    mutex_lock(&mutex_file_vfs);
    (void)finance_save_to_ramdisk_unlocked();
    mutex_unlock(&mutex_file_vfs);
    mutex_unlock(&mutex_buku_kas);
    task_exit();
}

void finance_deadlock_backup_ordered(void) {
    vga_puts("  Thread Auto-Backup VFS: [urutan baku] minta mutex_buku_kas ...\n");
    mutex_lock(&mutex_buku_kas);
    vga_puts("  Thread Auto-Backup VFS: [urutan baku] kunci mutex_file_vfs\n");
    mutex_lock(&mutex_file_vfs);
    backup_cek_saldo();
    mutex_unlock(&mutex_file_vfs);
    mutex_unlock(&mutex_buku_kas);
    task_exit();
}

/* Skenario 3: trylock + backoff -- tidak pernah menunggu sambil memegang kunci. */
void finance_deadlock_catat_trylock(void) {
    vga_puts("  Thread Catat Transaksi: kunci mutex_buku_kas (trylock mode)\n");
    mutex_lock(&mutex_buku_kas);
    kas_tulis_transaksi_staging();
    task_yield();
    mutex_lock(&mutex_file_vfs);
    (void)finance_save_to_ramdisk_unlocked();
    mutex_unlock(&mutex_file_vfs);
    mutex_unlock(&mutex_buku_kas);
    task_exit();
}

void finance_deadlock_backup_trylock(void) {
    if (!mutex_trylock(&mutex_buku_kas)) {
        vga_puts("  Thread Auto-Backup VFS: trylock mutex_buku_kas gagal -> backoff\n");
        task_yield();
        if (!mutex_trylock(&mutex_buku_kas)) {
            vga_puts("  Thread Auto-Backup VFS: masih dipakai -> lewati (tidak menunggu)\n");
            task_exit();
            return;
        }
    }
    vga_puts("  Thread Auto-Backup VFS: trylock mutex_buku_kas berhasil\n");

    if (!mutex_trylock(&mutex_file_vfs)) {
        vga_puts("  Thread Auto-Backup VFS: trylock mutex_file_vfs gagal -> lepas & backoff\n");
        mutex_unlock(&mutex_buku_kas);
        task_exit();
        return;
    }
    backup_cek_saldo();
    mutex_unlock(&mutex_file_vfs);
    mutex_unlock(&mutex_buku_kas);
    task_exit();
}

static int hitung_aktif_unlocked(void) {
    int c = 0;
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif) c++;
    }
    return c;
}

static void print_tabel_transaksi(void) {
    vga_set_color(0x0C);
    vga_puts("ID   TIPE   NOMINAL   KETERANGAN\n");
    vga_set_color(0x0F);

    int total_aktif = 0;
    int total_saldo = 0;

    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif) {
            total_aktif++;
            if (buku_kas[i].tipe == '+') total_saldo += buku_kas[i].nominal;
            else total_saldo -= buku_kas[i].nominal;

            print_int(buku_kas[i].id);
            vga_puts(buku_kas[i].id < 10 ? "    " : "   ");

            if (buku_kas[i].tipe == '+') {
                vga_puts("(+)    ");
            } else {
                vga_puts("(-)    ");
            }

            print_int(buku_kas[i].nominal);
            int d_len = 0;
            int tmp = buku_kas[i].nominal;
            if (tmp == 0) d_len = 1;
            while (tmp > 0) { d_len++; tmp /= 10; }
            for (int p = d_len; p < 10; p++) vga_puts(" ");

            vga_puts(buku_kas[i].ket);
            vga_puts("\n");
        }
    }

    vga_set_color(0x0C);
    vga_puts("Total: ");
    print_int(total_aktif);
    vga_puts(" transaksi | Saldo: Rp");
    print_int(total_saldo);
    vga_puts("\n");
    vga_set_color(0x0F);
}

/* ---- Application Menu Actions ------------------------------------------ */

static void do_lihat_locked(void) {
    if (!protect_check_perm(PERM_LIHAT)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin lihat transaksi dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    vga_puts("\n");
    if (hitung_aktif_unlocked() == 0) {
        vga_puts("(Belum ada data transaksi)\n\n");
        return;
    }
    print_tabel_transaksi();
    vga_puts("\n");
}

static void do_tambah_locked(void) {
    if (!protect_check_perm(PERM_TAMBAH)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin tambah transaksi dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    if (jumlah_data >= MAX_TRANSAKSI) {
        vga_set_color(0x0C);
        vga_puts("\n[!] Buku kas penuh (Maksimal 20 transaksi).\n\n");
        vga_set_color(0x0F);
        return;
    }

    char in_ket[32];
    char in_nom[24];
    char in_tipe[16];

    /* 1. Input Keterangan / Nama Transaksi */
    vga_puts("\nNama Transaksi / Keterangan : ");
    readline(in_ket, sizeof(in_ket));
    if (in_ket[0] == '\0') {
        f_strcpy(in_ket, "Kas", sizeof(in_ket));
    }

    /* 2. Input Nominal: diulang sampai valid, Enter kosong = batal.
     *    (Dulu input yang tidak terbaca langsung membuang kita ke menu kas.) */
    char detected_tipe = '+';
    int nominal = 0;
    for (;;) {
        vga_puts("Nominal (Rp) : ");
        readline(in_nom, sizeof(in_nom));
        if (in_nom[0] == '\0') {
            vga_set_color(0x0E);
            vga_puts("[i] Dibatalkan: nominal kosong. Tidak ada transaksi yang dicatat.\n\n");
            vga_set_color(0x0F);
            return;
        }
        detected_tipe = '+';
        nominal = parse_nominal_smart(in_nom, &detected_tipe);
        if (nominal > 0) break;

        vga_set_color(0x0C);
        vga_puts("[!] Nominal tidak valid. Yang terbaca: \"");
        vga_puts(in_nom);
        vga_puts("\"\n    Ketik angka saja, contoh 50000 (Enter kosong = batal).\n");
        vga_set_color(0x0F);
    }

    /* 3. Input Jenis (Jika belum terdeteksi dari tanda +/-) */
    char tipe = detected_tipe;
    if (in_nom[0] != '+' && in_nom[0] != '-') {
        vga_puts("Jenis [1=Pemasukan, 2=Pengeluaran]  : ");
        readline(in_tipe, sizeof(in_tipe));
        if (in_tipe[0] == '2' || in_tipe[0] == '-') {
            tipe = '-';
        } else {
            tipe = '+';
        }
    }

    int idx = jumlah_data++;
    buku_kas[idx].id = next_transaksi_id++;
    buku_kas[idx].aktif = 1;
    buku_kas[idx].tipe = tipe;
    buku_kas[idx].nominal = nominal;
    f_strcpy(buku_kas[idx].ket, in_ket, sizeof(buku_kas[idx].ket));

    /* Otomatis simpan data transaksi langsung ke RAMDisk */
    int save_res = finance_save_to_ramdisk_unlocked();

    /* Catat ke buku log */
    logbook_add("TAMBAH", buku_kas[idx].id, tipe, in_ket);

    if (save_res == 0) {
        vga_set_color(0x0A);
        vga_puts("[OK] Transaksi ID #");
        print_int(buku_kas[idx].id);
        vga_puts(" (");
        vga_puts(tipe == '+' ? "Pemasukan Rp" : "Pengeluaran Rp");
        print_int(nominal);
        vga_puts(") berhasil dicatat & disimpan ke RAMDisk.\n\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("[!] Transaksi ID #");
        print_int(buku_kas[idx].id);
        vga_puts(" tercatat di memori, tetapi GAGAL disimpan ke RAMDisk.\n\n");
    }
    vga_set_color(0x0F);
}

static void do_ubah_locked(void) {
    if (!protect_check_perm(PERM_UBAH)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin ubah nominal dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    if (hitung_aktif_unlocked() == 0) {
        vga_puts("\n(Belum ada data transaksi untuk diubah)\n\n");
        return;
    }

    vga_puts("\n");
    print_tabel_transaksi();

    char in_id[16];
    char in_nom[24];

    vga_puts("ID transaksi yang diubah : ");
    readline(in_id, sizeof(in_id));
    int target_id = parse_int_str(in_id);

    int found_idx = -1;
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif && buku_kas[i].id == target_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        vga_set_color(0x0C);
        vga_puts("[!] ID transaksi tidak ditemukan!\n\n");
        vga_set_color(0x0F);
        return;
    }

    vga_puts("Nominal baru (Rp) : ");
    readline(in_nom, sizeof(in_nom));
    char t = buku_kas[found_idx].tipe;
    int nominal_baru = parse_nominal_smart(in_nom, &t);
    if (nominal_baru <= 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Nominal baru tidak valid!\n\n");
        vga_set_color(0x0F);
        return;
    }

    buku_kas[found_idx].nominal = nominal_baru;
    finance_save_to_ramdisk_unlocked();
    logbook_add("UBAH", target_id, buku_kas[found_idx].tipe, "Nominal diubah");

    vga_set_color(0x0A);
    vga_puts("[OK] Transaksi ID #");
    print_int(target_id);
    vga_puts(" berhasil diubah menjadi Rp");
    print_int(nominal_baru);
    vga_puts(" & disimpan ke RAMDisk.\n\n");
    vga_set_color(0x0F);
}

static void do_hapus_locked(void) {
    if (!protect_check_perm(PERM_HAPUS)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin hapus dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    if (hitung_aktif_unlocked() == 0) {
        vga_puts("\n(Belum ada data transaksi untuk dihapus)\n\n");
        return;
    }

    vga_puts("\n");
    print_tabel_transaksi();

    char in_id[16];
    vga_puts("ID transaksi yang dihapus : ");
    readline(in_id, sizeof(in_id));
    int target_id = parse_int_str(in_id);

    int found_idx = -1;
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif && buku_kas[i].id == target_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        vga_set_color(0x0C);
        vga_puts("[!] ID tidak ditemukan!\n\n");
        vga_set_color(0x0F);
        return;
    }

    buku_kas[found_idx].aktif = 0;
    finance_save_to_ramdisk_unlocked();
    logbook_add("HAPUS", target_id, buku_kas[found_idx].tipe, "Transaksi dihapus");

    vga_set_color(0x0A);
    vga_puts("[OK] Transaksi ID #");
    print_int(target_id);
    vga_puts(" berhasil dihapus dari sistem & RAMDisk.\n\n");
    vga_set_color(0x0F);
}

static void do_simpan_locked(void) {
    if (!protect_check_perm(PERM_SIMPAN)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin simpan data dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    int res = finance_save_to_ramdisk_unlocked();
    if (res == 0) {
        logbook_add("SIMPAN", -1, ' ', "Data disimpan ke RAMDisk");
        vga_set_color(0x0A);
        vga_puts("\n[OK] Data transaksi (");
        print_int(hitung_aktif_unlocked());
        vga_puts(" item aktif) berhasil disimpan ke RAMDisk (kas.dat).\n\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("\n[!] Gagal menyimpan data ke RAMDisk.\n\n");
    }
    vga_set_color(0x0F);
}

/* ---- Ekspor laporan ke RAMDisk VFS ------------------------------------- */

static void do_ekspor_locked(void) {
    if (!protect_check_perm(PERM_SIMPAN)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin simpan laporan dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    int pemasukan = 0;
    int pengeluaran = 0;
    for (int i = 0; i < jumlah_data; i++) {
        if (!buku_kas[i].aktif) continue;
        if (buku_kas[i].tipe == '+') pemasukan += buku_kas[i].nominal;
        else pengeluaran += buku_kas[i].nominal;
    }

    char laporan[256];
    int p = 0;
    /* Format teks sederhana tanpa library standar C. */
    const char *header = "=== LAPORAN CATATAN KEUANGAN PRIBADI (WOWIT KAS) ===\n";
    for (int i = 0; header[i]; i++) laporan[p++] = header[i];
    const char *lab[] = {
        "Transaksi aktif : ", "Pemasukan (Rp)  : ", "Pengeluaran (Rp): ",
        "Saldo akhir (Rp): "
    };
    int val[] = { hitung_aktif_unlocked(), pemasukan, pengeluaran,
                  pemasukan - pengeluaran };

    for (int row = 0; row < 4; row++) {
        for (int i = 0; lab[row][i]; i++) laporan[p++] = lab[row][i];
        char angka[16];
        int n = 0;
        int v = val[row];
        if (v == 0) {
            angka[n++] = '0';
        } else {
            if (v < 0) {
                angka[n++] = '-';
                v = -v;
            }
            char tmp[12];
            int t = 0;
            while (v > 0) {
                tmp[t++] = (char)('0' + (v % 10));
                v /= 10;
            }
            while (t > 0) angka[n++] = tmp[--t];
        }
        angka[n] = '\0';
        for (int i = 0; i < n; i++) laporan[p++] = angka[i];
        laporan[p++] = '\n';
    }
    laporan[p] = '\0';

    int res = vfs_write("laporan_kas.txt", laporan, (size_t)f_strlen(laporan));
    if (res == 0) {
        vga_set_color(0x0A);
        vga_puts("\n[OK] Laporan diekspor ke VFS: laporan_kas.txt (");
        print_int(f_strlen(laporan));
        vga_puts(" byte). Buka Kernel Mode lalu 'cat laporan_kas.txt'.\n\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("\n[!] Gagal menulis laporan_kas.txt ke RAMDisk.\n\n");
    }
    vga_set_color(0x0F);
}

/* Setiap aksi kas mengunci kas_mutex dari awal sampai selesai. Helper
 * "_locked" di atas dipanggil hanya dari sini (atau dari aksi lain yang
 * sudah memegang kunci yang sama). */
static void do_lihat(void) {
    mutex_lock(&kas_mutex);
    do_lihat_locked();
    mutex_unlock(&kas_mutex);
}

static void do_tambah(void) {
    mutex_lock(&kas_mutex);
    do_tambah_locked();
    mutex_unlock(&kas_mutex);
}

static void do_ubah(void) {
    mutex_lock(&kas_mutex);
    do_ubah_locked();
    mutex_unlock(&kas_mutex);
}

static void do_hapus(void) {
    mutex_lock(&kas_mutex);
    do_hapus_locked();
    mutex_unlock(&kas_mutex);
}

static void do_simpan(void) {
    mutex_lock(&kas_mutex);
    do_simpan_locked();
    mutex_unlock(&kas_mutex);
}

static void do_ekspor(void) {
    mutex_lock(&kas_mutex);
    do_ekspor_locked();
    mutex_unlock(&kas_mutex);
}

/* =========================================================================
 * SUB-SHELL CATATAN KEUANGAN PRIBADI (WOWIT KAS)
 * ========================================================================= */
void finance_run(void) {
    /* Muat ulang data terbaru dari RAMDisk */
    finance_load_from_ramdisk();

    /* Thread logger dimulai saat aplikasi dibuka */
    int logger_id = task_register_logger_start();
    logbook_set_logger_active(1);
    logbook_add("MASUK", -1, ' ', "Catatan keuangan pribadi dibuka");

    char choice_buf[16];

    for (;;) {
        if (protect_get_mode() == MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("=== CATATAN KEUANGAN PRIBADI - WOWIT KAS (mode KERNEL) ===\n\n");
        } else {
            vga_set_color(0x09); /* Biru terang untuk mode USER */
            vga_puts("=== CATATAN KEUANGAN PRIBADI - WOWIT KAS (mode USER) ===\n\n");
        }
        vga_set_color(0x0F);

        vga_puts(" 1. Lihat daftar transaksi\n");
        vga_puts(" 2. Tambah transaksi\n");
        vga_puts(" 3. Ubah nominal\n");
        vga_puts(" 4. Hapus transaksi\n");
        vga_puts(" 5. Simpan data\n");
        vga_puts(" 6. Ekspor Laporan ke VFS (laporan_kas.txt)\n");
        vga_puts(" 0. Keluar\n");

        if (protect_get_mode() == MODE_KERNEL) {
            vga_set_color(0x0C);
        } else {
            vga_set_color(0x09); /* Biru terang untuk prompt Pilih> di mode USER */
        }
        vga_puts("Pilih> ");
        vga_set_color(0x0F);

        readline(choice_buf, sizeof(choice_buf));

        if (choice_buf[0] == '0' || choice_buf[0] == 'k' || choice_buf[0] == 'q') {
            vga_set_color(0x0C);
            vga_puts("Keluar dari WOWIT Kas.\n");
            vga_set_color(0x0F);
            logbook_add("KELUAR", -1, ' ', "Catatan keuangan pribadi ditutup");
            task_register_logger_stop(logger_id);
            logbook_set_logger_active(0);
            break;
        } else if (choice_buf[0] == '1' || choice_buf[0] == 'l') {
            do_lihat();
        } else if (choice_buf[0] == '2' || choice_buf[0] == 't') {
            do_tambah();
        } else if (choice_buf[0] == '3' || choice_buf[0] == 'u') {
            do_ubah();
        } else if (choice_buf[0] == '4' || choice_buf[0] == 'h') {
            do_hapus();
        } else if (choice_buf[0] == '5' || choice_buf[0] == 's') {
            do_simpan();
        } else if (choice_buf[0] == '6') {
            do_ekspor();
        } else if (choice_buf[0] == '\0') {
            /* Enter kosong: lanjutkan */
            continue;
        } else {
            vga_set_color(0x0C);
            vga_puts("Pilihan tidak valid. Silakan pilih 0 - 6.\n\n");
            vga_set_color(0x0F);
        }
    }
}
