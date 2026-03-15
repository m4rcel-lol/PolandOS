// PolandOS — Protokol UDP
#pragma once
#include "../../include/types.h"

typedef struct __attribute__((packed)) {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
} UDPHeader;

#define UDP_MAX_SOCKETS 8

typedef struct {
    u16 local_port;
    bool active;
    u8   recv_buf[1500];
    u16  recv_len;
    u32  recv_src_ip;
    u16  recv_src_port;
    bool has_data;
} UDPSocket;

int  udp_open(u16 local_port);   // returns socket fd, -1 on fail
void udp_close(int fd);
int  udp_send(int fd, u32 dst_ip, u16 dst_port, const u8 *data, u16 len);
int  udp_recv(int fd, u8 *buf, u16 *len, u32 *src_ip, u16 *src_port, u32 timeout_ms);
void udp_rx_handler(u32 src_ip, u32 dst_ip, const u8 *payload, u16 len);
