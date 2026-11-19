#include "kernel.h"
#include "thread.h"

void vga_putc(char c); // <-- TAMBAHKAN BARIS INI

#define MAX_THREADS 2

typedef enum {
    CPMK_THREAD_NEW = 0,
    CPMK_THREAD_READY,
    CPMK_THREAD_RUNNING,
    CPMK_THREAD_WAITING,
    CPMK_THREAD_TERMINATED
} cpmk_state_t;

typedef struct {
    int id;
    cpmk_state_t state;
    void (*task)(void);
} cpmk_thread_t;

// Array struktur data thread
static cpmk_thread_t threads[MAX_THREADS];

// Helper untuk mencetak status ke layar
static void print_thread_state(int id, const char* state_name) {
    vga_puts("[Thread ");
    vga_putc('1' + id); // Cetak angka 1 atau 2
    vga_puts("] Status berubah ke: ");
    vga_set_color(0x0E); // Kuning untuk status
    vga_puts(state_name);
    vga_set_color(0x09); // Kembali ke biru terang
    vga_puts("\n");
}

// Tugas spesifik untuk Thread 1
static void task1(void) {
    vga_puts("   -> Mengeksekusi Tugas 1: Memproses perhitungan algoritma CPU...\n");
}

// Tugas spesifik untuk Thread 2
static void task2(void) {
    vga_puts("   -> Mengeksekusi Tugas 2: Memeriksa dan membersihkan memori...\n");
}

void thread_init(void) {
    // Inisialisasi Thread 1 (Tugas: task1)
    threads[0].id = 0;
    threads[0].state = CPMK_THREAD_NEW;
    threads[0].task = task1;

    // Inisialisasi Thread 2 (Tugas: task2)
    threads[1].id = 1;
    threads[1].state = CPMK_THREAD_NEW;
    threads[1].task = task2;
}

// Fungsi pengujian (akan dipanggil dari CLI)
void run_thread_demo(void) {
    vga_puts("\n--- DEMO PENGELOLAAN THREAD (CPMK 2) ---\n\n");
    
    // 1. Tampilkan status NEW
    for(int i = 0; i < MAX_THREADS; i++) {
        print_thread_state(threads[i].id, "NEW");
    }
    vga_puts("\n");

    // 2. Tampilkan status READY (Dimasukkan ke antrean)
    for(int i = 0; i < MAX_THREADS; i++) {
        threads[i].state = CPMK_THREAD_READY;
        print_thread_state(threads[i].id, "READY");
    }
    vga_puts("\n");

    // 3. Eksekusi satu per satu
    for(int i = 0; i < MAX_THREADS; i++) {
        // Alihkan ke RUNNING
        threads[i].state = CPMK_THREAD_RUNNING;
        print_thread_state(threads[i].id, "RUNNING");
        
        // Eksekusi fungsi tugas thread
        threads[i].task();

        // Alihkan ke WAITING (Simulasi I/O)
        threads[i].state = CPMK_THREAD_WAITING;
        print_thread_state(threads[i].id, "WAITING (Menunggu I/O selesai)");

        // Alihkan ke TERMINATED (Selesai)
        threads[i].state = CPMK_THREAD_TERMINATED;
        print_thread_state(threads[i].id, "TERMINATED");
        vga_puts("\n");
    }

    // Reset ulang state ke NEW agar bisa dites berkali-kali di CLI
    thread_init(); 
}

// ==========================================
// KODE UNTUK CPMK 5 (DEADLOCK DEMO)
// ==========================================

// Struktur Mutex Sederhana
typedef struct {
    int locked;
    int owner_id; // Menyimpan ID Thread yang sedang memegang kunci
} mini_mutex_t;

// Deklarasi 2 Mutex
mini_mutex_t mtx_brankas = {0, 0};
mini_mutex_t mtx_buku = {0, 0};

