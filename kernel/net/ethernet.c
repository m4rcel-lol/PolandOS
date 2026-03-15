// PolandOS — Warstwa Ethernet II
#include "ethernet.h"
#include "../drivers/e1000.h"
#include "../lib/string.h"
#include "../lib/printf.h"

u8  net_mac[ETH_ALEN];
u32 net_ip       = 0;
u32 net_gateway  = 0;
u32 net_subnet   = 0;
u32 net_dns      = 0;
bool net_configured = false;

// ---------------------------------------------------------------------------
// RX ring buffer
// ---------------------------------------------------------------------------
static NetPacket rx_queue[NET_RX_QUEUE_SIZE];
static volatile u32 rx_head = 0;  // producer (IRQ)
static volatile u32 rx_tail = 0;  // consumer (poll)

void net_init(void) {
    memcpy(net_mac, e1000_mac, ETH_ALEN);
    net_ip       = 0;
    net_gateway  = 0;
    net_subnet   = 0;
    net_dns      = 0;
    net_configured = false;
    rx_head = 0;
    rx_tail = 0;
}

void net_rx_enqueue(const u8 *data, u16 len) {
    u32 next = (rx_head + 1) % NET_RX_QUEUE_SIZE;
    if (next == rx_tail) return;  // queue full, drop
    if (len > 1518) len = 1518;
    memcpy(rx_queue[rx_head].data, data, len);
    rx_queue[rx_head].len = len;
    rx_head = next;
}

int net_rx_dequeue(NetPacket *pkt) {
    if (rx_tail == rx_head) return 0;  // empty
    *pkt = rx_queue[rx_tail];
    rx_tail = (rx_tail + 1) % NET_RX_QUEUE_SIZE;
    return 1;
}

void net_rx_process(void) {
    // Poll e1000 directly (no-IRQ path)
    u8 buf[1518];
    u16 len;
    while (e1000_recv(buf, &len) == 1) {
        net_rx_enqueue(buf, len);
    }
    // Drain queue
    NetPacket pkt;
    while (net_rx_dequeue(&pkt)) {
        eth_rx_handler(pkt.data, pkt.len);
    }
}

// ---------------------------------------------------------------------------
// Byte-order helpers
// ---------------------------------------------------------------------------
u16 htons(u16 v) { return (u16)((v >> 8) | (v << 8)); }
u32 htonl(u32 v) {
    return ((v & 0xFF000000u) >> 24) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x000000FFu) << 24);
}
u16 ntohs(u16 v) { return htons(v); }
u32 ntohl(u32 v) { return htonl(v); }

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------
static const char hex_chars[] = "0123456789ABCDEF";

void mac_to_str(const u8 *mac, char *buf) {
    for (int i = 0; i < 6; i++) {
        buf[i * 3]     = hex_chars[(mac[i] >> 4) & 0xF];
        buf[i * 3 + 1] = hex_chars[mac[i] & 0xF];
        buf[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
}

void ip_to_str(u32 ip, char *buf) {
    // ip is in network byte order
    u8 *b = (u8 *)&ip;
    ksnprintf(buf, 16, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
}

u32 str_to_ip(const char *s) {
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        u32 octet = 0;
        while (*s >= '0' && *s <= '9') {
            octet = octet * 10 + (u32)(*s - '0');
            s++;
        }
        if (*s == '.') s++;
        ((u8 *)&result)[i] = (u8)octet;
    }
    return result;  // network byte order
}

// ---------------------------------------------------------------------------
// Send
// ---------------------------------------------------------------------------
int eth_send(const u8 *dst_mac, u16 ethertype, const u8 *payload, u16 payload_len) {
    // Maximum Ethernet frame: 14 header + 1500 payload
    if (payload_len > 1500) return -1;

    u8 frame[14 + 1500];
    EtherFrame *ef = (EtherFrame *)frame;

    memcpy(ef->dst, dst_mac, ETH_ALEN);
    memcpy(ef->src, net_mac, ETH_ALEN);
    ef->ethertype = htons(ethertype);
    memcpy(ef->payload, payload, payload_len);

    return e1000_send(frame, 14 + payload_len);
}

// ---------------------------------------------------------------------------
// Receive dispatch — forward declarations for upper-layer handlers
// ---------------------------------------------------------------------------
void arp_rx_handler(const u8 *payload, u16 len);
void ipv4_rx_handler(const u8 *payload, u16 len);

void eth_rx_handler(const u8 *frame, u16 len) {
    if (len < 14) return;
    const EtherFrame *ef = (const EtherFrame *)frame;
    u16 etype = ntohs(ef->ethertype);
    u16 payload_len = len - 14;

    switch (etype) {
        case ETH_TYPE_ARP:
            arp_rx_handler(ef->payload, payload_len);
            break;
        case ETH_TYPE_IPV4:
            ipv4_rx_handler(ef->payload, payload_len);
            break;
        default:
            break;
    }
}
