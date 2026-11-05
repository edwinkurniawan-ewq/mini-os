; kernel/src/switch.asm  --  Low-level context switch
;
; Week 2 implementation.  Signature (C side):
;   void context_switch(tcb_t *old, tcb_t *new);
;
; Saves callee-saved registers + ESP of `old` TCB,
; restores registers + ESP of `new` TCB, then returns
; into the new thread's execution context.
;
; ponytail: full implementation in Week 2 after IDT + timer IRQ wired
section .text
; (no exported symbols yet — _context_switch added in Week 2)
