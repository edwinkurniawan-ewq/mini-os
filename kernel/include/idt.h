#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/*
 * Vektor software interrupt yang dipakai sebagai "yield" kooperatif.
 * PIC 8259 di-remap ke 32..47, jadi 48 aman dan tidak bentrok dengan IRQ.
 */
#define IDT_YIELD_VECTOR 48

/* Stack frame yang disimpan isr.asm sebelum memanggil irq_dispatch(). */
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

void gdt_init(void);
void idt_init(void);
void irq_dispatch(registers_t *regs);

#endif /* IDT_H */
