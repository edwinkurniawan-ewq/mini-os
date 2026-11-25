#include "vga.h"
#include "cli.h"
#include "finance.h"
#include "protect.h"
#include "logbook.h"
#include "vfs.h"
#include "task.h"
#include "thread.h"
#include "rr.h"
#include "monitor.h"
#include "memory.h"
#include "timer.h"
#include "keyboard.h"
#include "sync.h"
#include "idt.h"

void kernel_main(void) {
    vga_set_color(0x0F);
    vga_clear();

    /* GDT -> IDT/PIC dipasang lebih dulu, tetapi interupsi baru dinyalakan
     * setelah semua struktur data kernel siap (lihat sti di bawah). */
    gdt_init();
    idt_init();
    memory_init();
    timer_init();
    keyboard_init();

    /* Inisialisasi thread, task, sync, protect, vfs, logbook, finance */
    thread_init();
    task_init();
    sync_init();
    protect_init();
    vfs_init();
    logbook_init();
    finance_init();
    rr_init();
    monitor_init();

    cli_init();

    /* Semua primitive (mutex, critical section, TCB) sudah atomik. */
    __asm__ volatile ("sti");

    cli_run();                  /* loop tak berhenti: tidak pernah kembali */
}
