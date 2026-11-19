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

/* ---- Local helpers ------------------------------------------------------ */

static void f_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

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

int finance_save_to_ramdisk(void) {
    kas_store_t store;
    store.count = jumlah_data;
    for (int i = 0; i < jumlah_data; i++) {
        store.list[i] = buku_kas[i];
    }
    return vfs_write("kas.dat", &store, sizeof(kas_store_t));
}

int finance_load_from_ramdisk(void) {
    if (vfs_exists("kas.dat")) {
        kas_store_t store;
        int bytes = vfs_read("kas.dat", &store, sizeof(kas_store_t));
        if (bytes == sizeof(kas_store_t)) {
            jumlah_data = store.count;
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

void finance_init(void) {
    for (int i = 0; i < MAX_TRANSAKSI; i++) {
        buku_kas[i].aktif = 0;
    }
    jumlah_data = 0;
    next_transaksi_id = 1;

    finance_load_from_ramdisk();
}

void finance_get_summary(int *out_count, int *out_saldo) {
    /* Pastikan data tersinkronisasi dari RAMDisk */
    finance_load_from_ramdisk();

    int c = 0;
    int s = 0;
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif) {
            c++;
            if (buku_kas[i].tipe == '+') s += buku_kas[i].nominal;
            else s -= buku_kas[i].nominal;
        }
    }
    if (out_count) *out_count = c;
    if (out_saldo) *out_saldo = s;
}

static int hitung_aktif(void) {
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

static void do_lihat(void) {
    if (!protect_check_perm(PERM_LIHAT)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin lihat transaksi dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    vga_puts("\n");
    if (hitung_aktif() == 0) {
        vga_puts("(Belum ada data transaksi)\n\n");
        return;
    }
    print_tabel_transaksi();
    vga_puts("\n");
}

static void do_tambah(void) {
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

    /* 2. Input Nominal */
    char detected_tipe = '+';
    vga_puts("Nominal (Rp) : ");
    readline(in_nom, sizeof(in_nom));
    int nominal = parse_nominal_smart(in_nom, &detected_tipe);
    if (nominal <= 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Nominal tidak valid (harus lebih dari 0)!\n\n");
        vga_set_color(0x0F);
        return;
    }

    /* 3. Input Jenis (Jika belum terdeteksi dari tanda +/-) */
    char tipe = detected_tipe;
    if (in_nom[0] != '+' && in_nom[0] != '-') {
        vga_puts("Jenis [1=Pemasukan, 2=Pengeluaran] (default: 1) : ");
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
    finance_save_to_ramdisk();

    /* Catat ke buku log */
    logbook_add("TAMBAH", buku_kas[idx].id, tipe, in_ket);

    vga_set_color(0x0A);
    vga_puts("[OK] Transaksi ID #");
    print_int(buku_kas[idx].id);
    vga_puts(" (");
    vga_puts(tipe == '+' ? "Pemasukan Rp" : "Pengeluaran Rp");
    print_int(nominal);
    vga_puts(") berhasil dicatat & disimpan ke RAMDisk.\n\n");
    vga_set_color(0x0F);
}

static void do_ubah(void) {
    if (!protect_check_perm(PERM_UBAH)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin ubah nominal dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    if (hitung_aktif() == 0) {
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
    finance_save_to_ramdisk();
    logbook_add("UBAH", target_id, buku_kas[found_idx].tipe, "Nominal diubah");

    vga_set_color(0x0A);
    vga_puts("[OK] Transaksi ID #");
    print_int(target_id);
    vga_puts(" berhasil diubah menjadi Rp");
    print_int(nominal_baru);
    vga_puts(" & disimpan ke RAMDisk.\n\n");
    vga_set_color(0x0F);
}

static void do_hapus(void) {
    if (!protect_check_perm(PERM_HAPUS)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin hapus dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    if (hitung_aktif() == 0) {
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
    finance_save_to_ramdisk();
    logbook_add("HAPUS", target_id, buku_kas[found_idx].tipe, "Transaksi dihapus");

    vga_set_color(0x0A);
    vga_puts("[OK] Transaksi ID #");
    print_int(target_id);
    vga_puts(" berhasil dihapus dari sistem & RAMDisk.\n\n");
    vga_set_color(0x0F);
}

static void do_simpan(void) {
    if (!protect_check_perm(PERM_SIMPAN)) {
        vga_set_color(0x0C);
        vga_puts("\n[AKSES DITOLAK] Izin simpan data dinonaktifkan oleh Kernel!\n\n");
        vga_set_color(0x0F);
        return;
    }

    int res = finance_save_to_ramdisk();
    if (res == 0) {
        logbook_add("SIMPAN", -1, ' ', "Data disimpan ke RAMDisk");
        vga_set_color(0x0A);
        vga_puts("\n[OK] Data transaksi (");
        print_int(hitung_aktif());
        vga_puts(" item aktif) berhasil disimpan ke RAMDisk (kas.dat).\n\n");
    } else {
        vga_set_color(0x0C);
        vga_puts("\n[!] Gagal menyimpan data ke RAMDisk.\n\n");
    }
    vga_set_color(0x0F);
}

/* =========================================================================
 * SUB-SHELL APLIKASI KAS (SESUAI FOTO REFERENSI)
 * ========================================================================= */
void finance_run(void) {
    /* Muat ulang data terbaru dari RAMDisk */
    finance_load_from_ramdisk();

    /* Thread logger dimulai saat aplikasi dibuka */
    int logger_id = task_register_logger_start();
    logbook_set_logger_active(1);
    logbook_add("MASUK", -1, ' ', "Manajemen kas dibuka");

    char choice_buf[16];

    for (;;) {
        if (protect_get_mode() == MODE_KERNEL) {
            vga_set_color(0x0C);
            vga_puts("=== MANAJEMEN KAS (mode KERNEL) ===\n\n");
        } else {
            vga_set_color(0x09); /* Biru terang untuk mode USER */
            vga_puts("=== MANAJEMEN KAS (mode USER) ===\n\n");
        }
        vga_set_color(0x0F);

        vga_puts(" 1. Lihat daftar transaksi\n");
        vga_puts(" 2. Tambah transaksi\n");
        vga_puts(" 3. Ubah nominal\n");
        vga_puts(" 4. Hapus transaksi\n");
        vga_puts(" 5. Simpan data\n");
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
            vga_puts("Keluar dari aplikasi kas.\n");
            vga_set_color(0x0F);
            logbook_add("KELUAR", -1, ' ', "Manajemen kas ditutup");
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
        } else if (choice_buf[0] == '\0') {
            /* Enter kosong: lanjutkan */
            continue;
        } else {
            vga_set_color(0x0C);
            vga_puts("Pilihan tidak valid. Silakan pilih 0 - 5.\n\n");
            vga_set_color(0x0F);
        }
    }
}
