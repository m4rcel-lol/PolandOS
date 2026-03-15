// PolandOS — Warstwa Ethernet II
#pragma once
#include "../../include/types.h"

#define ETH_ALEN 6
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_IPV6 0x86DD

typedef struct __attribute__((packed)) {
    u8  dst[ETH_ALEN];
    u8  src[ETH_ALEN];
    u16 ethertype;
    u8  payload[];
} EtherFrame;

extern u8 net_mac[ETH_ALEN];  // our MAC
extern u32 net_ip;            // our IP (network byte order)
extern u32 net_gateway;       // gateway IP
extern u32 net_subnet;        // subnet mask
extern u32 net_dns;           // DNS server IP
extern bool net_configured;   // DHCP done?

// Receive buffer
#define NET_RX_QUEUE_SIZE 32
typedef struct {
    u8  data[1518];
    u16 len;
} NetPacket;

void net_init(void);
void net_rx_enqueue(const u8 *data, u16 len);
int  net_rx_dequeue(NetPacket *pkt);
void net_rx_process(void);  // process pending RX packets
int  eth_send(const u8 *dst_mac, u16 ethertype, const u8 *payload, u16 payload_len);
void eth_rx_handler(const u8 *frame, u16 len);
u16  htons(u16 v);
u32  htonl(u32 v);
u16  ntohs(u16 v);
u32  ntohl(u32 v);
void mac_to_str(const u8 *mac, char *buf);
void ip_to_str(u32 ip, char *buf);
u32  str_to_ip(const char *s);
