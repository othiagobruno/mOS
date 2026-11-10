/*
 * mOS — teste no host do carregador Mach-O + libSystem.mos, sem QEMU.
 *   machotest <libSystem.mos> <raiz-do-vfs> <executável Mach-O> [args...]
 *   machotest --report <libSystem.mos> <executável>
 * O processo é montado nos mesmos endereços do mOS; cada "int 0x80" vira uma exceção
 * (VEH no Windows, seccomp+SIGSYS no Linux) que este programa trata como syscall do mOS
 * (kernel/include/syscall.h), sobre a pasta <raiz-do-vfs>.
 *
 * No Linux, zig cc ou clang:
 *   zig cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
 *       -Ikernel/include -Itools tools/machotest.c tools/host_unix.c tools/host_gpu.c tools/host_qos.c tools/host_id.c tools/host_bootstrap.c \
 *       tools/host_enter.S kernel/compat/macho.c \
 *       -o build/machotest -pthread
 * No Windows o comando continua o de docs/porte-vscode.md (zig cc -target x86_64-windows-gnu).
 */
#ifndef _WIN32
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#endif
#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#else
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <stddef.h>
#endif
#include "host_unix.h"
#include "host_qos.h"
#include "host_id.h"
#include "host_bootstrap.h"
long host_gpu_op(unsigned long long op, unsigned long long user, unsigned long long len, int *blocked);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
/* o MinGW expõe os nomes POSIX como _dup/_close/_fileno */
#ifndef fileno
#define fileno _fileno
#endif
#ifndef dup
#define dup _dup
#endif
#ifndef close
#define close _close
#endif
#ifndef fdopen
#define fdopen _fdopen
#endif
#endif
#include "compat.h"
#undef SYS_OPEN
/* O glibc define si_pid/si_addr/... como macros da siginfo_t do Linux. O mOS tem os mesmos
 * nomes como campos de mos_siginfo_t. Sem isto o syscall.h não compila no Linux. */
#ifdef __linux__
#undef si_pid
#undef si_uid
#undef si_status
#undef si_addr
#undef si_value
#undef si_band
#undef si_fd
#undef si_int
#undef si_ptr
#undef si_overrun
#undef si_timerid
#undef si_addr_lsb
#undef si_lower
#undef si_upper
#undef si_pkey
#undef si_call_addr
#undef si_syscall
#undef si_arch
#endif
#include "../kernel/include/syscall.h"
#include "../kernel/include/compat_gui.h"
#include "../userland/a64rt/a64rt_abi.h"

long long g_live_allocs, g_live_bytes;
extern const u8 *libsystem_mos, *libsystem_mos_end;
extern void host_enter(u64 entry, u64 rdi, u64 rsp);
extern void host_exit(void);
extern void host_jump(u64 entry, u64 rdi, u64 rsp);
extern u64 host_saved_rsp;

static const char *g_root;
static process_t g_proc;
static u64 brk_cur, brk_limit;
static u64 mmap_next = 0x400100000000ULL;

bool uvm_map(process_t *p, u64 va, u64 len) {
    u64 lo = va & ~0xFFFFULL, hi = (va + len + 0xFFFF) & ~0xFFFFULL;
    (void)p;
    if (hi <= lo) return true;
#ifdef _WIN32
    for (u64 a = lo; a < hi; a += 0x10000) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void *)a, &mbi, sizeof mbi) && mbi.State != MEM_FREE) {
            if (!VirtualAlloc((void *)a, 0x10000, MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return false;
            continue;
        }
        if (!VirtualAlloc((void *)a, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) {
            fprintf(stderr, "VirtualAlloc(%llx) falhou: %lu\n", a, GetLastError());
            return false;
        }
    }
    return true;
#else
    int fl = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE;
    if (mmap((void *)lo, (size_t)(hi - lo), PROT_READ | PROT_WRITE | PROT_EXEC, fl, -1, 0) != MAP_FAILED)
        return true;
    for (u64 a = lo; a < hi; a += 4096) {
        unsigned char one;
        if (mincore((void *)a, 4096, &one) == 0) continue;
        void *r = mmap((void *)a, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (r == MAP_FAILED) {
            fprintf(stderr, "mmap(%llx) falhou: %s\n", (unsigned long long)a, strerror(errno));
            return false;
        }
    }
    return true;
#endif
}

bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len) { (void)p; memcpy((void *)va, src, len); return true; }
bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len) { (void)p; memcpy(dst, (const void *)va, len); return true; }
i64 macho_dyld_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c);   /* dlopen (kernel/compat/macho.c) */

static u8 *read_file(const char *path, usize *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END);
    long long n = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
