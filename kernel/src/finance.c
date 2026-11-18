#include "finance.h"
#include "vga.h"

#define MAX_TRANSAKSI 20

char keyboard_getchar(void);
void vga_backspace(void);

typedef struct {
    int id;
    int aktif;       // 1 = ada, 0 = dihapus (soft delete)
    char tipe;       // '+' untuk masuk, '-' untuk keluar
    int nominal;
    char ket[32];
} transaksi_t;

static transaksi_t buku_kas[MAX_TRANSAKSI];
static int jumlah_data = 0;

// Helper: Perbandingan string
static int f_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int f_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return (*str == ' ' || *str == '\0');
}

// Helper: Mencetak angka integer ke layar VGA
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

// Helper: Mengambil angka dari awal string dan menggeser pointer ke argumen berikutnya
static int parse_int(const char** str) {
    const char* s = *str;
    while (*s == ' ') s++;
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    while (*s == ' ') s++;
    *str = s;
    return res;
}

// Helper: Menyalin teks keterangan
static void copy_ket(char* dest, const char* src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    if (i == 0) {
        const char* def = "Tanpa_ket";
        while (def[i] != '\0' && i < max_len - 1) {
            dest[i] = def[i];
            i++;
        }
    }
    dest[i] = '\0';
}

void finance_init(void) {
    for (int i = 0; i < MAX_TRANSAKSI; i++) {
        buku_kas[i].aktif = 0;
    }
    jumlah_data = 0;
}

// CREATE: Pemasukan & Pengeluaran
static void tambah_transaksi(char tipe, const char* args) {
    if (jumlah_data >= MAX_TRANSAKSI) {
        vga_set_color(0x0C);
        vga_puts("[!] Buku kas penuh (Maksimal 20 transaksi).\n");
        vga_set_color(0x0F);
        return;
    }

    int nominal = parse_int(&args);
    if (nominal <= 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Nominal harus lebih dari 0. Contoh: masuk 50000 Uang_saku\n");
        vga_set_color(0x0F);
        return;
    }

    int idx = jumlah_data++;
    buku_kas[idx].id = idx + 1;
    buku_kas[idx].aktif = 1;
    buku_kas[idx].tipe = tipe;
    buku_kas[idx].nominal = nominal;
    copy_ket(buku_kas[idx].ket, args, 32);

    vga_set_color(0x0A);
    vga_puts("[OK] ID #");
    print_int(buku_kas[idx].id);
    vga_puts(tipe == '+' ? " Pemasukan Rp" : " Pengeluaran Rp");
    print_int(nominal);
    vga_puts(" (");
    vga_puts(buku_kas[idx].ket);
    vga_puts(") dicatat.\n");
    vga_set_color(0x0F);
}

static void finance_masuk(const char* args) {
    tambah_transaksi('+', args);
}

static void finance_keluar(const char* args) {
    tambah_transaksi('-', args);
}

// READ: Cek Saldo
static void finance_saldo(void) {
    int total = 0;
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif) {
            if (buku_kas[i].tipe == '+') total += buku_kas[i].nominal;
            else total -= buku_kas[i].nominal;
        }
    }
    vga_puts("Total Saldo Saat Ini: ");
    vga_set_color(total >= 0 ? 0x0A : 0x0C);
    vga_puts("Rp");
    print_int(total);
    vga_puts("\n");
    vga_set_color(0x0F);
}

// READ: Cek Histori
static void finance_histori(void) {
    vga_set_color(0x0E);
    vga_puts("\n=== BUKU KAS WOWIT OS ===\n");
    vga_set_color(0x0F);

    int ada = 0;
    for (int i = 0; i < jumlah_data; i++) {
        if (buku_kas[i].aktif) {
            ada = 1;
            vga_puts("ID #");
            print_int(buku_kas[i].id);
            if (buku_kas[i].tipe == '+') {
                vga_set_color(0x0A);
                vga_puts(" [+] Rp");
            } else {
                vga_set_color(0x0C);
                vga_puts(" [-] Rp");
            }
            print_int(buku_kas[i].nominal);
            vga_set_color(0x0F);
            vga_puts(" - ");
            vga_puts(buku_kas[i].ket);
            vga_puts("\n");
        }
    }

    if (!ada) {
        vga_puts("(Belum ada catatan transaksi)\n");
    }
    vga_puts("-------------------------\n");
    finance_saldo();
    vga_puts("\n");
}