// Fungsi untuk mencoba mengunci Mutex
static void try_lock(mini_mutex_t* mtx, int thread_id, const char* resource_name) {
    vga_puts("   -> [Thread ");
    vga_putc('0' + thread_id);
    vga_puts("] Mencoba akses ");
    vga_puts(resource_name);
    vga_puts("...\n");

    if (mtx->locked == 1) {
        vga_set_color(0x0C); // Warna Merah
        vga_puts("      [!] DITOLAK: Sedang dikunci oleh Thread ");
        vga_putc('0' + mtx->owner_id);
        vga_puts(" (Masuk state WAITING)\n");
        vga_set_color(0x09); // Kembali ke Biru Terang
    } else {
        mtx->locked = 1;
        mtx->owner_id = thread_id;
        vga_set_color(0x0A); // Warna Hijau
        vga_puts("      [v] BERHASIL: Kunci dipegang oleh Thread ");
        vga_putc('0' + thread_id);
        vga_puts("\n");
        vga_set_color(0x09); // Kembali ke Biru Terang
    }
}

// Fungsi utama yang akan dipanggil di CLI
void run_deadlock_demo(void) {
    vga_puts("\n=== SIMULASI DEADLOCK (CPMK 5) ===\n");
    vga_puts("Kasus: 2 Kasir berebut Brankas (M1) dan Buku Kas (M2)\n\n");

    // Reset Mutex
    mtx_brankas.locked = 0; mtx_buku.locked = 0;

    vga_set_color(0x0E); vga_puts("[SKENARIO 1: TERJADINYA DEADLOCK]\n"); vga_set_color(0x09);
    try_lock(&mtx_brankas, 1, "Brankas (M1)");  // Thread 1 pegang M1
    try_lock(&mtx_buku, 2, "Buku Kas (M2)");    // Thread 2 pegang M2
    
    // Disinilah bencana terjadi
    try_lock(&mtx_buku, 1, "Buku Kas (M2)");    // Thread 1 butuh M2 (Tapi dipegang T2)
    try_lock(&mtx_brankas, 2, "Brankas (M1)");  // Thread 2 butuh M1 (Tapi dipegang T1)

    vga_set_color(0x0C); 
    vga_puts("\n[!!] FATAL ERROR: DEADLOCK TERJADI (Circular Wait) [!!]\n");
    vga_puts("     Kedua thread saling mengunci dan menunggu selamanya.\n\n");
    vga_set_color(0x09);


    vga_set_color(0x0E); vga_puts("[SKENARIO 2: SOLUSI (RESOURCE ORDERING)]\n"); vga_set_color(0x09);
    vga_puts("Aturan Baru: Semua Thread HARUS antre Brankas (M1) dulu!\n");
    
    // Reset Mutex
    mtx_brankas.locked = 0; mtx_buku.locked = 0;

    try_lock(&mtx_brankas, 1, "Brankas (M1)");  // Thread 1 pegang M1
    try_lock(&mtx_brankas, 2, "Brankas (M1)");  // Thread 2 mencoba M1 (TERTOLAK/WAITING)
    
    vga_puts("\n   (Karena Thread 2 menunggu, Thread 1 bisa lanjut)\n");
    try_lock(&mtx_buku, 1, "Buku Kas (M2)");    // Thread 1 pegang M2
    
    vga_puts("\n   -> [Thread 1] Selesai transaksi! Melepas M1 & M2...\n");
    mtx_brankas.locked = 0; mtx_buku.locked = 0; // Thread 1 melepaskan kunci

    vga_puts("\n   (Sekarang Thread 2 bangun dan bisa masuk)\n");
    try_lock(&mtx_brankas, 2, "Brankas (M1)");  // Thread 2 pegang M1
    try_lock(&mtx_buku, 2, "Buku Kas (M2)");    // Thread 2 pegang M2
    
    vga_set_color(0x0A);
    vga_puts("\n[v] SISTEM AMAN: Deadlock berhasil dicegah!\n");
    vga_set_color(0x09);
}