// PolandOS — Interrupt Descriptor Table
#pragma once
#include "../../../../include/types.h"

typedef struct __attribute__((packed)) {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rdi, rsi, rbp, rdx, rcx, rbx, rax;
    u64 int_no, err_code;
    u64 rip, cs, rflags, rsp, ss;
} InterruptFrame;

typedef void (*isr_handler_t)(InterruptFrame *frame);

void idt_init(void);
void idt_register_handler(u8 vector, isr_handler_t handler);
void idt_set_ist(u8 vector, u8 ist);

// Exception names
extern const char *exception_names[32];
