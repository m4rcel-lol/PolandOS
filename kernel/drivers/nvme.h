// PolandOS — Sterownik NVMe
#pragma once
#include "../../include/types.h"

int  nvme_init(void);
int  nvme_read_blocks(u32 nsid, u64 lba, u32 count, void *buf);
int  nvme_write_blocks(u32 nsid, u64 lba, u32 count, const void *buf);
u64  nvme_get_block_count(u32 nsid);
u32  nvme_get_block_size(u32 nsid);
