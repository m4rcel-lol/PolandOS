// PolandOS — Sterownik NVMe
// Implementacja NVM Express dla kernela PolandOS
#include "nvme.h"
#include "pci.h"
#include "../../include/io.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../arch/x86_64/mm/pmm.h"
#include "../arch/x86_64/mm/heap.h"

// ─── NVMe MMIO register offsets ──────────────────────────────────────────────
#define NVME_CAP    0x00   // Controller Capabilities (64-bit)
#define NVME_VS     0x08   // Version (32-bit)
#define NVME_INTMS  0x0C   // Interrupt Mask Set
#define NVME_INTMC  0x10   // Interrupt Mask Clear
#define NVME_CC     0x14   // Controller Configuration
#define NVME_CSTS   0x1C   // Controller Status
#define NVME_NSSR   0x20   // NVM Subsystem Reset
#define NVME_AQA    0x24   // Admin Queue Attributes
#define NVME_ASQ    0x28   // Admin Submission Queue Base (64-bit)
#define NVME_ACQ    0x30   // Admin Completion Queue Base (64-bit)

// CC bits
#define NVME_CC_EN      (1u << 0)
#define NVME_CC_CSS_NVM (0u << 4)   // NVM command set
#define NVME_CC_MPS_4K  (0u << 7)   // memory page size = 4KB (2^(12+0))
#define NVME_CC_AMS_RR  (0u << 11)  // round-robin arbitration
#define NVME_CC_IOSQES  (6u << 16)  // IO SQ entry size = 2^6 = 64 bytes
#define NVME_CC_IOCQES  (4u << 20)  // IO CQ entry size = 2^4 = 16 bytes
#define NVME_CC_DEFAULT (NVME_CC_CSS_NVM | NVME_CC_MPS_4K | NVME_CC_AMS_RR | \
                         NVME_CC_IOSQES | NVME_CC_IOCQES)

// CSTS bits
#define NVME_CSTS_RDY   (1u << 0)
#define NVME_CSTS_CFS   (1u << 1)

// CAP fields
#define NVME_CAP_MQES(cap)   ((u32)((cap) & 0xFFFF))         // max queue entries (0-based)
#define NVME_CAP_TO(cap)     ((u32)(((cap) >> 24) & 0xFF))   // timeout (500ms units)
#define NVME_CAP_DSTRD(cap)  ((u32)(((cap) >> 32) & 0xF))    // doorbell stride

// ─── Admin command opcodes ────────────────────────────────────────────────────
#define NVME_ADMIN_DELETE_SQ  0x00
#define NVME_ADMIN_CREATE_SQ  0x01
#define NVME_ADMIN_DELETE_CQ  0x04
#define NVME_ADMIN_CREATE_CQ  0x05
#define NVME_ADMIN_IDENTIFY   0x06

// NVM command opcodes
#define NVME_CMD_WRITE  0x01
#define NVME_CMD_READ   0x02

// Identify CNS values
#define NVME_CNS_NS         0x00  // identify namespace
#define NVME_CNS_CTRL       0x01  // identify controller

// ─── Data structures ─────────────────────────────────────────────────────────
// NVMe Submission Queue Entry (64 bytes)
typedef struct __attribute__((packed)) {
    u32 cdw0;    // opcode[7:0], fuse[9:8], psdt[15:14], cid[31:16]
    u32 nsid;
    u64 reserved;
    u64 mptr;
    u64 prp1;
    u64 prp2;
    u32 cdw10;
    u32 cdw11;
    u32 cdw12;
    u32 cdw13;
    u32 cdw14;
    u32 cdw15;
} NVMeSQE;

// NVMe Completion Queue Entry (16 bytes)
typedef struct __attribute__((packed)) {
    u32 dw0;        // command-specific
    u32 dw1;        // reserved
    u16 sq_head;    // SQ head pointer
    u16 sq_id;      // SQ identifier
    u16 cid;        // command identifier
    u16 status;     // status[15:1], phase[0]
} NVMeCQE;

#define QUEUE_DEPTH  64

// ─── Driver state ─────────────────────────────────────────────────────────────
static volatile u8 *nvme_base   = NULL;
static u32          nvme_dstrd  = 0;

// Admin queues
static NVMeSQE *asq      = NULL;   // virtual
static NVMeCQE *acq      = NULL;   // virtual
static u64      asq_phys = 0;
static u64      acq_phys = 0;
static u32      asq_tail = 0;
static u32      acq_head = 0;
static u8       acq_phase = 1;     // initial expected phase = 1
static u16      admin_cid = 0;

