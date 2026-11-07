/*
 * mOS â€” teste no Windows do carregador Mach-O + libSystem.mos, sem QEMU.
 *   machotest <libSystem.mos> <raiz-do-vfs> <executÃ¡vel Mach-O> [args...]
 *   machotest --report <libSystem.mos> <executÃ¡vel>
 * O processo Ã© montado nos mesmos endereÃ§os do mOS; cada "int 0x80" vira uma exceÃ§Ã£o que este
 * programa trata como syscall do mOS (kernel/include/syscall.h), sobre a pasta <raiz-do-vfs>.
 */
#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#include <time.h>
#include "compat.h"
#undef SYS_OPEN
#include "../kernel/include/syscall.h"
#include "../kernel/include/compat_gui.h"

long long g_live_allocs, g_live_bytes;
extern const u8 *libsystem_mos, *libsystem_mos_end;
extern void host_enter(u64 entry, u64 rdi, u64 rsp);
extern void host_exit(void);
extern u64 host_saved_rsp;

static const char *g_root;
static process_t g_proc;
static u64 brk_cur, brk_limit;
static u64 mmap_next = 0x400100000000ULL;

bool uvm_map(process_t *p, u64 va, u64 len) {
    u64 lo = va & ~0xFFFFULL, hi = (va + len + 0xFFFF) & ~0xFFFFULL;
    (void)p;
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
}

bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len) { (void)p; memcpy((void *)va, src, len); return true; }
bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len) { (void)p; memcpy(dst, (const void *)va, len); return true; }
i64 macho_dyld_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c);   /* dlopen (kernel/compat/macho.c) */

static u8 *read_file(const char *path, usize *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    _fseeki64(f, 0, SEEK_END);
    long long n = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    u8 *b = malloc(n ? n : 1);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (usize)n;
    return b;
}

/* dependÃªncias (dylibs do bundle) pedidas pelo carregador: caminho do mOS sob g_root, ou caminho
 * do host no --report. Ficam na memÃ³ria atÃ© o fim (o carregador guarda ponteiros para elas). */
const u8 *host_read_file(const char *path, usize *len) {
    char hp[1024];
    if (g_root) snprintf(hp, sizeof hp, "%s%s", g_root, path);
    else snprintf(hp, sizeof hp, "%s", path);
    for (char *c = hp; *c; c++) if (*c == '/') *c = '\\';
    return read_file(hp, len);
}

/* ---- syscalls do mOS sobre a pasta g_root */
static void host_path(u64 upath, char *out, size_t cap) {
    snprintf(out, cap, "%s%s", g_root, (const char *)upath);
    for (char *c = out; *c; c++) if (*c == '/') *c = '\\';
}

#define MAXFD 32
static FILE *fds[MAXFD];

/* ---- GUI emulada: imprime o conteÃºdo dos quadros e reproduz um roteiro de eventos
 * (MACHOTEST_EVENTS="f:3;down:265,75;up:265,75;key:a;text:abc;close") */
static int gw, gh, frames, poll_idle, nwin, curwin = 1;
static const char *script;
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
        else if (!strncmp(tok, "key:", 4)) { e->type = GEV_KEY_DOWN; e->key = !strcmp(tok + 4, "ret") ? 10 : !strcmp(tok + 4, "bs") ? 8 : tok[4]; if (e->key >= 32 && e->key < 127) e->text[0] = (char)e->key; }
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

/* ---- threads: uma thread do Windows por thread do mOS */
extern void host_jump(u64 entry, u64 rdi, u64 rsp);
static __thread long long my_tid = 1;
static __thread u64 my_gs;
static volatile long next_tid = 1;
static HANDLE thread_handles[256];
typedef struct { u64 entry, rsp, arg; long long tid; } thread_start_t;
static DWORD WINAPI thread_main(LPVOID p) {
    thread_start_t s = *(thread_start_t *)p;
    free(p);
    my_tid = s.tid;
    host_jump(s.entry, s.arg, s.rsp);
    return 0;
}
static CRITICAL_SECTION sys_lock;

static long long do_syscall_locked(u64 n, u64 a, u64 b, u64 c, u64 d);
static long long do_syscall(u64 n, u64 a, u64 b, u64 c, u64 d) {
    switch (n) {
    case SYS_SLEEP: Sleep((DWORD)a); return 0;
    case SYS_YIELD: SwitchToThread(); return 0;
    case SYS_GETTID: return my_tid;
    case SYS_THREAD_CREATE: {
        long long tid = InterlockedIncrement(&next_tid);
        if (tid >= 256) return -MOS_ENOMEM;
        thread_start_t *s = malloc(sizeof *s);
        s->entry = a; s->rsp = b; s->arg = c; s->tid = tid;
        HANDLE h = CreateThread(NULL, 1 << 20, thread_main, s, 0, NULL);
        if (!h) { free(s); return -MOS_ENOMEM; }
        thread_handles[tid] = h;
        return tid;
    }
    case SYS_THREAD_EXIT: ExitThread((DWORD)a); return 0;
    case SYS_THREAD_JOIN: {
        if (a >= 256 || !thread_handles[a]) return -MOS_ESRCH;
        WaitForSingleObject(thread_handles[a], INFINITE);
        DWORD code = 0;
        GetExitCodeThread(thread_handles[a], &code);
        return (long long)(int)code;
    }
    }
    EnterCriticalSection(&sys_lock);
    long long r = do_syscall_locked(n, a, b, c, d);
    LeaveCriticalSection(&sys_lock);
    return r;
}

