// PolandOS — Sterownik Intel e1000 (82540EM i kompatybilne)
#include "e1000.h"
#include "pci.h"
#include "../../include/io.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../arch/x86_64/mm/pmm.h"
#include "../arch/x86_64/mm/heap.h"
#include "../arch/x86_64/cpu/idt.h"
#include "../arch/x86_64/cpu/apic.h"

// ─── e1000 register offsets ───────────────────────────────────────────────────
#define E1000_CTRL    0x0000
#define E1000_STATUS  0x0008
#define E1000_EECD    0x0010
#define E1000_EERD    0x0014
#define E1000_ICR     0x00C0
#define E1000_ITR     0x00C4
#define E1000_ICS     0x00C8
#define E1000_IMS     0x00D0
#define E1000_IMC     0x00D8
#define E1000_RCTL    0x0100
#define E1000_TCTL    0x0400
#define E1000_TIPG    0x0410
#define E1000_RDBAL   0x2800
#define E1000_RDBAH   0x2804
#define E1000_RDLEN   0x2808
#define E1000_RDH     0x2810
#define E1000_RDT     0x2818
#define E1000_TDBAL   0x3800
#define E1000_TDBAH   0x3804
#define E1000_TDLEN   0x3808
#define E1000_TDH     0x3810
#define E1000_TDT     0x3818
#define E1000_MTA     0x5200   // Multicast Table Array (128 x 32-bit)
#define E1000_RAL0    0x5400
#define E1000_RAH0    0x5404

// ─── CTRL bits ────────────────────────────────────────────────────────────────
#define E1000_CTRL_FD       (1u << 0)    // full duplex
#define E1000_CTRL_SLU      (1u << 6)    // set link up
#define E1000_CTRL_SPEED_1G (2u << 8)    // 1000 Mbps
#define E1000_CTRL_RST      (1u << 26)   // device reset

// ─── RCTL bits ───────────────────────────────────────────────────────────────
#define E1000_RCTL_EN       (1u << 1)
#define E1000_RCTL_SBP      (1u << 2)
#define E1000_RCTL_UPE      (1u << 3)   // unicast promiscuous
#define E1000_RCTL_MPE      (1u << 4)   // multicast promiscuous
#define E1000_RCTL_BAM      (1u << 15)  // broadcast accept
#define E1000_RCTL_BSIZE_2K (0u << 16)  // buffer size = 2048
#define E1000_RCTL_SECRC    (1u << 26)  // strip Ethernet CRC

// ─── TCTL bits ───────────────────────────────────────────────────────────────
#define E1000_TCTL_EN       (1u << 1)
#define E1000_TCTL_PSP      (1u << 3)   // pad short packets
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

// ─── TX descriptor CMD bits ──────────────────────────────────────────────────
#define E1000_TXD_CMD_EOP   (1u << 0)   // end of packet
#define E1000_TXD_CMD_IFCS  (1u << 1)   // insert FCS
#define E1000_TXD_CMD_RS    (1u << 3)   // report status (set DD when done)

// ─── TX descriptor STATUS bits ───────────────────────────────────────────────
#define E1000_TXD_STAT_DD   (1u << 0)   // descriptor done

// ─── RX descriptor STATUS bits ───────────────────────────────────────────────
#define E1000_RXD_STAT_DD   (1u << 0)   // descriptor done
#define E1000_RXD_STAT_EOP  (1u << 1)   // end of packet

// ─── ICR/IMS bits ────────────────────────────────────────────────────────────
#define E1000_ICR_TXDW      (1u << 0)   // TX descriptor written back
#define E1000_ICR_LSC       (1u << 2)   // link status change
#define E1000_ICR_RXT0      (1u << 7)   // RX timer interrupt

// ─── EERD bits ───────────────────────────────────────────────────────────────
#define E1000_EERD_START    (1u << 0)
#define E1000_EERD_DONE     (1u << 4)
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_DATA_SHIFT 16

// ─── Ring sizes ───────────────────────────────────────────────────────────────
#define E1000_TX_RING_SIZE  16
#define E1000_RX_RING_SIZE  32
#define E1000_BUF_SIZE      2048

// ─── TX descriptor ───────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    u64 buffer_addr;
    u16 length;
    u8  cso;
    u8  cmd;
    u8  status;
    u8  css;
    u16 special;
} E1000TxDesc;

// ─── RX descriptor ───────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    u64 buffer_addr;
    u16 length;
    u16 checksum;
    u8  status;
    u8  errors;
    u16 special;
} E1000RxDesc;

// ─── IRQ vector ──────────────────────────────────────────────────────────────
#define E1000_IRQ_VECTOR  0x2B    // vector 43 = IRQ 11

// ─── Globals ─────────────────────────────────────────────────────────────────
u8 e1000_mac[ETH_ALEN];

static volatile u8 *e1000_base = NULL;

static E1000TxDesc *tx_ring     = NULL;
static u64          tx_ring_phys = 0;
static u8          *tx_bufs[E1000_TX_RING_SIZE];
static u64          tx_buf_phys[E1000_TX_RING_SIZE];
static u32          tx_tail     = 0;

static E1000RxDesc *rx_ring     = NULL;
static u64          rx_ring_phys = 0;
static u8          *rx_bufs[E1000_RX_RING_SIZE];
static u64          rx_buf_phys[E1000_RX_RING_SIZE];
static u32          rx_tail     = 0;
static u32          rx_head     = 0;

// ─── MMIO accessors ──────────────────────────────────────────────────────────
static inline u32 e1000_read(u32 reg) {
    return *(volatile u32 *)(e1000_base + reg);
}

static inline void e1000_write(u32 reg, u32 val) {
    *(volatile u32 *)(e1000_base + reg) = val;
}

// ─── EEPROM read ─────────────────────────────────────────────────────────────
static u16 e1000_eeprom_read(u8 addr) {
    e1000_write(E1000_EERD, E1000_EERD_START | ((u32)addr << E1000_EERD_ADDR_SHIFT));
    u32 val;
    u32 tries = 100000;
    do {
        val = e1000_read(E1000_EERD);
        if (--tries == 0) return 0;
    } while (!(val & E1000_EERD_DONE));
    return (u16)(val >> E1000_EERD_DATA_SHIFT);
}

// ─── MAC address detection ───────────────────────────────────────────────────
static void e1000_read_mac(void) {
    u32 ral = e1000_read(E1000_RAL0);
    u32 rah = e1000_read(E1000_RAH0);

    if (rah & (1u << 31)) {
        // RAL/RAH contain valid MAC
        e1000_mac[0] = (u8)(ral >>  0);
        e1000_mac[1] = (u8)(ral >>  8);
        e1000_mac[2] = (u8)(ral >> 16);
        e1000_mac[3] = (u8)(ral >> 24);
        e1000_mac[4] = (u8)(rah >>  0);
        e1000_mac[5] = (u8)(rah >>  8);
    } else {
        // Read from EEPROM (words 0, 1, 2)
        u16 w0 = e1000_eeprom_read(0);
        u16 w1 = e1000_eeprom_read(1);
        u16 w2 = e1000_eeprom_read(2);
        e1000_mac[0] = (u8)(w0 >> 0);
        e1000_mac[1] = (u8)(w0 >> 8);
        e1000_mac[2] = (u8)(w1 >> 0);
        e1000_mac[3] = (u8)(w1 >> 8);
        e1000_mac[4] = (u8)(w2 >> 0);
        e1000_mac[5] = (u8)(w2 >> 8);
    }
}

// ─── IRQ handler ─────────────────────────────────────────────────────────────
void e1000_irq_handler(void) {
    u32 icr = e1000_read(E1000_ICR);

    if (icr & E1000_ICR_LSC) {
        u32 status = e1000_read(E1000_STATUS);
        kprintf("[e1000] Link %s\n", (status & 2) ? "UP" : "DOWN");
    }

    apic_send_eoi();
}

static void e1000_irq_wrapper(InterruptFrame *frame) {
    (void)frame;
    e1000_irq_handler();
}

// ─── e1000_init ───────────────────────────────────────────────────────────────
int e1000_init(void) {
    // Try 0x100E first, then 0x100F
    PCIDevice *pdev = pci_find_device(0x8086, 0x100E);
    if (!pdev) pdev = pci_find_device(0x8086, 0x100F);
    if (!pdev) pdev = pci_find_device(0x8086, 0x1004);
    if (!pdev) {
        kprintf("[INFO] e1000: brak karty sieciowej Intel\n");
        return -1;
    }
    kprintf("[DOBRZE] e1000: znaleziono %04x:%04x [%02x:%02x.%x]\n",
            pdev->vendor_id, pdev->device_id,
            pdev->bus, pdev->slot, pdev->func);

    pci_enable_busmaster(pdev->bus, pdev->slot, pdev->func);

    u64 bar0 = pci_get_bar_addr(pdev->bus, pdev->slot, pdev->func, 0);
    if (bar0 == 0) {
        kprintf("[BLAD] e1000: BAR0 = 0!\n");
        return -1;
    }
    e1000_base = (volatile u8 *)(bar0 + hhdm_offset);

    // Hardware reset
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
    for (volatile int i = 0; i < 100000; i++) cpu_relax();

    // Wait until reset bit clears
    u32 tries = 1000000;
    while ((e1000_read(E1000_CTRL) & E1000_CTRL_RST) && --tries)
        cpu_relax();

    // Disable all interrupts
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    // Set link up, full duplex
    e1000_write(E1000_CTRL,
                E1000_CTRL_FD | E1000_CTRL_SLU | E1000_CTRL_SPEED_1G);

    // Read MAC address
    e1000_read_mac();
    kprintf("[DOBRZE] e1000: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            e1000_mac[0], e1000_mac[1], e1000_mac[2],
            e1000_mac[3], e1000_mac[4], e1000_mac[5]);

    // Clear multicast table
    for (int i = 0; i < 128; i++)
        e1000_write((u32)(E1000_MTA + i * 4), 0);

    // ── Init TX ring ────────────────────────────────────────────────────────
    tx_ring_phys = pmm_alloc();
    if (!tx_ring_phys) return -1;
    tx_ring = (E1000TxDesc *)(tx_ring_phys + hhdm_offset);
    memset(tx_ring, 0, 4096);

    for (int i = 0; i < E1000_TX_RING_SIZE; i++) {
        tx_buf_phys[i] = pmm_alloc();
        if (!tx_buf_phys[i]) return -1;
        tx_bufs[i] = (u8 *)(tx_buf_phys[i] + hhdm_offset);
        tx_ring[i].buffer_addr = tx_buf_phys[i];
        tx_ring[i].status = E1000_TXD_STAT_DD; // mark all as done initially
    }

    e1000_write(E1000_TDBAL, (u32)(tx_ring_phys & 0xFFFFFFFF));
    e1000_write(E1000_TDBAH, (u32)(tx_ring_phys >> 32));
    e1000_write(E1000_TDLEN, (u32)(E1000_TX_RING_SIZE * sizeof(E1000TxDesc)));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    tx_tail = 0;

    // Enable TX: EN + PSP + CT=0x0F + COLD=0x200 (full-duplex)
    e1000_write(E1000_TCTL,
                E1000_TCTL_EN |
                E1000_TCTL_PSP |
                (0x0Fu << E1000_TCTL_CT_SHIFT) |
                (0x200u << E1000_TCTL_COLD_SHIFT));
    // Recommended TIPG for copper GbE
    e1000_write(E1000_TIPG, 0x00702008);

    // ── Init RX ring ────────────────────────────────────────────────────────
    rx_ring_phys = pmm_alloc();
    if (!rx_ring_phys) return -1;
    rx_ring = (E1000RxDesc *)(rx_ring_phys + hhdm_offset);
    memset(rx_ring, 0, 4096);

    for (int i = 0; i < E1000_RX_RING_SIZE; i++) {
        rx_buf_phys[i] = pmm_alloc();
        if (!rx_buf_phys[i]) return -1;
        rx_bufs[i] = (u8 *)(rx_buf_phys[i] + hhdm_offset);
        rx_ring[i].buffer_addr = rx_buf_phys[i];
        rx_ring[i].status = 0;
    }

    e1000_write(E1000_RDBAL, (u32)(rx_ring_phys & 0xFFFFFFFF));
    e1000_write(E1000_RDBAH, (u32)(rx_ring_phys >> 32));
    e1000_write(E1000_RDLEN, (u32)(E1000_RX_RING_SIZE * sizeof(E1000RxDesc)));
    e1000_write(E1000_RDH, 0);
    rx_head = 0;
    rx_tail = E1000_RX_RING_SIZE - 1;
    e1000_write(E1000_RDT, rx_tail);

    // Enable RX: EN + UPE + MPE + BAM + BSIZE=2048 + SECRC
    e1000_write(E1000_RCTL,
                E1000_RCTL_EN |
                E1000_RCTL_UPE |
                E1000_RCTL_MPE |
                E1000_RCTL_BAM |
                E1000_RCTL_BSIZE_2K |
                E1000_RCTL_SECRC);

    // ── Interrupts ──────────────────────────────────────────────────────────
    idt_register_handler(E1000_IRQ_VECTOR, e1000_irq_wrapper);

    u8 irq = pdev->irq_line;
    if (irq == 0 || irq == 0xFF) irq = 11;
    ioapic_set_irq(irq, E1000_IRQ_VECTOR, 0);

    // Enable RXT0 + TXDW + LSC interrupts
    e1000_write(E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_TXDW | E1000_ICR_LSC);

    kprintf("[DOBRZE] e1000: inicjalizacja zakończona, IRQ%u -> vector 0x%x\n",
            irq, E1000_IRQ_VECTOR);
    return 0;
}