// I/O queues
static NVMeSQE *iosq      = NULL;
static NVMeCQE *iocq      = NULL;
static u64      iosq_phys = 0;
static u64      iocq_phys = 0;
static u32      iosq_tail = 0;
static u32      iocq_head = 0;
static u8       iocq_phase = 1;
static u16      io_cid    = 0;

// Namespace info
static u64 ns_block_count = 0;
static u32 ns_block_size  = 512;

// ─── Register accessors ───────────────────────────────────────────────────────
static inline u32 nvme_readl(u32 off) {
    return *(volatile u32 *)(nvme_base + off);
}

static inline u64 nvme_readq(u32 off) {
    u32 lo = *(volatile u32 *)(nvme_base + off);
    u32 hi = *(volatile u32 *)(nvme_base + off + 4);
    return ((u64)hi << 32) | lo;
}

static inline void nvme_writel(u32 off, u32 val) {
    *(volatile u32 *)(nvme_base + off) = val;
}

static inline void nvme_writeq(u32 off, u64 val) {
    *(volatile u32 *)(nvme_base + off)     = (u32)(val);
    *(volatile u32 *)(nvme_base + off + 4) = (u32)(val >> 32);
}

// Doorbell register: SQ/CQ tail/head at 0x1000 + (2*qid + 0/1) * (4 << dstrd)
static inline void nvme_ring_sq(u32 qid, u32 tail) {
    u32 off = 0x1000 + ((2 * qid + 0) * (4u << nvme_dstrd));
    nvme_writel(off, tail);
}

static inline void nvme_ring_cq(u32 qid, u32 head) {
    u32 off = 0x1000 + ((2 * qid + 1) * (4u << nvme_dstrd));
    nvme_writel(off, head);
}

// ─── Submit admin command and poll for completion ────────────────────────────
static int nvme_admin_submit(NVMeSQE *cmd, u32 *result) {
    u16 cid = admin_cid++;
    cmd->cdw0 = (cmd->cdw0 & 0xFFFF) | ((u32)cid << 16);

    asq[asq_tail] = *cmd;
    asq_tail = (asq_tail + 1) % QUEUE_DEPTH;
    nvme_ring_sq(0, asq_tail);

    // Poll completion queue
    u32 timeout = 10000000;
    while (timeout--) {
        NVMeCQE *cqe = &acq[acq_head];
        if ((cqe->status & 1) == acq_phase) {
            // Got a completion
            u16 status_code = (cqe->status >> 1) & 0x7FFF;
            if (result) *result = cqe->dw0;

            acq_head++;
            if (acq_head >= QUEUE_DEPTH) {
                acq_head = 0;
                acq_phase ^= 1;
            }
            nvme_ring_cq(0, acq_head);

            if (status_code != 0) {
                kprintf("[BLAD] NVMe: admin cmd blad status=0x%x\n", status_code);
                return -1;
            }
            return 0;
        }
        cpu_relax();
    }
    kprintf("[BLAD] NVMe: timeout admin cmd!\n");
    return -2;
}

// ─── Submit I/O command and poll for completion ──────────────────────────────
static int nvme_io_submit(NVMeSQE *cmd) {
    u16 cid = io_cid++;
    cmd->cdw0 = (cmd->cdw0 & 0xFFFF) | ((u32)cid << 16);

    iosq[iosq_tail] = *cmd;
    iosq_tail = (iosq_tail + 1) % QUEUE_DEPTH;
    nvme_ring_sq(1, iosq_tail);

    u32 timeout = 10000000;
    while (timeout--) {
        NVMeCQE *cqe = &iocq[iocq_head];
        if ((cqe->status & 1) == iocq_phase) {
            u16 status_code = (cqe->status >> 1) & 0x7FFF;

            iocq_head++;
            if (iocq_head >= QUEUE_DEPTH) {
                iocq_head = 0;
                iocq_phase ^= 1;
            }
            nvme_ring_cq(1, iocq_head);

            if (status_code != 0) {
                kprintf("[BLAD] NVMe: I/O cmd blad status=0x%x\n", status_code);
                return -1;
            }
            return 0;
        }
        cpu_relax();
    }
    kprintf("[BLAD] NVMe: timeout I/O cmd!\n");
    return -2;
}

