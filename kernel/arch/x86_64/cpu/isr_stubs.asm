; PolandOS — Procedury obsługi przerwań (ISR stubs) — wszystkie 256
; x86-64 NASM
[BITS 64]

extern isr_common_handler
global isr_stub_table

; ─── Macros ──────────────────────────────────────────────────────────────────

; ISR without error code: push dummy 0, then vector number
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common_stub
%endmacro

; ISR with error code already on stack: push vector number
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common_stub
%endmacro

; ─── CPU Exceptions (0–31) ───────────────────────────────────────────────────
ISR_NOERRCODE 0    ; #DE Divide-by-Zero
ISR_NOERRCODE 1    ; #DB Debug
ISR_NOERRCODE 2    ; NMI
ISR_NOERRCODE 3    ; #BP Breakpoint
ISR_NOERRCODE 4    ; #OF Overflow
ISR_NOERRCODE 5    ; #BR Bound Range Exceeded
ISR_NOERRCODE 6    ; #UD Invalid Opcode
ISR_NOERRCODE 7    ; #NM Device Not Available
ISR_ERRCODE   8    ; #DF Double Fault       (error code = 0)
ISR_NOERRCODE 9    ; Coprocessor Segment Overrun (legacy, no error code)
ISR_ERRCODE   10   ; #TS Invalid TSS
ISR_ERRCODE   11   ; #NP Segment Not Present
ISR_ERRCODE   12   ; #SS Stack-Segment Fault
ISR_ERRCODE   13   ; #GP General Protection Fault
ISR_ERRCODE   14   ; #PF Page Fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; #MF x87 FP Exception
ISR_ERRCODE   17   ; #AC Alignment Check
ISR_NOERRCODE 18   ; #MC Machine Check
ISR_NOERRCODE 19   ; #XM SIMD FP Exception
ISR_NOERRCODE 20   ; #VE Virtualization Exception
ISR_NOERRCODE 21   ; Reserved
ISR_NOERRCODE 22   ; Reserved
ISR_NOERRCODE 23   ; Reserved
ISR_NOERRCODE 24   ; Reserved
ISR_NOERRCODE 25   ; Reserved
ISR_NOERRCODE 26   ; Reserved
ISR_NOERRCODE 27   ; Reserved
ISR_NOERRCODE 28   ; Reserved
ISR_NOERRCODE 29   ; Reserved
ISR_ERRCODE   30   ; #SX Security Exception
ISR_NOERRCODE 31   ; Reserved