#else
    if (fseeko(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long long n = (long long)ftello(f);
    if (n < 0) { fclose(f); return NULL; }
    fseeko(f, 0, SEEK_SET);
#endif
    u8 *b = malloc(n ? (size_t)n : 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (usize)n;
    return b;
}

/* dependências (dylibs do bundle) pedidas pelo carregador: caminho do mOS sob g_root, ou caminho
 * do host no --report. Ficam na memória até o fim (o carregador guarda ponteiros para elas). */
const u8 *host_read_file(const char *path, usize *len) {
    char hp[4096];
    if (g_root) snprintf(hp, sizeof hp, "%s%s", g_root, path);
    else snprintf(hp, sizeof hp, "%s", path);
#ifdef _WIN32
    for (char *c = hp; *c; c++) if (*c == '/') *c = '\\';
#endif
    return read_file(hp, len);
}

/* ---- syscalls do mOS sobre a pasta g_root */
static void host_path(u64 upath, char *out, size_t cap) {
    const char *u = (const char *)upath;
    if (!strcmp(u, "/dev/null")) {
#ifdef _WIN32
        snprintf(out, cap, "NUL");
#else
        snprintf(out, cap, "/dev/null");
#endif
        return;
    }
    snprintf(out, cap, "%s%s", g_root ? g_root : "", u);
#ifdef _WIN32
    for (char *c = out; *c; c++) if (*c == '/') *c = '\\';
#endif
}


/* ---- GUI emulada: imprime o conteÃºdo dos quadros e reproduz um roteiro de eventos
 * (MACHOTEST_EVENTS="f:3;down:265,75;up:265,75;key:a;text:abc;close") */
static int gw, gh, frames, poll_idle, nwin, curwin = 1;
static const char *script;
static __thread u64 g_r8;
/* arrastar e soltar e IME simulados (o window server do mOS faz isto em kernel/compat/guisys_dragime.inc) */
static u8 *hd_data;
static u64 hd_len, hd_mask;
static int hd_seq, hd_src_win, hd_pending_end = -1, hd_foreign, ime_src;
/* controle de jogo, áudio e periférico Bluetooth LE simulados (GUI_DEVICE) */
static gui_gamepad_t hd_pad;
static u64 hd_audio_frames;
static int hd_audio_said, hd_bt_state = 2, hd_bt_scan, hd_bt_link, hd_periph;
static u8 hd_att[16][64];
static int hd_att_len[16], hd_att_rh, hd_att_rt;
static void hd_att_push(const u8 *p, int n) { memcpy(hd_att[hd_att_rh], p, n); hd_att_len[hd_att_rh] = n; hd_att_rh = (hd_att_rh + 1) % 16; }
static u16 hd_le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
/* servidor GATT de um sensor: Device Information (0x180A: fabricante) e Battery (0x180F: nível, com notificação) */
static long long hd_gatt_request(const u8 *q, int n) {
    static const struct { u16 h, type, uuid; u8 props; u16 vh; } at[] = {
        {1, 0x2800, 0x180A, 0, 0}, {2, 0x2803, 0x2A29, 0x02, 3}, {3, 0x2A29, 0, 0, 0},
        {4, 0x2800, 0x180F, 0, 0}, {5, 0x2803, 0x2A19, 0x12, 6}, {6, 0x2A19, 0, 0, 0}, {7, 0x2902, 0, 0, 0},
    };
    u8 r[64];
    int rl = 0;
    printf("[bt] ATT pedido 0x%02x (%d bytes)\n", q[0], n);
    switch (q[0]) {
    case 0x02: r[0] = 0x03; r[1] = 23; r[2] = 0; rl = 3; break;
    case 0x10: {
        u16 s = hd_le16(q + 1);
        r[0] = 0x11; r[1] = 6; rl = 2;
        for (int i = 0; i < 7; i++) if (at[i].type == 0x2800 && at[i].h >= s && rl + 6 <= 23) {
            u16 end = at[i].h == 1 ? 3 : 7;
            r[rl] = (u8)at[i].h; r[rl + 1] = 0; r[rl + 2] = (u8)end; r[rl + 3] = 0; r[rl + 4] = (u8)at[i].uuid; r[rl + 5] = (u8)(at[i].uuid >> 8); rl += 6;
        }
        if (rl == 2) { r[0] = 0x01; r[1] = 0x10; r[2] = q[1]; r[3] = q[2]; r[4] = 0x0A; rl = 5; }
        break;
    }
    case 0x08: {
        u16 s = hd_le16(q + 1), e = hd_le16(q + 3);
        r[0] = 0x09; r[1] = 7; rl = 2;
        for (int i = 0; i < 7; i++) if (at[i].type == 0x2803 && at[i].h >= s && at[i].h <= e && rl + 7 <= 23) {
            r[rl] = (u8)at[i].h; r[rl + 1] = 0; r[rl + 2] = at[i].props; r[rl + 3] = (u8)at[i].vh; r[rl + 4] = 0; r[rl + 5] = (u8)at[i].uuid; r[rl + 6] = (u8)(at[i].uuid >> 8); rl += 7;
        }
        if (rl == 2) { r[0] = 0x01; r[1] = 0x08; r[2] = q[1]; r[3] = q[2]; r[4] = 0x0A; rl = 5; }
        break;
    }
    case 0x04: {
        u16 s = hd_le16(q + 1), e = hd_le16(q + 3);
        r[0] = 0x05; r[1] = 1; rl = 2;
        for (int i = 0; i < 7; i++) if (at[i].h >= s && at[i].h <= e && rl + 4 <= 23) { r[rl] = (u8)at[i].h; r[rl + 1] = 0; r[rl + 2] = (u8)at[i].type; r[rl + 3] = (u8)(at[i].type >> 8); rl += 4; }
        if (rl == 2) { r[0] = 0x01; r[1] = 0x04; r[2] = q[1]; r[3] = q[2]; r[4] = 0x0A; rl = 5; }
        break;
    }
    case 0x0A: {
        u16 h = hd_le16(q + 1);
        r[0] = 0x0B; rl = 1;
        if (h == 3) { memcpy(r + 1, "mOS Labs", 8); rl = 9; }
        else if (h == 6) { r[1] = 87; rl = 2; }
        else if (h == 7) { r[1] = 0; r[2] = 0; rl = 3; }
        else { r[0] = 0x01; r[1] = 0x0A; r[2] = q[1]; r[3] = q[2]; r[4] = 0x02; rl = 5; }
        break;
    }
    case 0x12: {
        u16 h = hd_le16(q + 1);
        r[0] = 0x13; rl = 1;
        if (h == 7 && n >= 5 && (q[3] & 1)) { u8 nt[4] = {0x1B, 6, 0, 88}; hd_att_push(r, rl); hd_att_push(nt, 4); return 0; }
        break;
    }
    case 0x52: return 0;
    default: r[0] = 0x01; r[1] = q[0]; r[2] = 0; r[3] = 0; r[4] = 0x06; rl = 5;
    }
    hd_att_push(r, rl);
    return 0;
}
/* MACHOTEST_DUMP=<arquivo.bmp>: compõe os GC_CLEAR/GC_IMAGE dos quadros numa tela do tamanho da
 * janela e grava o último quadro apresentado como BMP (para conferir o conteúdo dos layers) */
static unsigned *dump_cv;
static int dump_w, dump_h;
static void dump_op(unsigned op, const unsigned char *q) {
    if (!getenv("MACHOTEST_DUMP") || gw <= 0 || gh <= 0) return;
    if (!dump_cv || dump_w != gw || dump_h != gh) { free(dump_cv); dump_cv = calloc((size_t)gw * gh, 4); dump_w = gw; dump_h = gh; }
    if (!dump_cv) return;
    if (op == GC_CLEAR) { unsigned c = *(const unsigned *)q; for (int i = 0; i < gw * gh; i++) dump_cv[i] = c; return; }
    if (op != GC_IMAGE) return;
    int x = ((const int *)q)[0], y = ((const int *)q)[1], w = ((const int *)q)[2], h = ((const int *)q)[3];
    const unsigned *px = (const unsigned *)(q + 16);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int X = x + i, Y = y + j;
            if (X >= 0 && Y >= 0 && X < gw && Y < gh) dump_cv[Y * gw + X] = px[j * w + i];
        }
}
static void dump_write(void) {
    const char *path = getenv("MACHOTEST_DUMP");
    if (!path || !dump_cv) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int row = (dump_w * 3 + 3) & ~3, size = 54 + row * dump_h;
    unsigned char h[54] = {'B', 'M'};
    *(int *)(h + 2) = size; *(int *)(h + 10) = 54; *(int *)(h + 14) = 40; *(int *)(h + 18) = dump_w; *(int *)(h + 22) = -dump_h;
    *(short *)(h + 26) = 1; *(short *)(h + 28) = 24;
    fwrite(h, 1, 54, f);
    unsigned char *r = calloc(1, row);
    for (int y = 0; y < dump_h; y++) {
        for (int x = 0; x < dump_w; x++) { unsigned c = dump_cv[y * dump_w + x]; r[x * 3] = c & 255; r[x * 3 + 1] = (c >> 8) & 255; r[x * 3 + 2] = (c >> 16) & 255; }
        fwrite(r, 1, row, f);
    }
    free(r);
    fclose(f);
}
static long long gui_syscall(u64 n, u64 a, u64 b, u64 c, u64 d) {
    switch (n) {
    case GUI_WIN_CREATE: gw = (int)b; gh = (int)c; nwin++; printf("[gui] janela %d \"%s\" %dx%d\n", nwin, (const char *)a, gw, gh); return nwin;
    case GUI_WIN_TITLE: printf("[gui] tÃ­tulo: %s\n", (const char *)b); return 0;
    case GUI_WIN_DESTROY: printf("[gui] janela fechada pelo app\n"); return 0;
    case GUI_DRAW: {
        const u8 *p = (const u8 *)b;
        u64 o = 0;
        int counts[16] = {0};
        char texts[2048] = "";
        while (o + 8 <= c) {
            u32 op = *(const u32 *)(p + o), len = *(const u32 *)(p + o + 4);
            if (len < 8) break;
            if (op < 16) counts[op]++;
            dump_op(op, p + o + 8);
            if (op == GC_RRECT && getenv("MACHOTEST_FRAMES") && getenv("MACHOTEST_FRAMES")[0] == '3' && strlen(texts) < 1800) {
                const float *r = (const float *)(p + o + 8);
                if (r[2] > 39.5f && r[2] < 40.5f) { char t[32]; snprintf(t, sizeof t, "h%d ", (int)(r[3] + 0.5f)); strcat(texts, t); }
            }
            if (op == GC_TEXT && strlen(texts) < 1800) {
                u32 tn = *(const u32 *)(p + o + 8 + 20);
                char t[256];
                const char *fm = getenv("MACHOTEST_FRAMES");
                if (fm && fm[0] == '2') snprintf(t, sizeof t, "%.*s@%d,%d", (int)(tn < 200 ? tn : 200), (const char *)(p + o + 8 + 24), (int)*(const float *)(p + o + 8), (int)*(const float *)(p + o + 12));
                else snprintf(t, sizeof t, "%.*s", (int)(tn < 200 ? tn : 200), (const char *)(p + o + 8 + 24));
                strcat(texts, "[");
                strcat(texts, t);
                strcat(texts, "] ");
            }
            o += len;
        }
        if (getenv("MACHOTEST_FRAMES") || frames < 2)
            printf("[gui] quadro %d: %d rrect, %d poly, %d linha, %d img, %d texto: %s\n", frames, counts[GC_RRECT], counts[GC_POLY], counts[GC_LINE], counts[GC_IMAGE], counts[GC_TEXT], texts);
        return 0;
    }
    case GUI_PRESENT: frames++; dump_write(); return 0;
    case GUI_POLL: {
        gui_event_t *e = (gui_event_t *)a;
        if (hd_pending_end >= 0) {                 /* o destino terminou de soltar: a fonte fica sabendo */
            memset(e, 0, sizeof *e);
            e->type = GEV_DRAG_END; e->win = (unsigned)hd_src_win; e->key = hd_pending_end; e->clicks = hd_seq;
            hd_pending_end = -1;
            printf("[gui] evento: fim do arrasto\n");
            return 1;
        }
        if (!script || !*script) {
            if (++poll_idle > 2000) { memset(e, 0, sizeof *e); e->type = GEV_CLOSE; e->win = 1; script = NULL; printf("[gui] (fim do roteiro: fechando)\n"); return 1; }
            return 0;
        }
        char tok[64];
        int k = 0;
        while (*script && *script != ';' && k < 63) tok[k++] = *script++;
        tok[k] = 0;
        if (*script == ';') script++;
        memset(e, 0, sizeof *e);
        e->win = (unsigned)curwin;
        int x = 0, y = 0;
        if (!strncmp(tok, "win:", 4)) { curwin = atoi(tok + 4); return 0; }
        if (!strncmp(tok, "f:", 2)) { int nwait = atoi(tok + 2); static int waited; if (waited < nwait) { waited++; script -= strlen(tok) + 1; } else waited = 0; return 0; }
        if (sscanf(tok, "down:%d,%d", &x, &y) == 2) { e->type = GEV_MOUSE_DOWN; e->x = x; e->y = y; e->clicks = 1; e->buttons = 1; }
        else if (sscanf(tok, "drag:%d,%d", &x, &y) == 2) { e->type = GEV_MOUSE_MOVE; e->x = x; e->y = y; e->buttons = 1; }
        else if (sscanf(tok, "up:%d,%d", &x, &y) == 2) { e->type = GEV_MOUSE_UP; e->x = x; e->y = y; }
        else if (!strncmp(tok, "key:", 4)) {
            const char *kn = tok + 4;
            e->type = GEV_KEY_DOWN;
            e->key = !strcmp(kn, "ret") ? 10 : !strcmp(kn, "bs") ? 8 : !strcmp(kn, "esc") ? 27 : !strcmp(kn, "spc") ? ' ' :
                     !strcmp(kn, "up") ? 0x100 : !strcmp(kn, "down") ? 0x101 : kn[0];
            if (e->key >= 32 && e->key < 127) e->text[0] = (char)e->key;
            if (e->key >= 'A' && e->key <= 'Z') e->mods = 1;   /* maiúscula = Shift */
        }
        else if (!strncmp(tok, "src:", 4)) { e->type = GEV_INPUT_SOURCE; e->key = ime_src = atoi(tok + 4); }
        else if (!strcmp(tok, "dgforeign")) { hd_foreign = !hd_foreign; return 0; }
        else if (!strncmp(tok, "pad:", 4)) {
            /* controle simulado no slot 0: pad:on, pad:off, pad:btn:<hex PAD_*>, pad:axes:lx,ly,rx,ry,lt,rt (×100) */
            const char *pv = tok + 4;
            if (!strcmp(pv, "on")) { memset(&hd_pad, 0, sizeof hd_pad); hd_pad.connected = 1; strcpy(hd_pad.name, "Controle Xbox (simulado)"); hd_pad.kind = PAD_KIND_XBOX360; hd_pad.caps = PAD_CAP_RUMBLE; hd_pad.vid = 0x045E; hd_pad.pid = 0x028E; }
            else if (!strcmp(pv, "off")) hd_pad.connected = 0;
            else if (!strncmp(pv, "btn:", 4)) hd_pad.buttons = (unsigned)strtoul(pv + 4, 0, 16);
            else if (!strncmp(pv, "axes:", 5)) { int v[6] = {0}; sscanf(pv + 5, "%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]); hd_pad.lx = v[0] / 100.f; hd_pad.ly = v[1] / 100.f; hd_pad.rx = v[2] / 100.f; hd_pad.ry = v[3] / 100.f; hd_pad.lt = v[4] / 100.f; hd_pad.rt = v[5] / 100.f; }
            hd_pad.seq++;
            printf("[gui] evento: %s\n", tok);
            return 0;
        }
        else if (!strcmp(tok, "btcentral")) {
            /* um central conecta no anúncio do app e faz a descoberta GATT de praxe, lê 2A19 e assina as notificações */
            static const u8 q[][8] = {{3, 0x02, 0x17, 0x00}, {7, 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28}, {7, 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x28},
                                      {3, 0x0A, 0x03, 0x00}, {5, 0x04, 0x04, 0x00, 0xff, 0xff}, {5, 0x12, 0x04, 0x00, 0x01, 0x00}};
            hd_periph = 1; hd_bt_link = 2;
            for (int i = 0; i < 6; i++) hd_att_push(q[i] + 1, q[i][0]);
            printf("[gui] evento: %s\n", tok);
            return 0;
        }
        else if (!strncmp(tok, "keyup:", 6)) { e->type = GEV_KEY_UP; e->key = tok[6]; e->text[0] = tok[6]; }
        else if (sscanf(tok, "dgenter:%d,%d", &x, &y) == 2 || sscanf(tok, "dgmove:%d,%d", &x, &y) == 2 || sscanf(tok, "dgdrop:%d,%d", &x, &y) == 2 || !strcmp(tok, "dgexit")) {
            /* "dgforeign": a sessão parece vir de outro processo (o destino lê o conteúdo por DG_DATA) */
            e->type = tok[2] == 'e' && tok[3] == 'n' ? GEV_DRAG_ENTER : tok[2] == 'm' ? GEV_DRAG_MOVE : tok[2] == 'd' ? GEV_DRAG_DROP : GEV_DRAG_EXIT;
            e->x = x; e->y = y; e->w = x; e->h = y;
            e->key = (int)hd_mask;
            e->clicks = hd_seq + (hd_foreign ? 1000 : 0);
        }
        else if (!strncmp(tok, "resize:", 7)) { e->type = GEV_RESIZE; sscanf(tok + 7, "%d,%d", &e->w, &e->h); }
        else if (!strcmp(tok, "close")) e->type = GEV_CLOSE;
        else if (!strncmp(tok, "menu:", 5)) { e->type = GEV_MENU; e->key = atoi(tok + 5); }
        else if (sscanf(tok, "dbl:%d,%d", &x, &y) == 2) { e->type = GEV_MOUSE_DOWN; e->x = x; e->y = y; e->clicks = 2; e->buttons = 1; }
        else if (sscanf(tok, "scroll:%d,%d,%d", &x, &y, &k) == 3) { e->type = GEV_SCROLL; e->x = x; e->y = y; e->w = k; }
        else if (!strncmp(tok, "cmd:", 4)) { e->type = GEV_KEY_DOWN; e->key = tok[4]; e->text[0] = tok[4]; e->mods = 8; }
        else return 0;
        printf("[gui] evento: %s\n", tok);
        return 1;
    }
    case GUI_MEASURE: {
        gui_metrics_t *m = (gui_metrics_t *)c;
        double size = (double)(d & 0xFFFF) / 100.0;
        m->width = (int)(b * size * 0.55);
        m->ascent = (int)(size * 0.8 + 0.5);
        m->descent = (int)(size * 0.25 + 0.5);
        return 0;
    }
    case GUI_WIN_INFO: { gui_wininfo_t *i = (gui_wininfo_t *)b; i->w = gw; i->h = gh; i->focused = 1; i->visible = 1; return 0; }
    case GUI_SCREEN: { gui_wininfo_t *i = (gui_wininfo_t *)a; i->w = 1440; i->h = 876; i->focused = 1; i->visible = 1; return 0; }
    case GUI_WIN_FOCUS: return 0;
    case GUI_SET_MENU: {
        const u8 *p = (const u8 *)a, *end = p + b;
        u32 nm = *(const u32 *)p; p += 4;
        printf("[gui] menus (%u):", nm);
        for (u32 i = 0; i < nm && p < end; i++) {
            u32 l = *(const u32 *)p; p += 4;
            printf(" %.*s{", (int)l, (const char *)p); p += l;
            u32 ni = *(const u32 *)p; p += 4;
            for (u32 j = 0; j < ni && p < end; j++) {
                u32 act = *(const u32 *)p, fl = *(const u32 *)(p + 4), ln = *(const u32 *)(p + 8); p += 12;
                const char *lab = (const char *)p; p += ln;
                u32 kl = *(const u32 *)p; p += 4;
                if (!act) printf("---;"); else printf("%.*s#%u%s%s%.*s;", (int)ln, lab, act, fl & 1 ? "(off)" : "", kl ? " ^" : "", (int)kl, (const char *)p);
                p += kl;
            }
            printf("}");
        }
        printf("\n");
        return 0;
    }
    case GUI_DEVICE: {
        switch (a) {
        case DEV_PAD_GET:
            if (b >= 4) return 0;
            if (c) { gui_gamepad_t z; memset(&z, 0, sizeof z); memcpy((void *)c, b == 0 ? &hd_pad : &z, sizeof z); }
            return 1;
        case DEV_PAD_RUMBLE: printf("[pad] vibração: controle %llu, forte %llu, fraco %llu, %llu ms\n", (unsigned long long)b, (unsigned long long)(c & 255), (unsigned long long)((c >> 8) & 255), (unsigned long long)d); return 0;
        case DEV_AUDIO_PLAY: hd_audio_frames += c; if (!hd_audio_said++) printf("[audio] o app tocou PCM (%llu quadros no 1º bloco)\n", (unsigned long long)c); return (long long)c;
        case DEV_BT_STATE: return hd_bt_state;
        case DEV_BT_SCAN: printf("[bt] varredura %s\n", b ? "ligada" : "desligada"); hd_bt_scan = (int)b; return 0;
        case DEV_BT_DEVICE:
            if (!hd_bt_scan || b >= 1) return 0;
            { gui_bt_device_t *o = (gui_bt_device_t *)c; memset(o, 0, sizeof *o); strcpy(o->name, "Sensor mOS"); strcpy(o->addr, "C0:FF:EE:00:00:01"); o->rssi = -52; o->connectable = 1; }
            return 1;
        case DEV_BT_CONNECT: printf("[bt] conectar ao dispositivo %llu\n", (unsigned long long)b); hd_bt_link = 2; return 0;
        case DEV_BT_DISCONNECT: printf("[bt] desconectar\n"); hd_bt_link = 0; return 0;
        case DEV_BT_LINK: { gui_bt_link_t *o = (gui_bt_link_t *)b; memset(o, 0, sizeof *o); o->state = hd_bt_link; o->mtu = 23; o->role = hd_periph; strcpy(o->addr, "C0:FF:EE:00:00:01"); return 0; }
        case DEV_BT_ATT_SEND:
            if (hd_periph) { printf("[bt] o app (periférico) respondeu:"); for (u64 i = 0; i < c; i++) printf(" %02x", ((const u8 *)b)[i]); printf("\n"); return 0; }
            return hd_gatt_request((const u8 *)b, (int)c);
        case DEV_BT_ADVERTISE: printf("[bt] anúncio %s (%llu bytes)\n", d ? "ligado" : "desligado", (unsigned long long)c); return 0;
        case DEV_BT_ATT_RECV:
            if (hd_att_rt == hd_att_rh) return hd_bt_link ? 0 : -1;
            { int n = hd_att_len[hd_att_rt]; memcpy((void *)b, hd_att[hd_att_rt], n); hd_att_rt = (hd_att_rt + 1) % 16; return n; }
        }
        return -MOS_EINVAL;
    }
    case GUI_DRAG: {
        u64 e5 = g_r8;
        switch (a) {
        case DG_BEGIN:
            free(hd_data);
            hd_data = malloc(d ? d : 1); memcpy(hd_data, (const void *)c, d); hd_len = d;
            hd_mask = e5; hd_src_win = (int)b; hd_seq++;
            printf("[drag] começou: janela %d, %llu bytes, máscara %llu\n", (int)b, (unsigned long long)d, (unsigned long long)e5);
            { u32 nt; memcpy(&nt, hd_data, 4); u64 o = 4;
              for (u32 i = 0; i < nt && o + 4 <= hd_len; i++) {
                  u32 tl, kd, dl; memcpy(&tl, hd_data + o, 4); o += 4;
                  printf("[drag]   tipo %.*s", (int)tl, (const char *)hd_data + o); o += tl;
                  memcpy(&kd, hd_data + o, 4); memcpy(&dl, hd_data + o + 4, 4); o += 8;
                  printf(" (%c, %u bytes)%s%.*s\n", (char)kd, dl, kd == 'S' ? ": " : "", kd == 'S' ? (int)dl : 0, (const char *)hd_data + o); o += dl;
              } }
            return hd_seq;
        case DG_DATA:
            if (!hd_data) return -MOS_ENOENT;
            if (b && c) memcpy((void *)b, hd_data, c < hd_len ? c : hd_len);
            return (long long)hd_len;
        case DG_STATUS: printf("[drag] destino aceita operação %llu\n", (unsigned long long)c); return 0;
        case DG_FINISH: printf("[drag] destino terminou: operação %llu\n", (unsigned long long)c); hd_pending_end = (int)c; return 0;
        case DG_CANCEL: printf("[drag] cancelado\n"); return 0;
        }
        return -MOS_EINVAL;
    }
    case GUI_IME: {
        u64 e5 = g_r8;
        switch (a) {
        case IME_SHOW: {
            const char *p = (const char *)c;
            u32 sel, cnt, fl;
            memcpy(&sel, p, 4); memcpy(&cnt, p + 4, 4); memcpy(&fl, p + 8, 4);
            p += 12;
            printf("[ime] candidatos em (%d,%d) sel=%u%s comp=\"%s\":", (int)(u32)e5, (int)(u32)(e5 >> 32), sel, (fl & 1) ? " (com composição)" : "", p);
            p += strlen(p) + 1;
            for (u32 i = 0; i < cnt; i++) { printf(" %u.%s", i + 1, p); p += strlen(p) + 1; }
            printf("\n");
            return 0;
        }
        case IME_HIDE: printf("[ime] candidatos escondidos\n"); return 0;
        case IME_SOURCE_GET: return ime_src;
        case IME_SOURCE_SET: ime_src = (int)b; printf("[ime] fonte de entrada %d\n", ime_src); return 0;
        }
        return -MOS_EINVAL;
    }
    case GUI_DECODE_IMAGE: {   /* o harness nÃ£o decodifica: devolve 4x4 sÃ³lido se os dados parecerem PNG */
        const u8 *in = (const u8 *)a;
        if (b < 8 || in[1] != 'P' || in[2] != 'N' || in[3] != 'G') return 0;
        u64 need = 8 + 16 * 4;
        if (d < need) return (long long)need;
        u32 *o = (u32 *)c; o[0] = 4; o[1] = 4;
        for (int i = 0; i < 16; i++) o[2 + i] = 0xFF3080E0u;
        printf("[gui] imagem decodificada (%llu bytes)\n", (unsigned long long)b);
        return (long long)need;
    }
    }
    return -MOS_EINVAL;
}

/* ---- threads, pipes, futex e portas Mach (o mesmo no Windows e no Linux) */
#ifdef _WIN32
#define HOST_LOCK() EnterCriticalSection(&sys_lock)
#define HOST_UNLOCK() LeaveCriticalSection(&sys_lock)
static CRITICAL_SECTION sys_lock;
static CONDITION_VARIABLE sys_cv = CONDITION_VARIABLE_INIT;
static void host_broadcast(void) { WakeAllConditionVariable(&sys_cv); }
static void host_lock_fn(void) { HOST_LOCK(); }
static void host_unlock_fn(void) { HOST_UNLOCK(); }
/* devolve 1 se o prazo venceu. O chamador já entrou em sys_lock exatamente uma vez. */
static int host_wait_ms(u64 ms) {
    if (ms == ~(u64)0) { SleepConditionVariableCS(&sys_cv, &sys_lock, INFINITE); return 0; }
    return SleepConditionVariableCS(&sys_cv, &sys_lock, (DWORD)ms) ? 0 : 1;
}
#else
#define HOST_LOCK() pthread_mutex_lock(&sys_lock)
#define HOST_UNLOCK() pthread_mutex_unlock(&sys_lock)
static pthread_mutex_t sys_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sys_cv;
static int sys_cv_ready;
static void host_cv_init(void) {
    if (sys_cv_ready) return;
    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
    pthread_cond_init(&sys_cv, &ca);
    pthread_condattr_destroy(&ca);
    sys_cv_ready = 1;
}
static void host_broadcast(void) { if (sys_cv_ready) pthread_cond_broadcast(&sys_cv); }
static void host_lock_fn(void) { HOST_LOCK(); }
static void host_unlock_fn(void) { HOST_UNLOCK(); }
static int host_wait_ms(u64 ms) {
    if (ms == ~(u64)0) { pthread_cond_wait(&sys_cv, &sys_lock); return 0; }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += (time_t)(ms / 1000);
    ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    return pthread_cond_timedwait(&sys_cv, &sys_lock, &ts) == ETIMEDOUT;
}
#endif

extern void host_jump(u64 entry, u64 rdi, u64 rsp);
static __thread long long my_tid = 1;
static __thread u64 my_gs;
static volatile long next_tid = 1;
static volatile u64 g_last_sys[3];
static volatile u64 g_guest_rip;
#ifdef _WIN32
static HANDLE thread_handles[256];
#else
static pthread_t thread_handles[256];
static __thread int in_sig;
#endif
typedef struct { u64 entry, rsp, arg; long long tid; } thread_start_t;

#ifdef _WIN32
static DWORD WINAPI thread_main(LPVOID p) {
    thread_start_t s = *(thread_start_t *)p;
    free(p);
    my_tid = s.tid;
    host_jump(s.entry, s.arg, s.rsp);
    return 0;
}
#else
static void host_linux_altstack(void) {
    stack_t ss;
    ss.ss_sp = malloc(1 << 20);
    ss.ss_size = 1 << 20;
    ss.ss_flags = 0;
    if (ss.ss_sp) sigaltstack(&ss, NULL);
}
static void *thread_main(void *p) {
    thread_start_t s = *(thread_start_t *)p;
    free(p);
    my_tid = s.tid;
    /* O kernel zera gs_base na criação (process.c). O clone do Linux copia o GS do pai:
     * sem isto a thread nova reusa a TSD/TLV da principal e um CHECK de "thread ainda
     * não registrada" (cmp [rsi], 0; int3; ud2) dispara no Electron. */
    __asm__ volatile("wrgsbase %0" :: "r"((unsigned long)0) : "memory");
    /* A thread nasce dentro do tratador de SIGSYS (a syscall SYS_THREAD_CREATE).
     * O pthread herda essa máscara: com SIGSYS bloqueado, o próximo int 0x80 do guest
     * mata o processo em vez de voltar para on_guest_signal. */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGSYS);
    sigaddset(&set, SIGSEGV);
    sigaddset(&set, SIGBUS);
    sigaddset(&set, SIGILL);
    sigaddset(&set, SIGFPE);
    sigaddset(&set, SIGTRAP);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    host_linux_altstack();
    host_jump(s.entry, s.arg, s.rsp);
    return NULL;
}
#endif

/* ---- descritores: arquivo do host ou pipe/socketpair (o mesmo modelo do kernel) */
#define MAXFD 256
enum { HFD_NONE = 0, HFD_FILE, HFD_PR, HFD_PW, HFD_RW, HFD_UNIX };
typedef struct hpipe {
    u8 buf[4096];
    unsigned head, tail, count;
    int readers, writers, busy;
} hpipe;
typedef struct {
    int kind;
    FILE *fp;
    char mode[4];
    char gpath[1024];   /* caminho do guest (SYS_FDPATH / fcntl F_GETPATH) */
    hpipe *a, *b;
    int nb, cloexec, acc, aux;
} hfd_t;
static hfd_t hfd[MAXFD];

static void pipe_maybe_free(hpipe *p) {
    if (p && p->readers <= 0 && p->writers <= 0 && p->busy == 0) free(p);
}
static void pipe_end(hpipe *p, int reader) {
    if (!p) return;
    if (reader) p->readers--; else p->writers--;
    host_broadcast();
    pipe_maybe_free(p);
}
static void pipe_addref(const hfd_t *e) {
    if (e->kind == HFD_PR && e->a) e->a->readers++;
    else if (e->kind == HFD_PW && e->a) e->a->writers++;
    else if (e->kind == HFD_RW) { if (e->a) e->a->readers++; if (e->b) e->b->writers++; }
}
static void pipe_drop(hfd_t *e) {
    if (e->kind == HFD_PR) pipe_end(e->a, 1);
    else if (e->kind == HFD_PW) pipe_end(e->a, 0);
    else if (e->kind == HFD_RW) { pipe_end(e->a, 1); pipe_end(e->b, 0); }
    e->a = e->b = NULL;
}
static void fd_clear(int fd) {
    hfd_t *e = &hfd[fd];
    if (e->kind == HFD_FILE && e->fp) fclose(e->fp);
    else if (e->kind == HFD_UNIX) host_unix_close(fd);
    else if (e->kind) pipe_drop(e);
    memset(e, 0, sizeof *e);
}
static int fd_alloc(void) {
    for (int i = 3; i < MAXFD; i++) if (!hfd[i].kind) return i;
    return -1;
}
static long pipe_io(int rd, int fd, u64 buf, u64 len) {
    hfd_t *e = &hfd[fd];
    if (rd && e->kind == HFD_PW) return -MOS_EBADF;
    if (!rd && e->kind == HFD_PR) return -MOS_EBADF;
    hpipe *p = rd ? e->a : (e->kind == HFD_RW ? e->b : e->a);
    if (!p) return -MOS_EBADF;
    if (!len) return 0;
    p->busy++;
    long ret = 0;
    for (;;) {
        if (rd) {
            if (p->count) {
                u64 n = len < p->count ? len : p->count;
                u8 *dst = (u8 *)buf;
                for (u64 i = 0; i < n; i++) { dst[i] = p->buf[p->tail]; p->tail = (p->tail + 1) % 4096; }
                p->count -= (unsigned)n;
                host_broadcast();
                ret = (long)n;
                break;
            }
            if (!p->writers) { ret = 0; break; }
        } else {
            if (!p->readers) { ret = -MOS_EPIPE; break; }
            if (p->count < 4096) {
                u64 space = 4096 - p->count;
                u64 n = len < space ? len : space;
                const u8 *src = (const u8 *)buf;
                for (u64 i = 0; i < n; i++) { p->buf[p->head] = src[i]; p->head = (p->head + 1) % 4096; }
                p->count += (unsigned)n;
                host_broadcast();
                ret = (long)n;
                break;
            }
        }
        if (e->nb) { ret = -MOS_EAGAIN; break; }
        host_wait_ms(~(u64)0);
    }
    p->busy--;
    pipe_maybe_free(p);
    return ret;
}
static long pipe_make(u64 user, int sock) {
    int rfd = fd_alloc();
    int wfd = -1;
    if (rfd >= 0) {
        hfd[rfd].kind = HFD_FILE; /* reserva o número */
        wfd = fd_alloc();
        hfd[rfd].kind = HFD_NONE;
    }
    if (rfd < 0 || wfd < 0) return -MOS_EMFILE;
    hpipe *p1 = calloc(1, sizeof *p1);
    if (!p1) return -MOS_ENOMEM;
    p1->readers = p1->writers = 1;
    int both[2] = {rfd, wfd};
    memcpy((void *)user, both, sizeof both);
    if (!sock) {
        hfd[rfd].kind = HFD_PR; hfd[rfd].a = p1; hfd[rfd].acc = 0;
        hfd[wfd].kind = HFD_PW; hfd[wfd].a = p1; hfd[wfd].acc = 1;
        return 0;
    }
    hpipe *p2 = calloc(1, sizeof *p2);
    if (!p2) { free(p1); return -MOS_ENOMEM; }
    p2->readers = p2->writers = 1;
    hfd[rfd].kind = HFD_RW; hfd[rfd].a = p1; hfd[rfd].b = p2; hfd[rfd].acc = 2;
    hfd[wfd].kind = HFD_RW; hfd[wfd].a = p2; hfd[wfd].b = p1; hfd[wfd].acc = 2;
    return 0;
}
static void pipe_stat(int fd, mos_stat_t *m) {
    hfd_t *e = &hfd[fd];
    hpipe *p = e->a;
    int rd = e->kind == HFD_PR || e->kind == HFD_RW;
    memset(m, 0, sizeof *m);
    m->is_dir = rd ? 2 : 3;
    m->size = rd ? p->count : 4096 - p->count;
    m->mtime = rd ? (p->writers == 0) : (p->readers == 0);
}

/* ---- futex (SYS_FUTEX_WAIT/WAKE): a comparação e a entrada na fila são atômicas com o WAKE */
typedef struct fwait { u64 addr; int woken; struct fwait *next; } fwait;
static fwait *fwaits;
static long futex_wait(u64 addr, u32 expected, u64 ms) {
    if ((addr & 3) || addr < USER_BASE || addr >= 0x0000800000000000ULL) return -MOS_EFAULT;
    u32 cur = *(volatile u32 *)addr;
    if (cur != expected) return -MOS_EAGAIN;
    if (!ms) return -MOS_ETIMEDOUT;
    fwait w;
    w.addr = addr; w.woken = 0; w.next = fwaits; fwaits = &w;
    int timed = 0;
    while (!w.woken && !timed) timed = host_wait_ms(ms);
    fwait **pp = &fwaits;
    while (*pp) { if (*pp == &w) { *pp = w.next; break; } pp = &(*pp)->next; }
    return w.woken ? 0 : -MOS_ETIMEDOUT;
}
static long futex_wake(u64 addr, u64 n) {
    if (!n) n = 1;
    long c = 0;
    for (fwait *w = fwaits; w && c < (long)n; w = w->next)
        if (w->addr == addr && !w->woken) { w->woken = 1; c++; }
    if (c) host_broadcast();
    return c;
}

/* ---- portas Mach no processo único do harness (mach_port_construct, mach_msg, socketpair do Electron) */
#define HT_SEND    0x10000u
#define HT_RECV    0x20000u
#define HT_SO      0x40000u
#define HT_DEAD    0x100000u
#define HPORT_N    1024
#define HMSG_N     32
#define KERN_OK 0
#define KERN_NO_SPACE_ 3
#define KERN_INVAL_ARG 4
#define KERN_SHORT 6
#define KERN_EXISTS 13
#define KERN_INVAL_NAME 15
#define KERN_INVAL_RIGHT 17
typedef struct {
    int used;
    u32 name, type, srefs, qlimit, seqno;
    u64 context;
    u8 *qdata[HMSG_N];
    u32 qsize[HMSG_N];
    int qh, qn;
    int memfd;
    u64 memsz, memoff;      /* entrada sobre parte de outro memfd (entrada feita de um endereço mapeado) */
    u32 memprot;            /* VM_PROT_* da entrada (perm & 7): só-leitura recusa mapa gravável */
} hport_t;
/* mapeamentos de entradas (MACH_VM_MAP): MACH_VM_MAKE_ENTRY de um endereço já mapeado reusa o mesmo memfd,
 * como o kernel reusa as mesmas páginas (o ReadOnlySharedMemoryRegion do Chromium escreve na entrada
 * gravável e manda a entrada só-leitura feita do endereço dela) */
#define HMAP_N 4096
static struct { u64 at, sz, off; int memfd; } hmaps[HMAP_N];
static int hmap_n;
static hport_t hports[HPORT_N];
static u32 hname_next = 0x11000;
static int said_construct;

static int hport_special(u32 name) {
    return name == 0x103u || name == 0x203u || name == 0x303u || (name >= 0x1000u && name < 0x10000u);
}
static hport_t *hport_get(u32 name) {
    if (!name) return NULL;
    for (int i = 0; i < HPORT_N; i++) if (hports[i].used && hports[i].name == name) return &hports[i];
    return NULL;
}
static hport_t *hport_new(u32 name, u32 type) {
    if (name && hport_get(name)) return NULL;
    for (int i = 0; i < HPORT_N; i++) if (!hports[i].used) {
        memset(&hports[i], 0, sizeof hports[i]);
        hports[i].used = 1;
        hports[i].name = name ? name : hname_next++;
        hports[i].type = type;
        hports[i].qlimit = 5;
        hports[i].memfd = -1;
        return &hports[i];
    }
    return NULL;
}
static unsigned bs_new_recv(void) {
    hport_t *p = hport_new(0, HT_RECV);
    return p ? p->name : 0;
}
static void hport_free_slot(hport_t *p) {
    for (int i = 0; i < p->qn; i++) free(p->qdata[(p->qh + i) % HMSG_N]);
#ifndef _WIN32
    if (p->memfd >= 0) close(p->memfd);
#endif
    memset(p, 0, sizeof *p);
    p->memfd = -1;
}
static u32 rd32u(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static void wr32u(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

static long mach_port_op(u64 op, u64 ap) {
    u64 *a = (u64 *)ap;
    if (!a) return KERN_INVAL_ARG;
    switch (op) {
    case MACH_OP_ALLOCATE: {
        u32 ty = a[0] == 1 ? HT_RECV : a[0] == 3 ? HT_SEND : a[0] == 4 ? HT_DEAD : HT_RECV;
        hport_t *p = hport_new((u32)a[1], ty);
        if (!p) return a[1] ? KERN_EXISTS : KERN_SHORT;
        if (ty == HT_SEND) p->srefs = 1;
        a[2] = p->name;
        return KERN_OK;
    }
    case MACH_OP_CONSTRUCT: {
        hport_t *p = hport_new(0, HT_RECV);
        if (!p) return KERN_SHORT;
        if (a[1]) p->qlimit = (u32)a[1];
        p->context = a[2];
        a[3] = p->name;
        if (!said_construct) {
            said_construct = 1;
            fprintf(stderr, "[machotest] mach_port_construct ok nome=%u qlimit=%u\n", p->name, p->qlimit);
        }
        return KERN_OK;
    }
    case MACH_OP_INSERT_RIGHT: {
        hport_t *src = hport_get((u32)a[1]);
        u32 disp = (u32)a[2];
        if (!src) return KERN_INVAL_NAME;
        if (a[0] && a[0] != src->name) return KERN_INVAL_RIGHT;
        if (disp == 16) { src->type |= HT_RECV; return KERN_OK; }
        if (disp == 18 || disp == 21) { src->type |= HT_SO; return KERN_OK; }
        src->type |= HT_SEND;
        src->srefs++;
        return KERN_OK;
    }
    case MACH_OP_DEALLOCATE:
    case MACH_OP_DESTRUCT: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return hport_special((u32)a[0]) ? KERN_OK : KERN_INVAL_NAME;
        if (p->type & HT_SEND) {
            if (p->srefs) p->srefs--;
            if (!p->srefs) p->type &= ~HT_SEND;
        } else if (p->type & HT_RECV) p->type &= ~HT_RECV;
        if (!(p->type & (HT_SEND | HT_RECV | HT_SO))) hport_free_slot(p);
        host_broadcast();
        return KERN_OK;
    }
    case MACH_OP_MOD_REFS: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return hport_special((u32)a[0]) ? KERN_OK : KERN_INVAL_NAME;
        i64 delta = (i64)a[2];
        if (a[1] == 0 && (p->type & HT_SEND)) {
            if ((i64)p->srefs + delta < 0) return 18;
            p->srefs = (u32)((i64)p->srefs + delta);
            if (!p->srefs) p->type &= ~HT_SEND;
        } else if (delta < 0 && a[1] == 1 && (p->type & HT_RECV)) p->type &= ~HT_RECV;
        if (!(p->type & (HT_SEND | HT_RECV | HT_SO))) hport_free_slot(p);
        return KERN_OK;
    }
    case MACH_OP_TYPE: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) { if (hport_special((u32)a[0])) { a[1] = HT_SEND; return KERN_OK; } return KERN_INVAL_NAME; }
        a[1] = p->type;
        return KERN_OK;
    }
    case MACH_OP_GET_REFS: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return KERN_INVAL_NAME;
        a[2] = a[1] == 0 ? p->srefs : (p->type & (a[1] == 1 ? HT_RECV : HT_SEND)) ? 1 : 0;
        return KERN_OK;
    }
    case MACH_OP_SET_CONTEXT: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return KERN_INVAL_NAME;
        p->context = a[1];
        return KERN_OK;
    }
    case MACH_OP_GET_CONTEXT: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return KERN_INVAL_NAME;
        a[1] = p->context;
        return KERN_OK;
    }
    case MACH_OP_SET_ATTR: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return KERN_INVAL_NAME;
        if (a[1] == 1) p->qlimit = (u32)a[2];
        return KERN_OK;
    }
    case MACH_OP_STATUS: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return KERN_INVAL_NAME;
        a[1] = (u64)p->qn; a[2] = p->seqno; a[3] = p->qlimit; a[4] = p->srefs;
        return KERN_OK;
    }
    case MACH_OP_PENDING: {
        hport_t *p = hport_get((u32)a[0]);
        a[1] = p ? (u64)p->qn : 0;
        a[2] = a[0];
        return KERN_OK;
    }
    case MACH_OP_PEEK: {       /* como kernel/proc/machipc.c: fila vazia (ou porta sem fila) = KERN_FAILURE (5) */
        hport_t *p = hport_get((u32)a[0]);
        if (!p || !p->qn) return 5;
        const u8 *m = p->qdata[p->qh];
        a[1] = p->qsize[p->qh];
        a[2] = p->seqno;
        a[3] = p->qsize[p->qh] >= 24 ? rd32u(m + 20) : 0;   /* msgh_id */
        return KERN_OK;
    }
    case MACH_OP_GET_SPECIAL:
        if (a[0] == 4) {
            if (!hport_get(0x303)) { hport_t *b = hport_new(0x303, HT_SEND); if (b) b->srefs = 1; }
            a[1] = 0x303;
            return KERN_OK;
        }
        a[1] = a[0] == 1 ? 0x103 : a[0] == 2 ? 0x203 : 0;
        return a[1] ? KERN_OK : KERN_INVAL_ARG;
    case MACH_OP_NOTIFY:
    case MACH_OP_INSERT_MEMBER:
    case MACH_OP_EXTRACT_MEMBER:
    case MACH_OP_MOVE_MEMBER:
    case MACH_OP_SET_SPECIAL:
    case MACH_OP_SPAWN_SPECIAL:
        return KERN_OK;
    case MACH_OP_EXTRACT_RIGHT: {
        hport_t *p = hport_get((u32)a[0]);
        if (!p) return KERN_INVAL_NAME;
        hport_t *n = hport_new(0, (a[1] == 18 || a[1] == 21) ? HT_SO : HT_SEND);
        if (!n) return KERN_SHORT;
        n->srefs = 1;
        a[2] = n->name;
        a[3] = a[1];
        return KERN_OK;
    }
    default:
        return KERN_INVAL_ARG;
    }
}

