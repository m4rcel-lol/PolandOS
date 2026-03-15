; PolandOS — Punkt startowy jadra
; Jadro Orzel — witamy w x86_64
[BITS 64]

section .text

global _start
extern kmain

_start:
    ; Set up a stack (use the stack Limine provided or set our own)
    ; Limine gives us a valid stack, but let's set our own for safety
    lea rsp, [rel kernel_stack_top]

    ; Clear direction flag
    cld

    ; Call kernel main
    call kmain

    ; Should never return, but just in case
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
kernel_stack:
    resb 65536  ; 64K stack
kernel_stack_top:
