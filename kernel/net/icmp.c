// PolandOS — Protokol ICMP
#include "icmp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/timer.h"

// Ping reply signalling
static volatile bool ping_reply_received = false;
static volatile u16  ping_reply_seq      = 0;

// ---------------------------------------------------------------------------
// Receive handler
// ---------------------------------------------------------------------------
void icmp_rx_handler(u32 src_ip, const u8 *payload, u16 len) {
    if (len < 8) return;

    const ICMPHeader *icmp = (const ICMPHeader *)payload;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        // Build reply: same payload, type=0, recalculate checksum
        u8 reply[1480];
        if (len > 1480) return;
        memcpy(reply, payload, len);

        ICMPHeader *rep = (ICMPHeader *)reply;
        rep->type     = ICMP_ECHO_REPLY;
        rep->checksum = 0;
        rep->checksum = ip_checksum(reply, len);

        ipv4_send(src_ip, IP_PROTO_ICMP, reply, len);

    } else if (icmp->type == ICMP_ECHO_REPLY) {
        ping_reply_seq      = ntohs(icmp->seq);
        ping_reply_received = true;
    }
}

// ---------------------------------------------------------------------------
// Ping
// ---------------------------------------------------------------------------
int icmp_ping(u32 dst_ip, u16 seq) {
    // Build echo request: 8-byte ICMP header + 32 bytes of data
    u8 pkt[8 + 32];
    ICMPHeader *icmp = (ICMPHeader *)pkt;

    icmp->type     = ICMP_ECHO_REQUEST;
    icmp->code     = 0;
    icmp->checksum = 0;
    icmp->id       = htons(0x504F);  // 'PO' for PolandOS
    icmp->seq      = htons(seq);

    // Fill payload with a pattern
    for (int i = 0; i < 32; i++) {
        pkt[8 + i] = (u8)i;
    }

    icmp->checksum = ip_checksum(pkt, sizeof(pkt));

    ping_reply_received = false;

    u64 t0 = timer_get_ms();
    if (ipv4_send(dst_ip, IP_PROTO_ICMP, pkt, sizeof(pkt)) != 0) {
        return -1;
    }

    // Wait up to 5 seconds for matching reply
    u64 deadline = t0 + 5000;
    while (timer_get_ms() < deadline) {
        net_rx_process();

        if (ping_reply_received && ping_reply_seq == seq) {
            ping_reply_received = false;
            return (int)(timer_get_ms() - t0);
        }
    }

    return -1;  // timeout
}
