// PolandOS — Protokol IPv4
#pragma once
#include "../../include/types.h"

typedef struct __attribute__((packed)) {
    u8  version_ihl;
    u8  dscp_ecn;
    u16 total_len;
    u16 id;
    u16 flags_frag;
    u8  ttl;
    u8  protocol;
    u16 checksum;
    u32 src;
    u32 dst;
} IPv4Header;

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

u16  ip_checksum(const void *data, u16 len);
int  ipv4_send(u32 dst_ip, u8 protocol, const u8 *payload, u16 payload_len);
void ipv4_rx_handler(const u8 *payload, u16 len);
u16  udp_checksum(u32 src, u32 dst, const u8 *data, u16 len);
