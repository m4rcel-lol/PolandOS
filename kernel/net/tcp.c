// PolandOS — Protokol TCP
#include "tcp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/timer.h"

static TCPSocket tcp_sockets[TCP_MAX_SOCKETS];

// Ephemeral port base
static u16 tcp_next_port = 49152;

// ---------------------------------------------------------------------------
// Pseudo-header checksum (reuse ip_checksum for summing)
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    u32 src;
    u32 dst;
    u8  zero;
    u8  protocol;
    u16 length;
} TCPPseudo;

static u16 tcp_checksum(u32 src_ip, u32 dst_ip, const u8 *segment, u16 seg_len) {
    TCPPseudo ph;
    ph.src      = src_ip;
    ph.dst      = dst_ip;
    ph.zero     = 0;
    ph.protocol = IP_PROTO_TCP;
    ph.length   = htons(seg_len);

    u32 sum = 0;

    // Sum pseudo-header via byte copy to avoid packed alignment warning
    u8 ph_bytes[sizeof(TCPPseudo)];
    memcpy(ph_bytes, &ph, sizeof(ph_bytes));
    for (u16 i = 0; i + 1 < (u16)sizeof(ph_bytes); i += 2) {
        sum += (u32)((ph_bytes[i] << 8) | ph_bytes[i + 1]);
    }

    const u16 *w = (const u16 *)segment;
    u16 count = seg_len;
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
// Send a TCP segment
// ---------------------------------------------------------------------------
static int tcp_send_segment(TCPSocket *s, u8 flags,
                            const u8 *data, u16 data_len) {
    u8 seg[20 + TCP_SEGMENT_MAX];
    if (data_len > TCP_SEGMENT_MAX) data_len = TCP_SEGMENT_MAX;

    TCPHeader *hdr = (TCPHeader *)seg;
    hdr->src_port   = htons(s->local_port);
    hdr->dst_port   = htons(s->remote_port);
    hdr->seq        = htonl(s->seq);
    hdr->ack        = htonl(s->ack);
    hdr->data_offset = (5 << 4);  // 5 * 4 = 20 bytes header
    hdr->flags      = flags;
    hdr->window     = htons(s->window);
    hdr->checksum   = 0;
    hdr->urgent     = 0;

    if (data && data_len > 0) {
        memcpy(seg + 20, data, data_len);
    }

    u16 total = (u16)(20 + data_len);
    hdr->checksum = tcp_checksum(s->local_ip, s->remote_ip, seg, total);

    return ipv4_send(s->remote_ip, IP_PROTO_TCP, seg, total);
}

// ---------------------------------------------------------------------------
// Connect (active open)
// ---------------------------------------------------------------------------
int tcp_connect(u32 dst_ip, u16 dst_port) {
    // Find free socket
    int fd = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].active) { fd = i; break; }
    }
    if (fd < 0) return -1;

    TCPSocket *s = &tcp_sockets[fd];
    memset(s, 0, sizeof(TCPSocket));

    s->active      = true;
    s->state       = TCP_SYN_SENT;
    s->local_ip    = net_ip;
    s->local_port  = tcp_next_port++;
    s->remote_ip   = dst_ip;
    s->remote_port = dst_port;
    s->seq         = (u32)(timer_get_ms() * 6364136223846793005ULL + 1442695040888963407ULL);  // timer-based ISN
    s->ack         = 0;
    s->window      = 4096;

    // Send SYN
    tcp_send_segment(s, TCP_SYN, NULL, 0);
    s->seq++;  // SYN consumes one sequence number

    // Wait for SYN-ACK
    u64 deadline = timer_get_ms() + 5000;
    while (timer_get_ms() < deadline) {
        net_rx_process();
        if (s->state == TCP_ESTABLISHED) {
            return fd;
        }
        if (s->state == TCP_CLOSED) {
            break;
        }
    }

    s->active = false;
    s->state  = TCP_CLOSED;
    return -1;
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------
void tcp_close(int fd) {
    if (fd < 0 || fd >= TCP_MAX_SOCKETS) return;
    TCPSocket *s = &tcp_sockets[fd];
    if (!s->active) return;

    if (s->state == TCP_ESTABLISHED || s->state == TCP_CLOSE_WAIT) {
        s->state = TCP_FIN_WAIT1;
        tcp_send_segment(s, TCP_FIN | TCP_ACK, NULL, 0);
        s->seq++;

        // Wait for FIN-ACK (FIN_WAIT2 or TIME_WAIT)
        u64 deadline = timer_get_ms() + 3000;
        while (timer_get_ms() < deadline) {
            net_rx_process();
            if (s->state == TCP_TIME_WAIT || s->state == TCP_CLOSED) break;
        }
    }

    memset(s, 0, sizeof(TCPSocket));
}

// ---------------------------------------------------------------------------
// Send data
// ---------------------------------------------------------------------------
int tcp_send(int fd, const u8 *data, u32 len) {
    if (fd < 0 || fd >= TCP_MAX_SOCKETS) return -1;
    TCPSocket *s = &tcp_sockets[fd];
    if (!s->active || s->state != TCP_ESTABLISHED) return -1;
    if (len > TCP_SEGMENT_MAX) len = TCP_SEGMENT_MAX;

    tcp_send_segment(s, TCP_PSH | TCP_ACK, data, (u16)len);
    s->seq += len;
    return (int)len;
}

