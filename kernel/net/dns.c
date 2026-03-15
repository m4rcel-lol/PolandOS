// PolandOS — Resolver DNS
#include "dns.h"
#include "ethernet.h"
#include "udp.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/timer.h"

#define DNS_PORT 53

// ---------------------------------------------------------------------------
// DNS header (RFC 1035)
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 nscount;
    u16 arcount;
} DNSHeader;

// ---------------------------------------------------------------------------
// Encode hostname as DNS labels
// "example.com" → \x07example\x03com\x00
// Returns number of bytes written.
// ---------------------------------------------------------------------------
static u16 dns_encode_name(const char *hostname, u8 *buf) {
    u16 pos = 0;
    const char *start = hostname;

    while (*start) {
        const char *dot = start;
        while (*dot && *dot != '.') dot++;
        u8 label_len = (u8)(dot - start);
        buf[pos++] = label_len;
        for (u8 i = 0; i < label_len; i++) {
            buf[pos++] = (u8)start[i];
        }
        start = (*dot == '.') ? dot + 1 : dot;
    }
    buf[pos++] = 0;  // root label
    return pos;
}

// Incrementing transaction ID to prevent DNS cache poisoning
static u16 dns_txid = 0x4F53;  // 'OS'

// ---------------------------------------------------------------------------
// DNS resolve: send A query, parse response
// ---------------------------------------------------------------------------
int dns_resolve(const char *hostname, u32 *ip_out) {
    if (!net_configured) return -1;

    // Use a fixed ephemeral source port for DNS responses
    int fd = udp_open(53001);
    if (fd < 0) return -1;

    // Build DNS query
    u8 query[512];
    u16 pos = 0;

    u16 txid = dns_txid++;
    DNSHeader *dhdr = (DNSHeader *)query;
    dhdr->id      = htons(txid);
    dhdr->flags   = htons(0x0100);  // recursion desired
    dhdr->qdcount = htons(1);
    dhdr->ancount = 0;
    dhdr->nscount = 0;
    dhdr->arcount = 0;
    pos += (u16)sizeof(DNSHeader);

    // Encode question
    pos += dns_encode_name(hostname, query + pos);

    // QTYPE = A (1), QCLASS = IN (1)
    query[pos++] = 0x00; query[pos++] = 0x01;  // QTYPE A
    query[pos++] = 0x00; query[pos++] = 0x01;  // QCLASS IN

    // Send query
    if (udp_send(fd, net_dns, DNS_PORT, query, pos) != 0) {
        udp_close(fd);
        return -1;
    }

    // Wait for response
    u8  resp[512];
    u16 resp_len;
    u32 src_ip;
    u16 src_port;

    if (udp_recv(fd, resp, &resp_len, &src_ip, &src_port, 5000) != 0) {
        udp_close(fd);
        return -1;
    }
    udp_close(fd);

    if (resp_len < (u16)sizeof(DNSHeader)) return -1;

    DNSHeader *rhdr = (DNSHeader *)resp;
    // Validate transaction ID matches our query
    if (ntohs(rhdr->id) != txid) return -1;
    u16 ancount = ntohs(rhdr->ancount);
    if (ancount == 0) return -1;

    // Skip question section
    u16 rpos = (u16)sizeof(DNSHeader);

    // Skip QDCOUNT questions
    u16 qdcount = ntohs(rhdr->qdcount);
    for (u16 q = 0; q < qdcount && rpos < resp_len; q++) {
        // Skip QNAME
        while (rpos < resp_len) {
            u8 llen = resp[rpos];
            if (llen == 0) { rpos++; break; }
            if ((llen & 0xC0) == 0xC0) { rpos += 2; break; }
            rpos += (u16)(1 + llen);
        }
        rpos += 4;  // QTYPE + QCLASS
    }

    // Parse answer records
    for (u16 a = 0; a < ancount && rpos < resp_len; a++) {
        // Skip NAME (may be compressed)
        if (rpos >= resp_len) break;
        u8 first = resp[rpos];
        if ((first & 0xC0) == 0xC0) {
            rpos += 2;
        } else {
            while (rpos < resp_len) {
                u8 llen = resp[rpos];
                if (llen == 0) { rpos++; break; }
                rpos += (u16)(1 + llen);
            }
        }

        if (rpos + 10 > resp_len) break;

        u16 rtype  = (u16)((resp[rpos] << 8) | resp[rpos + 1]);
        // u16 rclass = (u16)((resp[rpos+2] << 8) | resp[rpos + 3]);
        // u32 ttl    = (u32)((resp[rpos+4]<<24)|(resp[rpos+5]<<16)|(resp[rpos+6]<<8)|resp[rpos+7]);
        u16 rdlen  = (u16)((resp[rpos + 8] << 8) | resp[rpos + 9]);
        rpos += 10;

        if (rtype == 1 && rdlen == 4 && rpos + 4 <= resp_len) {
            // Type A record: IPv4 address
            u32 ip;
            memcpy(&ip, &resp[rpos], 4);
            *ip_out = ip;
            return 0;
        }
        rpos += rdlen;
    }

    return -1;  // no A record found
}
