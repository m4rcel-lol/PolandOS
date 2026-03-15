// PolandOS — Protokol IPv4
#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "../lib/string.h"
#include "../lib/printf.h"

static u16 ip_id_counter = 1;

// ---------------------------------------------------------------------------
// Checksum: one's complement sum of 16-bit words
// ---------------------------------------------------------------------------
u16 ip_checksum(const void *data, u16 len) {
    const u16 *words = (const u16 *)data;
    u32 sum = 0;
    u16 count = len;

    while (count > 1) {
        sum += *words++;
        count -= 2;
    }
    if (count == 1) {
        sum += *(const u8 *)words;  // last odd byte
    }

    // Fold 32-bit sum into 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (u16)(~sum);
}

// ---------------------------------------------------------------------------
// UDP pseudo-header checksum
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    u32 src;
    u32 dst;
    u8  zero;
    u8  protocol;
    u16 length;
} UDPPseudo;

u16 udp_checksum(u32 src, u32 dst, const u8 *data, u16 len) {
    UDPPseudo ph;
    ph.src      = src;
    ph.dst      = dst;
    ph.zero     = 0;
    ph.protocol = IP_PROTO_UDP;
    ph.length   = htons(len);

    u32 sum = 0;

    // Sum pseudo-header via byte copy to avoid packed alignment warning
    u8 ph_bytes[sizeof(UDPPseudo)];
    memcpy(ph_bytes, &ph, sizeof(ph_bytes));
    for (u16 i = 0; i + 1 < (u16)sizeof(ph_bytes); i += 2) {
        sum += (u32)((ph_bytes[i] << 8) | ph_bytes[i + 1]);
    }

    // Sum data
    const u16 *w = (const u16 *)data;
    u16 count = len;
    while (count > 1) {
        sum += *w++;
        count -= 2;
    }
    if (count == 1) {
        sum += *(const u8 *)w;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (u16)(~sum);
}

// ---------------------------------------------------------------------------
// Forward declarations for upper-layer handlers
// ---------------------------------------------------------------------------
void icmp_rx_handler(u32 src_ip, const u8 *payload, u16 len);
void udp_rx_handler(u32 src_ip, u32 dst_ip, const u8 *payload, u16 len);
void tcp_rx_handler(u32 src_ip, u32 dst_ip, const u8 *payload, u16 len);

// ---------------------------------------------------------------------------
// Send
// ---------------------------------------------------------------------------
int ipv4_send(u32 dst_ip, u8 protocol, const u8 *payload, u16 payload_len) {
    if (payload_len > 1480) return -1;  // MTU 1500 - 20 byte header

    u8 mac[6];
    u32 next_hop;

    // Determine next hop
    if ((dst_ip & net_subnet) == (net_ip & net_subnet)) {
        next_hop = dst_ip;
    } else {
        next_hop = net_gateway;
    }

    if (arp_resolve(next_hop, mac) != 0) {
        return -1;
    }

    // Build IPv4 header
    u8 buf[20 + 1480];
    IPv4Header *hdr = (IPv4Header *)buf;

    hdr->version_ihl  = (4 << 4) | 5;  // version=4, IHL=5 (20 bytes)
    hdr->dscp_ecn     = 0;
    hdr->total_len    = htons((u16)(20 + payload_len));
    hdr->id           = htons(ip_id_counter++);
    hdr->flags_frag   = 0;
    hdr->ttl          = 64;
    hdr->protocol     = protocol;
    hdr->checksum     = 0;
    hdr->src          = net_ip;
    hdr->dst          = dst_ip;

    hdr->checksum = ip_checksum(hdr, 20);

    memcpy(buf + 20, payload, payload_len);

    return eth_send(mac, ETH_TYPE_IPV4, buf, (u16)(20 + payload_len));
}

// ---------------------------------------------------------------------------
// Receive
// ---------------------------------------------------------------------------
void ipv4_rx_handler(const u8 *payload, u16 len) {
    if (len < 20) return;

    const IPv4Header *hdr = (const IPv4Header *)payload;

    // Only handle IPv4
    if ((hdr->version_ihl >> 4) != 4) return;

    u16 ihl = (u16)((hdr->version_ihl & 0x0F) * 4);
    if (ihl < 20 || ihl > len) return;

    // Validate header checksum
    u16 saved = hdr->checksum;
    // Temporarily cast to mutable for checksum calc (non-destructive)
    // We compute over the header bytes with checksum field as-is;
    // a valid header will produce 0xFFFF or 0x0000 depending on carry.
    // Standard approach: compute over header, result should be 0.
    IPv4Header tmp;
    memcpy(&tmp, hdr, ihl);
    tmp.checksum = 0;
    u16 computed = ip_checksum(&tmp, (u16)ihl);
    if (computed != saved) return;

    u16 total_len = ntohs(hdr->total_len);
    if (total_len > len) return;

    u32 src_ip = hdr->src;
    u32 dst_ip = hdr->dst;
    const u8 *upper = payload + ihl;
    u16 upper_len = (u16)(total_len - ihl);

    switch (hdr->protocol) {
        case IP_PROTO_ICMP:
            icmp_rx_handler(src_ip, upper, upper_len);
            break;
        case IP_PROTO_UDP:
            udp_rx_handler(src_ip, dst_ip, upper, upper_len);
            break;
        case IP_PROTO_TCP:
            tcp_rx_handler(src_ip, dst_ip, upper, upper_len);
            break;
        default:
            break;
    }
}