static long mach_msg_call(u64 args_ptr) {
    mos_mach_msg_args_t a;
    if (!args_ptr) return KERN_INVAL_ARG;
    memcpy(&a, (void *)args_ptr, sizeof a);
    if (a.option & 1) {
        if (a.send_size < 24) return 0x10000008;
        u8 *msg = (u8 *)a.msg;
        u32 remote = rd32u(msg + 8);
        hport_t *dest = hport_get(remote);
        if (!dest || !(dest->type & HT_RECV)) return 0x10000003;
        u32 cap = dest->qlimit ? dest->qlimit : 5;
        if ((u32)dest->qn >= cap || dest->qn >= HMSG_N) return 0x10000004;
        u8 *copy = malloc(a.send_size ? a.send_size : 1);
        if (!copy) return KERN_SHORT;
        memcpy(copy, msg, a.send_size);
        int slot = (dest->qh + dest->qn) % HMSG_N;
        dest->qdata[slot] = copy;
        dest->qsize[slot] = a.send_size;
        dest->qn++;
        dest->seqno++;
        host_broadcast();
    }
    if (a.option & 2) {
        hport_t *rcv = hport_get(a.rcv_name);
        if (!rcv || !(rcv->type & HT_RECV)) return 0x10004002;
        while (!rcv->qn) {
            if ((a.option & 0x100) && a.timeout == 0) return 0x10004003;
            u64 ms = (a.option & 0x100) ? a.timeout : ~(u64)0;
            if (host_wait_ms(ms) && !rcv->qn) return 0x10004003;
        }
        u8 *data = rcv->qdata[rcv->qh];
        u32 sz = rcv->qsize[rcv->qh];
        u32 el = (a.option >> 24) & 0xF;
        u32 tsize = el == 0 ? 8 : el == 1 ? 12 : el == 2 ? 20 : 52;
        u32 body = (sz + 3u) & ~3u;
        u32 need = body + tsize;
        u8 *dst = (u8 *)a.msg;
        if (need > a.rcv_size) {
            if (a.option & 4) { memcpy(dst, data, sz < 24 ? sz : 24); wr32u(dst + 4, need); return 0x10004004; }
            free(data);
            rcv->qdata[rcv->qh] = NULL;
            rcv->qh = (rcv->qh + 1) % HMSG_N;
            rcv->qn--;
            return 0x10004004;
        }
        memcpy(dst, data, sz);
        if (need > sz) memset(dst + sz, 0, need - sz);
        wr32u(dst + 4, need);
        wr32u(dst + body, 0);
        wr32u(dst + body + 4, tsize);
        if (tsize >= 52) { wr32u(dst + body + 20, 42); wr32u(dst + body + 40, 42); }
        free(data);
        rcv->qdata[rcv->qh] = NULL;
        rcv->qh = (rcv->qh + 1) % HMSG_N;
        rcv->qn--;
    }
    return KERN_OK;
}

