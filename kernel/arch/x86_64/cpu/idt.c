// PolandOS — IDT (Interrupt Descriptor Table)
#include "idt.h"
#include "apic.h"
#include "../../../lib/panic.h"
#include "../../../lib/printf.h"
#include "../../../../include/types.h"

// ─── IDT Gate (16 bytes) ──────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    u16 offset_lo;
    u16 selector;
    u8  ist;        // bits [2:0] = IST index, rest reserved
    u8  type_attr;  // P | DPL(2) | 0 | Type(4)
    u16 offset_mid;
    u32 offset_hi;
    u32 zero;
} IDTGate;

typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} IDTR;

// ─── Exception names ──────────────────────────────────────────────────────────
const char *exception_names[32] = {
    "Divide-by-Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt (NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved (15)",
    "x87 FP Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD FP Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Reserved (21)",
    "Reserved (22)",
    "Reserved (23)",
    "Reserved (24)",
    "Reserved (25)",
    "Reserved (26)",
    "Reserved (27)",
    "Reserved (28)",
    "Reserved (29)",
    "Security Exception (#SX)",
    "Reserved (31)",
};

// ─── IDT storage ─────────────────────────────────────────────────────────────
static IDTGate   idt_entries[256];
static IDTR      idtr;
static isr_handler_t handlers[256];

// ─── ISR stub declarations ────────────────────────────────────────────────────
extern void *isr_stub_table[256]; // defined in isr_stubs.asm

// ─── Gate setup ──────────────────────────────────────────────────────────────
static void idt_set_gate(u8 vector, void *handler, u8 ist, u8 type_attr) {
    u64 addr = (u64)handler;
    idt_entries[vector].offset_lo  = (u16)(addr & 0xFFFF);
    idt_entries[vector].selector   = 0x08; // kernel code segment
    idt_entries[vector].ist        = ist & 0x07;
    idt_entries[vector].type_attr  = type_attr;
    idt_entries[vector].offset_mid = (u16)((addr >> 16) & 0xFFFF);
    idt_entries[vector].offset_hi  = (u32)((addr >> 32) & 0xFFFFFFFF);
    idt_entries[vector].zero       = 0;
}

void idt_set_ist(u8 vector, u8 ist) {
    idt_entries[vector].ist = ist & 0x07;
}

void idt_register_handler(u8 vector, isr_handler_t handler) {
    handlers[vector] = handler;
}

// ─── Common C handler (called from asm stubs) ────────────────────────────────
void isr_common_handler(InterruptFrame *frame) {
    u64 vec = frame->int_no;

    if (vec < 32) {
        // CPU exception
        if (handlers[vec]) {
            handlers[vec](frame);
        } else {
            kpanic("Wyjatek CPU: %s\n"
                   "  RIP=0x%lx  CS=0x%lx  RFLAGS=0x%lx\n"
                   "  RSP=0x%lx  SS=0x%lx\n"
                   "  ERR=0x%lx  RAX=0x%lx  RBX=0x%lx\n"
                   "  RCX=0x%lx  RDX=0x%lx  RSI=0x%lx\n"
                   "  RDI=0x%lx  RBP=0x%lx",
                   exception_names[vec],
                   frame->rip, frame->cs, frame->rflags,
                   frame->rsp, frame->ss,
                   frame->err_code, frame->rax, frame->rbx,
                   frame->rcx, frame->rdx, frame->rsi,
                   frame->rdi, frame->rbp);
        }
    } else {
        // Hardware IRQ or software interrupt
        if (handlers[vec]) {
            handlers[vec](frame);
        }
        // Send EOI for hardware IRQs (32–255)
        apic_send_eoi();
    }
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate((u8)i, isr_stub_table[i],
                     0,    // IST = 0 (use current stack)
                     0x8E  // P=1, DPL=0, type=0xE (64-bit interrupt gate)
        );
        handlers[i] = (isr_handler_t)0;
    }

    // Use IST 1 for Double Fault (so it has a known-good stack)
    idt_set_ist(8, 1);

    idtr.limit = (u16)(sizeof(idt_entries) - 1);
    idtr.base  = (u64)idt_entries;

    __asm__ volatile("lidt %0" :: "m"(idtr));
}
