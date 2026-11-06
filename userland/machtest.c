/*
 * mOS — teste do IPC Mach entre processos (kernel/proc/machipc.c).
 *
 * Um processo faz fork; pai e filho usam as portas Mach do kernel para o mesmo caminho do rendezvous
 * do VS Code/Chromium:
 *   - o pai registra um serviço no bootstrap (check_in) e espera uma mensagem;
 *   - o filho encontra o serviço (look_up) e envia uma mensagem COMPLEXA com: direito de resposta
 *     (send-once) no header, um descritor de porta (transfere um direito de envio novo) e um bloco de
 *     memória "out-of-line" com uma string;
 *   - o pai confere a string, confere que a porta transferida chegou com um nome válido e responde;
 *   - o filho confere a resposta.
 * Imprime "machtest: PASS" ou "machtest: FALHA ...". Rode no Terminal do mOS: `machtest`.
 */
#include "mos.h"

typedef unsigned char u8;
typedef unsigned int u32;

static i64 mmsg(mos_mach_msg_args_t *a) { return sys3(SYS_MACH_MSG, (u64)a, 0, 0); }
static i64 mport(u64 op, u64 *a) { return sys3(SYS_MACH_PORT, op, (u64)a, 0); }
static i64 mbs(u64 op, const char *n, u32 *p) { return sys3(SYS_MACH_BOOTSTRAP, op, (u64)n, (u64)p); }

static void w32(u8 *b, u32 o, u32 v) { b[o] = v; b[o+1] = v>>8; b[o+2] = v>>16; b[o+3] = v>>24; }
static u32 r32(const u8 *b, u32 o) { return (u32)b[o] | (u32)b[o+1]<<8 | (u32)b[o+2]<<16 | (u32)b[o+3]<<24; }
static void w64(u8 *b, u32 o, u64 v) { w32(b, o, (u32)v); w32(b, o+4, (u32)(v>>32)); }
static u64 r64(const u8 *b, u32 o) { return (u64)r32(b,o) | (u64)r32(b,o+4)<<32; }

/* opções e disposições (iguais às do XNU / syscall.h) */
#define SEND 0x1
#define RCV  0x2
#define COMPLEX 0x80000000u
#define COPY_SEND 19
#define MAKE_SEND 20
#define MAKE_SEND_ONCE 21
#define MOVE_SEND_ONCE 18
#define RIGHT_RECEIVE 1
#define D_PORT 0
#define D_OOL  1

static const char *SVC = "mOS.machtest.servico";
static u8 oolbuf[4096] __attribute__((aligned(4096)));

static int filho(void) {
    sleep_ms(80);                                   /* deixa o pai registrar o serviço primeiro */
    u32 cli = 0;
    int r = (int)mbs(MACH_BS_LOOK_UP, SVC, &cli);
    if (r != 0 || !cli) { print("machtest: FALHA look_up r=%d\n", (i64)r); return 1; }

    u64 a[8] = {RIGHT_RECEIVE, 0,0,0,0,0,0,0};      /* porta de resposta */
    if (mport(MACH_OP_ALLOCATE, a) != 0) { print("machtest: FALHA alocar resposta\n"); return 1; }
    u32 reply = (u32)a[2];
    u64 b[8] = {RIGHT_RECEIVE, 0,0,0,0,0,0,0};      /* porta nova que será transferida ao pai */
    if (mport(MACH_OP_ALLOCATE, b) != 0) { print("machtest: FALHA alocar porta extra\n"); return 1; }
    u32 extra = (u32)b[2];

    for (int i = 0; i < 8; i++) oolbuf[i] = "ola-mach"[i];

    u8 msg[256];
    for (int i = 0; i < 256; i++) msg[i] = 0;
    /* header: remote = serviço (COPY_SEND), local = resposta (MAKE_SEND_ONCE), complexo */
    w32(msg, 0, COMPLEX | (u32)COPY_SEND | ((u32)MAKE_SEND_ONCE << 8));
    w32(msg, 8, cli);
    w32(msg, 12, reply);
    w32(msg, 20, 0xA1);                             /* id */
    w32(msg, 24, 2);                                /* 2 descritores */
    u32 off = 28;
    /* descritor de porta (12 bytes): envia um MAKE_SEND da porta "extra" */
    w32(msg, off, extra);
    msg[off+10] = MAKE_SEND;
    msg[off+11] = D_PORT;
    off += 12;
    /* descritor OOL (16 bytes): endereço(8), ...disp(10), tipo(11), tamanho(12) */
    w64(msg, off, (u64)(u64)oolbuf);
    msg[off+11] = D_OOL;
    w32(msg, off+12, 8);
    off += 16;
    u32 total = off;
    w32(msg, 4, total);

    mos_mach_msg_args_t s = {(u64)msg, SEND | RCV, total, sizeof msg, reply, 0, 0};
    r = (int)mmsg(&s);
    if (r != 0) { print("machtest: FALHA envio/recv filho r=0x%x\n", (i64)(u32)r); return 1; }
    u32 id = r32(msg, 20), payload = r32(msg, 24);
    if (id != 0xA2 || payload != 0xCAFE) { print("machtest: FALHA resposta id=0x%x carga=0x%x\n", (i64)id, (i64)payload); return 1; }
    print("machtest: filho recebeu resposta id=0x%x carga=0x%x -> OK\n", (i64)id, (i64)payload);
    return 0;
}

