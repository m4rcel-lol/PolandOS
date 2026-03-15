; PolandOS — Przeładowanie GDT i TSS
[BITS 64]
global gdt_flush
global tss_flush

; gdt_flush(GDTR *gdtr_ptr)
; rdi = pointer to GDTR struct
gdt_flush:
    lgdt [rdi]
    ; Reload CS via far return trick
    pop rax              ; save return address
    push qword 0x08      ; kernel code segment selector
    push rax
    retfq                ; far return: pops RIP and CS

; tss_flush — load TSS selector into TR
tss_flush:
    mov ax, 0x28         ; TSS descriptor is at index 5, offset 0x28
    ltr ax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
