// PolandOS — Sterownik szeregowy UART COM1
#pragma once
#include "../../include/types.h"

void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *s);
void serial_write_hex(u64 v);
