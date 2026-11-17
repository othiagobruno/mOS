/* mOS — log de boot em \EFI\mOS\boot.log da ESP (docs/instalar-no-pc.md, tools/ler-boot-log.py).
 *
 * O instalador deixa o arquivo alocado: 4 MiB, 2 slots de 2 MiB. O boot N usa o slot (N mod 2): a primeira linha é
 * "mOS boot <N> <versão>", depois o texto do console em sequência e um zero no fim; ao encher, para (sem dar a volta)
 * com "...[cortado]". N = maior N dos dois slots + 1.
 *
 * Só se reescreve o conteúdo dos clusters que o arquivo já tem: as faixas vêm da FAT (fat_extents, só leitura) e
 * entram na política de escrita como faixas de log (storage_allow_log). A escrita vai direto ao driver do disco, em
 * setores inteiros, sem o page cache: nada de FAT, diretório, tamanho ou data muda. No modo seguro do primeiro boot
 * (storage_seal) é a única escrita do boot inteiro.
 *
 * O texto fica numa cópia do slot na RAM (bootlog_putc, chamado pelo kprintf, sem E/S); bootlog_tick grava os setores
 * novos ~1 vez por segundo; bootlog_panic grava o que falta depois da tela de pânico. */
#include "kernel.h"
#include "fs.h"
#include "storage.h"
#include "update.h"

#define BL_PATH   "/EFI/mOS/boot.log"
#define BL_SLOT   (2u << 20)
#define BL_EXT    8
#define BL_TICKMS 1000

static blkdev_t *disk;                  /* disco físico */
static u64 ext_off[BL_EXT], ext_len[BL_EXT];   /* faixas do arquivo, em bytes absolutos do disco */
static int next;
static u8 *img;                         /* cópia do slot deste boot */
static volatile u32 pos;                /* bytes de texto em img */
static u32 flushed;                     /* bytes já gravados */
static u32 slot;
static bool active, busy, failed;
static u64 last_tick;

/* lê ou grava [off, off+len) do arquivo, setores inteiros, direto no driver */
static bool file_io(u64 off, u8 *buf, u64 len, bool write) {
    for (int i = 0; i < next && len; i++) {
        if (off >= ext_len[i]) { off -= ext_len[i]; continue; }
        u64 k = MIN(len, ext_len[i] - off);
        bool ok = write ? disk->write(disk, ext_off[i] + off, buf, k) : disk->read(disk, ext_off[i] + off, buf, k);
        if (!ok) return false;
        buf += k; len -= k; off = 0;
    }
    return len == 0;
}

static u64 header_n(const u8 *b, bool *ok) {
    *ok = false;
    if (memcmp(b, "mOS boot ", 9)) return 0;
    u64 n = 0;
    int i = 9;
    for (; i < 29 && b[i] >= '0' && b[i] <= '9'; i++) n = n * 10 + (u64)(b[i] - '0');
    *ok = i > 9 && (b[i] == ' ' || b[i] == '\n');
    return n;
}

#define BL_CUT "\n...[cortado]\n"
#define BL_ROOM (BL_SLOT - 1 - (sizeof BL_CUT - 1))   /* texto cabe até aqui; o resto é a marca e o zero final */

void bootlog_putc(char c) {
    if (!active || !c) return;
    u32 p = pos;
    if (p >= BL_ROOM) {                 /* slot cheio: para sem dar a volta, com a marca uma vez só */
        if (p == BL_ROOM) { memcpy(img + p, BL_CUT, sizeof BL_CUT - 1); pos = p + (u32)(sizeof BL_CUT - 1); }
        return;
    }
    img[p] = (u8)c;
    pos = p + 1;
}

static void flush(void) {
    if (!active || busy || failed) return;
    u32 end = pos;
    if (end == flushed) return;
    busy = true;
    u32 first = flushed & ~511u;
    u32 stop = (end + 511u) & ~511u;
    if (!(end & 511u) && stop < BL_SLOT) stop += 512;   /* o zero depois do texto sempre vai junto */
    if (stop > BL_SLOT) stop = BL_SLOT;
    if (file_io((u64)slot * BL_SLOT + first, img + first, stop - first, true)) {
        flushed = end;
        if (disk->flush) disk->flush(disk);
    } else {
        failed = true;                  /* não tenta de novo a cada segundo; o motivo já está no log do driver */
        kprintf("bootlog: falha ao gravar o log de boot; desligado neste boot\n");
    }
    busy = false;
}

void bootlog_tick(void) {
    if (!active) return;
    u64 now = timer_ticks();
    if (now - last_tick < BL_TICKMS) return;
    last_tick = now;
    flush();
}

/* depois da tela de pânico: grava o que falta (com interrupções desligadas; o driver espera pela fase) */
void bootlog_panic(void) { flush(); }

/* fs = volume do sistema (a ESP no PC), part_off = início dele no disco físico disk_index */
void bootlog_start(fatfs_t *fs, int disk_index, u64 part_off) {
    blkdev_t *d = storage_disk(disk_index);
    if (!fs || !d || !d->write || active) return;
    u64 size = 0, off[BL_EXT], len[BL_EXT];
    int n = fat_extents(fs, BL_PATH, off, len, BL_EXT, &size);
    if (n < 0) return;                  /* sem boot.log alocado: não é um PC instalado pelo instalador */
    if (size < 2ull * BL_SLOT) { kprintf("bootlog: %s com %llu bytes (precisa de %u): sem log\n", BL_PATH, size, 2 * BL_SLOT); return; }
    u64 total = 0;
    for (int i = 0; i < n; i++) {
        off[i] += part_off;
        if (total + len[i] > 2ull * BL_SLOT) len[i] = 2ull * BL_SLOT - total;   /* só os 2 slots */
        total += len[i];
        if ((off[i] | len[i]) & 511) { kprintf("bootlog: faixa fora de setor: sem log\n"); return; }
        ext_off[i] = off[i];
        ext_len[i] = len[i];
        if (total >= 2ull * BL_SLOT) { n = i + 1; break; }
    }
    disk = d;
    next = n;
    /* o próximo número: maior N dos dois slots + 1 */
    u8 *h = kmalloc(512);
    if (!h) return;
    u64 best = 0;
    bool any = false;
    for (u32 s = 0; s < 2; s++) {
        bool ok;
        if (!file_io((u64)s * BL_SLOT, h, 512, false)) continue;
        u64 v = header_n(h, &ok);
        if (ok && (!any || v > best)) { best = v; any = true; }
    }
    kfree(h);
    u64 bn = any ? best + 1 : 1;
    img = kmalloc_pages(BL_SLOT);
    if (!img) return;
    memset(img, 0, BL_SLOT);
    for (int i = 0; i < next; i++)
        if (!storage_allow_log(disk, ext_off[i], ext_len[i])) { kfree(img); img = NULL; return; }
    slot = (u32)(bn % 2);
    int hl = snprintf((char *)img, 128, "mOS boot %llu %s\n", bn, update_current_version());
    u32 p = hl > 0 ? (u32)hl : 0;
    extern char klog_buf[];
    extern usize klog_len;
    u64 f = irq_save();
    u32 k = (u32)MIN((u64)klog_len, (u64)(BL_ROOM - p));
    memcpy(img + p, klog_buf, k);       /* o que o kernel já escreveu desde o início do boot */
    pos = p + k;
    active = true;
    irq_restore(f);
    kprintf("bootlog: boot %llu no slot %u de %s (%d faixa(s))\n", bn, slot, BL_PATH, next);
    flush();
}
