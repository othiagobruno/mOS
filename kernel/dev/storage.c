/* mOS — registro de discos físicos e montagem dos volumes deles em /Volumes */
#include "storage.h"

#define MAX_DISKS 16

static struct { blkdev_t *dev; char name[16]; char desc[96]; } disks[MAX_DISKS];
static int ndisks;

void storage_register(blkdev_t *dev, const char *name, const char *desc) {
    if (!dev || ndisks >= MAX_DISKS) return;
    disks[ndisks].dev = dev;
    strncpy(disks[ndisks].name, name, sizeof disks[ndisks].name - 1);
    strncpy(disks[ndisks].desc, desc ? desc : name, sizeof disks[ndisks].desc - 1);
    ndisks++;
    kprintf("storage: %s = %s%s\n", name, desc ? desc : "", blk_writable(dev) ? "" : " (somente leitura)");
}

int storage_disk_count(void) { return ndisks; }
blkdev_t *storage_disk(int i) { return i >= 0 && i < ndisks ? disks[i].dev : NULL; }
const char *storage_disk_name(int i) { return i >= 0 && i < ndisks ? disks[i].name : ""; }
const char *storage_disk_desc(int i) { return i >= 0 && i < ndisks ? disks[i].desc : ""; }

/* chamado depois de vfs_init/dfs_mount_all: volumes HFS+/APFS/FAT dos discos aparecem em /Volumes */
void storage_mount_all(void) {
    for (int i = 0; i < ndisks; i++) {
        dmount_t *before = dfs_mounts();
        if (dfs_mount_disk(disks[i].dev, disks[i].name)) dfs_publish_new(before);
    }
    extern void install_selftest(void);
    install_selftest();
}
