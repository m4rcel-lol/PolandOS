// PolandOS — Sterownik szeregowy UART COM1
// Comprehensive serial port driver
#pragma once
#include "../../include/types.h"

// Initialization
void serial_init(void);

// Character I/O
void serial_write_char(char c);
char serial_read_char(void);           // Blocking read
char serial_read_char_nowait(void);    // Non-blocking read

// String I/O
void serial_write(const char *s);
int serial_read_line(char *buf, int max_len);

// Numeric output
void serial_write_hex(u64 v);
void serial_write_dec(i64 v);
void serial_write_bin(u64 v);

// Configuration
void serial_set_baud(u32 baud);
void serial_set_format(u8 data_bits, u8 parity, u8 stop_bits);
void serial_enable_interrupts(bool rx, bool tx);

// Status and statistics
bool serial_is_initialized(void);
void serial_get_stats(u64 *tx, u64 *rx, u64 *errors);
void serial_reset_stats(void);

// Diagnostics
void serial_dump_status(void);

