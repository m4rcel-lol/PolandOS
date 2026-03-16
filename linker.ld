/* PolandOS — Skrypt linkera */
/* Jadro Orzel — wyzszy polswiat (higher half) */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

PHDRS {
    text    PT_LOAD FLAGS(5);  /* R-X */
    rodata  PT_LOAD FLAGS(4);  /* R-- */
    data    PT_LOAD FLAGS(6);  /* RW- */
    bss     PT_LOAD FLAGS(6);  /* RW- */
}

SECTIONS {
    . = KERNEL_VIRT_BASE;

    __kernel_start = .;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_reqs))
        KEEP(*(.limine_requests_end))
    } :text

    .text ALIGN(0x1000) : {
        *(.text .text.*)
    } :text

    .rodata ALIGN(0x1000) : {
        *(.rodata .rodata.*)
    } :rodata

    .data ALIGN(0x1000) : {
        *(.data .data.*)
    } :data

    .bss ALIGN(0x1000) : {
        *(COMMON)
        *(.bss .bss.*)
    } :bss

    __kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
        *(.comment*)
    }
}