// ─── nvme_init ────────────────────────────────────────────────────────────────
int nvme_init(void) {
    // Find NVMe controller: class=0x01, subclass=0x08
    PCIDevice *pdev = pci_find_class(0x01, 0x08);
    if (!pdev) {
        kprintf("[INFO] NVMe: brak kontrolera NVMe\n");
        return -1;
    }
    kprintf("[DOBRZE] NVMe: znaleziono %04x:%04x [%02x:%02x.%x]\n",
            pdev->vendor_id, pdev->device_id,
            pdev->bus, pdev->slot, pdev->func);

    pci_enable_busmaster(pdev->bus, pdev->slot, pdev->func);

    u64 bar0 = pci_get_bar_addr(pdev->bus, pdev->slot, pdev->func, 0);
    if (bar0 == 0) {
        kprintf("[BLAD] NVMe: BAR0 = 0!\n");
        return -1;
    }

    nvme_base = (volatile u8 *)(bar0 + hhdm_offset);

    // Read capabilities
    u64 cap = nvme_readq(NVME_CAP);
    nvme_dstrd = NVME_CAP_DSTRD(cap);
    u32 mqes   = NVME_CAP_MQES(cap) + 1;
    u32 to     = NVME_CAP_TO(cap);
    kprintf("[DOBRZE] NVMe: CAP mqes=%u dstrd=%u to=%u*500ms\n", mqes, nvme_dstrd, to);

    // Disable controller (CC.EN = 0), wait CSTS.RDY = 0
    nvme_writel(NVME_CC, 0);
    u32 timeout = to * 500000;
    while (nvme_readl(NVME_CSTS) & NVME_CSTS_RDY) {
        cpu_relax();
        if (--timeout == 0) {
            kprintf("[BLAD] NVMe: timeout oczekiwania na wylaczenie!\n");
            return -1;
        }
    }

    // Allocate admin queues (each fits in a single 4K page)
    asq_phys = pmm_alloc();
    acq_phys = pmm_alloc();
    if (!asq_phys || !acq_phys) {
        kprintf("[BLAD] NVMe: brak pamieci na kolejki admin!\n");
        return -1;
    }
    asq = (NVMeSQE *)(asq_phys + hhdm_offset);
    acq = (NVMeCQE *)(acq_phys + hhdm_offset);
    memset(asq, 0, QUEUE_DEPTH * sizeof(NVMeSQE));
    memset(acq, 0, QUEUE_DEPTH * sizeof(NVMeCQE));

    // Set AQA: admin CQ size - 1 (bits 27:16), admin SQ size - 1 (bits 11:0)
    nvme_writel(NVME_AQA, (u32)(((QUEUE_DEPTH - 1) << 16) | (QUEUE_DEPTH - 1)));
    nvme_writeq(NVME_ASQ, asq_phys);
    nvme_writeq(NVME_ACQ, acq_phys);

    // Enable controller with CC defaults
    nvme_writel(NVME_CC, NVME_CC_DEFAULT | NVME_CC_EN);

    // Wait CSTS.RDY = 1
    timeout = to * 500000;
    while (!(nvme_readl(NVME_CSTS) & NVME_CSTS_RDY)) {
        cpu_relax();
        if (nvme_readl(NVME_CSTS) & NVME_CSTS_CFS) {
            kprintf("[BLAD] NVMe: blad kontrolera (CFS)!\n");
            return -1;
        }
        if (--timeout == 0) {
            kprintf("[BLAD] NVMe: timeout oczekiwania na gotowos!\n");
            return -1;
        }
    }
    kprintf("[DOBRZE] NVMe: kontroler wlaczony\n");

    // Identify Controller (CNS=1): just to verify it works
    u64 id_phys = pmm_alloc();
    if (!id_phys) return -1;
    u8 *id_buf = (u8 *)(id_phys + hhdm_offset);
    memset(id_buf, 0, 4096);

    NVMeSQE cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0  = NVME_ADMIN_IDENTIFY;
    cmd.prp1  = id_phys;
    cmd.cdw10 = NVME_CNS_CTRL;
    if (nvme_admin_submit(&cmd, NULL) != 0) {
        kprintf("[BLAD] NVMe: Identify Controller nieudany!\n");
        pmm_free(id_phys);
        return -1;
    }
    // id_buf[24..63] = model number (40 bytes, ASCII, space-padded)
    kprintf("[DOBRZE] NVMe: Identify OK, model=%.40s\n", id_buf + 24);

    // Identify Namespace 1 (CNS=0, NSID=1)
    u64 ns_phys = pmm_alloc();
    if (!ns_phys) { pmm_free(id_phys); return -1; }
    u8 *ns_buf = (u8 *)(ns_phys + hhdm_offset);
    memset(ns_buf, 0, 4096);

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0  = NVME_ADMIN_IDENTIFY;
    cmd.nsid  = 1;
    cmd.prp1  = ns_phys;
    cmd.cdw10 = NVME_CNS_NS;
    if (nvme_admin_submit(&cmd, NULL) != 0) {
        kprintf("[BLAD] NVMe: Identify Namespace nieudany!\n");
        pmm_free(ns_phys);
        pmm_free(id_phys);
        return -1;
    }

    // Parse NSZE (bytes 0-7) and LBAF
    ns_block_count = *(u64 *)(ns_buf + 0);
    u8 flbas    = ns_buf[26];
    u8 lbaf_idx = flbas & 0x0F;
    u32 lbaf    = *(u32 *)(ns_buf + 128 + lbaf_idx * 4);
    u8 lbads    = (u8)((lbaf >> 16) & 0x0F);
    ns_block_size = 1u << lbads;

    kprintf("[DOBRZE] NVMe: NS1 bloki=%lu, rozmiar=%u B\n",
            ns_block_count, ns_block_size);

    pmm_free(ns_phys);
    pmm_free(id_phys);

    // Create I/O Completion Queue (admin cmd 0x05), QID=1
    iocq_phys = pmm_alloc();
    if (!iocq_phys) return -1;
    iocq = (NVMeCQE *)(iocq_phys + hhdm_offset);
    memset(iocq, 0, QUEUE_DEPTH * sizeof(NVMeCQE));

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0  = NVME_ADMIN_CREATE_CQ;
    cmd.prp1  = iocq_phys;
    cmd.cdw10 = (u32)(((QUEUE_DEPTH - 1) << 16) | 1);  // QSIZE-1 | QID=1
    cmd.cdw11 = 0x1;  // PC=1 (physically contiguous), IEN=0
    if (nvme_admin_submit(&cmd, NULL) != 0) {
        kprintf("[BLAD] NVMe: Create IO CQ nieudany!\n");
        return -1;
    }

    // Create I/O Submission Queue (admin cmd 0x01), QID=1, CQID=1
    iosq_phys = pmm_alloc();
    if (!iosq_phys) return -1;
    iosq = (NVMeSQE *)(iosq_phys + hhdm_offset);
    memset(iosq, 0, QUEUE_DEPTH * sizeof(NVMeSQE));

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0  = NVME_ADMIN_CREATE_SQ;
    cmd.prp1  = iosq_phys;
    cmd.cdw10 = (u32)(((QUEUE_DEPTH - 1) << 16) | 1);  // QSIZE-1 | QID=1
    cmd.cdw11 = (u32)((1u << 16) | (1u << 0));  // CQID=1 | PC=1
    if (nvme_admin_submit(&cmd, NULL) != 0) {
        kprintf("[BLAD] NVMe: Create IO SQ nieudany!\n");
        return -1;
    }

    kprintf("[DOBRZE] NVMe: inicjalizacja zakonczona\n");
    return 0;
}

// ─── Block I/O ────────────────────────────────────────────────────────────────
int nvme_read_blocks(u32 nsid, u64 lba, u32 count, void *buf) {
    if (!nvme_base || !iosq) return -1;
    if (count == 0 || count > (4096 / ns_block_size)) return -1;

    // Get physical address of buffer (assumes buf is already from pmm or mapped)
    u64 buf_phys = (u64)buf - hhdm_offset;

    NVMeSQE cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0  = NVME_CMD_READ;
    cmd.nsid  = (nsid == 0) ? 1 : nsid;
    cmd.prp1  = buf_phys;
    cmd.prp2  = 0;
    cmd.cdw10 = (u32)(lba & 0xFFFFFFFF);
    cmd.cdw11 = (u32)(lba >> 32);
    cmd.cdw12 = count - 1;   // NLB is 0-based

    return nvme_io_submit(&cmd);
}

int nvme_write_blocks(u32 nsid, u64 lba, u32 count, const void *buf) {
    if (!nvme_base || !iosq) return -1;
    if (count == 0 || count > (4096 / ns_block_size)) return -1;

    u64 buf_phys = (u64)buf - hhdm_offset;

    NVMeSQE cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0  = NVME_CMD_WRITE;
    cmd.nsid  = (nsid == 0) ? 1 : nsid;
    cmd.prp1  = buf_phys;
    cmd.prp2  = 0;
    cmd.cdw10 = (u32)(lba & 0xFFFFFFFF);
    cmd.cdw11 = (u32)(lba >> 32);
    cmd.cdw12 = count - 1;

    return nvme_io_submit(&cmd);
}

u64 nvme_get_block_count(u32 nsid) {
    (void)nsid;
    return ns_block_count;
}

u32 nvme_get_block_size(u32 nsid) {
    (void)nsid;
    return ns_block_size;
}