static long mach_vm_op(u64 op, u64 ap) {
    u64 *a = (u64 *)ap;
    if (!a) return KERN_INVAL_ARG;
    if (op == MACH_VM_PROTECT || op == MACH_VM_JIT_WPROTECT) return KERN_OK;
    if (op == MACH_VM_REGION) {
        u64 addr = a[0];
        if (addr < USER_BASE || addr >= USER_END) return 1;
        a[1] = addr & ~0xFFFULL;
        a[2] = 0x1000;
        a[3] = 7;
        a[4] = 7;
        return KERN_OK;
    }
    if (op == MACH_VM_MAKE_ENTRY) {
        u64 sz = a[0];
        if (getenv("MACHOTEST_DEBUG")) fprintf(stderr, "[vm] entrada sz=0x%llx end=0x%llx perm=0x%llx pai=0x%llx\n",
            (unsigned long long)a[0], (unsigned long long)a[1], (unsigned long long)a[2], (unsigned long long)a[3]);
        if (!sz || sz > (256ull << 20)) return KERN_SHORT;
        sz = (sz + 0xFFFULL) & ~0xFFFULL;
        hport_t *p = hport_new(0, HT_SEND);
        if (!p) return KERN_SHORT;
        p->srefs = 1;
        p->memsz = sz;
        p->memprot = (u32)(a[2] & 7);
#ifndef _WIN32
        if (!(a[2] & 0x20000) && a[1]) {                 /* sem MAP_MEM_NAMED_CREATE: as páginas do endereço a1 */
            for (int i = hmap_n - 1; i >= 0; i--) {
                if (a[1] < hmaps[i].at || a[1] + a[0] > hmaps[i].at + hmaps[i].sz) continue;
                p->memfd = dup(hmaps[i].memfd);
                p->memoff = hmaps[i].off + (a[1] - hmaps[i].at);
                if (p->memfd < 0) { hport_free_slot(p); return KERN_SHORT; }
                a[4] = p->name;
                a[5] = sz;
                return KERN_OK;
            }
        }
        p->memfd = memfd_create("mos-mach", 0);
        if (p->memfd < 0 || ftruncate(p->memfd, (off_t)sz) != 0) { hport_free_slot(p); return KERN_SHORT; }
#else
        p->memfd = -1;
#endif
        a[4] = p->name;
        a[5] = sz;
        return KERN_OK;
    }
    if (op == MACH_VM_MAP) {
        hport_t *p = hport_get((u32)a[3]);
        if (getenv("MACHOTEST_DEBUG")) fprintf(stderr, "[vm] map obj=0x%llx at=0x%llx sz=0x%llx fl=0x%llx off=0x%llx prot=0x%llx porta=%s memsz=0x%llx\n",
            (unsigned long long)a[3], (unsigned long long)a[0], (unsigned long long)a[1], (unsigned long long)a[2],
            (unsigned long long)a[4], (unsigned long long)a[5], p ? "sim" : "não", p ? (unsigned long long)p->memsz : 0ULL);
        if (!p || !p->memsz) return KERN_INVAL_ARG;
        /* como kernel/proc/machipc.c: pedir escrita numa entrada só-leitura é KERN_INVALID_RIGHT. O Chromium confere
         * isso (CheckPlatformHandlePermissionsCorrespond) e dá CHECK se o mapa gravável passar. */
        if (((a[5] | a[6]) & 7) & ~p->memprot) return KERN_INVAL_RIGHT;
        u64 sz = a[1] ? a[1] : p->memsz;
        u64 at = a[0];
        if ((a[2] & 1) || !at) {
            if (!g_proc.mmap_next) g_proc.mmap_next = mmap_next;
            at = (g_proc.mmap_next + 0xFFFFULL) & ~0xFFFFULL;
            g_proc.mmap_next = at + ((sz + 0xFFFFULL) & ~0xFFFFULL);
        }
        if (!uvm_map(&g_proc, at, sz)) return KERN_NO_SPACE_;
#ifndef _WIN32
        if (p->memfd >= 0) {
            int prot = PROT_READ | ((a[5] & 2) || (!a[5] && (p->memprot & 2)) ? PROT_WRITE : 0);
            if (a[5] & 4) prot |= PROT_EXEC;
            u64 foff = p->memoff + a[4];
            void *r = mmap((void *)at, (size_t)sz, prot, MAP_SHARED | MAP_FIXED, p->memfd, (off_t)foff);
            if (r == MAP_FAILED) return KERN_NO_SPACE_;
            if (hmap_n < HMAP_N) { hmaps[hmap_n].at = at; hmaps[hmap_n].sz = sz; hmaps[hmap_n].off = foff; hmaps[hmap_n].memfd = dup(p->memfd); if (hmaps[hmap_n].memfd >= 0) hmap_n++; }
        }
#endif
        a[0] = at;
        return KERN_OK;
    }
    return 5;
}

static long long do_syscall_locked(u64 n, u64 a, u64 b, u64 c, u64 d);
static long long do_syscall(u64 n, u64 a, u64 b, u64 c, u64 d) {
    if (my_tid == 1) { g_last_sys[0] = n; g_last_sys[1] = a; g_last_sys[2] = b; }
    switch (n) {
    case SYS_SLEEP:
#ifdef _WIN32
        Sleep((DWORD)a);
#else
        { struct timespec ts; ts.tv_sec = (time_t)(a / 1000); ts.tv_nsec = (long)(a % 1000) * 1000000L; nanosleep(&ts, NULL); }
#endif
        return 0;
    case SYS_YIELD:
#ifdef _WIN32
        SwitchToThread();
#else
        sched_yield();
#endif
        return 0;
    case SYS_GETTID: return my_tid;
    case SYS_THREAD_CREATE: {
        long long tid = 
#ifdef _WIN32
            InterlockedIncrement(&next_tid);
#else
            __atomic_add_fetch(&next_tid, 1, __ATOMIC_SEQ_CST);
#endif
        if (tid <= 0 || tid >= 256) return -MOS_ENOMEM;
        thread_start_t *s = malloc(sizeof *s);
        if (!s) return -MOS_ENOMEM;
        s->entry = a; s->rsp = b; s->arg = c; s->tid = tid;
#ifdef _WIN32
        HANDLE h = CreateThread(NULL, 1 << 20, thread_main, s, 0, NULL);
        if (!h) { free(s); return -MOS_ENOMEM; }
        thread_handles[tid] = h;
#else
        if (pthread_create(&thread_handles[tid], NULL, thread_main, s) != 0) { free(s); return -MOS_ENOMEM; }
#endif
        host_thr_add((int)tid);
        return tid;
    }
    case SYS_THREAD_EXIT:
        host_thr_dead((int)my_tid);
#ifdef _WIN32
        ExitThread((DWORD)a);
#else
        pthread_exit((void *)(intptr_t)a);
#endif
        return 0;
    case SYS_THREAD_JOIN: {
        if (a >= 256 || a == 0) return -MOS_ESRCH;
#ifdef _WIN32
        if (!thread_handles[a]) return -MOS_ESRCH;
        WaitForSingleObject(thread_handles[a], INFINITE);
        DWORD code = 0;
        GetExitCodeThread(thread_handles[a], &code);
        return (long long)(int)code;
#else
        void *code = NULL;
        if (pthread_join(thread_handles[a], &code) != 0) return -MOS_ESRCH;
        return (long long)(intptr_t)code;
#endif
    }
    }
    HOST_LOCK();
    long long r = do_syscall_locked(n, a, b, c, d);
    HOST_UNLOCK();
    return r;
}

static int ux_alloc(int slot, int nb, int clo) {
    int fd = fd_alloc();
    if (fd < 0) return -MOS_EMFILE;
    hfd[fd].kind = HFD_UNIX;
    hfd[fd].aux = slot;
    hfd[fd].nb = nb;
    hfd[fd].cloexec = clo;
    return fd;
}
static int ux_slot(int fd) {
    if (fd < 0 || fd >= MAXFD || hfd[fd].kind != HFD_UNIX) return -1;
    return hfd[fd].aux;
}
static void ux_set_slot(int fd, int slot) {
    if (fd >= 0 && fd < MAXFD) hfd[fd].aux = slot;
}
static int ux_dup(int fd) {
    if (fd < 0 || fd >= MAXFD || !hfd[fd].kind) return -MOS_EBADF;
    int nfd = fd_alloc();
    if (nfd < 0) return -MOS_EMFILE;
    if (hfd[fd].kind == HFD_UNIX) host_unix_hold(fd);
    else if (hfd[fd].kind == HFD_FILE) {
        int dfd = dup(fileno(hfd[fd].fp));
        if (dfd < 0) return -MOS_EMFILE;
        FILE *nf = fdopen(dfd, hfd[fd].mode[0] ? hfd[fd].mode : "rb");
        if (!nf) { close(dfd); return -MOS_EMFILE; }
        hfd[nfd] = hfd[fd];
        hfd[nfd].fp = nf;
        return nfd;
    } else pipe_addref(&hfd[fd]);
    hfd[nfd] = hfd[fd];
    return nfd;
}
static int ux_parent(const char *path) {
    char tmp[1024], hp[4096];
    snprintf(tmp, sizeof tmp, "%s", path);
    char *sl = strrchr(tmp, '/');
    if (!sl || sl == tmp) return 1;
    *sl = 0;
    host_path((u64)tmp, hp, sizeof hp);
    struct stat st;
    return stat(hp, &st) == 0 && S_ISDIR(st.st_mode);
}
static int ux_touch(const char *path) {
    char hp[4096];
    host_path((u64)path, hp, sizeof hp);
    if (access(hp, F_OK) == 0) return -MOS_EADDRINUSE;
    FILE *f = fopen(hp, "wb");
    if (!f) return -MOS_ENOENT;
    fclose(f);
    return 0;
}
static int ux_unlink(const char *path) {
    char hp[4096];
    host_path((u64)path, hp, sizeof hp);
    unlink(hp);
    return 0;
}
static void ux_hooks(void) {
    static int once;
    if (once) return;
    once = 1;
    host_thr_boot();
    host_unix_hooks hk = { ux_alloc, ux_slot, ux_set_slot, ux_dup, ux_parent, ux_touch, ux_unlink,
                           host_unlock_fn, host_lock_fn };
    host_unix_set_hooks(&hk);
}

