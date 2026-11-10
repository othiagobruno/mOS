/* mOS — MAP_SHARED, msync, munmap parcial, MAP_SHARED|MAP_ANON e página NX.
 *
 * Programa ELF de ring 3 (userland/mmshared). O autoteste (userland/teste.c) o dispara quando a imagem
 * o traz. Fala com o kernel direto (int 0x80): o mmap do libSystem ainda pode copiar o arquivo no
 * processo, e este teste não pode passar por essa cópia.
 *
 * ABI exercida (kernel/proc/process.c, kernel/mm/lazy.c):
 *   SYS_MMAP b = 5  arquivo MAP_SHARED. c = fd, r10 = deslocamento, r8 = prot (1 leitura, 2 escrita, 4 execução)
 *   SYS_MMAP b = 6  MAP_SHARED|MAP_ANON. r8 = prot
 *   SYS_MMAP b = 7  proteção no PTE de [c, c + len). r8 = prot. Sem o bit 4 a página fica NX.
 *   SYS_MMAP b = 8  MAP_FIXED arquivo privado. b = 9 compartilhado. c = endereço, r10 = deslocamento,
 *                  r8 = fd | (prot << 32). Devolve o endereço pedido com o conteúdo do arquivo.
 *   SYS_MUNMAP c = MOS_MUNMAP_SYNC ('SYNC')  grava o trecho no arquivo e não desmapeia (msync)
 *
 * O que se espera ver no log: uma linha "ok" por checagem e, no fim, "mmshared: TODOS OS TESTES OK".
 */
#include "mos.h"

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

/* constantes novas: o cabeçalho as define quando o kernel já as publicou */
#ifndef MOS_MUNMAP_SYNC
#define MOS_MUNMAP_SYNC 0x53594E43u /* 'SYNC' */
#endif
#define MM_FILE  5u
#define MM_ANON  6u
#define MM_PROT  7u
#define MM_FIXED_PRIV 8u
#define MM_FIXED_SH   9u
#define PROT_R   1u
#define PROT_W   2u
#define PROT_X   4u
#define PROT_RW  (PROT_R | PROT_W)
#define PROT_RWX (PROT_R | PROT_W | PROT_X)

static inline i64 sys5(u64 n, u64 a, u64 b, u64 c, u64 d, u64 e) {
    i64 r;
    register u64 r10 __asm__("r10") = d;
    register u64 r8 __asm__("r8") = e;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8) : "memory", "rcx", "r9", "r11");
    return r;
}

static i64 mmap_fd(u64 len, u64 mode, int fd, u64 off, u64 prot) {
    return sys5(SYS_MMAP, len, mode, (u64)fd, off, prot);
}

/* mov eax, imm32; ret — o imediato fica nos bytes 1..4 */
static void poke_ret(volatile unsigned char *p, unsigned imm) {
    p[0] = 0xB8;
    p[1] = (unsigned char)imm;
    p[2] = (unsigned char)(imm >> 8);
    p[3] = (unsigned char)(imm >> 16);
    p[4] = (unsigned char)(imm >> 24);
    p[5] = 0xC3;
}

static int call_page(void *p) {
    int (*fn)(void) = (int (*)(void))p;
    return fn();
}

/* o tratador recebe o número do sinal em edi (entrega antiga do SYS_SIGACTION) e sai com 100 + sinal */
static void die_sig(int sig) { exit(100 + sig); }

static int child_fault(void (*body)(void)) {
    int fk = fork();
    if (fk == 0) {
        signal(MOS_SIGBUS, die_sig);
        signal(MOS_SIGSEGV, die_sig);
        body();
        exit(2); /* chegou aqui: a falta não veio */
    }
    return fk > 0 ? wait(fk) : fk;
}

static volatile void *fault_page;
static int fault_kind; /* 1 = executar, 2 = escrever */
static void fault_body(void) {
    if (fault_kind == 1) call_page((void *)fault_page);
    else *(volatile unsigned char *)fault_page = 0xEE;
}

