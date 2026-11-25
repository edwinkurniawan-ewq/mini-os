; boot/flush.asm -- pemuat tabel descriptor x86 32-bit.
;
; void gdt_flush(struct gdt_ptr *gp);
; void idt_flush(struct idt_ptr *ip);

section .text

global _gdt_flush
global _idt_flush

_gdt_flush:
    mov     eax, [esp + 4]
    lgdt    [eax]

    mov     ax, 0x10            ; kernel data selector
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    jmp     0x08:.flush         ; kernel code selector, singkirkan pipeline
.flush:
    ret

_idt_flush:
    mov     eax, [esp + 4]
    lidt    [eax]
    ret
