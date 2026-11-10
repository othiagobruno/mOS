/* Selo do Keychain em disco.
 * A chave do arquivo são 32 bytes lidos de getentropy() (libSystem; no mOS,
 * extra.c encaminha getentropy para arc4random_buf). Não há gerador neste
 * arquivo. A chave fica em <home>/Library/Keychains/mos-keychain.key, modo
 * 0600, dono = o usuário, pasta 0700. Leitura recusa symlink, modo diferente
 * de 0600, dono diferente, tamanho diferente de 32 e pasta que não seja 0700.
 * A chave não é impressa.
 *
 * kc_key_from() permanece só para abrir um arquivo versão 2 antigo (chave
 * derivada do nome de login) durante a migração. A migração regrava o selo
 * com a chave nova. Se a regravação falha, o arquivo original não é apagado.
 *
 * Arquivo versão 2: "mOS-keychain 2\n" + nonce de 16 bytes + ciphertext + MAC de 32.
 * O miolo cifrado é o texto da versão 1. Quem inclui este arquivo já declarou
 * u8, u32, size_t, ssize_t, malloc, free, memcpy, memcmp, strlen, open, read, close.
 */
void *memset(void *s, int c, size_t n);

#ifdef KC_HOST
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
int getentropy(void *buf, size_t n);
int lstat(const char *path, void *st);
int chmod(const char *path, unsigned short mode);
unsigned geteuid(void);
typedef struct {
    unsigned st_dev;
    unsigned short st_mode, st_nlink;
    unsigned long long st_ino;
    unsigned st_uid, st_gid, st_rdev;
    long long atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    long long st_size, st_blocks;
    int st_blksize;
    unsigned st_flags, st_gen;
    int lspare;
    long long qspare[2];
} kc_mac_stat;
#endif

#ifndef MAC_O_RDONLY
#define MAC_O_RDONLY 0
#endif
#ifdef KC_HOST
#define KC_O_RDONLY O_RDONLY
#define KC_O_WRONLY O_WRONLY
#define KC_O_CREAT O_CREAT
#define KC_O_EXCL O_EXCL
#define KC_O_TRUNC O_TRUNC
#define KC_O_NOFOLLOW O_NOFOLLOW
#else
#define KC_O_RDONLY 0
#define KC_O_WRONLY 1
#define KC_O_CREAT 0x200
#define KC_O_EXCL 0x800
#define KC_O_TRUNC 0x400
#define KC_O_NOFOLLOW 0
#endif

static u32 kc_rotr(u32 x, int n) { return (x >> n) | (x << (32 - n)); }