static long long do_syscall_locked(u64 n, u64 a, u64 b, u64 c, u64 d) {
    ux_hooks();
    if (n >= 64 && n <= 79) return gui_syscall(n, a, b, c, d);
    if (n == SYS_FUTEX_WAIT) return futex_wait(a, (u32)b, c);
    if (n == SYS_FUTEX_WAKE) return futex_wake(a, b);
    if (n == SYS_MACH_PORT) return mach_port_op(a, b);
    if (n == SYS_MACH_MSG) return mach_msg_call(a);
    if (n == SYS_MACH_VM) return mach_vm_op(a, b);
    if (n == SYS_MACH_BOOTSTRAP) {
        /* Igual a bootstrap_op: check_in de nome novo cria o direito de receber.
         * look_up devolve esse porto e marca o direito de enviar. */
        const char *nm = (const char *)b;
        u32 *pp = (u32 *)c;
        unsigned port = pp ? *pp : 0;
        unsigned (*mk)(void) = NULL;
        if (a == MACH_BS_CHECK_IN || a == MACH_BS_RESERVE) mk = bs_new_recv;
        long r = host_bootstrap((unsigned)a, nm, &port, mk);
        if (r == 0 && pp) *pp = port;
        if (r == 0 && a == MACH_BS_LOOK_UP) {
            hport_t *hp = hport_get(port);
            if (hp) { hp->type |= HT_SEND; hp->srefs++; }
        }
        return r;
    }
    if (n == SYS_PIPE) return pipe_make(a, b == MOS_PIPE_SOCKPAIR);
    char hp[4096];
    (void)d;
    switch (n) {
    case SYS_WRITE:
        if (a < MAXFD && hfd[a].kind == HFD_UNIX) return host_unix_write((int)a, (void *)b, c);
        if (a < MAXFD && (hfd[a].kind == HFD_PR || hfd[a].kind == HFD_PW || hfd[a].kind == HFD_RW))
            return pipe_io(0, (int)a, b, c);
        if ((a == 1 || a == 2) && hfd[a].kind == HFD_NONE) {
            fwrite((void *)b, 1, (size_t)c, a == 1 ? stdout : stderr);
            fflush(a == 1 ? stdout : stderr);
            return (long long)c;
        }
        if (a < MAXFD && hfd[a].kind == HFD_FILE && hfd[a].fp) return (long long)fwrite((void *)b, 1, (size_t)c, hfd[a].fp);
        return -MOS_EBADF;
    case SYS_READ:
        if (a < MAXFD && hfd[a].kind == HFD_UNIX) return host_unix_read((int)a, (void *)b, c);
        if (a < MAXFD && (hfd[a].kind == HFD_PR || hfd[a].kind == HFD_PW || hfd[a].kind == HFD_RW))
            return pipe_io(1, (int)a, b, c);
        if (a == 0 && hfd[a].kind == HFD_NONE) return 0;
        if (a < MAXFD && hfd[a].kind == HFD_FILE && hfd[a].fp) return (long long)fread((void *)b, 1, (size_t)c, hfd[a].fp);
        return -MOS_EBADF;
    case SYS_PREAD: {
        /* kernel/proc/process.c: offset em r10, a posição do fd não muda. Pipe, pty e AF_UNIX são ESPIPE. */
        if (a >= MAXFD || !hfd[a].kind) return -MOS_EBADF;
        if (hfd[a].kind != HFD_FILE) return -MOS_ESPIPE;
        if (hfd[a].acc == 1 || !hfd[a].fp) return -MOS_EBADF;
        if (!strcmp(hfd[a].gpath, "/dev/null")) return 0;
        return host_pread_fd(fileno(hfd[a].fp), (void *)b, (unsigned long)c, (unsigned long)d);
    }
#ifndef _WIN32
    case SYS_FTRUNCATE:        /* (fd, tamanho): o -shm do SQLite em WAL e os índices do disk_cache */
        if (a >= MAXFD || hfd[a].kind != HFD_FILE || !hfd[a].fp) return -MOS_EBADF;
        if (hfd[a].acc == 0) return -MOS_EINVAL;
        fflush(hfd[a].fp);
        return ftruncate(fileno(hfd[a].fp), (off_t)b) ? -MOS_EINVAL : 0;
    case SYS_PWRITE: {
        /* como o SYS_PREAD: deslocamento em r10, a posição do fd não muda (fs.writeFile do Node usa pwrite) */
        if (a >= MAXFD || !hfd[a].kind) return -MOS_EBADF;
        if (hfd[a].kind != HFD_FILE) return -MOS_ESPIPE;
        if (hfd[a].acc == 0 || !hfd[a].fp) return -MOS_EBADF;
        if (!strcmp(hfd[a].gpath, "/dev/null")) return (long long)c;
        fflush(hfd[a].fp);                       /* o FILE* pode ter escrita pendente antes desta posição */
        ssize_t w = pwrite(fileno(hfd[a].fp), (const void *)b, (size_t)c, (off_t)d);
        return w < 0 ? -MOS_EINVAL : (long long)w;
    }
#endif
    case SYS_GETUID:
    case SYS_GETEUID:
    case SYS_GETGID:
    case SYS_GETEGID:
        return host_getid((unsigned)n);
    case SYS_GETPID: return 42;
    case SYS_PROCLIST: {
        /* Um processo só, o deste harness (pid 42, sem pai), como proc_spawn_in sem processo corrente. */
        if (a != 0) return 0;
        mos_procinfo_t pi;
        memset(&pi, 0, sizeof pi);
        pi.pid = 42;
        pi.state = 1; /* TH_RUNNING */
        const char *nm = strrchr(g_proc.path, '/');
        nm = nm ? nm + 1 : g_proc.path;
        snprintf(pi.name, sizeof pi.name, "%s", nm);
        memcpy((void *)b, &pi, sizeof pi);
        return 1;
    }
    case SYS_DESKTOP:
        /* MOS_DESK_TAKE com a fila vazia devolve 0 (deskbridge.c). Banner e selo não têm mesa neste harness. */
        if (a == MOS_DESK_TAKE) return 0;
        if (a == MOS_DESK_BANNER || a == MOS_DESK_BADGE) return 0;
        return -MOS_EINVAL;
    case SYS_CPUS:
#ifdef _WIN32
        { SYSTEM_INFO si; GetSystemInfo(&si); return (long long)si.dwNumberOfProcessors; }
#else
        return sysconf(_SC_NPROCESSORS_ONLN);
#endif
    case 0x7FFF: {
        static unsigned long long calls;
        static char seen[8192];
        const char *nm = (const char *)a;
        calls++;
        if (nm && !strstr(seen, nm) && strlen(seen) + strlen(nm) + 2 < sizeof seen) {
            strcat(seen, nm); strcat(seen, " ");
            fprintf(stderr, "[ausente #%llu] %s\n", calls, nm);
        }
        if (calls > 200000) { fprintf(stderr, "[machotest] chamadas a stubs demais, parando\n"); return -1; }
        return 0;
    }
    case SYS_TIME:
#ifdef _WIN32
        return (long long)GetTickCount64();
#else
        { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
#endif
    case SYS_UNIXTIME: return (long long)time(NULL);
    case SYS_BRK:
        if (!a) return (long long)brk_cur;
        if (a > brk_limit) return (long long)brk_cur;
        if (a > brk_cur && !uvm_map(&g_proc, brk_cur, a - brk_cur)) return (long long)brk_cur;
        brk_cur = a;
        return (long long)brk_cur;
    case SYS_MMAP: {
        if (!g_proc.mmap_next) g_proc.mmap_next = mmap_next;
        if (b == 2 || (b == 3 && c)) return uvm_map(&g_proc, c, a) ? 0 : -MOS_ENOMEM;
        /* b = 4: mmap de arquivo (MAP_PRIVATE). Sem isto o icudtl.dat do Electron vira página zerada
         * e o ICU aborta com U_INVALID_FORMAT_ERROR. Pipe devolve EINVAL, como o kernel, e o libSystem
         * copia com read(). */
        /* b=4 privado, b=5 compartilhado, b=8/9 MAP_FIXED. Sem o conteúdo do arquivo o
         * icudtl.dat chega zerado e o ICU do Electron devolve U_INVALID_FORMAT_ERROR. */
        if (b == 4 || b == 5 || b == 8 || b == 9) {
            u64 off = d, addr = 0;
            unsigned long long fdi = c;
            if (b == 8 || b == 9) { addr = c; fdi = g_r8 & 0xffffffffull; }
            if (!a || (off & 0xFFF)) return -MOS_EINVAL;
            if (fdi >= MAXFD || !hfd[fdi].kind) return -MOS_EBADF;
            if (hfd[fdi].kind != HFD_FILE || !hfd[fdi].fp) return -MOS_EINVAL;
            if (hfd[fdi].acc == 1) return -MOS_EACCES;
            u64 len = (a + 0xFFFF) & ~0xFFFFULL;
            u64 at = addr ? addr : (g_proc.mmap_next + 0xFFFF) & ~0xFFFFULL;
            if (!uvm_map(&g_proc, at, len)) return -MOS_ENOMEM;
            if (!addr) g_proc.mmap_next = at + len;
#ifndef _WIN32
            ssize_t got = pread(fileno(hfd[fdi].fp), (void *)at, (size_t)a, (off_t)off);
            if (got < 0) got = 0;
#else
            long long cur = _ftelli64(hfd[fdi].fp);
            _fseeki64(hfd[fdi].fp, (long long)off, SEEK_SET);
            size_t got = fread((void *)at, 1, (size_t)a, hfd[fdi].fp);
            if (cur >= 0) _fseeki64(hfd[fdi].fp, cur, SEEK_SET);
            (void)got;
#endif
            return (long long)at;
        }
        u64 len = (a + 0xFFFF) & ~0xFFFFULL, at = (g_proc.mmap_next + 0xFFFF) & ~0xFFFFULL;
        if (b != 1 && !uvm_map(&g_proc, at, len)) return -MOS_ENOMEM;
        g_proc.mmap_next = at + len;
        return (long long)at;
    }
    case SYS_DYLD: return macho_dyld_syscall(&g_proc, a, b, c, d);
    case SYS_MUNMAP: return 0;
    case SYS_OPEN: {
        host_path(a, hp, sizeof hp);
        const char *mode = "rb";
        if (b & MOS_O_TRUNC) mode = (b & MOS_O_RDWR) ? "w+b" : "wb";
        else if (b & MOS_O_APPEND) mode = (b & MOS_O_RDWR) ? "a+b" : "ab";
        else if (b & (MOS_O_WRONLY | MOS_O_RDWR)) {
            mode = "r+b";
#ifdef _WIN32
            if ((b & MOS_O_CREAT) && _access(hp, 0)) mode = "w+b";
#else
            if ((b & MOS_O_CREAT) && access(hp, F_OK) != 0) mode = "w+b";
#endif
        }
        int fd = fd_alloc();
        if (fd < 0) return -MOS_EMFILE;
        FILE *fp = fopen(hp, mode);
        if (getenv("MACHOTEST_TRACE_FS")) fprintf(stderr, "[fs] open %s -> %s\n", (const char *)a, fp ? "ok" : "não existe");
        if (!fp) return -MOS_ENOENT;
        hfd[fd].kind = HFD_FILE;
        hfd[fd].fp = fp;
        snprintf(hfd[fd].gpath, sizeof hfd[fd].gpath, "%s", (const char *)a);
        snprintf(hfd[fd].mode, sizeof hfd[fd].mode, "%s", mode);
        hfd[fd].acc = (b & MOS_O_RDWR) ? 2 : (b & MOS_O_WRONLY) ? 1 : 0;
        hfd[fd].cloexec = (b & MOS_O_CLOEXEC) ? 1 : 0;
        return fd;
    }
    case SYS_CLOSE:
        if (a >= MAXFD || !hfd[a].kind) return -MOS_EBADF;
        fd_clear((int)a);
        return 0;
    case SYS_FDPATH: {
        if (a >= MAXFD || hfd[a].kind != HFD_FILE) return -MOS_EBADF;
        size_t n = strlen(hfd[a].gpath) + 1;
        if (n <= 1 || n > c) return -MOS_EINVAL;
        memcpy((void *)b, hfd[a].gpath, n);
        return (long long)n - 1;
    }
    case SYS_LSEEK:
        if (a >= MAXFD || hfd[a].kind != HFD_FILE || !hfd[a].fp) return -MOS_EBADF;
#ifdef _WIN32
        _fseeki64(hfd[a].fp, (long long)b, (int)c);
        return _ftelli64(hfd[a].fp);
#else
        if (fseeko(hfd[a].fp, (off_t)b, (int)c) != 0) return -MOS_EINVAL;
        return (long long)ftello(hfd[a].fp);
#endif
    case SYS_DUP2: {
        if (a >= MAXFD || !hfd[a].kind) return -MOS_EBADF;
        int nfd;
        if (b == MOS_DUP_ANY) {
            int min = (int)(c > 3 ? c : 3);
            nfd = -1;
            for (int i = min; i < MAXFD; i++) if (!hfd[i].kind) { nfd = i; break; }
            if (nfd < 0) return -MOS_EMFILE;
        } else {
            if (b >= MAXFD) return -MOS_EBADF;
            if (a == b) return (long long)a;
            nfd = (int)b;
        }
        if (hfd[a].kind == HFD_UNIX) {
            host_unix_hold((int)a);
            if (hfd[nfd].kind) fd_clear(nfd);
            hfd[nfd] = hfd[a];
            return nfd;
        }
        if (hfd[a].kind == HFD_FILE) {
            int dfd = dup(fileno(hfd[a].fp));
            if (dfd < 0) return -MOS_EMFILE;
            FILE *nf = fdopen(dfd, hfd[a].mode[0] ? hfd[a].mode : "rb");
            if (!nf) { close(dfd); return -MOS_EMFILE; }
            if (hfd[nfd].kind) fd_clear(nfd);
            hfd[nfd] = hfd[a];
            hfd[nfd].fp = nf;
            return nfd;
        }
        pipe_addref(&hfd[a]);
        if (hfd[nfd].kind) fd_clear(nfd);
        hfd[nfd] = hfd[a];
        return nfd;
    }
    case SYS_FDFLAGS:
        if (a >= MAXFD || !hfd[a].kind) return -MOS_EBADF;
        if (b == 2) return hfd[a].acc & 3;
        if (b) hfd[a].cloexec = c ? 1 : 0;
        return hfd[a].cloexec ? 1 : 0;
    case SYS_TTY:
        if (a == MOS_TTY_SETFL) {
            if (b >= MAXFD || !hfd[b].kind) return -MOS_EBADF;
            hfd[b].nb = c ? 1 : 0;
            if (hfd[b].kind == HFD_UNIX) host_unix_set_nb((int)b, hfd[b].nb);
            return 0;
        }
        return -MOS_ENOTTY;
    case SYS_FSTAT: case SYS_STAT: {
        if (n == SYS_FSTAT && a < MAXFD && hfd[a].kind == HFD_UNIX) {
            mos_stat_t *m = (mos_stat_t *)b;
            memset(m, 0, sizeof *m);
            unsigned long long sz = 0; int dir = 6; long long mt = 0;
            host_unix_fstat((int)a, &sz, &dir, &mt);
            m->size = sz; m->is_dir = (unsigned long long)dir; m->mtime = mt;
            return 0;
        }
        if (n == SYS_FSTAT && a < MAXFD && (hfd[a].kind == HFD_PR || hfd[a].kind == HFD_PW || hfd[a].kind == HFD_RW)) {
            pipe_stat((int)a, (mos_stat_t *)b);
            return 0;
        }
#ifdef _WIN32
        struct _stat64 s;
        int r;
        if (n == SYS_FSTAT) { if (a >= MAXFD || hfd[a].kind != HFD_FILE || !hfd[a].fp) return -MOS_EBADF; r = _fstat64(_fileno(hfd[a].fp), &s); }
        else { host_path(a, hp, sizeof hp); r = _stat64(hp, &s); }
        if (getenv("MACHOTEST_TRACE_FS") && n == SYS_STAT) fprintf(stderr, "[fs] stat %s -> %s\n", (const char *)a, r ? "não existe" : "ok");
        if (r) return -MOS_ENOENT;
        mos_stat_t *m = (mos_stat_t *)b;
        m->size = (unsigned long long)s.st_size;
        m->is_dir = (s.st_mode & _S_IFDIR) != 0;
        m->mtime = s.st_mtime;
#else
        struct stat s;
        int r;
        if (n == SYS_FSTAT) { if (a >= MAXFD || hfd[a].kind != HFD_FILE || !hfd[a].fp) return -MOS_EBADF; r = fstat(fileno(hfd[a].fp), &s); }
        else { host_path(a, hp, sizeof hp); r = stat(hp, &s); }
        if (getenv("MACHOTEST_TRACE_FS") && n == SYS_STAT) fprintf(stderr, "[fs] stat %s -> %s\n", (const char *)a, r ? "não existe" : "ok");
        if (r) return -MOS_ENOENT;
        mos_stat_t *m = (mos_stat_t *)b;
        memset(m, 0, sizeof *m);
        m->size = (unsigned long long)s.st_size;
        m->is_dir = S_ISDIR(s.st_mode) ? 1 : 0;
        m->mtime = (long long)s.st_mtime;
        m->mode = (unsigned)(s.st_mode & 07777);   /* o SYS_CHMOD abaixo grava no arquivo do host */
#endif
        return 0;
    }
    case SYS_READDIR: {
        host_path(a, hp, sizeof hp);
#ifdef _WIN32
        strcat(hp, "\\*");
        WIN32_FIND_DATAA wfd;
        HANDLE h = FindFirstFileA(hp, &wfd);
        if (h == INVALID_HANDLE_VALUE) return -MOS_ENOENT;
        u64 i = 0;
        do {
            if (!strcmp(wfd.cFileName, ".") || !strcmp(wfd.cFileName, "..")) continue;
            if (i++ == b) { snprintf((char *)c, d, "%s", wfd.cFileName); FindClose(h); return 1; }
        } while (FindNextFileA(h, &wfd));
        FindClose(h);
        return 0;
#else
        DIR *dir = opendir(hp);
        if (!dir) return -MOS_ENOENT;
        struct dirent *de;
        u64 i = 0;
        while ((de = readdir(dir))) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            if (i++ == b) { snprintf((char *)c, (size_t)d, "%s", de->d_name); closedir(dir); return 1; }
        }
        closedir(dir);
        return 0;
#endif
    }
    case SYS_MKDIR:
        host_path(a, hp, sizeof hp);
#ifdef _WIN32
        return _mkdir(hp) ? -MOS_EEXIST : 0;
#else
        if (!mkdir(hp, 0755)) return 0;
        /* pai ausente = ENOENT (o mkdir recursivo do Node cria os pais só com esse código) */
        return errno == ENOENT ? -MOS_ENOENT : errno == ENOTDIR ? -MOS_ENOTDIR : -MOS_EEXIST;
#endif
#ifndef _WIN32
    case SYS_FCHMOD:           /* (fd, mode) */
        if (a >= MAXFD || hfd[a].kind != HFD_FILE || !hfd[a].fp) return -MOS_EBADF;
        return fchmod(fileno(hfd[a].fp), (mode_t)(b & 07777)) ? -MOS_EINVAL : 0;
    case SYS_CHMOD:            /* (path, mode): o stat devolve esse mode, como no kernel (process.c) */
        host_path(a, hp, sizeof hp);
        return chmod(hp, (mode_t)(b & 07777)) ? -MOS_ENOENT : 0;
#endif
    case SYS_UNLINK: host_path(a, hp, sizeof hp); return remove(hp) ? -MOS_ENOENT : 0;
    case SYS_RENAME: {
        char hp2[4096];
        host_path(a, hp, sizeof hp);
        host_path(b, hp2, sizeof hp2);
        return rename(hp, hp2) ? -MOS_ENOENT : 0;
    }
    case SYS_UNIX: return host_unix_op(a, b, c, d);
#ifndef _WIN32
    case SYS_AIO: {
        /* userland/libsystem/aio.c e o fsync do libc.c: op em a, aiocb* em b. O harness conclui na hora (o kernel
         * enfileira): 1 read, 2 write, 5 fsync guardam o resultado; 3 = erro (0 pronto), 4 = retorno, 6 = suspend. */
        static struct { u64 cb; long long ret; int err; } aio_done[64];
        struct hcb { int fd; int pad; long long off; void *buf; unsigned long long n; int prio, op; } *cb = (struct hcb *)b;
        if (a == 6) return 0;
        if (!cb) return -MOS_EINVAL;
        int slot = -1;
        for (int i = 0; i < 64; i++) if (aio_done[i].cb == b) { slot = i; break; }
        if (a == 3) return slot < 0 ? -MOS_EINVAL : aio_done[slot].err;
        if (a == 4) {
            if (slot < 0) return -MOS_EINVAL;
            long long r = aio_done[slot].err ? -aio_done[slot].err : aio_done[slot].ret;
            aio_done[slot].cb = 0;
            return r;
        }
        if (a != 1 && a != 2 && a != 5) break;
        if ((u64)cb->fd >= MAXFD || hfd[cb->fd].kind != HFD_FILE || !hfd[cb->fd].fp) return -MOS_EBADF;
        int hf = fileno(hfd[cb->fd].fp);
        long long r;
        fflush(hfd[cb->fd].fp);
        if (a == 1) r = pread(hf, cb->buf, (size_t)cb->n, (off_t)cb->off);
        else if (a == 2) r = pwrite(hf, cb->buf, (size_t)cb->n, (off_t)cb->off);
        else r = fsync(hf);
        if (slot < 0) for (int i = 0; i < 64; i++) if (!aio_done[i].cb) { slot = i; break; }
        if (slot < 0) return -MOS_EAGAIN;
        aio_done[slot].cb = b;
        aio_done[slot].ret = r < 0 ? -1 : r;
        aio_done[slot].err = r < 0 ? MOS_EIO : 0;
        return 0;
    }
#endif
    case SYS_NET:
        /* as raízes da Mozilla que o kernel embute (kernel/arch/certs.S): o mesmo assets/certs/cacert.pem,
         * lido de MACHOTEST_CACERT ou do diretório atual (a raiz do repositório) */
        if (a == MOS_NET_TLS_ROOTS) {
            const char *cp = getenv("MACHOTEST_CACERT");
            FILE *cf = fopen(cp && *cp ? cp : "assets/certs/cacert.pem", "rb");
            if (!cf) return -MOS_ENOENT;
            fseek(cf, 0, SEEK_END);
            long clen = ftell(cf);
            fseek(cf, 0, SEEK_SET);
            if (b && (long long)c < clen) { fclose(cf); return -MOS_EINVAL; }
            if (b && fread((void *)(uintptr_t)b, 1, (size_t)clen, cf) != (size_t)clen) { fclose(cf); return -MOS_EFAULT; }
            fclose(cf);
            return clen;
        }
        break;
    case SYS_THREADCTL:
        if (a == MOS_THR_SETQOS) return host_thr_setqos((int)b, (int)my_tid, (unsigned)c);
        break;
    case SYS_GPU: {
        int blocked = 0;
        long gr = host_gpu_op(a, b, c, &blocked);
        if (blocked) return -MOS_EAGAIN;
        return gr;
    }
    }
    {
        static unsigned char seen_sys[256];
        if (n < 256) {
            if (seen_sys[n]) return -MOS_EINVAL;
            seen_sys[n] = 1;
        }
        fprintf(stderr, "[machotest] syscall %llu não emulada (a=%llx b=%llx c=%llx)\n",
                (unsigned long long)n, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c);
    }
    return -MOS_EINVAL;
}

