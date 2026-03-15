// PolandOS — Klient DHCP
#include "dhcp.h"
#include "ethernet.h"
#include "ipv4.h"
#include "udp.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/timer.h"

// ---------------------------------------------------------------------------
// BOOTP/DHCP packet
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    u8  op;           // 1=BOOTREQUEST, 2=BOOTREPLY
    u8  htype;        // 1=Ethernet
    u8  hlen;         // 6
    u8  hops;         // 0
    u32 xid;          // transaction ID
    u16 secs;         // seconds elapsed
    u16 flags;        // 0x8000 = broadcast
    u32 ciaddr;       // client IP (0.0.0.0 for discover)
    u32 yiaddr;       // 'your' IP (offered IP)
    u32 siaddr;       // next server IP
    u32 giaddr;       // relay agent IP
    u8  chaddr[16];   // client hardware address (padded to 16)
    u8  sname[64];    // server host name
    u8  file[128];    // boot file name
    u32 magic;        // 0x63825363
    u8  options[308]; // DHCP options (variable, up to end of min 300-byte packet)
} DHCPPacket;

#define DHCP_MAGIC        0x63825363u
#define DHCPDISCOVER      1
#define DHCPOFFER         2
#define DHCPREQUEST       3
#define DHCPACK           5
#define DHCP_OPT_MSGTYPE  53
#define DHCP_OPT_SUBNET   1
#define DHCP_OPT_ROUTER   3
#define DHCP_OPT_DNS      6
#define DHCP_OPT_LEASE    51
#define DHCP_OPT_SERVERID 54
#define DHCP_OPT_REQIP    50
#define DHCP_OPT_CLIENTID 61
#define DHCP_OPT_PARAMREQ 55
#define DHCP_OPT_END      255
#define DHCP_OPT_PAD      0

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

// ---------------------------------------------------------------------------
// Option builder
// ---------------------------------------------------------------------------
static void add_option(u8 *buf, u16 *pos, u8 type, u8 len, const u8 *data) {
    buf[(*pos)++] = type;
    buf[(*pos)++] = len;
    for (u8 i = 0; i < len; i++) {
        buf[(*pos)++] = data[i];
    }
}

// ---------------------------------------------------------------------------
// Build DHCP packet
// ---------------------------------------------------------------------------
static u16 build_dhcp(DHCPPacket *pkt, u8 msg_type, u32 xid,
                      u32 offered_ip, u32 server_ip) {
    memset(pkt, 0, sizeof(DHCPPacket));

    pkt->op    = 1;  // BOOTREQUEST
    pkt->htype = 1;  // Ethernet
    pkt->hlen  = 6;
    pkt->hops  = 0;
    pkt->xid   = xid;
    pkt->secs  = 0;
    pkt->flags = htons(0x8000);  // broadcast flag
    pkt->ciaddr = 0;
    pkt->yiaddr = 0;
    pkt->siaddr = 0;
    pkt->giaddr = 0;

    memcpy(pkt->chaddr, net_mac, 6);
    pkt->magic = htonl(DHCP_MAGIC);

    u16 pos = 0;

    // Option 53: DHCP Message Type
    u8 mt = msg_type;
    add_option(pkt->options, &pos, DHCP_OPT_MSGTYPE, 1, &mt);

    // Option 61: Client Identifier (type=1 Ethernet + MAC)
    u8 clientid[7];
    clientid[0] = 1;
    memcpy(&clientid[1], net_mac, 6);
    add_option(pkt->options, &pos, DHCP_OPT_CLIENTID, 7, clientid);

    if (msg_type == DHCPREQUEST) {
        // Option 50: Requested IP Address
        add_option(pkt->options, &pos, DHCP_OPT_REQIP, 4, (u8 *)&offered_ip);
        // Option 54: DHCP Server Identifier
        add_option(pkt->options, &pos, DHCP_OPT_SERVERID, 4, (u8 *)&server_ip);
    }

    // Option 55: Parameter Request List
    u8 params[] = {
        DHCP_OPT_SUBNET,
        DHCP_OPT_ROUTER,
        DHCP_OPT_DNS,
        DHCP_OPT_LEASE
    };
    add_option(pkt->options, &pos, DHCP_OPT_PARAMREQ, sizeof(params), params);

    // End
    pkt->options[pos++] = DHCP_OPT_END;

    // Total DHCP packet size: fixed header (240) + options used
    return (u16)(240 + pos);
}

