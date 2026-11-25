/*
 * kernel/gdt.c -- Global Descriptor Table minimal 32-bit.
 *
 * Null (0x00), Kernel Code (0x08), Kernel Data (0x10).
 * Seluruh sistem berjalan di ring 0; proteksi hak akses implementasinya
 * ada pada modul protection/protect.c (kontrol akses sederhana).
 */

#include <stdint.h>
#include "idt.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

extern void gdt_flush(gdt_ptr_t *gp);

static gdt_entry_t gdt_entries[3];
static gdt_ptr_t   gdt_ptr;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t granularity) {
    gdt_entry_t *e = &gdt_entries[num];

    e->base_low     = (uint16_t)(base & 0xFFFF);
    e->base_middle  = (uint8_t)((base >> 16) & 0xFF);
    e->base_high    = (uint8_t)((base >> 24) & 0xFF);
    e->limit_low    = (uint16_t)(limit & 0xFFFF);
    e->granularity  = (uint8_t)((limit >> 16) & 0x0F);
    e->granularity |= (uint8_t)(granularity & 0xF0);
    e->access       = access;
}

void gdt_init(void) {
    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);
    /* Kernel code: present, ring0, execute/read, 4 KB granularity, 32-bit */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
    /* Kernel data: present, ring0, read/write, 4 KB granularity, 32-bit */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entries) - 1);
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    gdt_flush(&gdt_ptr);
}
