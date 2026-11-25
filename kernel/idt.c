/*
 * kernel/idt.c -- Interrupt Descriptor Table + PIC 8259 remapping.
 *
 * IRQ 0..15 diremap ke vektor 32..47. IRQ0 (PIT) adalah satu-satunya
 * interupsi hardware yang di-unmask: keyboard dan mouse sengaja tetap
 * di-polling lewat port 0x60/0x64 di drivers/keyboard.c.
 * Vektor 48 dipakai sebagai software interrupt "yield" scheduler.
 */

#include <stdint.h>
#include "idt.h"
#include "io.h"
#include "kernel.h"
#include "vga.h"
#include "task.h"
#include "timer.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

extern void idt_flush(idt_ptr_t *ip);
extern uint32_t isr_table[256];

static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;

static void idt_set_gate(int num, uint32_t base, uint16_t selector,
                         uint8_t flags) {
    idt_entry_t *e = &idt_entries[num];

    e->base_low  = (uint16_t)(base & 0xFFFF);
    e->base_high = (uint16_t)((base >> 16) & 0xFFFF);
    e->selector  = selector;
    e->zero      = 0;
    e->flags     = flags;
}

static void pic_init(void) {
    /* ICW1: init + cascade */
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    /* ICW2: vector offset */
    outb(PIC1_DATA, 0x20);      /* IRQ0  -> vector 32 */
    outb(PIC2_DATA, 0x28);      /* IRQ8  -> vector 40 */
    /* ICW3: master slave on IRQ2, slave cascade id 2 */
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    /* OCW1: mask semua, lalu unmask IRQ0 saja */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    outb(PIC1_DATA, 0xFE);
}

void idt_init(void) {
    pic_init();

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, isr_table[i], 0x08, 0x8E);
    }

    idt_ptr.limit = (uint16_t)(sizeof(idt_entries) - 1);
    idt_ptr.base  = (uint32_t)&idt_entries;

    idt_flush(&idt_ptr);
}

void irq_dispatch(registers_t *regs) {
    if (regs->int_no == 32) {
        /* EOI dikirim SEBELUM timer_tick() karena timer_tick() bisa
         * melakukan context switch ke task lain. */
        outb(PIC1_CMD, PIC_EOI);
        timer_tick();
        return;
    }

    if (regs->int_no == IDT_YIELD_VECTOR) {
        schedule();             /* yield kooperatif via software interrupt */
        return;
    }

    if (regs->int_no >= 32 && regs->int_no <= 47) {
        outb(PIC1_CMD, PIC_EOI);
        if (regs->int_no >= 40) outb(PIC2_CMD, PIC_EOI);
        return;                 /* IRQ lain tidak dipakai / masih di-mask */
    }

    /* CPU exception: tampilkan informasi minimal lalu hentikan kernel. */
    vga_set_color(0x0C);
    vga_puts("\n[EXCEPTION] Interrupt: ");
    vga_put_int((int)regs->int_no);
    vga_puts(" Error: ");
    vga_put_int((int)regs->err_code);
    vga_puts(" EIP: ");
    vga_put_int((int)regs->eip);
    vga_puts("\n");
    vga_set_color(0x0F);

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