// ---------------------------------------------------------------------------
// Parse DHCP options
// ---------------------------------------------------------------------------
static void parse_options(const u8 *opts, u16 opts_len,
                          u8  *msg_type,
                          u32 *subnet,
                          u32 *router,
                          u32 *dns,
                          u32 *server_id) {
    u16 i = 0;
    while (i < opts_len) {
        u8 code = opts[i++];
        if (code == DHCP_OPT_PAD) continue;
        if (code == DHCP_OPT_END) break;
        if (i >= opts_len) break;
        u8 len = opts[i++];
        if (i + len > opts_len) break;

        switch (code) {
            case DHCP_OPT_MSGTYPE:
                if (len >= 1) *msg_type = opts[i];
                break;
            case DHCP_OPT_SUBNET:
                if (len >= 4) memcpy(subnet, &opts[i], 4);
                break;
            case DHCP_OPT_ROUTER:
                if (len >= 4) memcpy(router, &opts[i], 4);
                break;
            case DHCP_OPT_DNS:
                if (len >= 4) memcpy(dns, &opts[i], 4);
                break;
            case DHCP_OPT_SERVERID:
                if (len >= 4) memcpy(server_id, &opts[i], 4);
                break;
            default:
                break;
        }
        i += len;
    }
}

// ---------------------------------------------------------------------------
// DORA sequence
// ---------------------------------------------------------------------------
int dhcp_discover(void) {
    int fd = udp_open(DHCP_CLIENT_PORT);
    if (fd < 0) return -1;

    // Use a fixed XID for simplicity
    u32 xid = 0xDEAD0001u;

    // ---- DISCOVER ----
    DHCPPacket pkt;
    u16 pkt_len = build_dhcp(&pkt, DHCPDISCOVER, xid, 0, 0);

    // Before IP is configured we must send with src=0.0.0.0, dst=255.255.255.255
    // Temporarily set net_ip to 0 (it should already be 0 at this point)
    u32 saved_ip = net_ip;
    net_ip = 0;

    // Build raw IPv4 + UDP and send via eth_send (broadcast)
    {
        // UDP header
        u8 udp_payload[8 + sizeof(DHCPPacket)];
        u16 udp_len = (u16)(8 + pkt_len);

        u16 *udp_hdr = (u16 *)udp_payload;
        udp_hdr[0] = htons(DHCP_CLIENT_PORT);  // src port
        udp_hdr[1] = htons(DHCP_SERVER_PORT);  // dst port
        udp_hdr[2] = htons(udp_len);
        udp_hdr[3] = 0;  // checksum=0 (optional for UDP)
        memcpy(udp_payload + 8, &pkt, pkt_len);

        // IPv4 header
        u8 ipv4_buf[20 + 8 + sizeof(DHCPPacket)];
        IPv4Header *ip_hdr = (IPv4Header *)ipv4_buf;
        ip_hdr->version_ihl = (4 << 4) | 5;
        ip_hdr->dscp_ecn    = 0;
        ip_hdr->total_len   = htons((u16)(20 + udp_len));
        ip_hdr->id          = htons(1);
        ip_hdr->flags_frag  = 0;
        ip_hdr->ttl         = 64;
        ip_hdr->protocol    = IP_PROTO_UDP;
        ip_hdr->checksum    = 0;
        ip_hdr->src         = 0;           // 0.0.0.0
        ip_hdr->dst         = 0xFFFFFFFFu; // 255.255.255.255
        ip_hdr->checksum    = ip_checksum(ip_hdr, 20);
        memcpy(ipv4_buf + 20, udp_payload, udp_len);

        static const u8 bcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        eth_send(bcast_mac, ETH_TYPE_IPV4, ipv4_buf, (u16)(20 + udp_len));
    }

    // ---- Wait for OFFER ----
    u32 offered_ip  = 0;
    u32 server_id   = 0;
    u32 subnet      = 0;
    u32 router      = 0;
    u32 dns_srv     = 0;

    u64 deadline = timer_get_ms() + 5000;
    bool got_offer = false;

    while (timer_get_ms() < deadline && !got_offer) {
        // Poll e1000 directly (no IP configured yet)
        u8 buf[1518];
        u16 buf_len;
        extern int e1000_recv(void *buf, u16 *len);
        while (e1000_recv(buf, &buf_len) == 1) {
            if (buf_len < 14 + 20 + 8 + 240) continue;
            u8 *ip_start  = buf + 14;
            u8 *udp_start = ip_start + 20;
            u8 *dhcp_data = udp_start + 8;
            u16 dhcp_data_len = (u16)(buf_len - 14 - 20 - 8);

            // Check UDP dst port 68
            u16 dst_port = (u16)((udp_start[2] << 8) | udp_start[3]);
            if (dst_port != DHCP_CLIENT_PORT) continue;

            DHCPPacket *offer = (DHCPPacket *)dhcp_data;
            if (ntohl(offer->magic) != DHCP_MAGIC) continue;
            if (offer->xid != xid) continue;

            u16 opts_len = (u16)(dhcp_data_len > 240 ? dhcp_data_len - 240 : 0);
            u8 msg_type = 0;
            parse_options(offer->options, opts_len,
                          &msg_type, &subnet, &router, &dns_srv, &server_id);

            if (msg_type == DHCPOFFER) {
                offered_ip = offer->yiaddr;
                got_offer  = true;
                break;
            }
        }
    }

    if (!got_offer) {
        udp_close(fd);
        net_ip = saved_ip;
        return -1;
    }

    // ---- REQUEST ----
    pkt_len = build_dhcp(&pkt, DHCPREQUEST, xid, offered_ip, server_id);
    {
        u8 udp_payload[8 + sizeof(DHCPPacket)];
        u16 udp_len = (u16)(8 + pkt_len);
        u16 *udp_hdr = (u16 *)udp_payload;
        udp_hdr[0] = htons(DHCP_CLIENT_PORT);
        udp_hdr[1] = htons(DHCP_SERVER_PORT);
        udp_hdr[2] = htons(udp_len);
        udp_hdr[3] = 0;
        memcpy(udp_payload + 8, &pkt, pkt_len);

        u8 ipv4_buf[20 + 8 + sizeof(DHCPPacket)];
        IPv4Header *ip_hdr = (IPv4Header *)ipv4_buf;
        ip_hdr->version_ihl = (4 << 4) | 5;
        ip_hdr->dscp_ecn    = 0;
        ip_hdr->total_len   = htons((u16)(20 + udp_len));
        ip_hdr->id          = htons(2);
        ip_hdr->flags_frag  = 0;
        ip_hdr->ttl         = 64;
        ip_hdr->protocol    = IP_PROTO_UDP;
        ip_hdr->checksum    = 0;
        ip_hdr->src         = 0;
        ip_hdr->dst         = 0xFFFFFFFFu;
        ip_hdr->checksum    = ip_checksum(ip_hdr, 20);
        memcpy(ipv4_buf + 20, udp_payload, udp_len);

        static const u8 bcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        eth_send(bcast_mac, ETH_TYPE_IPV4, ipv4_buf, (u16)(20 + udp_len));
    }

    // ---- Wait for ACK ----
    bool got_ack = false;
    deadline = timer_get_ms() + 5000;

    while (timer_get_ms() < deadline && !got_ack) {
        u8 buf[1518];
        u16 buf_len;
        extern int e1000_recv(void *buf, u16 *len);
        while (e1000_recv(buf, &buf_len) == 1) {
            if (buf_len < 14 + 20 + 8 + 240) continue;
            u8 *udp_start = buf + 14 + 20;
            u8 *dhcp_data = udp_start + 8;
            u16 dhcp_data_len = (u16)(buf_len - 14 - 20 - 8);

            u16 dst_port = (u16)((udp_start[2] << 8) | udp_start[3]);
            if (dst_port != DHCP_CLIENT_PORT) continue;

            DHCPPacket *ack_pkt = (DHCPPacket *)dhcp_data;
            if (ntohl(ack_pkt->magic) != DHCP_MAGIC) continue;
            if (ack_pkt->xid != xid) continue;

            u16 opts_len = (u16)(dhcp_data_len > 240 ? dhcp_data_len - 240 : 0);
            u8 msg_type = 0;
            u32 s2 = 0, r2 = 0, d2 = 0, sid2 = 0;
            parse_options(ack_pkt->options, opts_len,
                          &msg_type, &s2, &r2, &d2, &sid2);

            if (msg_type == DHCPACK) {
                if (s2) subnet  = s2;
                if (r2) router  = r2;
                if (d2) dns_srv = d2;
                got_ack = true;
                break;
            }
        }
    }

    udp_close(fd);

    if (!got_ack) {
        net_ip = saved_ip;
        return -1;
    }

    // ---- Configure network ----
    net_ip         = offered_ip;
    net_gateway    = router;
    net_subnet     = subnet;
    net_dns        = dns_srv;
    net_configured = true;

    char ip_str[16];
    ip_to_str(net_ip, ip_str);
    kprintf("[DOBRZE] DHCP: adres IP = %s\n", ip_str);

    return 0;
}