static int pai(void) {
    u32 svc = 0;
    int r = (int)mbs(MACH_BS_CHECK_IN, SVC, &svc);
    if (r != 0 || !svc) { print("machtest: FALHA check_in r=%d\n", (i64)r); return 1; }

    int pid = fork();
    if (pid == 0) { int rc = filho(); exit(rc); }
    if (pid < 0) { print("machtest: FALHA fork\n"); return 1; }

    u8 msg[512];
    mos_mach_msg_args_t rcv = {(u64)msg, RCV, 0, sizeof msg, svc, 0, 0};
    r = (int)mmsg(&rcv);
    if (r != 0) { print("machtest: FALHA recv pai r=0x%x\n", (i64)(u32)r); return 1; }
    u32 id = r32(msg, 20);
    u32 reply = r32(msg, 8);                         /* direito de resposta (send-once) reescrito pelo kernel */
    u32 nd = r32(msg, 24);
    /* confere a porta transferida e a string OOL */
    u32 off = 28, portname = 0; u64 ooladdr = 0;
    for (u32 i = 0; i < nd; i++) {
        u8 dt = msg[off+11];
        if (dt == D_PORT) { portname = r32(msg, off); off += 12; }
        else { ooladdr = r64(msg, off); off += 16; }
    }
    int ok_ool = ooladdr && !strncmp((const char *)(u64)ooladdr, "ola-mach", 8);
    print("machtest: pai recebeu id=0x%x porta_transf=%d ool='%s'\n", (i64)id,
          (i64)(int)portname, ok_ool ? "ola-mach" : "?");
    if (id != 0xA1 || !portname || !ok_ool || !reply) {
        print("machtest: FALHA conteudo (reply=%d)\n", (i64)(int)reply);
        return 1;
    }
    /* responde pela porta send-once */
    u8 rep[64];
    for (int i = 0; i < 64; i++) rep[i] = 0;
    w32(rep, 0, (u32)MOVE_SEND_ONCE);
    w32(rep, 8, reply);
    w32(rep, 20, 0xA2);
    w32(rep, 24, 0xCAFE);
    w32(rep, 4, 28);
    mos_mach_msg_args_t s = {(u64)rep, SEND, 28, 0, 0, 0, 0};
    r = (int)mmsg(&s);
    if (r != 0) { print("machtest: FALHA envio resposta r=0x%x\n", (i64)(u32)r); return 1; }

    int st = 0;
    waitany(&st);
    print("machtest: filho saiu com %d\n", (i64)st);
    if (st != 0) { print("machtest: FALHA (filho)\n"); return 1; }
    print("machtest: PASS\n");
    return 0;
}

void _start(const char *arg) {
    (void)arg;
    exit(pai());
}
