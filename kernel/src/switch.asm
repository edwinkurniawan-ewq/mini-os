; kernel/src/switch.asm  --  Low-level context switch
;
; void context_switch(tcb_t *old, tcb_t *new);
;
; old->esp is at offset 0 in the TCB struct.
; Saves callee-saved registers (EBX, ESI, EDI) + EBP onto old stack,
; stores ESP into old->esp, loads new->esp, restores registers, returns.
;
; If old == NULL: skip save (first switch from kernel bootstrap).

section .text
global _context_switch

_context_switch:
    push    ebp
    mov     ebp, esp            ; standard prologue

    mov     eax, [ebp + 8]      ; eax = old
    test    eax, eax
    jz      .load_new           ; old == NULL -> skip save

    ; --- save callee-saved registers ---
    push    ebx
    push    esi
    push    edi

    ; --- save ESP into old->esp (offset 0 in TCB) ---
    mov     [eax], esp

.load_new:
    mov     eax, [ebp + 12]     ; eax = new
    mov     esp, [eax]          ; esp = new->esp

    ; --- restore callee-saved registers ---
    pop     edi
    pop     esi
    pop     ebx

    pop     ebp                 ; restore EBP
    ret                         ; return into new thread's context
