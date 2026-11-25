; process/switch.asm -- Low-level context switch untuk scheduler preemptive.
;
; void context_switch(tcb_t *old, tcb_t *new);
;
; Dipanggil DARI DALAM handler interupsi (IRQ0 atau int 0x30). Setiap task
; memiliki interrupt frame lengkap di stack-nya, sehingga setelah switch
; epilogue isr.asm melakukan popa + iretd dan kembali ke task yang dipilih.
;
; Prolog menyimpan EBP/EBX/ESI/EDI sesuai ABI cdecl; tcb->esp menyimpan
; posisi ESP setelah keempat register tersebut ditumpuk.

section .text
global _context_switch

_context_switch:
    push    ebp
    push    ebx
    push    esi
    push    edi

    mov     eax, [esp + 20]     ; argumen pertama: old
    test    eax, eax
    jz      .load_new
    mov     [eax], esp          ; simpan ESP (termasuk 4 regs callee-saved)

.load_new:
    mov     eax, [esp + 24]     ; argumen kedua: new
    mov     esp, [eax]

    pop     edi
    pop     esi
    pop     ebx
    pop     ebp
    ret                         ; lanjutkan eksekusi task baru