static long long do_syscall_locked(u64 n, u64 a, u64 b, u64 c, u64 d) {
    if (n >= 64 && n <= 79) return gui_syscall(n, a, b, c, d);
    char hp[1024];
    (void)d;
    switch (n) {
    case SYS_WRITE:
        if (a == 1 || a == 2) { fwrite((void *)b, 1, c, a == 1 ? stdout : stderr); fflush(a == 1 ? stdout : stderr); return (long long)c; }
        if (a < MAXFD && fds[a]) return (long long)fwrite((void *)b, 1, c, fds[a]);
        return -MOS_EBADF;
    case SYS_READ:
        if (a == 0) return 0;
        if (a < MAXFD && fds[a]) return (long long)fread((void *)b, 1, c, fds[a]);
        return -MOS_EBADF;
    case SYS_GETPID: return 42;
    case 0x7FFF: {   /* stub de sÃ­mbolo ausente com MACHOTEST_STUB_LOG (a = nome): registra e devolve 0 */
        static unsigned long long calls;
        static char seen[8192];
        const char *nm = (const char *)a;
        calls++;
        if (!strstr(seen, nm) && strlen(seen) + strlen(nm) + 2 < sizeof seen) {
            strcat(seen, nm); strcat(seen, " ");
            fprintf(stderr, "[ausente #%llu] %s\n", calls, nm);
        }
        if (calls > 200000) { fprintf(stderr, "[machotest] chamadas a stubs demais, parando\n"); return -1; }
        return 0;
    }
    case SYS_TIME: return (long long)GetTickCount64();
    case SYS_UNIXTIME: return (long long)time(NULL);
    case SYS_BRK:
        if (!a) return (long long)brk_cur;
        if (a > brk_limit) return (long long)brk_cur;
        if (a > brk_cur && !uvm_map(&g_proc, brk_cur, a - brk_cur)) return (long long)brk_cur;
        brk_cur = a;
        return (long long)brk_cur;
    case SYS_MMAP: {
        /* a mesma regiÃ£o que o dlopen usa (g_proc.mmap_next, compartilhado com macho.c) */
        if (!g_proc.mmap_next) g_proc.mmap_next = mmap_next;
        if (b == 2 || (b == 3 && c)) return uvm_map(&g_proc, c, a) ? 0 : -MOS_ENOMEM;   /* aloca trecho de uma reserva (b = 3: sob demanda no kernel; aqui, na hora) */
        u64 len = (a + 0xFFFF) & ~0xFFFFULL, at = (g_proc.mmap_next + 0xFFFF) & ~0xFFFFULL;
        if (b != 1 && !uvm_map(&g_proc, at, len)) return -MOS_ENOMEM;   /* b = 1: sÃ³ reserva endereÃ§os */
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
        else if (b & (MOS_O_WRONLY | MOS_O_RDWR)) { mode = "r+b"; if ((b & MOS_O_CREAT) && _access(hp, 0)) mode = "w+b"; }
        int fd = 3;
        while (fd < MAXFD && fds[fd]) fd++;
        if (fd == MAXFD) return -MOS_EMFILE;
        fds[fd] = fopen(hp, mode);
        if (getenv("MACHOTEST_TRACE_FS")) fprintf(stderr, "[fs] open %s -> %s\n", (const char *)a, fds[fd] ? "ok" : "nÃ£o existe");
        return fds[fd] ? fd : -MOS_ENOENT;
    }
    case SYS_CLOSE: if (a < MAXFD && fds[a]) { fclose(fds[a]); fds[a] = NULL; return 0; } return -MOS_EBADF;
    case SYS_LSEEK: if (a < MAXFD && fds[a]) { _fseeki64(fds[a], (long long)b, (int)c); return _ftelli64(fds[a]); } return -MOS_EBADF;
    case SYS_FSTAT: case SYS_STAT: {
        struct _stat64 s;
        int r;
        if (n == SYS_FSTAT) { if (a >= MAXFD || !fds[a]) return -MOS_EBADF; r = _fstat64(_fileno(fds[a]), &s); }
        else { host_path(a, hp, sizeof hp); r = _stat64(hp, &s); }
        if (getenv("MACHOTEST_TRACE_FS") && n == SYS_STAT) fprintf(stderr, "[fs] stat %s -> %s\n", (const char *)a, r ? "nÃ£o existe" : "ok");
        if (r) return -MOS_ENOENT;
        mos_stat_t *m = (mos_stat_t *)b;
        m->size = (unsigned long long)s.st_size;
        m->is_dir = (s.st_mode & _S_IFDIR) != 0;
        m->mtime = s.st_mtime;
        return 0;
    }
    case SYS_READDIR: {
        host_path(a, hp, sizeof hp);
        strcat(hp, "\\*");
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(hp, &fd);
        if (h == INVALID_HANDLE_VALUE) return -MOS_ENOENT;
        u64 i = 0;
        do {
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            if (i++ == b) { snprintf((char *)c, d, "%s", fd.cFileName); FindClose(h); return 1; }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        return 0;
    }
    case SYS_MKDIR: host_path(a, hp, sizeof hp); return _mkdir(hp) ? -MOS_EEXIST : 0;
    case SYS_UNLINK: host_path(a, hp, sizeof hp); return remove(hp) ? -MOS_ENOENT : 0;
    case SYS_RENAME: { char hp2[1024]; host_path(a, hp, sizeof hp); host_path(b, hp2, sizeof hp2); return rename(hp, hp2) ? -MOS_ENOENT : 0; }
    }
    fprintf(stderr, "[machotest] syscall %llu nÃ£o emulada\n", n);
    return -MOS_EINVAL;
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
        c->Rax = (DWORD64)do_syscall(c->Rax, c->Rdi, c->Rsi, c->Rdx, c->R10);
        c->Rip += 2;
        return EXCEPTION_CONTINUE_EXECUTION;
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

int main(int argc, char **argv) {
    SetConsoleOutputCP(CP_UTF8);
    InitializeCriticalSection(&sys_lock);
    if (argc >= 4 && !strcmp(argv[1], "--report")) {
        usize ll, ml;
        u8 *ls = read_file(argv[2], &ll), *m = read_file(argv[3], &ml);
        if (!ls || !m) { fprintf(stderr, "nÃ£o foi possÃ­vel ler os arquivos\n"); return 2; }
        libsystem_mos = ls; libsystem_mos_end = ls + ll;
        static char out[8192];
        bool ok = macho_report_image(m, ml, argv[3], out, sizeof out);
        fputs(out, stdout);
        return ok ? 0 : 1;
    }
    if (argc < 4) { fprintf(stderr, "uso: machotest <libSystem.mos> <raiz> <mach-o> [args]\n"); return 2; }
    usize ll, ml;
    u8 *ls = read_file(argv[1], &ll), *m = read_file(argv[3], &ml);
    if (!ls || !m) { fprintf(stderr, "nÃ£o foi possÃ­vel ler os arquivos\n"); return 2; }
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
    AddVectoredExceptionHandler(1, veh);
    if (getenv("MACHOTEST_DEBUG")) fprintf(stderr, "[machotest] entry=%llx rsp=%llx rdi=%llx brk=%llx\n", entry, rsp, rdi, brk_cur);
    int code = 0;
    {
        int (*enter)(u64, u64, u64) = (int (*)(u64, u64, u64))host_enter;
        code = enter(entry, rdi, rsp);
    }
    fflush(stdout);
    fprintf(stderr, "[machotest] cÃ³digo de saÃ­da: %d\n", code);
    return code;
}
