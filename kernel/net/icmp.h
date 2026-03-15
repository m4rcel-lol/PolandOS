// PolandOS — Protokol ICMP
#pragma once
#include "../../include/types.h"

typedef struct __attribute__((packed)) {
    u8  type;
    u8  code;
    u16 checksum;
    u16 id;
    u16 seq;
    u8  data[];
} ICMPHeader;

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

void icmp_rx_handler(u32 src_ip, const u8 *payload, u16 len);
int  icmp_ping(u32 dst_ip, u16 seq);  // returns RTT in ms, -1 on timeout
