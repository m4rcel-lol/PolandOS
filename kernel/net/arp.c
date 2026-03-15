// PolandOS — Protokol ARP
#include "arp.h"
#include "ethernet.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/timer.h"

// ---------------------------------------------------------------------------
// ARP packet structure (RFC 826, Ethernet + IPv4)
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    u16 htype;    // hardware type: 1 = Ethernet
    u16 ptype;    // protocol type: 0x0800 = IPv4
    u8  hlen;     // hardware addr len: 6
    u8  plen;     // protocol addr len: 4
    u16 oper;     // 1 = request, 2 = reply
    u8  sha[6];   // sender hardware (MAC) address
    u32 spa;      // sender protocol (IP) address
    u8  tha[6];   // target hardware (MAC) address
    u32 tpa;      // target protocol (IP) address
} ARPPacket;

ARPEntry arp_cache[ARP_CACHE_SIZE];

void arp_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
}

// ---------------------------------------------------------------------------
// Cache management
// ---------------------------------------------------------------------------
void arp_cache_update(u32 ip, const u8 *mac) {
    // Update existing entry if present
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }
    // Find a free slot
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = true;
            return;
        }
    }
    // Cache full: evict slot 0 (simple policy)
    arp_cache[0].ip = ip;
    memcpy(arp_cache[0].mac, mac, 6);
    arp_cache[0].valid = true;
}

ARPEntry *arp_cache_lookup(u32 ip) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            return &arp_cache[i];
        }
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Send helpers
// ---------------------------------------------------------------------------
void arp_send_request(u32 target_ip) {
    ARPPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.htype = htons(1);
    pkt.ptype = htons(0x0800);
    pkt.hlen  = 6;
    pkt.plen  = 4;
    pkt.oper  = htons(1);  // request

    memcpy(pkt.sha, net_mac, 6);
    pkt.spa = net_ip;
    memset(pkt.tha, 0x00, 6);
    pkt.tpa = target_ip;

    static const u8 broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    eth_send(broadcast, ETH_TYPE_ARP, (u8 *)&pkt, sizeof(pkt));
}

void arp_send_reply(u32 dst_ip, const u8 *dst_mac) {
    ARPPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.htype = htons(1);
    pkt.ptype = htons(0x0800);
    pkt.hlen  = 6;
    pkt.plen  = 4;
    pkt.oper  = htons(2);  // reply

    memcpy(pkt.sha, net_mac, 6);
    pkt.spa = net_ip;
    memcpy(pkt.tha, dst_mac, 6);
    pkt.tpa = dst_ip;

    eth_send(dst_mac, ETH_TYPE_ARP, (u8 *)&pkt, sizeof(pkt));
}

// ---------------------------------------------------------------------------
// Receive handler
// ---------------------------------------------------------------------------
void arp_rx_handler(const u8 *payload, u16 len) {
    if (len < (u16)sizeof(ARPPacket)) return;

    const ARPPacket *pkt = (const ARPPacket *)payload;

    // Validate: Ethernet + IPv4
    if (ntohs(pkt->htype) != 1)      return;
    if (ntohs(pkt->ptype) != 0x0800) return;
    if (pkt->hlen != 6)              return;
    if (pkt->plen != 4)              return;

    u16 oper = ntohs(pkt->oper);

    // Always update cache from sender
    arp_cache_update(pkt->spa, pkt->sha);

    if (oper == 1 && pkt->tpa == net_ip && net_ip != 0) {
        // ARP request for our IP — send reply
        arp_send_reply(pkt->spa, pkt->sha);
    }
    // oper == 2: reply already recorded in cache above
}

// ---------------------------------------------------------------------------
// Resolve: block until resolved or timeout (3 s)
// ---------------------------------------------------------------------------
int arp_resolve(u32 ip, u8 *mac_out) {
    // Check cache first
    ARPEntry *entry = arp_cache_lookup(ip);
    if (entry) {
        memcpy(mac_out, entry->mac, 6);
        return 0;
    }

    // Send request and wait
    arp_send_request(ip);

    u64 deadline = timer_get_ms() + 3000;
    while (timer_get_ms() < deadline) {
        // Poll for incoming packets
        u8 buf[1518];
        u16 plen;
        extern int e1000_recv(void *buf, u16 *len);
        if (e1000_recv(buf, &plen) == 1) {
            net_rx_enqueue(buf, plen);
            NetPacket pkt;
            while (net_rx_dequeue(&pkt)) {
                eth_rx_handler(pkt.data, pkt.len);
            }
        }

        entry = arp_cache_lookup(ip);
        if (entry) {
            memcpy(mac_out, entry->mac, 6);
            return 0;
        }
    }

    return -1;  // timeout
}
