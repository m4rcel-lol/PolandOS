// PolandOS — Wirtualny system plikow (VFS)
#pragma once
#include "../../include/types.h"

#define VFS_MAX_NAME      64
#define VFS_MAX_NODES     256
#define VFS_MAX_MOUNTS    8

// Node types
#define VFS_FILE          0x01
#define VFS_DIRECTORY     0x02
#define VFS_CHARDEVICE    0x03
#define VFS_BLOCKDEVICE   0x04
#define VFS_PIPE          0x05
#define VFS_SYMLINK       0x06
#define VFS_MOUNTPOINT    0x08

typedef struct vfs_node VFSNode;

typedef i64  (*vfs_read_fn)(VFSNode *node, u64 offset, u64 size, u8 *buffer);
typedef i64  (*vfs_write_fn)(VFSNode *node, u64 offset, u64 size, const u8 *buffer);
typedef int  (*vfs_open_fn)(VFSNode *node);
typedef void (*vfs_close_fn)(VFSNode *node);

struct vfs_node {
    char    name[VFS_MAX_NAME];
    u32     type;
    u64     size;
    u32     inode;
    u32     uid;
    u32     gid;
    u32     permissions;
    VFSNode *parent;
    VFSNode *children;
    VFSNode *next;       // sibling linked list

    // Callbacks
    vfs_read_fn  read;
    vfs_write_fn write;
    vfs_open_fn  open;
    vfs_close_fn close;
};

void     vfs_init(void);
VFSNode *vfs_get_root(void);
VFSNode *vfs_lookup(const char *path);
VFSNode *vfs_create_node(const char *name, u32 type);
int      vfs_add_child(VFSNode *parent, VFSNode *child);
i64      vfs_read(VFSNode *node, u64 offset, u64 size, u8 *buffer);
i64      vfs_write(VFSNode *node, u64 offset, u64 size, const u8 *buffer);
void     vfs_list_dir(VFSNode *dir);