static void fill(char *b, usize n, char c) { for (usize i = 0; i < n; i++) b[i] = c; }

void _start(const char *arg) {
    (void)arg;
    print("mmshared: pid %d\n", (i64)getpid());

    /* ---- MAP_SHARED de arquivo: dois mapeamentos, msync parcial, munmap parcial ---- */
    {
        enum { N = 8192 };
        static char img[N], back[N];
        fill(img, N, 'a');
        CHECK(write_file("/tmp/mm-shared.bin", img, N) == 0, "arquivo de teste criado");
        int fd = open("/tmp/mm-shared.bin", MOS_O_RDWR);
        volatile unsigned char *m = (volatile unsigned char *)mmap_fd(N, MM_FILE, fd, 0, PROT_RW);
        int ok = fd >= 3 && (i64)m > 0 && m[0] == 'a' && m[4096] == 'a' && m[N - 1] == 'a';
        CHECK(ok, "MAP_SHARED lê o arquivo (fd %d, mapa %d)", (i64)fd, (i64)m);
        CHECK(mmap_fd(4096, MM_FILE, 99, 0, PROT_RW) == -MOS_EBADF, "MAP_SHARED com fd inválido: EBADF");

        int fk = ok ? fork() : -1;
        if (fk == 0) {
            int fd2 = open("/tmp/mm-shared.bin", MOS_O_RDWR);
            volatile unsigned char *q = (volatile unsigned char *)mmap_fd(N, MM_FILE, fd2, 0, PROT_RW);
            int good = fd2 >= 0 && (i64)q > 0 && q != m && q[0] == 'a';
            if (good) { q[0] = 'B'; q[10] = 'D'; q[4096] = 'C'; }
            exit(good && q[0] == 'B' ? 61 : 1);
        }
        int st = fk > 0 ? wait(fk) : -1;
        int seen = ok && st == 61 && m[0] == 'B' && m[10] == 'D' && m[4096] == 'C';
        CHECK(seen, "escrita do outro mapeamento visível sem msync (filho %d, %u/%u/%u)",
              (i64)st, ok ? m[0] : 0, ok ? m[10] : 0, ok ? m[4096] : 0);

        i64 sy = sys3(SYS_MUNMAP, (u64)m, 4096, MOS_MUNMAP_SYNC);
        i64 rn = 0;
        if (fd >= 0) {
            lseek(fd, 0, MOS_SEEK_SET);
            rn = read(fd, back, N);
        }
        CHECK(sy == 0 && rn == N && back[0] == 'B' && back[10] == 'D' && back[4096] == 'a',
              "msync da 1ª página grava só ela (sync %d, lidos %d, %u/%u)", (i64)sy, rn,
              (unsigned)back[0], (unsigned)back[4096]);

        i64 u2 = sys3(SYS_MUNMAP, (u64)m + 4096, 4096, 0);
        if (fd >= 0) { lseek(fd, 0, MOS_SEEK_SET); rn = read(fd, back, N); }
        int still = ok && m[0] == 'B' && m[10] == 'D';
        CHECK(u2 == 0 && rn == N && back[4096] == 'C' && back[0] == 'B' && still,
              "munmap parcial da 2ª página grava ela e a 1ª segue mapeada (%u)", (unsigned)back[4096]);

        i64 u1 = sys3(SYS_MUNMAP, (u64)m, 4096, 0);
        if (fd >= 0) close(fd);
        CHECK(u1 == 0, "munmap do resto");
        unlink("/tmp/mm-shared.bin");
    }

    /* ---- MAP_PRIVATE continua cópia: escrever no mapa não muda o arquivo ---- */
    {
        static char one[4096], back[16];
        fill(one, sizeof one, 'Z');
        write_file("/tmp/mm-priv.bin", one, sizeof one);
        int fd = open("/tmp/mm-priv.bin", MOS_O_RDONLY);
        volatile unsigned char *m = (volatile unsigned char *)sys4(SYS_MMAP, 4096, 4, (u64)fd, 0);
        int ok = fd >= 0 && (i64)m > 0 && m[0] == 'Z';
        if (ok) m[0] = 'Q';
        if (fd >= 0) close(fd);
        i64 rn = read_file("/tmp/mm-priv.bin", back, sizeof back);
        CHECK(ok && m[0] == 'Q' && rn >= 1 && back[0] == 'Z', "MAP_PRIVATE não grava de volta (%u no arquivo)", (unsigned)back[0]);
        if (ok) munmap((void *)m, 4096);
        unlink("/tmp/mm-priv.bin");
    }

    /* ---- MAP_SHARED|MAP_ANON entre pai e filho: não é cópia na escrita ---- */
    {
        volatile unsigned char *a = (volatile unsigned char *)mmap_fd(8192, MM_ANON, -1, 0, PROT_RW);
        int ok = (i64)a > 0;
        if (ok) { a[0] = 11; a[4096] = 22; }
        int fk = ok ? fork() : -1;
        if (fk == 0) {
            int good = a[0] == 11 && a[4096] == 22;
            a[0] = 33;
            a[100] = 44;
            exit(good && a[0] == 33 ? 62 : 1);
        }
        int st = fk > 0 ? wait(fk) : -1;
        CHECK(ok && st == 62 && a[0] == 33 && a[100] == 44 && a[4096] == 22,
              "MAP_SHARED|MAP_ANON visível nos dois lados (filho %d, %u/%u)", (i64)st, ok ? a[0] : 0, ok ? a[4096] : 0);
        if (ok) munmap((void *)a, 8192);
    }

    /* ---- página comum (sem prot) continua executável: processos x86_64 e o JIT RWX ---- */
    {
        volatile unsigned char *p = (volatile unsigned char *)sys3(SYS_MMAP, 4096, 3, 0);
        int ok = (i64)p > 0;
        if (ok) poke_ret(p, 0x5A);
        int v = ok ? call_page((void *)p) : -1;
        CHECK(ok && v == 0x5A, "página anônima RWX executa (retorno %d)", (i64)v);
        if (ok) munmap((void *)p, 4096);
    }

    /* ---- mprotect pede execução: a página NX some e o retorna o imediato ---- */
    {
        volatile unsigned char *p = (volatile unsigned char *)sys3(SYS_MMAP, 4096, 3, 0);
        int ok = (i64)p > 0;
        if (ok) poke_ret(p, 0x11);
        i64 pr = ok ? sys5(SYS_MMAP, 4096, MM_PROT, (u64)p, 0, PROT_RWX) : -1;
        int v = pr == 0 ? call_page((void *)p) : -1;
        CHECK(ok && pr == 0 && v == 0x11, "prot RWX executa (prot %d, retorno %d)", pr, (i64)v);
        if (ok) munmap((void *)p, 4096);
    }

    /* ---- página NX: a busca de instrução vira falta (SIGBUS, proteção), a escrita ainda vale ---- */
    {
        volatile unsigned char *p = (volatile unsigned char *)sys3(SYS_MMAP, 4096, 3, 0);
        int ok = (i64)p > 0;
        if (ok) { poke_ret(p, 0x22); p[16] = 0x7F; }
        i64 pr = ok ? sys5(SYS_MMAP, 4096, MM_PROT, (u64)p, 0, PROT_RW) : -1;
        int wrote = pr == 0 && (p[16] = 0x7E, p[16] == 0x7E);
        fault_page = p;
        fault_kind = 1;
        int st = (pr == 0) ? child_fault(fault_body) : -1;
        CHECK(ok && pr == 0 && wrote && st == 100 + MOS_SIGBUS,
              "página NX não executa; falta entregue como SIGBUS (filho %d)", (i64)st);
        if (ok) munmap((void *)p, 4096);
    }

    /* ---- MAP_SHARED|MAP_ANON já nasce sem execução quando o prot não pede ---- */
    {
        volatile unsigned char *p = (volatile unsigned char *)mmap_fd(4096, MM_ANON, -1, 0, PROT_RW);
        int ok = (i64)p > 0;
        if (ok) poke_ret(p, 0x33);
        fault_page = p;
        fault_kind = 1;
        int st = ok ? child_fault(fault_body) : -1;
        CHECK(ok && st == 100 + MOS_SIGBUS, "MAP_SHARED anônimo sem EXEC nasce NX (filho %d)", (i64)st);
        if (ok) munmap((void *)p, 4096);
    }

    /* ---- prot só leitura: escrever vira a mesma falta de proteção ---- */
    {
        volatile unsigned char *p = (volatile unsigned char *)sys3(SYS_MMAP, 4096, 3, 0);
        int ok = (i64)p > 0;
        if (ok) p[0] = 1;
        i64 pr = ok ? sys5(SYS_MMAP, 4096, MM_PROT, (u64)p, 0, PROT_R) : -1;
        fault_page = p;
        fault_kind = 2;
        int st = pr == 0 ? child_fault(fault_body) : -1;
        CHECK(ok && pr == 0 && st == 100 + MOS_SIGBUS, "página só leitura não aceita escrita (filho %d)", (i64)st);
        if (ok) munmap((void *)p, 4096);
    }

    /* ---- W^X do JIT (PTE_JWP): tira a escrita, a página continua executando, escrever falta ---- */
    {
        volatile unsigned char *p = (volatile unsigned char *)sys3(SYS_MMAP, 4096, 3, 0);
        int fk = (i64)p > 0 ? fork() : -1;
        if (fk == 0) {
            poke_ret(p, 0x44);
            if (call_page((void *)p) != 0x44) exit(3);
            u64 args[8] = {(u64)p, 1, 0, 0, 0, 0, 0, 0};
            i64 kr = sys3(SYS_MACH_VM, 18, (u64)args, 0); /* MACH_VM_JIT_WPROTECT, op 1 tira a escrita */
            if (kr != 0 || args[2] != 1) exit(4);
            if (call_page((void *)p) != 0x44) exit(5); /* continua executável */
            signal(MOS_SIGBUS, die_sig);
            signal(MOS_SIGSEGV, die_sig);
            p[0] = 0x90; /* escrever com a escrita tirada tem de faltar */
            exit(6);
        }
        int st = fk > 0 ? wait(fk) : fk;
        CHECK(st == 100 + MOS_SIGBUS, "JIT W^X: executa sem escrita e a escrita falta (filho %d)", (i64)st);
        if ((i64)p > 0) munmap((void *)p, 4096);
    }

    /* ---- MAP_FIXED de arquivo e mach_vm_map no endereço pedido ---- */
    {
        enum { N = 8192 };
        static char img[N], back[8];
        volatile unsigned char *slot;
        i64 hole;
        u64 pack, args[8], map[8], again[8], ow[8];
        i64 m, s, sy, kr, kr2, kr3, kr4;
        unsigned name;
        int fd, ok, bytes, sh;
        unsigned char before;
        fill(img, 4096, 'A');
        fill(img + 4096, 4096, 'B');
        CHECK(write_file("/tmp/mm-fixed.bin", img, N) == 0, "arquivo MAP_FIXED criado");
        fd = open("/tmp/mm-fixed.bin", MOS_O_RDWR);
        slot = (volatile unsigned char *)sys3(SYS_MMAP, 8192, 3, 0);
        ok = fd >= 0 && (i64)slot > 0;
        if (ok) { slot[0] = 'x'; slot[4096] = 'y'; }
        pack = ok ? ((u64)(unsigned)fd | ((u64)PROT_RW << 32)) : 0;
        CHECK(!ok || sys5(SYS_MMAP, 4096, MM_FIXED_PRIV, (u64)slot + 1, 0, pack) == -MOS_EINVAL,
              "MAP_FIXED desalinhado: EINVAL");
        CHECK(sys5(SYS_MMAP, 4096, MM_FIXED_PRIV, ok ? (u64)slot : 0x400100000000ULL, 0, 99) == -MOS_EBADF,
              "MAP_FIXED com fd inválido: EBADF");
        m = ok ? sys5(SYS_MMAP, 4096, MM_FIXED_PRIV, (u64)slot + 4096, 4096, pack) : -1;
        bytes = m == (i64)slot + 4096 && slot[4096] == 'B' && slot[0] == 'x';
        CHECK(ok && bytes, "MAP_FIXED privado no endereço pedido, offset 4096 (mapa %d, byte %u)",
              m, ok ? slot[4096] : 0);
        s = ok ? sys5(SYS_MMAP, 4096, MM_FIXED_SH, (u64)slot, 0, pack) : -1;
        sh = s == (i64)slot && slot[0] == 'A';
        if (sh) slot[0] = 'C';
        sy = sh ? sys3(SYS_MUNMAP, (u64)slot, 4096, MOS_MUNMAP_SYNC) : -1;
        if (fd >= 0) { lseek(fd, 0, MOS_SEEK_SET); read(fd, back, 8); }
        CHECK(sh && sy == 0 && back[0] == 'C', "MAP_FIXED compartilhado grava no arquivo (%u)", (unsigned)back[0]);

        args[0] = 4096; args[1] = (u64)slot; args[2] = PROT_RW; args[3] = 0;
        args[4] = args[5] = args[6] = args[7] = 0;
        kr = sh ? sys3(SYS_MACH_VM, 1, (u64)args, 0) : -1; /* MACH_VM_MAKE_ENTRY */
        name = (unsigned)args[4];
        hole = sys3(SYS_MMAP, 4096, 3, 0);
        if (hole > 0) munmap((void *)hole, 4096);
        map[0] = hole > 0 ? (u64)hole : 0;
        map[1] = 4096; map[2] = 0; map[3] = name; map[4] = 0; map[5] = PROT_RW; map[6] = PROT_RW; map[7] = 0;
        kr2 = (kr == 0 && name && hole > 0) ? sys3(SYS_MACH_VM, 2, (u64)map, 0) : -1;
        CHECK(kr == 0 && kr2 == 0 && map[0] == (u64)hole && *(volatile unsigned char *)map[0] == 'C',
              "mach_vm_map fixo devolve o endereço e o byte do arquivo (kr %d/%d)", kr, kr2);
        again[0] = (u64)slot; again[1] = 4096; again[2] = 0; again[3] = name;
        again[4] = 0; again[5] = PROT_RW; again[6] = PROT_RW; again[7] = 0;
        before = sh ? slot[0] : 0;
        kr3 = (kr == 0 && name) ? sys3(SYS_MACH_VM, 2, (u64)again, 0) : -1;
        CHECK(kr3 == 3 && slot[0] == before, "mach_vm_map sem OVERWRITE em faixa ocupada: KERN_NO_SPACE (%d)", kr3);
        ow[0] = (u64)slot + 4096; ow[1] = 4096; ow[2] = 0x4000; ow[3] = name;
        ow[4] = 0; ow[5] = PROT_RW; ow[6] = PROT_RW; ow[7] = 0;
        kr4 = (kr == 0 && name) ? sys3(SYS_MACH_VM, 2, (u64)ow, 0) : -1;
        CHECK(kr4 == 0 && ow[0] == (u64)slot + 4096 && slot[4096] == 'C',
              "mach_vm_map com OVERWRITE substitui e mostra o byte (%d, %u)", kr4, ok ? slot[4096] : 0);
        if (ok) munmap((void *)slot, 8192);
        if (kr2 == 0) munmap((void *)map[0], 4096);
        if (fd >= 0) close(fd);
        unlink("/tmp/mm-fixed.bin");
    }

    print(fails ? "mmshared: %d FALHA(S)\n" : "mmshared: TODOS OS TESTES OK\n", (i64)fails);
    exit(fails);
}
