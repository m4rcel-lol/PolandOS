// PolandOS — Protokol UDP
#include "udp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/timer.h"

static UDPSocket udp_sockets[UDP_MAX_SOCKETS];

// ---------------------------------------------------------------------------
// Socket management
// ---------------------------------------------------------------------------
int udp_open(u16 local_port) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!udp_sockets[i].active) {
            udp_sockets[i].active     = true;
            udp_sockets[i].local_port = local_port;
            udp_sockets[i].has_data   = false;
            udp_sockets[i].recv_len   = 0;
            return i;
        }
    }
    return -1;
}

void udp_close(int fd) {
    if (fd < 0 || fd >= UDP_MAX_SOCKETS) return;
    memset(&udp_sockets[fd], 0, sizeof(UDPSocket));
}

// ---------------------------------------------------------------------------
// Send
// ---------------------------------------------------------------------------
int udp_send(int fd, u32 dst_ip, u16 dst_port, const u8 *data, u16 len) {
    if (fd < 0 || fd >= UDP_MAX_SOCKETS) return -1;
    if (!udp_sockets[fd].active) return -1;
    if (len > UDP_MAX_PAYLOAD) return -1;  // 1500 - 20 IP - 8 UDP

    u8 buf[8 + UDP_MAX_PAYLOAD];
    UDPHeader *udp = (UDPHeader *)buf;

    udp->src_port = htons(udp_sockets[fd].local_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons((u16)(8 + len));
    udp->checksum = 0;

    memcpy(buf + 8, data, len);

    // Compute checksum (optional but correct)
    udp->checksum = udp_checksum(net_ip, dst_ip, buf, (u16)(8 + len));

    return ipv4_send(dst_ip, IP_PROTO_UDP, buf, (u16)(8 + len));
}

// ---------------------------------------------------------------------------
// Receive (blocking with timeout)
// ---------------------------------------------------------------------------
int udp_recv(int fd, u8 *buf, u16 *len, u32 *src_ip, u16 *src_port, u32 timeout_ms) {
    if (fd < 0 || fd >= UDP_MAX_SOCKETS) return -1;
    if (!udp_sockets[fd].active) return -1;

    u64 deadline = timer_get_ms() + timeout_ms;
    while (timer_get_ms() < deadline) {
        net_rx_process();

        if (udp_sockets[fd].has_data) {
            *len      = udp_sockets[fd].recv_len;
            *src_ip   = udp_sockets[fd].recv_src_ip;
            *src_port = udp_sockets[fd].recv_src_port;
            memcpy(buf, udp_sockets[fd].recv_buf, *len);
            udp_sockets[fd].has_data = false;
            return 0;
        }
    }
    return -1;  // timeout
}

// ---------------------------------------------------------------------------
// Receive dispatch (called from IPv4)
// ---------------------------------------------------------------------------
void udp_rx_handler(u32 src_ip, u32 dst_ip, const u8 *payload, u16 len) {
    (void)dst_ip;
    if (len < 8) return;

    const UDPHeader *udp = (const UDPHeader *)payload;
    u16 dst_port = ntohs(udp->dst_port);
    u16 data_len = (u16)(len - 8);
    if (data_len > 1500) data_len = 1500;

    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].active && udp_sockets[i].local_port == dst_port) {
            memcpy(udp_sockets[i].recv_buf, payload + 8, data_len);
            udp_sockets[i].recv_len      = data_len;
            udp_sockets[i].recv_src_ip   = src_ip;
            udp_sockets[i].recv_src_port = ntohs(udp->src_port);
            udp_sockets[i].has_data      = true;
            return;
        }
    }
}
