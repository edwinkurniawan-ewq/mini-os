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

/* Implementasi stub di kernel/stubs.c (GDT/IDT belum dipakai QEMU). */
void gdt_init(void);
void idt_init(void);

void kernel_main(void) {
    vga_set_color(0x0F);
    vga_clear();

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
    cli_run();

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
