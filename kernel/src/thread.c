#include "kernel.h"
#include "thread.h"

void vga_putc(char c); // <-- TAMBAHKAN BARIS INI

#define MAX_THREADS 2

// Array struktur data thread
tcb_t threads[MAX_THREADS];

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
    threads[0].state = THREAD_NEW;
    threads[0].task = task1;

    // Inisialisasi Thread 2 (Tugas: task2)
    threads[1].id = 1;
    threads[1].state = THREAD_NEW;
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
        threads[i].state = THREAD_READY;
        print_thread_state(threads[i].id, "READY");
    }
    vga_puts("\n");

    // 3. Eksekusi satu per satu
    for(int i = 0; i < MAX_THREADS; i++) {
        // Alihkan ke RUNNING
        threads[i].state = THREAD_RUNNING;
        print_thread_state(threads[i].id, "RUNNING");
        
        // Eksekusi fungsi tugas thread
        threads[i].task();

        // Alihkan ke WAITING (Simulasi I/O)
        threads[i].state = THREAD_WAITING;
        print_thread_state(threads[i].id, "WAITING (Menunggu I/O selesai)");

        // Alihkan ke TERMINATED (Selesai)
        threads[i].state = THREAD_TERMINATED;
        print_thread_state(threads[i].id, "TERMINATED");
        vga_puts("\n");
    }

    // Reset ulang state ke NEW agar bisa dites berkali-kali di CLI
    thread_init(); 
}