// ---------------------------------------------------------------------------
// Receive data (blocking)
// ---------------------------------------------------------------------------
int tcp_recv(int fd, u8 *buf, u32 max_len, u32 timeout_ms) {
    if (fd < 0 || fd >= TCP_MAX_SOCKETS) return -1;
    TCPSocket *s = &tcp_sockets[fd];
    if (!s->active) return -1;

    u64 deadline = timer_get_ms() + timeout_ms;
    while (timer_get_ms() < deadline) {
        net_rx_process();

        if (s->recv_len > 0) {
            u32 copy = s->recv_len < max_len ? s->recv_len : max_len;
            memcpy(buf, s->recv_buf, copy);
            // Shift remaining data
            if (copy < s->recv_len) {
                memmove(s->recv_buf, s->recv_buf + copy, s->recv_len - copy);
            }
            s->recv_len -= copy;
            return (int)copy;
        }

        if (s->state == TCP_CLOSE_WAIT || s->state == TCP_CLOSED) {
            return 0;  // EOF
        }
    }

    return -1;  // timeout
}

// ---------------------------------------------------------------------------
// RX handler (called from IPv4)
// ---------------------------------------------------------------------------
void tcp_rx_handler(u32 src_ip, u32 dst_ip, const u8 *payload, u16 len) {
    if (len < 20) return;

    const TCPHeader *hdr = (const TCPHeader *)payload;
    u8 hdr_len = (u8)((hdr->data_offset >> 4) * 4);
    if (hdr_len < 20 || hdr_len > len) return;

    // Validate checksum
    u16 saved_cksum = hdr->checksum;
    // Build mutable copy for checksum validation
    u8 tmp[1500];
    if (len > 1500) return;
    memcpy(tmp, payload, len);
    ((TCPHeader *)tmp)->checksum = 0;
    u16 computed = tcp_checksum(src_ip, dst_ip, tmp, len);
    if (computed != saved_cksum) return;

    u16 src_port = ntohs(hdr->src_port);
    u16 dst_port = ntohs(hdr->dst_port);
    u32 seg_seq  = ntohl(hdr->seq);
    u32 seg_ack  = ntohl(hdr->ack);
    u8  flags    = hdr->flags;

    const u8 *data     = payload + hdr_len;
    u16       data_len = (u16)(len - hdr_len);

    // Find matching socket
    TCPSocket *s = NULL;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active &&
            tcp_sockets[i].remote_ip   == src_ip   &&
            tcp_sockets[i].remote_port == src_port &&
            tcp_sockets[i].local_port  == dst_port) {
            s = &tcp_sockets[i];
            break;
        }
    }
    if (!s) return;

    // RST: always close
    if (flags & TCP_RST) {
        s->state  = TCP_CLOSED;
        s->active = false;
        return;
    }

    switch (s->state) {
        case TCP_SYN_SENT:
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                // Verify ACK matches our SYN
                if (seg_ack != s->seq) break;
                s->ack = seg_seq + 1;
                s->remote_seq = seg_seq;
                s->state = TCP_ESTABLISHED;
                // Send ACK
                tcp_send_segment(s, TCP_ACK, NULL, 0);
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_ACK) {
                // Data segment
                if (data_len > 0) {
                    u32 space = 4096 - s->recv_len;
                    u32 copy  = data_len < space ? data_len : space;
                    memcpy(s->recv_buf + s->recv_len, data, copy);
                    s->recv_len += copy;
                    s->ack = seg_seq + data_len;
                    // Send ACK
                    tcp_send_segment(s, TCP_ACK, NULL, 0);
                }
                if (flags & TCP_FIN) {
                    s->ack = seg_seq + data_len + 1;
                    s->state = TCP_CLOSE_WAIT;
                    tcp_send_segment(s, TCP_ACK, NULL, 0);
                }
            }
            break;

        case TCP_FIN_WAIT1:
            if (flags & TCP_ACK) {
                s->state = TCP_FIN_WAIT2;
            }
            if (flags & TCP_FIN) {
                s->ack = seg_seq + 1;
                tcp_send_segment(s, TCP_ACK, NULL, 0);
                s->state = TCP_TIME_WAIT;
            }
            break;

        case TCP_FIN_WAIT2:
            if (flags & TCP_FIN) {
                s->ack = seg_seq + 1;
                tcp_send_segment(s, TCP_ACK, NULL, 0);
                s->state = TCP_TIME_WAIT;
            }
            break;

        case TCP_CLOSE_WAIT:
            // Waiting for application to call tcp_close
            break;

        case TCP_LAST_ACK:
            if (flags & TCP_ACK) {
                s->state  = TCP_CLOSED;
                s->active = false;
            }
            break;

        case TCP_TIME_WAIT:
            s->state  = TCP_CLOSED;
            s->active = false;
            break;

        default:
            break;
    }
}
