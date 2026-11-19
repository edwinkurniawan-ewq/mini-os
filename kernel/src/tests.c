#include "tests.h"
#include "task.h"
#include "kernel.h"
#include "thread.h"

/*
 * Test Harness untuk Pengujian Proyek Mini OS
 * Sesuai pembagian tugas Roadmap Kelompok:
 *  - PIC 1 (Edwin) : Core, CLI, Driver, Kas App, Integrasi
 *  - PIC 2 (Rasya) : Concurrency, Thread (CPMK 2), Deadlock (CPMK 5), Proteksi
 *  - PIC 3 (Andika): Memori Dinamis (CPMK 6), VFS RAMDisk (CPMK 7)
 */

void test_tasks(void) {
    vga_puts("[TEST] Uji coba Task & Scheduler...\n");
    run_thread_demo();
}

void test_sched(void) {
    vga_puts("[TEST] Uji coba Thread Lifecycle (CPMK 2):\n");
    run_thread_demo();
}

void test_deadlock(void) {
    /* CPMK 5: Sudah selesai dikerjakan */
    run_deadlock_demo();
}

void test_race(void) {
    vga_puts("[INFO] Modul pengujian race condition & semaphore sedang dalam pengerjaan oleh Rasya (PIC 2).\n");
}

void test_malloc(void) {
    vga_puts("[INFO] Modul alokator memori dinamis (kmalloc/kfree) sedang dalam pengerjaan oleh Andika (PIC 3).\n");
}

void test_vfs(void) {
    vga_puts("[INFO] Modul Virtual File System (VFS RAMDisk) sedang dalam pengerjaan oleh Andika (PIC 3).\n");
}

void test_protect(void) {
    vga_puts("[INFO] Skema proteksi memori & ring privilege sedang dalam pengerjaan oleh Rasya (PIC 2).\n");
}
