#include "thread.h"
#include "vga.h"

void cli_init(void);
void cli_run(void);
void gdt_init(void);
void idt_init(void);
void keyboard_init(void);
void memory_init(void);
void thread_init(void);
void timer_init(void);
void vfs_init(void);
void sync_init(void);
void protect_init(void);

void kernel_main(void) {
    vga_set_color(0x0F);
    vga_clear();
    vga_puts("KERNEL HIDUP\n");

    gdt_init();
    idt_init();
    memory_init();
    timer_init();
    keyboard_init();
<<<<<<< HEAD
    
    // Ini yang diubah dari task_init() menjadi thread_init()
    thread_init(); 
    
=======
    task_init();
    sync_init();
    protect_init();
>>>>>>> origin/main
    vfs_init();

    vga_puts("INIT SELESAI\n");
    cli_init();
    cli_run();
<<<<<<< HEAD

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
=======
}
>>>>>>> origin/main
