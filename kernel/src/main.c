#include "kernel.h"
#include "vga.h"
#include "finance.h"
#include "protect.h"
#include "logbook.h"
#include "vfs.h"
#include "task.h"
#include "thread.h"

void cli_init(void);
void cli_run(void);
void gdt_init(void);
void idt_init(void);
void keyboard_init(void);
void memory_init(void);
void thread_init(void);
void timer_init(void);
void sync_init(void);

void kernel_main(void) {
    vga_set_color(0x0F);
    vga_clear();

    gdt_init();
    idt_init();
    memory_init();
    timer_init();
    keyboard_init();
    
    // Inisialisasi thread, task, sync, protect, vfs, logbook, finance
    thread_init(); 
    task_init();
    sync_init();
    protect_init();
    vfs_init();
    logbook_init();
    finance_init();

    cli_init();
    cli_run();

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