; ─── Hardware IRQs and software interrupts (32–255) ──────────────────────────
ISR_NOERRCODE 32
ISR_NOERRCODE 33
ISR_NOERRCODE 34
ISR_NOERRCODE 35
ISR_NOERRCODE 36
ISR_NOERRCODE 37
ISR_NOERRCODE 38
ISR_NOERRCODE 39
ISR_NOERRCODE 40
ISR_NOERRCODE 41
ISR_NOERRCODE 42
ISR_NOERRCODE 43
ISR_NOERRCODE 44
ISR_NOERRCODE 45
ISR_NOERRCODE 46
ISR_NOERRCODE 47
ISR_NOERRCODE 48
ISR_NOERRCODE 49
ISR_NOERRCODE 50
ISR_NOERRCODE 51
ISR_NOERRCODE 52
ISR_NOERRCODE 53
ISR_NOERRCODE 54
ISR_NOERRCODE 55
ISR_NOERRCODE 56
ISR_NOERRCODE 57
ISR_NOERRCODE 58
ISR_NOERRCODE 59
ISR_NOERRCODE 60
ISR_NOERRCODE 61
ISR_NOERRCODE 62
ISR_NOERRCODE 63
ISR_NOERRCODE 64
ISR_NOERRCODE 65
ISR_NOERRCODE 66
ISR_NOERRCODE 67
ISR_NOERRCODE 68
ISR_NOERRCODE 69
ISR_NOERRCODE 70
ISR_NOERRCODE 71
ISR_NOERRCODE 72
ISR_NOERRCODE 73
ISR_NOERRCODE 74
ISR_NOERRCODE 75
ISR_NOERRCODE 76
ISR_NOERRCODE 77
ISR_NOERRCODE 78
ISR_NOERRCODE 79
ISR_NOERRCODE 80
ISR_NOERRCODE 81
ISR_NOERRCODE 82
ISR_NOERRCODE 83
ISR_NOERRCODE 84
ISR_NOERRCODE 85
ISR_NOERRCODE 86
ISR_NOERRCODE 87
ISR_NOERRCODE 88
ISR_NOERRCODE 89
ISR_NOERRCODE 90
ISR_NOERRCODE 91
ISR_NOERRCODE 92
ISR_NOERRCODE 93
ISR_NOERRCODE 94
ISR_NOERRCODE 95
ISR_NOERRCODE 96
ISR_NOERRCODE 97
ISR_NOERRCODE 98
ISR_NOERRCODE 99
ISR_NOERRCODE 100
ISR_NOERRCODE 101
ISR_NOERRCODE 102
ISR_NOERRCODE 103
ISR_NOERRCODE 104
ISR_NOERRCODE 105
ISR_NOERRCODE 106
ISR_NOERRCODE 107
ISR_NOERRCODE 108
ISR_NOERRCODE 109
ISR_NOERRCODE 110
ISR_NOERRCODE 111
ISR_NOERRCODE 112
ISR_NOERRCODE 113
ISR_NOERRCODE 114
ISR_NOERRCODE 115
ISR_NOERRCODE 116
ISR_NOERRCODE 117
ISR_NOERRCODE 118
ISR_NOERRCODE 119
ISR_NOERRCODE 120
ISR_NOERRCODE 121
ISR_NOERRCODE 122
ISR_NOERRCODE 123
ISR_NOERRCODE 124
ISR_NOERRCODE 125
ISR_NOERRCODE 126
ISR_NOERRCODE 127
ISR_NOERRCODE 128
ISR_NOERRCODE 129
ISR_NOERRCODE 130
ISR_NOERRCODE 131
ISR_NOERRCODE 132
ISR_NOERRCODE 133
ISR_NOERRCODE 134
ISR_NOERRCODE 135
ISR_NOERRCODE 136
ISR_NOERRCODE 137
ISR_NOERRCODE 138
ISR_NOERRCODE 139
ISR_NOERRCODE 140
ISR_NOERRCODE 141
ISR_NOERRCODE 142
ISR_NOERRCODE 143
ISR_NOERRCODE 144
ISR_NOERRCODE 145
ISR_NOERRCODE 146
ISR_NOERRCODE 147
ISR_NOERRCODE 148
ISR_NOERRCODE 149
ISR_NOERRCODE 150
ISR_NOERRCODE 151
ISR_NOERRCODE 152
ISR_NOERRCODE 153
ISR_NOERRCODE 154
ISR_NOERRCODE 155
ISR_NOERRCODE 156
ISR_NOERRCODE 157
ISR_NOERRCODE 158
ISR_NOERRCODE 159
ISR_NOERRCODE 160
ISR_NOERRCODE 161
ISR_NOERRCODE 162
ISR_NOERRCODE 163
ISR_NOERRCODE 164
ISR_NOERRCODE 165
ISR_NOERRCODE 166
ISR_NOERRCODE 167
ISR_NOERRCODE 168
ISR_NOERRCODE 169
ISR_NOERRCODE 170
ISR_NOERRCODE 171
ISR_NOERRCODE 172
ISR_NOERRCODE 173
ISR_NOERRCODE 174
ISR_NOERRCODE 175
ISR_NOERRCODE 176
ISR_NOERRCODE 177
ISR_NOERRCODE 178
ISR_NOERRCODE 179
ISR_NOERRCODE 180
ISR_NOERRCODE 181
ISR_NOERRCODE 182
ISR_NOERRCODE 183
ISR_NOERRCODE 184
ISR_NOERRCODE 185
ISR_NOERRCODE 186
ISR_NOERRCODE 187
ISR_NOERRCODE 188
ISR_NOERRCODE 189
ISR_NOERRCODE 190
ISR_NOERRCODE 191
ISR_NOERRCODE 192
ISR_NOERRCODE 193
ISR_NOERRCODE 194
ISR_NOERRCODE 195
ISR_NOERRCODE 196
ISR_NOERRCODE 197
ISR_NOERRCODE 198
ISR_NOERRCODE 199
ISR_NOERRCODE 200
ISR_NOERRCODE 201
ISR_NOERRCODE 202
ISR_NOERRCODE 203
ISR_NOERRCODE 204
ISR_NOERRCODE 205
ISR_NOERRCODE 206
ISR_NOERRCODE 207
ISR_NOERRCODE 208
ISR_NOERRCODE 209
ISR_NOERRCODE 210
ISR_NOERRCODE 211
ISR_NOERRCODE 212
ISR_NOERRCODE 213
ISR_NOERRCODE 214
ISR_NOERRCODE 215
ISR_NOERRCODE 216
ISR_NOERRCODE 217
ISR_NOERRCODE 218
ISR_NOERRCODE 219
ISR_NOERRCODE 220
ISR_NOERRCODE 221
ISR_NOERRCODE 222
ISR_NOERRCODE 223
ISR_NOERRCODE 224
ISR_NOERRCODE 225
ISR_NOERRCODE 226
ISR_NOERRCODE 227
ISR_NOERRCODE 228
ISR_NOERRCODE 229
ISR_NOERRCODE 230
ISR_NOERRCODE 231
ISR_NOERRCODE 232
ISR_NOERRCODE 233
ISR_NOERRCODE 234
ISR_NOERRCODE 235
ISR_NOERRCODE 236
ISR_NOERRCODE 237
ISR_NOERRCODE 238
ISR_NOERRCODE 239
ISR_NOERRCODE 240
ISR_NOERRCODE 241
ISR_NOERRCODE 242
ISR_NOERRCODE 243
ISR_NOERRCODE 244
ISR_NOERRCODE 245
ISR_NOERRCODE 246
ISR_NOERRCODE 247
ISR_NOERRCODE 248
ISR_NOERRCODE 249
ISR_NOERRCODE 250
ISR_NOERRCODE 251
ISR_NOERRCODE 252
ISR_NOERRCODE 253
ISR_NOERRCODE 254
ISR_NOERRCODE 255

