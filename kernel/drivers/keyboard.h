// PolandOS — Sterownik klawiatury PS/2
#pragma once
#include "../../include/types.h"

#define KB_BUFFER_SIZE 256

void kb_init(void);
char kb_getchar(void);  // blocking
int  kb_poll(char *c);  // non-blocking, returns 1 if got char
void kb_irq_handler(void);