// UPDATE: Ubah nominal berdasarkan ID
static void finance_ubah(const char* args) {
    int id = parse_int(&args);
    int nominal_baru = parse_int(&args);

    if (id <= 0 || id > jumlah_data || !buku_kas[id - 1].aktif || nominal_baru <= 0) {
        vga_set_color(0x0C);
        vga_puts("[!] Format salah atau ID tidak ditemukan. Contoh: ubah 1 75000\n");
        vga_set_color(0x0F);
        return;
    }

    buku_kas[id - 1].nominal = nominal_baru;
    vga_set_color(0x0A);
    vga_puts("[OK] Transaksi ID #");
    print_int(id);
    vga_puts(" berhasil diubah menjadi Rp");
    print_int(nominal_baru);
    vga_puts("\n");
    vga_set_color(0x0F);
}

// DELETE: Hapus transaksi berdasarkan ID
static void finance_hapus(const char* args) {
    int id = parse_int(&args);

    if (id <= 0 || id > jumlah_data || !buku_kas[id - 1].aktif) {
        vga_set_color(0x0C);
        vga_puts("[!] ID tidak ditemukan. Contoh: hapus 1\n");
        vga_set_color(0x0F);
        return;
    }

    buku_kas[id - 1].aktif = 0;
    vga_set_color(0x0E);
    vga_puts("[OK] Transaksi ID #");
    print_int(id);
    vga_puts(" berhasil dihapus.\n");
    vga_set_color(0x0F);
}

// ==========================================
// SUB-SHELL APLIKASI KEUANGAN
// ==========================================
void finance_run(void) {
    vga_set_color(0x0E);
    vga_puts("\n=== APLIKASI PENCATAT KEUANGAN WOWIT OS ===\n");
    vga_set_color(0x0F);
    vga_puts("Ketik 'help' untuk bantuan perintah kas, atau 'exit' untuk keluar.\n\n");

    char buf[64];
    int len;

    for (;;) {
        vga_set_color(0x0A);
        vga_puts("wowit-os/kas> ");
        vga_set_color(0x0F);

        len = 0;
        for (;;) {
            char c = keyboard_getchar();
            if (c == '\n') { vga_putc('\n'); break; }
            if (c == '\b') {
                if (len > 0) { len--; vga_backspace(); }
            } else if (c >= 32 && c < 127 && len < 63) {
                buf[len++] = c;
                vga_putc(c);
            }
        }
        buf[len] = 0;

        if (buf[0] == 0) continue;

        if (f_streq(buf, "exit")) {
            vga_puts("Keluar dari aplikasi pencatat keuangan.\n\n");
            break;
        } else if (f_streq(buf, "help")) {
            vga_puts("Perintah Buku Kas:\n");
            vga_puts("  masuk <nom> <ket>  - Catat pemasukan (cth: masuk 50000 Gaji)\n");
            vga_puts("  keluar <nom> <ket> - Catat pengeluaran (cth: keluar 15000 Makan)\n");
            vga_puts("  histori            - Lihat semua transaksi & saldo\n");
            vga_puts("  saldo              - Cek total sisa saldo\n");
            vga_puts("  ubah <id> <nom>    - Ubah nominal (cth: ubah 1 60000)\n");
            vga_puts("  hapus <id>         - Hapus catatan (cth: hapus 1)\n");
            vga_puts("  clear              - Bersihkan layar\n");
            vga_puts("  exit               - Kembali ke shell utama WOWIT OS\n");
        } else if (f_starts_with(buf, "masuk")) {
            finance_masuk(buf + 5);
        } else if (f_starts_with(buf, "keluar")) {
            finance_keluar(buf + 6);
        } else if (f_streq(buf, "histori")) {
            finance_histori();
        } else if (f_streq(buf, "saldo")) {
            finance_saldo();
        } else if (f_starts_with(buf, "ubah")) {
            finance_ubah(buf + 4);
        } else if (f_starts_with(buf, "hapus")) {
            finance_hapus(buf + 5);
        } else if (f_streq(buf, "clear")) {
            vga_clear();
        } else {
            vga_puts("Perintah kas tidak dikenal. Ketik 'help' atau 'exit'.\n");
        }
    }
}