; ─── Common stub ─────────────────────────────────────────────────────────────
; Stack layout on entry to isr_common_stub (grows downward):
;
;   Pushed by our macros (top of stack):
;   [rsp+0]  = int_no       (vector number)
;   [rsp+8]  = err_code     (CPU error code, or dummy 0 pushed by ISR_NOERRCODE)
;
;   Pushed by the CPU on exception/interrupt:
;   [rsp+16] = RIP
;   [rsp+24] = CS
;   [rsp+32] = RFLAGS
;   [rsp+40] = RSP          (only on privilege-level change)
;   [rsp+48] = SS
;
; Macro summary:
;   ISR_NOERRCODE: push 0 (dummy), push int_no  → stack as above
;   ISR_ERRCODE:   CPU already pushed err_code, we push int_no
;
; We then save all GP registers to form an InterruptFrame struct in C:
;   r15..r8, rdi, rsi, rbp, rdx, rcx, rbx, rax  (15 regs × 8 bytes)
;   followed by int_no and err_code already on stack

isr_common_stub:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; RSP now points to InterruptFrame (r15 at bottom)
    mov rdi, rsp            ; pass frame pointer as first arg (System V AMD64 ABI)

    ; Align stack to 16 bytes before call (ABI requirement)
    mov rbp, rsp
    and rsp, ~0xF
    call isr_common_handler
    mov rsp, rbp

    ; Restore general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove int_no and err_code from stack
    add rsp, 16

    iretq

; ─── Stub pointer table (used by idt.c to set gates) ─────────────────────────
section .data
isr_stub_table:
    dq isr0,   isr1,   isr2,   isr3,   isr4,   isr5,   isr6,   isr7
    dq isr8,   isr9,   isr10,  isr11,  isr12,  isr13,  isr14,  isr15
    dq isr16,  isr17,  isr18,  isr19,  isr20,  isr21,  isr22,  isr23
    dq isr24,  isr25,  isr26,  isr27,  isr28,  isr29,  isr30,  isr31
    dq isr32,  isr33,  isr34,  isr35,  isr36,  isr37,  isr38,  isr39
    dq isr40,  isr41,  isr42,  isr43,  isr44,  isr45,  isr46,  isr47
    dq isr48,  isr49,  isr50,  isr51,  isr52,  isr53,  isr54,  isr55
    dq isr56,  isr57,  isr58,  isr59,  isr60,  isr61,  isr62,  isr63
    dq isr64,  isr65,  isr66,  isr67,  isr68,  isr69,  isr70,  isr71
    dq isr72,  isr73,  isr74,  isr75,  isr76,  isr77,  isr78,  isr79
    dq isr80,  isr81,  isr82,  isr83,  isr84,  isr85,  isr86,  isr87
    dq isr88,  isr89,  isr90,  isr91,  isr92,  isr93,  isr94,  isr95
    dq isr96,  isr97,  isr98,  isr99,  isr100, isr101, isr102, isr103
    dq isr104, isr105, isr106, isr107, isr108, isr109, isr110, isr111
    dq isr112, isr113, isr114, isr115, isr116, isr117, isr118, isr119
    dq isr120, isr121, isr122, isr123, isr124, isr125, isr126, isr127
    dq isr128, isr129, isr130, isr131, isr132, isr133, isr134, isr135
    dq isr136, isr137, isr138, isr139, isr140, isr141, isr142, isr143
    dq isr144, isr145, isr146, isr147, isr148, isr149, isr150, isr151
    dq isr152, isr153, isr154, isr155, isr156, isr157, isr158, isr159
    dq isr160, isr161, isr162, isr163, isr164, isr165, isr166, isr167
    dq isr168, isr169, isr170, isr171, isr172, isr173, isr174, isr175
    dq isr176, isr177, isr178, isr179, isr180, isr181, isr182, isr183
    dq isr184, isr185, isr186, isr187, isr188, isr189, isr190, isr191
    dq isr192, isr193, isr194, isr195, isr196, isr197, isr198, isr199
    dq isr200, isr201, isr202, isr203, isr204, isr205, isr206, isr207
    dq isr208, isr209, isr210, isr211, isr212, isr213, isr214, isr215
    dq isr216, isr217, isr218, isr219, isr220, isr221, isr222, isr223
    dq isr224, isr225, isr226, isr227, isr228, isr229, isr230, isr231
    dq isr232, isr233, isr234, isr235, isr236, isr237, isr238, isr239
    dq isr240, isr241, isr242, isr243, isr244, isr245, isr246, isr247
    dq isr248, isr249, isr250, isr251, isr252, isr253, isr254, isr255
