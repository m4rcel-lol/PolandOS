// PolandOS — Wirtualny system plikow (VFS)
// Warstwa abstrakcji systemu plikow
#include "vfs.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../arch/x86_64/mm/heap.h"

// ─── Node pool ────────────────────────────────────────────────────────────────
static VFSNode node_pool[VFS_MAX_NODES];
static int     node_count = 0;

static VFSNode *vfs_root = NULL;

// ─── Allocate a node ──────────────────────────────────────────────────────────
VFSNode *vfs_create_node(const char *name, u32 type) {
    if (node_count >= VFS_MAX_NODES) return NULL;
    VFSNode *node = &node_pool[node_count++];
    memset(node, 0, sizeof(VFSNode));
    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->name[VFS_MAX_NAME - 1] = '\0';
    node->type  = type;
    node->inode = (u32)node_count;
    return node;
}

// ─── Add child to directory ───────────────────────────────────────────────────
int vfs_add_child(VFSNode *parent, VFSNode *child) {
    if (!parent || !child) return -1;
    if (parent->type != VFS_DIRECTORY && parent->type != VFS_MOUNTPOINT) return -1;
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
    return 0;
}

// ─── Initialize VFS ───────────────────────────────────────────────────────────
void vfs_init(void) {
    node_count = 0;
    memset(node_pool, 0, sizeof(node_pool));

    // Create root directory
    vfs_root = vfs_create_node("/", VFS_DIRECTORY);

    // Create standard directories
    VFSNode *dev  = vfs_create_node("dev",  VFS_DIRECTORY);
    VFSNode *proc = vfs_create_node("proc", VFS_DIRECTORY);
    VFSNode *sys  = vfs_create_node("sys",  VFS_DIRECTORY);
    VFSNode *tmp  = vfs_create_node("tmp",  VFS_DIRECTORY);
    VFSNode *home = vfs_create_node("home", VFS_DIRECTORY);
    VFSNode *etc  = vfs_create_node("etc",  VFS_DIRECTORY);

    vfs_add_child(vfs_root, dev);
    vfs_add_child(vfs_root, proc);
    vfs_add_child(vfs_root, sys);
    vfs_add_child(vfs_root, tmp);
    vfs_add_child(vfs_root, home);
    vfs_add_child(vfs_root, etc);

    // Create device nodes
    VFSNode *fb0   = vfs_create_node("fb0",   VFS_CHARDEVICE);
    VFSNode *tty0  = vfs_create_node("tty0",  VFS_CHARDEVICE);
    VFSNode *mouse = vfs_create_node("mouse", VFS_CHARDEVICE);
    VFSNode *kbd   = vfs_create_node("kbd",   VFS_CHARDEVICE);

    vfs_add_child(dev, fb0);
    vfs_add_child(dev, tty0);
    vfs_add_child(dev, mouse);
    vfs_add_child(dev, kbd);

    kprintf("[DOBRZE] VFS zainicjalizowany (korzen: /)\n");
}

// ─── Get root ─────────────────────────────────────────────────────────────────
VFSNode *vfs_get_root(void) {
    return vfs_root;
}

// ─── Path lookup ──────────────────────────────────────────────────────────────
VFSNode *vfs_lookup(const char *path) {
    if (!path || !vfs_root) return NULL;
    if (path[0] == '/' && path[1] == '\0') return vfs_root;

    // Skip leading '/'
    const char *p = path;
    if (*p == '/') p++;

    VFSNode *current = vfs_root;

    char component[VFS_MAX_NAME];
    while (*p && current) {
        // Extract next path component
        int i = 0;
        while (*p && *p != '/' && i < VFS_MAX_NAME - 1) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        if (*p == '/') p++;

        // Search children
        VFSNode *child = current->children;
        VFSNode *found = NULL;
        while (child) {
            if (strcmp(child->name, component) == 0) {
                found = child;
                break;
            }
            child = child->next;
        }
        current = found;
    }

    return current;
}

// ─── Read/Write ───────────────────────────────────────────────────────────────
i64 vfs_read(VFSNode *node, u64 offset, u64 size, u8 *buffer) {
    if (!node || !node->read) return -1;
    return node->read(node, offset, size, buffer);
}

i64 vfs_write(VFSNode *node, u64 offset, u64 size, const u8 *buffer) {
    if (!node || !node->write) return -1;
    return node->write(node, offset, size, buffer);
}

// ─── List directory contents ──────────────────────────────────────────────────
void vfs_list_dir(VFSNode *dir) {
    if (!dir) {
        kprintf("VFS: pusty wezel\n");
        return;
    }
    if (dir->type != VFS_DIRECTORY && dir->type != VFS_MOUNTPOINT) {
        kprintf("VFS: %s nie jest katalogiem\n", dir->name);
        return;
    }

    kprintf("Zawartosc /%s:\n", dir->name);
    VFSNode *child = dir->children;
    while (child) {
        const char *type_str = "???";
        switch (child->type) {
        case VFS_FILE:        type_str = "plik";  break;
        case VFS_DIRECTORY:   type_str = "kat.";  break;
        case VFS_CHARDEVICE:  type_str = "char";  break;
        case VFS_BLOCKDEVICE: type_str = "blk";   break;
        case VFS_PIPE:        type_str = "pipe";  break;
        case VFS_SYMLINK:     type_str = "link";  break;
        case VFS_MOUNTPOINT:  type_str = "mount"; break;
        }
        kprintf("  [%s] %s\n", type_str, child->name);
        child = child->next;
    }
}