// ─── e1000_send ──────────────────────────────────────────────────────────────
int e1000_send(const void *data, u16 len) {
    if (!e1000_base) return -1;
    if (len > E1000_BUF_SIZE) return -1;

    // Find a free TX descriptor (status DD = done)
    u32 idx = tx_tail;
    if (!(tx_ring[idx].status & E1000_TXD_STAT_DD))
        return -1;  // ring full

    memcpy(tx_bufs[idx], data, len);
    tx_ring[idx].length = len;
    tx_ring[idx].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    tx_ring[idx].status = 0;

    tx_tail = (tx_tail + 1) % E1000_TX_RING_SIZE;
    e1000_write(E1000_TDT, tx_tail);

    return 0;
}

// ─── e1000_has_packet ─────────────────────────────────────────────────────────
int e1000_has_packet(void) {
    if (!e1000_base) return 0;
    return (rx_ring[rx_head].status & E1000_RXD_STAT_DD) != 0;
}

// ─── e1000_recv ──────────────────────────────────────────────────────────────
int e1000_recv(void *buf, u16 *len) {
    if (!e1000_base) return 0;

    E1000RxDesc *desc = &rx_ring[rx_head];
    if (!(desc->status & E1000_RXD_STAT_DD))
        return 0;  // no packet

    u16 pkt_len = desc->length;
    if (pkt_len > ETH_MAX_FRAME) pkt_len = ETH_MAX_FRAME;

    memcpy(buf, rx_bufs[rx_head], pkt_len);
    if (len) *len = pkt_len;

    // Give descriptor back to hardware
    desc->status = 0;
    rx_tail = rx_head;
    e1000_write(E1000_RDT, rx_tail);

    rx_head = (rx_head + 1) % E1000_RX_RING_SIZE;

    return 1;
}
