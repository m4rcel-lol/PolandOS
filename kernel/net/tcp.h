// PolandOS — Protokol TCP
#pragma once
#include "../../include/types.h"

#define TCP_MAX_SOCKETS 4

// TCP flags
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10
#define TCP_URG  0x20

typedef enum {
    TCP_CLOSED, TCP_SYN_SENT, TCP_SYN_RECEIVED,
    TCP_ESTABLISHED, TCP_FIN_WAIT1, TCP_FIN_WAIT2,
    TCP_CLOSE_WAIT, TCP_CLOSING, TCP_LAST_ACK,
    TCP_TIME_WAIT
} TCPState;

typedef struct __attribute__((packed)) {
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u32 ack;
    u8  data_offset;  // upper 4 bits = header length in 32-bit words
    u8  flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} TCPHeader;

typedef struct {
    TCPState state;
    u32 local_ip;
    u16 local_port;
    u32 remote_ip;
    u16 remote_port;
    u32 seq;      // our sequence number
    u32 ack;      // acknowledgment number
    u32 remote_seq;
    u16 window;
    // receive buffer
    u8  recv_buf[4096];
    u32 recv_len;
    u32 recv_head;
    // send buffer
    u8  send_buf[4096];
    u32 send_len;
    bool active;
} TCPSocket;

int  tcp_connect(u32 dst_ip, u16 dst_port);  // returns fd or -1
void tcp_close(int fd);
int  tcp_send(int fd, const u8 *data, u32 len);
int  tcp_recv(int fd, u8 *buf, u32 max_len, u32 timeout_ms);
void tcp_rx_handler(u32 src_ip, u32 dst_ip, const u8 *payload, u16 len);
