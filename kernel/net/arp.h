// PolandOS — Protokol ARP
#pragma once
#include "../../include/types.h"

#define ARP_CACHE_SIZE 32

typedef struct {
    u32 ip;
    u8  mac[6];
    bool valid;
} ARPEntry;

extern ARPEntry arp_cache[ARP_CACHE_SIZE];

void arp_init(void);
void arp_rx_handler(const u8 *payload, u16 len);
int  arp_resolve(u32 ip, u8 *mac_out);   // returns 0 on success, -1 on timeout
void arp_send_request(u32 target_ip);
void arp_send_reply(u32 dst_ip, const u8 *dst_mac);
void arp_cache_update(u32 ip, const u8 *mac);
ARPEntry *arp_cache_lookup(u32 ip);
