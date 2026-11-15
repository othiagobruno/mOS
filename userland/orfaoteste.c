/* mOS — arquivo apagado ou substituído com fd aberto ou mapeado (POSIX: o conteúdo vive até o último uso).
 *
 * ELF de ring 3. O autoteste (userland/teste.c) o dispara quando a imagem o traz.
 * Kernel: orphan_if_open / orphan_gc em kernel/proc/process.c e lazy_rename_path em kernel/mm/lazy.c.
 * O que se espera no log: uma linha "ok" por checagem e "orfaoteste: TODOS OS TESTES OK".
 */
#include "mos.h"

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

static int orfaos(void) {   /* quantos arquivos ainda estão em /tmp/.orfaos */
    char nm[64];
    int n = 0;
    while (readdir("/tmp/.orfaos", n, nm, sizeof nm) > 0) n++;
    return n;
}

static int reread(int fd, char *b, usize cap) {
    for (usize i = 0; i < cap; i++) b[i] = 0;
    lseek(fd, 0, MOS_SEEK_SET);
    return (int)read(fd, b, cap - 1);
}

void _start(const char *arg) {
    (void)arg;
    char b[32];
    print("orfaoteste: pid %d\n", (i64)getpid());
    int base = orfaos();

    /* ---- unlink com o fd aberto: o fd continua lendo; um arquivo novo no mesmo nome é outro ---- */
    {
        write_file("/tmp/of-a.txt", "velho", 5);
        int fd = open("/tmp/of-a.txt", MOS_O_RDONLY);
        int u = unlink("/tmp/of-a.txt");
        int n = fd >= 0 ? reread(fd, b, sizeof b) : -1;
        CHECK(fd >= 0 && u == 0 && n == 5 && !memcmp(b, "velho", 5), "unlink com fd aberto: o fd lê o conteúdo (%d bytes)", (i64)n);
        CHECK(file_size("/tmp/of-a.txt") < 0, "o nome sumiu depois do unlink");
        write_file("/tmp/of-a.txt", "novo!!", 6);
        n = fd >= 0 ? reread(fd, b, sizeof b) : -1;
        CHECK(n == 5 && !memcmp(b, "velho", 5), "arquivo novo no mesmo nome não muda o fd antigo (%d bytes)", (i64)n);
        CHECK(orfaos() == base + 1, "o apagado fica guardado enquanto o fd existe (%d)", (i64)orfaos());
        if (fd >= 0) close(fd);
        CHECK(orfaos() == base, "o último close apaga o guardado (%d)", (i64)orfaos());
        unlink("/tmp/of-a.txt");
    }

    /* ---- rename por cima de um arquivo aberto (salvar atômico, git X.lock -> X) ---- */
    {
        write_file("/tmp/of-b.txt", "antes", 5);
        write_file("/tmp/of-b.tmp", "depois", 6);
        int fd = open("/tmp/of-b.txt", MOS_O_RDONLY);
        int rn = rename("/tmp/of-b.tmp", "/tmp/of-b.txt");
        int n = fd >= 0 ? reread(fd, b, sizeof b) : -1;
        CHECK(fd >= 0 && rn == 0 && n == 5 && !memcmp(b, "antes", 5), "rename por cima: o fd antigo lê o antigo (%d bytes)", (i64)n);
        n = (int)read_file("/tmp/of-b.txt", b, sizeof b);
        CHECK(n == 6 && !memcmp(b, "depois", 6), "o nome mostra o arquivo novo (%d bytes)", (i64)n);
        if (fd >= 0) close(fd);
        CHECK(orfaos() == base, "close apaga o substituído (%d)", (i64)orfaos());
        unlink("/tmp/of-b.txt");
    }

    /* ---- rename de pasta com arquivo aberto dentro: o fd acompanha ---- */
    {
        mkdir("/tmp/of-dir");
        write_file("/tmp/of-dir/x.txt", "dentro", 6);
        int fd = open("/tmp/of-dir/x.txt", MOS_O_RDONLY);
        int rn = rename("/tmp/of-dir", "/tmp/of-dir2");
        int n = fd >= 0 ? reread(fd, b, sizeof b) : -1;
        CHECK(rn == 0 && n == 6 && !memcmp(b, "dentro", 6), "rename da pasta: o fd segue o arquivo (%d bytes)", (i64)n);
        if (fd >= 0) close(fd);
        unlink("/tmp/of-dir2");
    }

    /* ---- mmap de arquivo (sob demanda) e unlink antes de tocar a página ---- */
    {
        static char pg[8192];
        for (int i = 0; i < 8192; i++) pg[i] = (char)('A' + (i >> 12));
        write_file("/tmp/of-m.bin", pg, sizeof pg);
        int fd = open("/tmp/of-m.bin", MOS_O_RDONLY);
        volatile char *m = (volatile char *)sys4(SYS_MMAP, 8192, 4, (u64)fd, 0);   /* b = 4: arquivo, privado */
        if (fd >= 0) close(fd);
        int u = unlink("/tmp/of-m.bin");
        int ok = (i64)m > 0 && u == 0;
        char c0 = ok ? m[0] : 0, c1 = ok ? m[4096] : 0;       /* as duas páginas faltam só agora */
        CHECK(ok && c0 == 'A' && c1 == 'B', "mmap sob demanda depois do unlink lê o arquivo (%u/%u)", (unsigned)c0, (unsigned)c1);
        CHECK(orfaos() == base + 1, "o mapa segura o arquivo apagado (%d)", (i64)orfaos());
        if ((i64)m > 0) munmap((void *)m, 8192);
        CHECK(orfaos() == base, "munmap apaga o guardado (%d)", (i64)orfaos());
    }

    /* ---- unlink sem ninguém usando: some na hora, sem passar pelos guardados ---- */
    {
        write_file("/tmp/of-c.txt", "x", 1);
        CHECK(unlink("/tmp/of-c.txt") == 0 && orfaos() == base && file_size("/tmp/of-c.txt") < 0,
              "unlink sem fd aberto apaga direto");
    }

    print(fails ? "orfaoteste: %d FALHA(S)\n" : "orfaoteste: TODOS OS TESTES OK\n", (i64)fails);
    exit(fails);
}
