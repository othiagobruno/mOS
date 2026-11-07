/* mOS — registro de discos físicos, política de escrita e montagem dos volumes deles em /Volumes */
#include "storage.h"

#define MAX_DISKS  16
#define MAX_GRANTS 8

static struct { blkdev_t *dev; char name[16]; char desc[96]; diskphys_t phys; } disks[MAX_DISKS];
static int ndisks;

void storage_register(blkdev_t *dev, const char *name, const char *desc) {
    if (!dev || ndisks >= MAX_DISKS) return;
    disks[ndisks].dev = dev;
    strncpy(disks[ndisks].name, name, sizeof disks[ndisks].name - 1);
    strncpy(disks[ndisks].desc, desc ? desc : name, sizeof disks[ndisks].desc - 1);
    ndisks++;
    kprintf("storage: %s = %s%s\n", name, desc ? desc : "", blk_hw_writable(dev) ? "" : " (somente leitura)");
}

int storage_disk_count(void) { return ndisks; }
blkdev_t *storage_disk(int i) { return i >= 0 && i < ndisks ? disks[i].dev : NULL; }
const char *storage_disk_name(int i) { return i >= 0 && i < ndisks ? disks[i].name : ""; }
const char *storage_disk_desc(int i) { return i >= 0 && i < ndisks ? disks[i].desc : ""; }

void storage_set_phys(blkdev_t *dev, const diskphys_t *p) {
    for (int i = 0; i < ndisks; i++)
        if (disks[i].dev == dev) { disks[i].phys = *p; return; }
}
const diskphys_t *storage_disk_phys(int i) { return i >= 0 && i < ndisks ? &disks[i].phys : NULL; }
int storage_disk_index(blkdev_t *root) {
    for (int i = 0; i < ndisks; i++) if (disks[i].dev == root) return i;
    return -1;
}

/* ------------------------------------------------------------------ política de escrita */
static struct { blkdev_t *disk; u64 off, len; } grants[MAX_GRANTS];
static int ngrants;
static u64 denied;

bool storage_grant_write(blkdev_t *disk, u64 off, u64 len) {
    if (!disk || storage_disk_index(disk) < 0 || !blk_hw_writable(disk) || !len) return false;
    if (off > disk->size || len > disk->size - off) return false;
    if (ngrants >= MAX_GRANTS) return false;
    grants[ngrants].disk = disk; grants[ngrants].off = off; grants[ngrants].len = len;
    ngrants++;
    kprintf("storage: escrita liberada em %s: [%llu, %llu)\n", storage_disk_name(storage_disk_index(disk)), off, off + len);
    return true;
}

void storage_revoke_write(blkdev_t *disk, u64 off, u64 len) {
    for (int i = 0; i < ngrants; i++)
        if (grants[i].disk == disk && grants[i].off == off && grants[i].len == len) {
            grants[i] = grants[--ngrants];
            kprintf("storage: escrita revogada em %s: [%llu, %llu)\n", storage_disk_name(storage_disk_index(disk)), off, off + len);
            return;
        }
}

void storage_revoke_all(void) {
    if (ngrants) kprintf("storage: %d liberação(ões) de escrita revogada(s)\n", ngrants);
    ngrants = 0;
}
int storage_grant_count(void) { return ngrants; }
u64 storage_denied_writes(void) { return denied; }

bool storage_write_ok(blkdev_t *root, u64 off, u64 len) {
    if (len == 0) return true;
    for (int i = 0; i < ngrants; i++)
        if (grants[i].disk == root && off >= grants[i].off && len <= grants[i].len && off - grants[i].off <= grants[i].len - len)
            return true;
    if (denied++ < 8) kprintf("storage: ESCRITA RECUSADA em %s (off %llu, %llu bytes): sem liberação\n",
                              storage_disk_name(storage_disk_index(root)), off, len);
    return false;
}

/* um dispositivo (disco ou fatia) "aceita escrita" se alguma faixa liberada se sobrepõe a ele */
bool blk_policy_allows(blkdev_t *d) {
    u64 base = 0;
    blkdev_t *r = d;
    while (r->parent) { base += r->base; r = r->parent; }
    if (storage_disk_index(r) < 0) return true;      /* não é disco físico (imagem/RAM): a política não se aplica */
    u64 end = base + d->size;
    for (int i = 0; i < ngrants; i++)
        if (grants[i].disk == r && base < grants[i].off + grants[i].len && grants[i].off < end) return true;
    return false;
}

/* chamado depois de vfs_init/dfs_mount_all: volumes HFS+/APFS/FAT dos discos aparecem em /Volumes */
void storage_mount_all(void) {
    for (int i = 0; i < ndisks; i++) {
        dmount_t *before = dfs_mounts();
        if (dfs_mount_disk(disks[i].dev, disks[i].name)) dfs_publish_new(before);
    }
}
