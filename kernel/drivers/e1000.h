// PolandOS — Sterownik karty sieciowej Intel e1000
#pragma once
#include "../../include/types.h"

#define ETH_ALEN 6
#define ETH_MAX_FRAME 1518

extern u8 e1000_mac[ETH_ALEN];

int  e1000_init(void);
int  e1000_send(const void *data, u16 len);
int  e1000_recv(void *buf, u16 *len);  // returns 1 if got packet, 0 if none
void e1000_irq_handler(void);
int  e1000_has_packet(void);
