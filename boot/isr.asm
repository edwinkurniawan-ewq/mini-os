; boot/isr.asm -- 256 entri handler interupsi untuk WOWIT OS.
;
; Setiap stub menumpuk err-code (0 bila CPU tidak menyediakannya) dan nomor
; vektor, lalu melompat ke _isr_common. Handler C menerima registers_t*.

section .text

extern _irq_dispatch

%macro ISR_NOERR 1
global _isr%1
_isr%1:
    push    dword 0
    push    dword %1
    jmp     _isr_common
%endmacro

%macro ISR_ERR 1
global _isr%1
_isr%1:
    push    dword %1
    jmp     _isr_common
%endmacro

%assign i 0
%rep 256
    %if i = 8 || i = 10 || i = 11 || i = 12 || i = 13 || i = 14 || i = 17
        ISR_ERR i
    %else
        ISR_NOERR i
    %endif
    %assign i i + 1
%endrep

_isr_common:
    pusha                       ; EAX ECX EDX EBX ESP EBP ESI EDI

    xor     eax, eax
    mov     ax, ds
    push    eax                 ; ds
    mov     ax, es
    push    eax                 ; es
    mov     ax, fs
    push    eax                 ; fs
    mov     ax, gs
    push    eax                 ; gs

    mov     ax, 0x10            ; selector data kernel
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    mov     eax, esp
    push    eax                 ; argumen: registers_t *
    call    _irq_dispatch
    add     esp, 4

; Titik kembali untuk task yang baru dibuat: context_switch() me-RET ke
; alamat ini dengan stack frame interupsi yang sudah disiapkan task.c.
global _isr_restore
_isr_restore:
    pop     gs
    pop     fs
    pop     es
    pop     ds
    popa

    add     esp, 8              ; buang int_no + err_code
    iretd                       ; kembali ke konteks yang terinterupsi

; Tabel alamat seluruh stub, dipakai idt.c saat mengisi IDT.
global _isr_table
_isr_table:
%assign j 0
%rep 256
    dd _isr %+ j
    %assign j j + 1
%endrep