static void kc_compress(u32 st[8], const u8 blk[64]) {
    static const u32 K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    u32 w[64];
    for (int i = 0; i < 16; i++) {
        const u8 *p = blk + (u32)i * 4;
        w[i] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    }
    for (int i = 16; i < 64; i++) {
        u32 s0 = kc_rotr(w[i - 15], 7) ^ kc_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        u32 s1 = kc_rotr(w[i - 2], 17) ^ kc_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = st[0], b = st[1], c = st[2], d = st[3], e = st[4], f = st[5], g = st[6], h = st[7];
    for (int i = 0; i < 64; i++) {
        u32 S1 = kc_rotr(e, 6) ^ kc_rotr(e, 11) ^ kc_rotr(e, 25);
        u32 ch = (e & f) ^ (~e & g);
        u32 t1 = h + S1 + ch + K[i] + w[i];
        u32 S0 = kc_rotr(a, 2) ^ kc_rotr(a, 13) ^ kc_rotr(a, 22);
        u32 mj = (a & b) ^ (a & c) ^ (b & c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + S0 + mj;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d; st[4] += e; st[5] += f; st[6] += g; st[7] += h;
}

static void kc_sha256(const u8 *msg, u32 len, u8 out[32]) {
    u32 st[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u8 block[64];
    u32 off = 0;
    unsigned long long bits = (unsigned long long)len * 8ull;
    while (off + 64 <= len) {
        kc_compress(st, msg + off);
        off += 64;
    }
    u32 rem = len - off;
    memset(block, 0, 64);
    if (rem && msg) memcpy(block, msg + off, rem);
    block[rem] = 0x80;
    if (rem >= 56) {
        kc_compress(st, block);
        memset(block, 0, 64);
    }
    block[56] = (u8)(bits >> 56); block[57] = (u8)(bits >> 48); block[58] = (u8)(bits >> 40); block[59] = (u8)(bits >> 32);
    block[60] = (u8)(bits >> 24); block[61] = (u8)(bits >> 16); block[62] = (u8)(bits >> 8); block[63] = (u8)bits;
    kc_compress(st, block);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (u8)(st[i] >> 24); out[i * 4 + 1] = (u8)(st[i] >> 16);
        out[i * 4 + 2] = (u8)(st[i] >> 8); out[i * 4 + 3] = (u8)st[i];
    }
    memset(block, 0, 64);
    memset(st, 0, sizeof st);
}

static void kc_hmac(const u8 *key, u32 klen, const u8 *msg, u32 mlen, u8 mac[32]) {
    u8 k[64], ipad[64], opad[64], inner[32];
    memset(k, 0, 64);
    if (klen > 64) kc_sha256(key, klen, k);
    else if (klen) memcpy(k, key, klen);
    for (int i = 0; i < 64; i++) { ipad[i] = (u8)(k[i] ^ 0x36); opad[i] = (u8)(k[i] ^ 0x5c); }
    u32 catn = 64 + (mlen > 32 ? mlen : 32);
    u8 *cat = (u8 *)malloc(catn);
    if (!cat) { memset(mac, 0, 32); memset(k, 0, 64); return; }
    memcpy(cat, ipad, 64);
    if (mlen) memcpy(cat + 64, msg, mlen);
    kc_sha256(cat, 64 + mlen, inner);
    memcpy(cat, opad, 64);
    memcpy(cat + 64, inner, 32);
    kc_sha256(cat, 96, mac);
    free(cat);
    memset(k, 0, 64);
    memset(inner, 0, 32);
}

static void kc_login(char *out, size_t n) {
    const char *u = getenv("USER");
    if (!u || !u[0]) u = getenv("LOGNAME");
    if (!u || !u[0]) {
        const char *h = getenv("HOME");
        if (h && h[0]) {
            const char *s = h;
            for (const char *p = h; *p; p++) if (*p == '/') s = p + 1;
            u = s[0] ? s : h;
        }
    }
    if (!u || !u[0]) u = "visitante";
    size_t i = 0;
    for (; u[i] && i + 1 < n; i++) out[i] = u[i];
    out[i] = 0;
}

static int kc_parent_dir(const char *path, char *dir, size_t n) {
    size_t L = strlen(path);
    if (!L || L + 1 > n) return -1;
    memcpy(dir, path, L + 1);
    char *slash = 0;
    for (char *p = dir; *p; p++) if (*p == '/') slash = p;
    if (!slash || slash == dir) return -1;
    *slash = 0;
    return 0;
}

static void kc_mkdir_p(char *dir) {
    for (char *p = dir + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(dir, 0700); *p = '/'; }
    }
    mkdir(dir, 0700);
    chmod(dir, 0700);
}

#ifdef KC_HOST
static int kc_stat_ok_file(const struct stat *st) {
    if (!S_ISREG(st->st_mode)) return 0;
    if (S_ISLNK(st->st_mode)) return 0;
    if ((st->st_mode & 0777) != 0600) return 0;
    if (st->st_uid != geteuid()) return 0;
    if (st->st_size != 32) return 0;
    return 1;
}
static int kc_stat_ok_dir(const struct stat *st) {
    if (!S_ISDIR(st->st_mode)) return 0;
    if ((st->st_mode & 0777) != 0700) return 0;
    if (st->st_uid != geteuid()) return 0;
    return 1;
}
#else
static int kc_stat_ok_file(const kc_mac_stat *st) {
    unsigned short kind = (unsigned short)(st->st_mode & 0170000);
    if (kind == 0120000) return 0;
    if (kind != 0100000) return 0;
    if ((st->st_mode & 0777) != 0600) return 0;
    if (st->st_uid != geteuid()) return 0;
    if (st->st_size != 32) return 0;
    return 1;
}
static int kc_stat_ok_dir(const kc_mac_stat *st) {
    if ((st->st_mode & 0170000) != 0040000) return 0;
    if ((st->st_mode & 0777) != 0700) return 0;
    if (st->st_uid != geteuid()) return 0;
    return 1;
}
#endif

/* 0 = 32 bytes válidos. -2 = arquivo ausente. -1 = recusado (symlink, modo, dono, tamanho, pasta). */
static int kc_key_read(const char *path, u8 out[32]) {
    if (out) memset(out, 0, 32);
    if (!path || !out) return -1;
    char dir[512];
    if (kc_parent_dir(path, dir, sizeof dir)) return -1;
#ifdef KC_HOST
    struct stat ds, fs;
    if (lstat(dir, &ds) != 0) return -2;
    if (!kc_stat_ok_dir(&ds)) return -1;
    if (lstat(path, &fs) != 0) return -2;
    if (S_ISLNK(fs.st_mode)) return -1;
    if (!kc_stat_ok_file(&fs)) return -1;
#else
    kc_mac_stat ds, fs;
    if (lstat(dir, &ds) != 0) return -2;
    if (!kc_stat_ok_dir(&ds)) return -1;
    if (lstat(path, &fs) != 0) return -2;
    if ((fs.st_mode & 0170000) == 0120000) return -1;
    if (!kc_stat_ok_file(&fs)) return -1;
#endif
    int fd = open(path, KC_O_RDONLY | KC_O_NOFOLLOW);
    if (fd < 0) return -1;
    u8 buf[32];
    u32 got = 0;
    while (got < 32) {
        ssize_t k = read(fd, buf + got, 32 - got);
        if (k <= 0) break;
        got += (u32)k;
    }
    close(fd);
    if (got != 32) { memset(buf, 0, 32); return -1; }
    memcpy(out, buf, 32);
    memset(buf, 0, 32);
    return 0;
}

/* Cria a pasta 0700 e o arquivo 0600. Não substitui um arquivo que já existe. */
static int kc_key_create(const char *path, u8 out[32]) {
    if (out) memset(out, 0, 32);
    if (!path || !out) return -1;
    char dir[512];
    if (kc_parent_dir(path, dir, sizeof dir)) return -1;
    kc_mkdir_p(dir);
    u8 buf[32];
    if (getentropy(buf, 32) != 0) { memset(buf, 0, 32); return -1; }
    int fd = open(path, KC_O_WRONLY | KC_O_CREAT | KC_O_EXCL | KC_O_NOFOLLOW, 0600);
    if (fd < 0) { memset(buf, 0, 32); return -1; }
    if (chmod(path, 0600) != 0) { close(fd); unlink(path); memset(buf, 0, 32); return -1; }
    u32 done = 0;
    while (done < 32) {
        ssize_t k = write(fd, buf + done, 32 - done);
        if (k <= 0) { close(fd); unlink(path); memset(buf, 0, 32); return -1; }
        done += (u32)k;
    }
    close(fd);
    u8 check[32];
    int r = kc_key_read(path, check);
    if (r != 0 || memcmp(check, buf, 32) != 0) {
        memset(buf, 0, 32);
        memset(check, 0, 32);
        return -1;
    }
    memcpy(out, buf, 32);
    memset(buf, 0, 32);
    memset(check, 0, 32);
    return 0;
}

/* Lê a chave. Se o arquivo não existe, gera. Arquivo recusado não é substituído. */
static int kc_key_ensure(const char *path, u8 out[32]) {
    int r = kc_key_read(path, out);
    if (r == 0) return 0;
    if (r == -2) return kc_key_create(path, out);
    return -1;
}

static int kc_read_all(const char *path, u8 **out, u32 *n) {
    if (out) *out = 0;
    if (n) *n = 0;
    int fd = open(path, KC_O_RDONLY);
    if (fd < 0) return -1;
    u8 *buf = 0;
    u32 len = 0, cap = 0;
    for (;;) {
        if (len + 4096 > cap) {
            cap = (len + 4096) * 2;
            u8 *nb = (u8 *)realloc(buf, cap ? cap : 1);
            if (!nb) { free(buf); close(fd); return -1; }
            buf = nb;
        }
        ssize_t k = read(fd, buf + len, cap - len);
        if (k < 0) { free(buf); close(fd); return -1; }
        if (k == 0) break;
        len += (u32)k;
    }
    close(fd);
    if (!buf) { buf = (u8 *)malloc(1); if (!buf) return -1; }
    *out = buf;
    if (n) *n = len;
    return 0;
}

/* Troca o arquivo só depois de escrever o temporário inteiro. Nunca apaga path. */
static int kc_replace_file(const char *path, const u8 *data, u32 n) {
    size_t L = strlen(path);
    if (L + 5 >= 512) return -1;
    char tmp[512];
    memcpy(tmp, path, L);
    memcpy(tmp + L, ".tmp", 5);
    int fd = open(tmp, KC_O_WRONLY | KC_O_CREAT | KC_O_TRUNC, 0600);
    if (fd < 0) return -1;
    chmod(tmp, 0600);
    u32 done = 0;
    while (done < n) {
        ssize_t k = write(fd, data + done, n - done);
        if (k <= 0) { close(fd); unlink(tmp); return -1; }
        done += (u32)k;
    }
    close(fd);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* HKDF enxuto, só para a chave antiga da migração: extract(sal fixo, nome de login). */
static void kc_key_from(const char *login, u8 out[32]) {
    static const u8 salt[] = "mOS-keychain-v2";
    u8 prk[32];
    kc_hmac(salt, (u32)(sizeof salt - 1), (const u8 *)(login ? login : ""), login ? (u32)strlen(login) : 0, prk);
    kc_hmac(prk, 32, (const u8 *)"file", 4, out);
    memset(prk, 0, 32);
}

static void kc_subkeys(const u8 filekey[32], u8 enc[32], u8 mac[32]) {
    kc_hmac(filekey, 32, (const u8 *)"enc", 3, enc);
    kc_hmac(filekey, 32, (const u8 *)"mac", 3, mac);
}

static u32 kc_nonce_seq;
static void kc_nonce(u8 n[16]) {
    int fd = open("/dev/urandom", MAC_O_RDONLY);
    if (fd >= 0) {
        u32 got = 0;
        while (got < 16) {
            ssize_t k = read(fd, n + got, 16 - got);
            if (k <= 0) break;
            got += (u32)k;
        }
        close(fd);
        if (got == 16) return;
    }
    kc_nonce_seq++;
    unsigned long mix = (unsigned long)kc_nonce_seq * 0x9E3779B9ul ^ (unsigned long)(size_t)n;
    for (int i = 0; i < 16; i++) {
        mix = mix * 1664525ul + 1013904223ul;
        n[i] = (u8)(mix >> 16);
    }
}

static void kc_stream(const u8 enc[32], const u8 nonce[16], const u8 *in, u8 *out, u32 n) {
    u32 off = 0, ctr = 0;
    while (off < n) {
        u8 msg[20], block[32];
        memcpy(msg, nonce, 16);
        msg[16] = (u8)(ctr >> 24); msg[17] = (u8)(ctr >> 16); msg[18] = (u8)(ctr >> 8); msg[19] = (u8)ctr;
        kc_hmac(enc, 32, msg, 20, block);
        u32 take = n - off;
        if (take > 32) take = 32;
        for (u32 i = 0; i < take; i++) out[off + i] = (u8)(in[off + i] ^ block[i]);
        off += take;
        ctr++;
        memset(block, 0, 32);
    }
}

static int kc_mac_ok(const u8 *a, const u8 *b) {
    u8 d = 0;
    for (int i = 0; i < 32; i++) d |= (u8)(a[i] ^ b[i]);
    return d == 0;
}

/* 0 = ok. *out é o arquivo inteiro (cabeçalho + nonce + cifra + mac). */
static int kc_seal(const u8 filekey[32], const u8 *plain, u32 n, u8 **out, u32 *outn) {
    if (out) *out = 0;
    if (outn) *outn = 0;
    if (!filekey || !out || !plain) return -1;
    u8 enc[32], mac_key[32], nonce[16], mac[32];
    kc_subkeys(filekey, enc, mac_key);
    kc_nonce(nonce);
    u32 total = 15u + 16u + n + 32u;
    u8 *buf = (u8 *)malloc(total ? total : 1);
    if (!buf) { memset(enc, 0, 32); memset(mac_key, 0, 32); return -1; }
    memcpy(buf, "mOS-keychain 2\n", 15);
    memcpy(buf + 15, nonce, 16);
    kc_stream(enc, nonce, plain, buf + 31, n);
    u8 *mac_in = (u8 *)malloc((size_t)16 + n);
    if (!mac_in) { free(buf); memset(enc, 0, 32); memset(mac_key, 0, 32); return -1; }
    memcpy(mac_in, nonce, 16);
    if (n) memcpy(mac_in + 16, buf + 31, n);
    kc_hmac(mac_key, 32, mac_in, 16 + n, mac);
    free(mac_in);
    memcpy(buf + 31 + n, mac, 32);
    memset(enc, 0, 32);
    memset(mac_key, 0, 32);
    *out = buf;
    if (outn) *outn = total;
    return 0;
}

/* 0 = ok e *plain é o texto da versão 1. -1 = não é v2, MAC ruim ou sem memória. */
static int kc_open(const u8 filekey[32], const u8 *file, u32 n, u8 **plain, u32 *pn) {
    if (plain) *plain = 0;
    if (pn) *pn = 0;
    if (!filekey || !file || n < 15 + 16 + 32 || memcmp(file, "mOS-keychain 2\n", 15)) return -1;
    u32 body = n - 15 - 16 - 32;
    const u8 *nonce = file + 15;
    const u8 *ct = file + 31;
    const u8 *tag = file + 31 + body;
    u8 enc[32], mac_key[32], mac[32];
    kc_subkeys(filekey, enc, mac_key);
    u8 *mac_in = (u8 *)malloc((size_t)16 + body);
    if (!mac_in) { memset(enc, 0, 32); memset(mac_key, 0, 32); return -1; }
    memcpy(mac_in, nonce, 16);
    if (body) memcpy(mac_in + 16, ct, body);
    kc_hmac(mac_key, 32, mac_in, 16 + body, mac);
    free(mac_in);
    int ok = kc_mac_ok(mac, tag);
    memset(mac_key, 0, 32);
    if (!ok) { memset(enc, 0, 32); return -1; }
    u8 *p = (u8 *)malloc(body + 1);
    if (!p) { memset(enc, 0, 32); return -1; }
    kc_stream(enc, nonce, ct, p, body);
    p[body] = 0;
    memset(enc, 0, 32);
    *plain = p;
    if (pn) *pn = body;
    return 0;
}

/* 0 = o arquivo passou a abrir com newkey. -1 = nada foi apagado em path. */
static int kc_migrate_file(const char *path, const u8 oldkey[32], const u8 newkey[32]) {
    u8 *file = 0;
    u32 fn = 0;
    if (kc_read_all(path, &file, &fn) != 0) return -1;
    u8 *plain = 0;
    u32 pn = 0;
    if (kc_open(oldkey, file, fn, &plain, &pn) != 0) { free(file); return -1; }
    u8 *sealed = 0;
    u32 sn = 0;
    int sr = kc_seal(newkey, plain, pn, &sealed, &sn);
    memset(plain, 0, pn);
    free(plain);
    free(file);
    if (sr != 0 || !sealed) return -1;
    int wr = kc_replace_file(path, sealed, sn);
    memset(sealed, 0, sn);
    free(sealed);
    return wr;
}