#ifdef _WIN32
/* ---- sinais (SYS_SIG), o bastante para o runtime do tradutor arm64 (userland/a64rt/sig.c) e para tratadores x86:
 * ações por processo, máscara por thread, faltas do Windows (VEH) viram o quadro do kernel (siginfo + ucontext +
 * mcontext x86 do Darwin) e desviam para o trampolim; MOS_SIGOP_RETURN restaura o CONTEXT. TKILL para outra thread
 * fica pendente até a próxima syscall dela. Ação padrão de um sinal que mata: encerra com 128 + sinal. */
static mos_sigaction_t sig_act[MOS_NSIG];
static __thread u32 sig_mask_t;
static volatile LONG sig_pend[256];
static int sig_default_kills(int s) {
    return !(s == MOS_SIGCHLD || s == MOS_SIGURG || s == MOS_SIGWINCH || s == MOS_SIGINFO || s == MOS_SIGCONT || s == MOS_SIGIO);
}
typedef struct { mos_siginfo_t info; mos_ucontext_t uc; u64 pad; mos_mcontext_t mc; } host_sigframe_t;
static void ctx_to_ss(const CONTEXT *c, mos_thread_state_t *s) {
    s->rax = c->Rax; s->rbx = c->Rbx; s->rcx = c->Rcx; s->rdx = c->Rdx; s->rdi = c->Rdi; s->rsi = c->Rsi; s->rbp = c->Rbp;
    s->rsp = c->Rsp; s->r8 = c->R8; s->r9 = c->R9; s->r10 = c->R10; s->r11 = c->R11; s->r12 = c->R12; s->r13 = c->R13;
    s->r14 = c->R14; s->r15 = c->R15; s->rip = c->Rip; s->rflags = c->EFlags; s->cs = 0x1B;
}
static void ss_to_ctx(const mos_thread_state_t *s, CONTEXT *c) {
    c->Rax = s->rax; c->Rbx = s->rbx; c->Rcx = s->rcx; c->Rdx = s->rdx; c->Rdi = s->rdi; c->Rsi = s->rsi; c->Rbp = s->rbp;
    c->Rsp = s->rsp; c->R8 = s->r8; c->R9 = s->r9; c->R10 = s->r10; c->R11 = s->r11; c->R12 = s->r12; c->R13 = s->r13;
    c->R14 = s->r14; c->R15 = s->r15; c->Rip = s->rip; c->EFlags = (DWORD)((c->EFlags & ~0xDD5u) | (s->rflags & 0xDD5u));
}
static void sig_die(int s) {
    fflush(stdout);
    fprintf(stderr, "[machotest] encerrado pelo sinal %d (thread %lld)\n", s, my_tid);
    ExitProcess(128 + s);
}
/* monta o quadro na pilha da thread e desvia para o trampolim; devolve 0 se a ação é a padrão/ignorar */
static int sig_deliver_ctx(CONTEXT *c, int s, int code, u64 addr, u32 err) {
    mos_sigaction_t a = sig_act[s];
    if (a.handler == MOS_SIG_IGN) return 1;
    if (a.handler == MOS_SIG_DFL || (sig_mask_t & (1u << s))) { if (sig_default_kills(s)) sig_die(s); return 1; }
    /* o Windows despacha a exceção na mesma pilha, logo abaixo do rsp (CONTEXT, registro, quadros da VEH): o quadro vai
     * abaixo da pilha desta VEH, senão a sobrescreve e o NtContinue falha com STATUS_INVALID_PARAMETER */
    u64 below = ((u64)__builtin_frame_address(0) - 16384) & ~15ULL;
    u64 top = c->Rsp - 128 < below ? c->Rsp - 128 : below;
    u64 fa = (top - sizeof(host_sigframe_t)) & ~15ULL;
    host_sigframe_t *f = (host_sigframe_t *)fa;
    memset(f, 0, sizeof *f);
    f->info.si_signo = s; f->info.si_code = code; f->info.si_addr = addr; f->info.si_pid = 42; f->info.si_uid = 501;
    f->uc.uc_sigmask = sig_mask_t;
    f->uc.uc_stack.flags = MOS_SS_DISABLE;
    f->uc.uc_mcsize = sizeof(mos_mcontext_t);
    f->uc.uc_mcontext = (u64)&f->mc;
    f->mc.err = err; f->mc.faultvaddr = addr; f->mc.trapno = s == MOS_SIGSEGV ? 14 : 0;
    ctx_to_ss(c, &f->mc.ss);
    memcpy(f->mc.fx, &c->FltSave, 512);
    sig_mask_t |= a.mask | ((a.flags & MOS_SA_NODEFER) ? 0 : (1u << s));
    c->Rsp = fa - 8;
    if (a.tramp) {
        c->Rip = a.tramp; c->Rdi = a.handler; c->Rsi = (a.flags & MOS_SA_SIGINFO) ? 30 : 1; c->Rdx = (u64)s;
        c->Rcx = (u64)&f->info; c->R8 = (u64)&f->uc;
    } else {
        c->Rip = a.handler; c->Rdi = (u64)s; c->Rsi = (u64)&f->info; c->Rdx = (u64)&f->uc;
    }
    *(u64 *)c->Rsp = 0;
    return 1;
}
static void sig_check_pending(CONTEXT *c) {
    if (my_tid >= 256) return;
    u32 p = (u32)sig_pend[my_tid] & ~sig_mask_t;
    if (!p) return;
    int s = __builtin_ctz(p);
    InterlockedAnd(&sig_pend[my_tid], ~(LONG)(1u << s));
    sig_deliver_ctx(c, s, MOS_SI_USER, 0, 0);
}
/* SYS_SIG no CONTEXT da thread: devolve 1 se já tratou (rax e rip ajustados) */
static int sig_syscall_ctx(CONTEXT *c) {
    u64 op = c->Rdi, a = c->Rsi, b = c->Rdx, d = c->R10;
    long long r = 0;
    switch (op) {
    case MOS_SIGOP_ACTION:
        if (a == 0 || a >= MOS_NSIG) { r = -MOS_EINVAL; break; }
        if (d) memcpy((void *)d, &sig_act[a], sizeof(mos_sigaction_t));
        if (b) memcpy(&sig_act[a], (const void *)b, sizeof(mos_sigaction_t));
        break;
    case MOS_SIGOP_MASK: {
        u32 old = sig_mask_t, set = b ? *(const u32 *)b : 0;
        if (b) {
            if (a == MOS_SIG_BLOCK) sig_mask_t |= set; else if (a == MOS_SIG_UNBLOCK) sig_mask_t &= ~set;
            else if (a == MOS_SIG_SETMASK) sig_mask_t = set; else { r = -MOS_EINVAL; break; }
            sig_mask_t &= ~((1u << MOS_SIGKILL) | (1u << MOS_SIGSTOP));
        }
        if (d) *(u32 *)d = old;
        break;
    }
    case MOS_SIGOP_PENDING: *(u32 *)a = my_tid < 256 ? (u32)sig_pend[my_tid] : 0; break;
    case MOS_SIGOP_ALTSTACK: if (b) memset((void *)b, 0, sizeof(mos_sigstack_t)), ((mos_sigstack_t *)b)->flags = MOS_SS_DISABLE; break;
    case MOS_SIGOP_RETURN: {
        const mos_ucontext_t *uc = (const mos_ucontext_t *)a;
        const mos_mcontext_t *mc = (const mos_mcontext_t *)uc->uc_mcontext;
        ss_to_ctx(&mc->ss, c);
        memcpy(&c->FltSave, mc->fx, 512);
        sig_mask_t = uc->uc_sigmask & ~((1u << MOS_SIGKILL) | (1u << MOS_SIGSTOP));
        sig_check_pending(c);
        return 1;
    }
    case MOS_SIGOP_TKILL:
        if (b == 0) break;
        if (b >= MOS_NSIG || a >= 256) { r = -MOS_EINVAL; break; }
        if ((long long)a == my_tid) {
            c->Rax = 0; c->Rip += 2;
            if (sig_act[b].handler == MOS_SIG_DFL && sig_default_kills((int)b) && !(sig_mask_t & (1u << b))) sig_die((int)b);
            InterlockedOr(&sig_pend[a], (LONG)(1u << b));
            sig_check_pending(c);
            return 1;
        }
        InterlockedOr(&sig_pend[a], (LONG)(1u << b));
        break;
    case MOS_SIGOP_SUSPEND: Sleep(1); r = -MOS_EINTR; break;
    case MOS_SIGOP_WAIT: r = -MOS_EAGAIN; break;
    default: r = -MOS_EINVAL;
    }
    c->Rax = (u64)r;
    c->Rip += 2;
    sig_check_pending(c);
    return 1;
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *ep) {
    CONTEXT *c = ep->ContextRecord;
    if (getenv("MACHOTEST_DEBUG")) fprintf(stderr, "[veh] code=%lx rip=%llx rax=%llx\n", ep->ExceptionRecord->ExceptionCode, c->Rip, c->Rax);
    const u8 *ip = (const u8 *)c->Rip;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if ((code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_PRIV_INSTRUCTION) && c->Rip >= USER_BASE && c->Rip < USER_END && ip[0] == 0xCD && ip[1] == 0x80) {
        if (c->Rax == SYS_EXIT && my_tid != 1) {        /* exit numa thread secundÃ¡ria: encerra tudo */
            fflush(stdout);
            fprintf(stderr, "[machotest] cÃ³digo de saÃ­da: %d (thread %lld)\n", (int)c->Rdi, my_tid);
            ExitProcess((UINT)(int)c->Rdi);
        }
        if (c->Rax == SYS_EXIT) {
            c->Rax = (u32)(int)c->Rdi;
            c->Rip = (DWORD64)host_exit;
            c->Rsp = host_saved_rsp;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (c->Rax == SYS_SIG && sig_syscall_ctx(c)) return EXCEPTION_CONTINUE_EXECUTION;
        if (c->Rax == SYS_MACH_VM && c->Rdi == MACH_VM_JIT_WPROTECT && !getenv("MACHOTEST_NO_WP")) {   /* rede de SMC do JIT arm64: VirtualProtect */
            u64 *a = (u64 *)c->Rsi, pg = a[0] & ~0xFFFULL;
            MEMORY_BASIC_INFORMATION mbi;
            a[2] = 0;
            if (VirtualQuery((void *)pg, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
                DWORD p = mbi.Protect & 0xFF, np = 0, old;
                bool w = p == PAGE_READWRITE || p == PAGE_EXECUTE_READWRITE;
                if (a[1] == 2) a[2] = w;
                else if (a[1] == 1) { if (w) { np = p == PAGE_READWRITE ? PAGE_READONLY : PAGE_EXECUTE_READ; a[2] = VirtualProtect((void *)pg, 4096, np, &old) != 0; } }
                else { a[2] = 1; if (p == PAGE_READONLY || p == PAGE_EXECUTE_READ) VirtualProtect((void *)pg, 4096, p == PAGE_READONLY ? PAGE_READWRITE : PAGE_EXECUTE_READWRITE, &old); }
            }
            c->Rax = 0; c->Rip += 2;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        g_r8 = c->R8;                               /* 5º argumento (GUI_DRAG/GUI_IME) */
        c->Rax = (DWORD64)do_syscall(c->Rax, c->Rdi, c->Rsi, c->Rdx, c->R10);
        c->Rip += 2;
        sig_check_pending(c);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    /* falta com tratador instalado (SYS_SIG): vira sinal da thread, como no kernel */
    if (c->Rip >= USER_BASE && c->Rip < USER_END) {
        int s = 0, sc = 0;
        u64 fa = 0;
        u32 ferr = 0;
        if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) {
            s = MOS_SIGSEGV; sc = MOS_SEGV_MAPERR;
            fa = ep->ExceptionRecord->NumberParameters > 1 ? (u64)ep->ExceptionRecord->ExceptionInformation[1] : 0;
            u64 kind = ep->ExceptionRecord->NumberParameters > 0 ? (u64)ep->ExceptionRecord->ExceptionInformation[0] : 0;
            ferr = kind == 1 ? 2 : kind == 8 ? 0x10 : 0;   /* #PF: escrita / busca de instrução */
        } else if (code == EXCEPTION_ILLEGAL_INSTRUCTION) { s = MOS_SIGILL; sc = MOS_ILL_ILLOPC; fa = c->Rip; }
        else if (code == EXCEPTION_BREAKPOINT) { s = MOS_SIGTRAP; sc = MOS_TRAP_BRKPT; fa = c->Rip; }
        else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) { s = MOS_SIGFPE; sc = MOS_FPE_INTDIV; fa = c->Rip; }
        /* só para o runtime arm64 (ou com MACHOTEST_SIGNALS=1): apps x86 com crashpad sumiriam em silêncio */
        bool rt = sig_act[s].handler >= A64RT_BASE && sig_act[s].handler < A64RT_BASE + A64RT_SPAN_MAX;
        if (s && (rt || getenv("MACHOTEST_SIGNALS")) && sig_act[s].handler > MOS_SIG_IGN && !(sig_mask_t & (1u << s))) {
            sig_deliver_ctx(c, s, sc, fa, ferr);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (s && sig_act[s].handler == MOS_SIG_DFL && getenv("MACHOTEST_SIGDIE")) sig_die(s);
    }
    /* rdgsbase/wrgsbase (F3 REX.W 0F AE /1 e /3): o Windows nÃ£o deixa o cÃ³digo de usuÃ¡rio mudar o GS;
     * guarda um valor por processo (o harness tem uma thread sÃ³) */
    if (code == EXCEPTION_ILLEGAL_INSTRUCTION && c->Rip >= USER_BASE && c->Rip < USER_END &&
        ip[0] == 0xF3 && (ip[1] & 0xF8) == 0x48 && ip[2] == 0x0F && ip[3] == 0xAE && (ip[4] & 0xC0) == 0xC0) {
        u64 fake_gs = my_gs;
        int reg = (ip[4] >> 3) & 7, rm = (ip[4] & 7) | ((ip[1] & 1) << 3);
        DWORD64 *regs[16] = {&c->Rax, &c->Rcx, &c->Rdx, &c->Rbx, &c->Rsp, &c->Rbp, &c->Rsi, &c->Rdi,
                             &c->R8, &c->R9, &c->R10, &c->R11, &c->R12, &c->R13, &c->R14, &c->R15};
        if (reg == 1) { *regs[rm] = fake_gs; c->Rip += 5; return EXCEPTION_CONTINUE_EXECUTION; }
        if (reg == 3) { my_gs = *regs[rm]; c->Rip += 5; return EXCEPTION_CONTINUE_EXECUTION; }
    }
    if (c->Rip >= USER_BASE && c->Rip < USER_END) {
        fprintf(stderr, "[machotest] exceÃ§Ã£o 0x%lx em rip=%llx (endereÃ§o %llx)\n", code, c->Rip,
                ep->ExceptionRecord->NumberParameters > 1 ? (u64)ep->ExceptionRecord->ExceptionInformation[1] : 0);
        /* pilha: endereÃ§os dentro do processo (libSystem simbolizado; imagens Mach-O como base+offset) */
        extern const char *macho_host_symbolize(u64 addr, u64 *off);
        u64 off;
        const char *fn = macho_host_symbolize(c->Rip, &off);
        if (fn && c->Rip < USER_BASE + 0x01000000ULL) fprintf(stderr, "  em libSystem: %s+0x%llx\n", fn, off);
        fprintf(stderr, "  rdi=%llx rsi=%llx rdx=%llx rax=%llx rbx=%llx\n", c->Rdi, c->Rsi, c->Rdx, c->Rax, c->Rbx);
        const u64 *sp = (const u64 *)c->Rsp;
        int shown = 0;
        for (int i = 0; i < 4096 && shown < 14 && (u64)(sp + i) < USER_STACK_TOP; i++) {
            u64 v = sp[i];
            if (v < USER_BASE + 0x100000ULL || v >= USER_BASE + 0x40000000ULL) continue;
            fn = v < USER_BASE + 0x01000000ULL ? macho_host_symbolize(v, &off) : NULL;
            if (fn) fprintf(stderr, "  [sp+%d] libSystem %s+0x%llx\n", i * 8, fn, off);
            else fprintf(stderr, "  [sp+%d] imagem %llx\n", i * 8, v);
            shown++;
        }
        if (my_tid != 1) { fflush(stdout); fprintf(stderr, "[machotest] falha na thread %lld; encerrando\n", my_tid); ExitProcess((UINT)-11); }
        c->Rax = (u32)-11;
        c->Rip = (DWORD64)host_exit;
        c->Rsp = host_saved_rsp;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* MACHOTEST_WATCHDOG=<ms>: se o programa ainda roda depois desse tempo, congela a thread principal e
 * mostra onde ela está (rip e pilha, com o libSystem simbolizado), 3 amostras, e encerra. Para travamentos. */
static HANDLE g_main_thread;
static DWORD WINAPI watchdog(LPVOID arg) {
    DWORD ms = (DWORD)(uintptr_t)arg;
    Sleep(ms);
    extern const char *macho_host_symbolize(u64 addr, u64 *off);
    for (int k = 0; k < 3; k++) {
        SuspendThread(g_main_thread);
        CONTEXT c = {0};
        c.ContextFlags = CONTEXT_FULL;
        GetThreadContext(g_main_thread, &c);
        u64 off;
        const char *fn = c.Rip < USER_BASE + 0x01000000ULL ? macho_host_symbolize(c.Rip, &off) : NULL;
        fprintf(stderr, "[watchdog] amostra %d: rip=%llx %s+0x%llx (última syscall %llu a=%llx b=%llx)\n", k, c.Rip,
                fn ? fn : "(imagem)", fn ? off : 0, g_last_sys[0], g_last_sys[1], g_last_sys[2]);
        const u64 *sp = (const u64 *)c.Rsp;
        int shown = 0;
        for (int i = 0; i < 8192 && shown < 16 && (u64)(sp + i) < USER_STACK_TOP && c.Rsp >= USER_BASE; i++) {
            u64 v = sp[i];
            if (v < USER_BASE + 0x100000ULL || v >= USER_BASE + 0x40000000ULL) continue;
            fn = v < USER_BASE + 0x01000000ULL ? macho_host_symbolize(v, &off) : NULL;
            if (fn) fprintf(stderr, "  [sp+%d] libSystem %s+0x%llx\n", i * 8, fn, off);
            else fprintf(stderr, "  [sp+%d] imagem %llx\n", i * 8, v);
            shown++;
        }
        ResumeThread(g_main_thread);
        Sleep(300);
    }
    fflush(stdout);
    ExitProcess(124);
    return 0;
}
#else

/* ---- Linux: int 0x80 vira SIGSYS (seccomp AUDIT_ARCH_I386). O RIP já aponta depois da instrução.
 * Faltas do guest viram o quadro de sinal do mOS, como a VEH do Windows. */
static mos_sigaction_t sig_act[MOS_NSIG];
static __thread u32 sig_mask_t;
static volatile unsigned sig_pend[256];
static int sig_default_kills(int s) {
    return !(s == MOS_SIGCHLD || s == MOS_SIGURG || s == MOS_SIGWINCH || s == MOS_SIGINFO || s == MOS_SIGCONT || s == MOS_SIGIO);
}
typedef struct { mos_siginfo_t info; mos_ucontext_t uc; u64 pad; mos_mcontext_t mc; } host_sigframe_t;
static void lx_to_ss(ucontext_t *uc, mos_thread_state_t *s) {
    greg_t *g = uc->uc_mcontext.gregs;
    s->rax = (u64)g[REG_RAX]; s->rbx = (u64)g[REG_RBX]; s->rcx = (u64)g[REG_RCX]; s->rdx = (u64)g[REG_RDX];
    s->rdi = (u64)g[REG_RDI]; s->rsi = (u64)g[REG_RSI]; s->rbp = (u64)g[REG_RBP]; s->rsp = (u64)g[REG_RSP];
    s->r8 = (u64)g[REG_R8]; s->r9 = (u64)g[REG_R9]; s->r10 = (u64)g[REG_R10]; s->r11 = (u64)g[REG_R11];
    s->r12 = (u64)g[REG_R12]; s->r13 = (u64)g[REG_R13]; s->r14 = (u64)g[REG_R14]; s->r15 = (u64)g[REG_R15];
    s->rip = (u64)g[REG_RIP]; s->rflags = (u64)g[REG_EFL]; s->cs = 0x1B;
}
static void ss_to_lx(const mos_thread_state_t *s, ucontext_t *uc) {
    greg_t *g = uc->uc_mcontext.gregs;
    g[REG_RAX] = (greg_t)s->rax; g[REG_RBX] = (greg_t)s->rbx; g[REG_RCX] = (greg_t)s->rcx; g[REG_RDX] = (greg_t)s->rdx;
    g[REG_RDI] = (greg_t)s->rdi; g[REG_RSI] = (greg_t)s->rsi; g[REG_RBP] = (greg_t)s->rbp; g[REG_RSP] = (greg_t)s->rsp;
    g[REG_R8] = (greg_t)s->r8; g[REG_R9] = (greg_t)s->r9; g[REG_R10] = (greg_t)s->r10; g[REG_R11] = (greg_t)s->r11;
    g[REG_R12] = (greg_t)s->r12; g[REG_R13] = (greg_t)s->r13; g[REG_R14] = (greg_t)s->r14; g[REG_R15] = (greg_t)s->r15;
    g[REG_RIP] = (greg_t)s->rip; g[REG_EFL] = (greg_t)((g[REG_EFL] & ~0xDD5u) | (s->rflags & 0xDD5u));
}
static void sig_die(int s) {
    fflush(stdout);
    fprintf(stderr, "[machotest] encerrado pelo sinal %d (thread %lld)\n", s, my_tid);
    _exit(128 + s);
}
static int sig_deliver_uc(ucontext_t *uc, int s, int code, u64 addr, u32 err) {
    greg_t *g = uc->uc_mcontext.gregs;
    mos_sigaction_t sa = sig_act[s];
    if (sa.handler == MOS_SIG_IGN) return 1;
    if (sa.handler == MOS_SIG_DFL || (sig_mask_t & (1u << s))) { if (sig_default_kills(s)) sig_die(s); return 1; }
    u64 fa = ((u64)g[REG_RSP] - 128 - sizeof(host_sigframe_t)) & ~15ULL;
    host_sigframe_t *f = (host_sigframe_t *)fa;
    memset(f, 0, sizeof *f);
    f->info.si_signo = s; f->info.si_code = code; f->info.si_addr = addr; f->info.si_pid = 42; f->info.si_uid = 501;
    f->uc.uc_sigmask = sig_mask_t;
    f->uc.uc_stack.flags = MOS_SS_DISABLE;
    f->uc.uc_mcsize = sizeof(mos_mcontext_t);
    f->uc.uc_mcontext = (u64)&f->mc;
    f->mc.err = err; f->mc.faultvaddr = addr; f->mc.trapno = s == MOS_SIGSEGV ? 14 : 0;
    lx_to_ss(uc, &f->mc.ss);
    if (uc->uc_mcontext.fpregs) memcpy(f->mc.fx, uc->uc_mcontext.fpregs, 512);
    sig_mask_t |= sa.mask | ((sa.flags & MOS_SA_NODEFER) ? 0 : (1u << s));
    g[REG_RSP] = (greg_t)(fa - 8);
    if (sa.tramp) {
        g[REG_RIP] = (greg_t)sa.tramp; g[REG_RDI] = (greg_t)sa.handler;
        g[REG_RSI] = (sa.flags & MOS_SA_SIGINFO) ? 30 : 1; g[REG_RDX] = (greg_t)s;
        g[REG_RCX] = (greg_t)&f->info; g[REG_R8] = (greg_t)&f->uc;
    } else {
        g[REG_RIP] = (greg_t)sa.handler; g[REG_RDI] = (greg_t)s; g[REG_RSI] = (greg_t)&f->info; g[REG_RDX] = (greg_t)&f->uc;
    }
    *(u64 *)(u64)g[REG_RSP] = 0;
    return 1;
}
static void sig_check_uc(ucontext_t *uc) {
    if (my_tid < 0 || my_tid >= 256) return;
    u32 p = sig_pend[my_tid] & ~sig_mask_t;
    if (!p) return;
    int s = __builtin_ctz(p);
    __atomic_and_fetch((unsigned *)&sig_pend[my_tid], ~(1u << s), __ATOMIC_SEQ_CST);
    sig_deliver_uc(uc, s, MOS_SI_USER, 0, 0);
}
static int sig_syscall_uc(ucontext_t *uc) {
    greg_t *g = uc->uc_mcontext.gregs;
    u64 op = (u64)g[REG_RDI], a = (u64)g[REG_RSI], b = (u64)g[REG_RDX], d = (u64)g[REG_R10];
    long long r = 0;
    switch (op) {
    case MOS_SIGOP_ACTION:
        if (a == 0 || a >= MOS_NSIG) { r = -MOS_EINVAL; break; }
        if (d) memcpy((void *)d, &sig_act[a], sizeof(mos_sigaction_t));
        if (b) memcpy(&sig_act[a], (const void *)b, sizeof(mos_sigaction_t));
        break;
    case MOS_SIGOP_MASK: {
        u32 old = sig_mask_t, set = b ? *(const u32 *)b : 0;
        if (b) {
            if (a == MOS_SIG_BLOCK) sig_mask_t |= set; else if (a == MOS_SIG_UNBLOCK) sig_mask_t &= ~set;
            else if (a == MOS_SIG_SETMASK) sig_mask_t = set; else { r = -MOS_EINVAL; break; }
            sig_mask_t &= ~((1u << MOS_SIGKILL) | (1u << MOS_SIGSTOP));
        }
        if (d) *(u32 *)d = old;
        break;
    }
    case MOS_SIGOP_PENDING: *(u32 *)a = my_tid < 256 ? sig_pend[my_tid] : 0; break;
    case MOS_SIGOP_ALTSTACK: if (b) memset((void *)b, 0, sizeof(mos_sigstack_t)), ((mos_sigstack_t *)b)->flags = MOS_SS_DISABLE; break;
    case MOS_SIGOP_RETURN: {
        const mos_ucontext_t *u = (const mos_ucontext_t *)a;
        const mos_mcontext_t *mc = (const mos_mcontext_t *)u->uc_mcontext;
        ss_to_lx(&mc->ss, uc);
        if (uc->uc_mcontext.fpregs) memcpy(uc->uc_mcontext.fpregs, mc->fx, 512);
        sig_mask_t = u->uc_sigmask & ~((1u << MOS_SIGKILL) | (1u << MOS_SIGSTOP));
        sig_check_uc(uc);
        return 1;
    }
    case MOS_SIGOP_TKILL:
        if (b == 0) break;
        if (b >= MOS_NSIG || a >= 256) { r = -MOS_EINVAL; break; }
        __atomic_or_fetch((unsigned *)&sig_pend[a], 1u << b, __ATOMIC_SEQ_CST);
        if ((long long)a == my_tid) sig_check_uc(uc);
        break;
    case MOS_SIGOP_SUSPEND: r = -MOS_EINTR; break;
    case MOS_SIGOP_WAIT: r = -MOS_EAGAIN; break;
    default: r = -MOS_EINVAL;
    }
    g[REG_RAX] = (greg_t)r;
    return 1;
}

static int prot_spin;
static struct { u64 page; u8 prot; int used; } ph[65536];
static void prot_lock(void) { while (__atomic_exchange_n(&prot_spin, 1, __ATOMIC_ACQUIRE)) {} }
static void prot_unlock(void) { __atomic_store_n(&prot_spin, 0, __ATOMIC_RELEASE); }
static u8 prot_get(u64 page) {
    unsigned i = (unsigned)((page / 4096) & 65535u);
    prot_lock();
    u8 r = 7;
    for (int n = 0; n < 48; n++) {
        unsigned j = (i + (unsigned)n) & 65535u;
        if (!ph[j].used) break;
        if (ph[j].page == page) { r = ph[j].prot; break; }
    }
    prot_unlock();
    return r;
}
static void prot_set(u64 page, u8 prot) {
    unsigned i = (unsigned)((page / 4096) & 65535u);
    prot_lock();
    for (int n = 0; n < 48; n++) {
        unsigned j = (i + (unsigned)n) & 65535u;
        if (!ph[j].used || ph[j].page == page) { ph[j].used = 1; ph[j].page = page; ph[j].prot = prot; break; }
    }
    prot_unlock();
}
static int jit_wprotect(u64 *a) {
    u64 pg = a[0] & ~0xFFFULL;
    int cur = prot_get(pg);
    int has_w = (cur & 2) != 0;
    a[2] = 0;
    if (a[1] == 2) { a[2] = has_w; return 1; }
    if (a[1] == 1) {
        if (!has_w) return 1;
        int np = (cur & 4) ? (PROT_READ | PROT_EXEC) : PROT_READ;
        if (mprotect((void *)pg, 4096, np) == 0) { prot_set(pg, (u8)(cur & ~2)); a[2] = 1; }
        return 1;
    }
    a[2] = 1;
    if (!(cur & 2)) {
        int np = (cur & 4) ? (PROT_READ | PROT_WRITE | PROT_EXEC) : (PROT_READ | PROT_WRITE);
        if (mprotect((void *)pg, 4096, np) == 0) prot_set(pg, (u8)(cur | 2));
    }
    return 1;
}

static void lx_leave(ucontext_t *uc, long long code) {
    greg_t *g = uc->uc_mcontext.gregs;
    g[REG_RAX] = (greg_t)code;
    g[REG_RIP] = (greg_t)(u64)host_exit;
    g[REG_RSP] = (greg_t)host_saved_rsp;
}
static void on_guest_signal(int sig, siginfo_t *si, void *v) {
    ucontext_t *uc = v;
    greg_t *g = uc->uc_mcontext.gregs;
    if (in_sig) _exit(139);
    in_sig = 1;
    if (sig == SIGUSR1) { g_guest_rip = (u64)g[REG_RIP]; in_sig = 0; return; }
    if (sig == SIGSYS) {
        if (my_tid == 1) g_guest_rip = (u64)g[REG_RIP];
        if (g[REG_RAX] == SYS_EXIT && my_tid != 1) {
            fflush(stdout);
            fprintf(stderr, "[machotest] código de saída: %d (thread %lld)\n", (int)g[REG_RDI], my_tid);
            _exit((unsigned)(int)g[REG_RDI] & 255u);
        }
        if (g[REG_RAX] == SYS_EXIT) { lx_leave(uc, (long long)(int)g[REG_RDI]); in_sig = 0; return; }
        if (g[REG_RAX] == SYS_SIG && sig_syscall_uc(uc)) { in_sig = 0; return; }
        if (g[REG_RAX] == SYS_MACH_VM && g[REG_RDI] == MACH_VM_JIT_WPROTECT && !getenv("MACHOTEST_NO_WP")) {
            jit_wprotect((u64 *)g[REG_RSI]);
            g[REG_RAX] = 0;
            in_sig = 0;
            return;
        }
        g_r8 = (u64)g[REG_R8];
        g[REG_RAX] = (greg_t)do_syscall((u64)g[REG_RAX], (u64)g[REG_RDI], (u64)g[REG_RSI], (u64)g[REG_RDX], (u64)g[REG_R10]);
        sig_check_uc(uc);
        in_sig = 0;
        return;
    }
    u64 rip = (u64)g[REG_RIP];
    if (sig == SIGILL && rip >= USER_BASE && rip < USER_END) {
        const u8 *ip = (const u8 *)rip;
        if (ip[0] == 0xF3 && (ip[1] & 0xF8) == 0x48 && ip[2] == 0x0F && ip[3] == 0xAE && (ip[4] & 0xC0) == 0xC0) {
            int reg = (ip[4] >> 3) & 7, rm = (ip[4] & 7) | ((ip[1] & 1) << 3);
            int map[16] = { REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
                            REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15 };
            if (reg == 1) { g[map[rm]] = (greg_t)my_gs; g[REG_RIP] += 5; in_sig = 0; return; }
            if (reg == 3) { my_gs = (u64)g[map[rm]]; g[REG_RIP] += 5; in_sig = 0; return; }
        }
    }
    if (rip < USER_BASE || rip >= USER_END) {
        fprintf(stderr, "[machotest] sinal %d no host rip=%llx\n", sig, (unsigned long long)rip);
        _exit(139);
    }
    int s = 0, sc = 0;
    u64 fa = 0;
    u32 ferr = 0;
    if (sig == SIGSEGV || sig == SIGBUS) { s = MOS_SIGSEGV; sc = MOS_SEGV_MAPERR; fa = (u64)si->_sifields._sigfault.si_addr; ferr = si->si_code == SEGV_ACCERR ? 2 : 0; }
    else if (sig == SIGILL) { s = MOS_SIGILL; sc = MOS_ILL_ILLOPC; fa = rip; }
    else if (sig == SIGTRAP) { s = MOS_SIGTRAP; sc = MOS_TRAP_BRKPT; fa = rip; }
    else if (sig == SIGFPE) { s = MOS_SIGFPE; sc = MOS_FPE_INTDIV; fa = rip; }
    bool rt = s && sig_act[s].handler >= A64RT_BASE && sig_act[s].handler < A64RT_BASE + A64RT_SPAN_MAX;
    if (s && (rt || getenv("MACHOTEST_SIGNALS")) && sig_act[s].handler > MOS_SIG_IGN && !(sig_mask_t & (1u << s))) {
        sig_deliver_uc(uc, s, sc, fa, ferr);
        in_sig = 0;
        return;
    }
    if (s && sig_act[s].handler == MOS_SIG_DFL && getenv("MACHOTEST_SIGDIE")) sig_die(s);
    fprintf(stderr, "[machotest] exceção sinal %d em rip=%llx (endereço %llx)\n", sig, (unsigned long long)rip, (unsigned long long)fa);
    extern const char *macho_host_symbolize(u64 addr, u64 *off);
    u64 off;
    const char *fn = macho_host_symbolize(rip, &off);
    if (fn && rip < USER_BASE + 0x01000000ULL) fprintf(stderr, "  em libSystem: %s+0x%llx\n", fn, (unsigned long long)off);
    fprintf(stderr, "  rdi=%llx rsi=%llx rdx=%llx rax=%llx rbx=%llx\n",
            (unsigned long long)g[REG_RDI], (unsigned long long)g[REG_RSI], (unsigned long long)g[REG_RDX],
            (unsigned long long)g[REG_RAX], (unsigned long long)g[REG_RBX]);
    if (my_tid != 1) { fflush(stdout); fprintf(stderr, "[machotest] falha na thread %lld; encerrando\n", my_tid); _exit(245); }
    lx_leave(uc, -11);
    in_sig = 0;
}

static pthread_t g_main_thread;
static int g_watch_ms;
static void *watchdog(void *arg) {
    (void)arg;
    struct timespec ts = { g_watch_ms / 1000, (long)(g_watch_ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
    extern const char *macho_host_symbolize(u64 addr, u64 *off);
    for (int k = 0; k < 3; k++) {
        u64 rip = g_guest_rip, off = 0;
        const char *fn = rip && rip < USER_BASE + 0x01000000ULL ? macho_host_symbolize(rip, &off) : NULL;
        fprintf(stderr, "[watchdog] amostra %d: rip=%llx %s+0x%llx (última syscall %llu a=%llx b=%llx)\n",
                k, (unsigned long long)rip, fn ? fn : "(imagem)", fn ? (unsigned long long)off : 0,
                (unsigned long long)g_last_sys[0], (unsigned long long)g_last_sys[1], (unsigned long long)g_last_sys[2]);
        fflush(stderr);
        struct timespec p = { 0, 300000000L };
        nanosleep(&p, NULL);
    }
    fflush(stdout);
    _exit(124);
    return NULL;
}
static int host_linux_init(void) {
    host_cv_init();
    host_linux_altstack();
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_guest_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSYS, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP, SIGUSR1 };
    for (unsigned i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
        if (sigaction(sigs[i], &sa, NULL) != 0) { perror("sigaction"); return -1; }
    signal(SIGPIPE, SIG_IGN);
    struct sock_filter f[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (unsigned)offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_I386, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog prog = { .len = 4, .filter = f };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        perror("seccomp");
        fprintf(stderr, "[machotest] sem o filtro seccomp o int 0x80 vira syscall do Linux; não dá para rodar o guest\n");
        return -1;
    }
    g_main_thread = pthread_self();
    if (getenv("MACHOTEST_WATCHDOG")) {
        g_watch_ms = atoi(getenv("MACHOTEST_WATCHDOG"));
        pthread_t th;
        pthread_create(&th, NULL, watchdog, NULL);
    }
    return 0;
}
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    if (getenv("MACHOTEST_WATCHDOG")) CreateThread(NULL, 0, watchdog, (LPVOID)(uintptr_t)atoi(getenv("MACHOTEST_WATCHDOG")), 0, NULL);
    InitializeCriticalSection(&sys_lock);
#else
    if (host_linux_init() != 0) return 2;
#endif
    if (argc >= 4 && !strcmp(argv[1], "--report")) {
        usize ll, ml;
        u8 *ls = read_file(argv[2], &ll), *m = read_file(argv[3], &ml);
        if (!ls || !m) { fprintf(stderr, "não foi possível ler os arquivos\n"); return 2; }
        libsystem_mos = ls; libsystem_mos_end = ls + ll;
        static char out[8192];
        bool ok = macho_report_image(m, ml, argv[3], out, sizeof out);
        fputs(out, stdout);
        return ok ? 0 : 1;
    }
    if (argc < 4) { fprintf(stderr, "uso: machotest <libSystem.mos> <raiz> <mach-o> [args]\n"); return 2; }
    usize ll, ml;
    u8 *ls = read_file(argv[1], &ll), *m = read_file(argv[3], &ml);
    if (!ls || !m) { fprintf(stderr, "não foi possível ler os arquivos\n"); return 2; }
    libsystem_mos = ls; libsystem_mos_end = ls + ll;
    g_root = argv[2];
    const char *base = strrchr(argv[3], '/');
    if (!base) base = strrchr(argv[3], '\\');
    snprintf(g_proc.path, sizeof g_proc.path, "/Users/Visitante/%s", base ? base + 1 : argv[3]);
    if (getenv("MACHOTEST_PATH")) snprintf(g_proc.path, sizeof g_proc.path, "%s", getenv("MACHOTEST_PATH"));
    char arg[2048] = "";
    for (int i = 4; i < argc; i++) { strcat(arg, i > 4 ? " \"" : "\""); strcat(arg, argv[i]); strcat(arg, "\""); }
    u64 entry, rsp, rdi;
    char err[256];
    if (!macho_load(&g_proc, m, ml, arg, &entry, &rsp, &rdi, err, sizeof err)) { fprintf(stderr, "macho_load: %s\n", err); return 3; }
    script = getenv("MACHOTEST_EVENTS");
    brk_cur = g_proc.brk;
    brk_limit = brk_cur + (512ull << 20);
#ifdef _WIN32
    AddVectoredExceptionHandler(1, veh);
#endif
    if (getenv("MACHOTEST_DEBUG")) fprintf(stderr, "[machotest] entry=%llx rsp=%llx rdi=%llx brk=%llx\n",
        (unsigned long long)entry, (unsigned long long)rsp, (unsigned long long)rdi, (unsigned long long)brk_cur);
    int code = 0;
    {
        int (*enter)(u64, u64, u64) = (int (*)(u64, u64, u64))host_enter;
        code = enter(entry, rdi, rsp);
    }
    fflush(stdout);
    fprintf(stderr, "[machotest] código de saída: %d\n", code);
    return code;
}
