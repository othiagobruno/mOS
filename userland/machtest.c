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
static i64 mvm(u64 op, u64 *a) { return sys3(SYS_MACH_VM, op, (u64)a, 0); }

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
#define RCV_TIMEOUT 0x100
#define NOTE_NO_SENDERS 70
#define NOTE_DEAD_NAME  72

static u32 nova_porta(void) {
    u64 a[8] = {RIGHT_RECEIVE, 0,0,0,0,0,0,0};
    return mport(MACH_OP_ALLOCATE, a) ? 0 : (u32)a[2];
}
/* espera um aviso do kernel na porta: id (70/72) e o argumento depois do NDR (offset 32) */
static int espera_aviso_ms(u32 port, u32 *id, u32 *arg, u32 ms) {
    u8 m[128];
    for (int i = 0; i < 128; i++) m[i] = 0;
    mos_mach_msg_args_t a = {(u64)m, RCV | RCV_TIMEOUT, 0, sizeof m, port, ms, 0};
    int r = (int)mmsg(&a);
    *id = r32(m, 20);
    *arg = r32(m, 32);
    return r;
}
static int espera_aviso(u32 port, u32 *id, u32 *arg) { return espera_aviso_ms(port, id, arg, 2000); }

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
    /* memória compartilhada: objeto novo e zerado (MAP_MEM_NAMED_CREATE), mapeado aqui e enviado ao pai */
    u64 v[8] = {8192, 0, 0x20000 | 3, 0, 0,0,0,0};
    if (mvm(MACH_VM_MAKE_ENTRY, v) != 0) { print("machtest: FALHA make_memory_entry\n"); return 1; }
    u32 ment = (u32)v[4];
    u64 mp[8] = {0, 0, 1 /* ANYWHERE */, ment, 0, 3, 0,0};
    if (mvm(MACH_VM_MAP, mp) != 0 || !mp[0]) { print("machtest: FALHA vm_map filho\n"); return 1; }
    char *shm = (char *)mp[0];
    for (int i = 0; i < 8; i++) shm[i] = "compart"[i];
    /* entrada só-leitura derivada (perm = VM_PROT_READ): mapear com escrita tem de dar KERN_INVALID_RIGHT (17),
     * só leitura tem de funcionar e ver os mesmos bytes (o teste de permissão do Chromium) */
    u64 ro[8] = {8192, 0, 1 /* VM_PROT_READ */, ment, 0,0,0,0};
    if (mvm(MACH_VM_MAKE_ENTRY, ro) != 0) { print("machtest: FALHA entrada so-leitura\n"); return 1; }
    u64 mw[8] = {0, 0, 1, ro[4], 0, 3, 3, 0};
    i64 kw = mvm(MACH_VM_MAP, mw);
    u64 mr[8] = {0, 0, 1, ro[4], 0, 1, 1, 0};
    i64 kr = mvm(MACH_VM_MAP, mr);
    if (kw != 17 || kr != 0 || strncmp((const char *)mr[0], "compart", 7)) {
        print("machtest: FALHA so-leitura (escrita kr=%d, leitura kr=%d)\n", kw, kr);
        return 1;
    }
    print("machtest: entrada so-leitura OK (escrita -> 17, leitura ve os bytes)\n");

    for (int i = 0; i < 8; i++) oolbuf[i] = "ola-mach"[i];

    u8 msg[256];
    for (int i = 0; i < 256; i++) msg[i] = 0;
    /* header: remote = serviço (COPY_SEND), local = resposta (MAKE_SEND_ONCE), complexo */
    w32(msg, 0, COMPLEX | (u32)COPY_SEND | ((u32)MAKE_SEND_ONCE << 8));
    w32(msg, 8, cli);
    w32(msg, 12, reply);
    w32(msg, 20, 0xA1);                             /* id */
    w32(msg, 24, 3);                                /* 3 descritores */
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
    /* descritor de porta: envia (COPY_SEND) a entrada de memória compartilhada */
    w32(msg, off, ment);
    msg[off+10] = COPY_SEND;
    msg[off+11] = D_PORT;
    off += 12;
    u32 total = off;
    w32(msg, 4, total);

    mos_mach_msg_args_t s = {(u64)msg, SEND | RCV, total, sizeof msg, reply, 0, 0};
    r = (int)mmsg(&s);
    if (r != 0) { print("machtest: FALHA envio/recv filho r=0x%x\n", (i64)(u32)r); return 1; }
    u32 id = r32(msg, 20), payload = r32(msg, 24);
    if (id != 0xA2 || payload != 0xCAFE) { print("machtest: FALHA resposta id=0x%x carga=0x%x\n", (i64)id, (i64)payload); return 1; }
    print("machtest: filho recebeu resposta id=0x%x carga=0x%x -> OK\n", (i64)id, (i64)payload);
    /* o pai escreveu na MESMA memória pelo mapeamento dele: aparece aqui sem cópia */
    if (strncmp(shm + 100, "resposta", 8)) { print("machtest: FALHA memoria compartilhada: filho nao ve a escrita do pai\n"); return 1; }
    print("machtest: memoria compartilhada OK (filho ve '%s' escrito pelo pai)\n", "resposta");
    return 0;
}

/* proteção registrada (mach_vm_region): mprotect(READ) numa faixa, RW no meio dela, e endereço sem registro */
static u8 protbuf[4 * 4096] __attribute__((aligned(4096)));
static int teste_protecao(void) {
    u64 base = (u64)protbuf;
    u64 p1[8] = {base, 4 * 4096, 1 /* READ */, 0,0,0,0,0};
    u64 p2[8] = {base + 4096, 4096, 3 /* RW */, 0,0,0,0,0};
    if (mvm(MACH_VM_PROTECT, p1) || mvm(MACH_VM_PROTECT, p2)) { print("machtest: FALHA vm_protect\n"); return 1; }
    u64 r0[8] = {base + 10, 0,0,0,0,0,0,0}, r1[8] = {base + 4096 + 8, 0,0,0,0,0,0,0}, r2[8] = {base + 3 * 4096, 0,0,0,0,0,0,0};
    u64 r3[8] = {base + 64 * 4096 * 16, 0,0,0,0,0,0,0};    /* longe: sem registro */
    if (mvm(MACH_VM_REGION, r0) || mvm(MACH_VM_REGION, r1) || mvm(MACH_VM_REGION, r2) || mvm(MACH_VM_REGION, r3)) {
        print("machtest: FALHA vm_region\n"); return 1;
    }
    int ok = r0[3] == 1 && r0[1] == base && r0[2] == 4096 &&       /* [base, base+4K): só leitura */
             r1[3] == 3 && r1[1] == base + 4096 && r1[2] == 4096 && /* o meio: RW */
             r2[3] == 1 && r2[1] == base + 2 * 4096 && r2[2] == 2 * 4096 &&   /* o resto da divisão: só leitura */
             r3[3] == 7;                                            /* sem registro: RWX, como antes */
    if (!ok) {
        print("machtest: FALHA protecao (r0 %d/%d r1 %d/%d r2 %d/%d r3 %d)\n", (i64)r0[3], (i64)r0[2],
              (i64)r1[3], (i64)r1[2], (i64)r2[3], (i64)r2[2], (i64)r3[3]);
        return 1;
    }
    print("machtest: protecao registrada OK (READ, RW no meio, divisao, sem registro = RWX)\n");
    return 0;
}

static int pai(void) {
    if (teste_protecao()) return 1;
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
    u32 off = 28, portname = 0, memport = 0; u64 ooladdr = 0;
    for (u32 i = 0; i < nd; i++) {
        u8 dt = msg[off+11];
        if (dt == D_PORT) { if (!portname) portname = r32(msg, off); else memport = r32(msg, off); off += 12; }
        else { ooladdr = r64(msg, off); off += 16; }
    }
    /* mapeia a entrada de memória recebida: tem de ver o que o filho escreveu, e o filho vê o que escrevemos */
    u64 mp[8] = {0, 0, 1 /* ANYWHERE */, memport, 0, 3, 0,0};
    if (!memport || mvm(MACH_VM_MAP, mp) != 0 || !mp[0]) { print("machtest: FALHA vm_map pai (porta %d)\n", (i64)(int)memport); return 1; }
    char *shm = (char *)mp[0];
    if (strncmp(shm, "compart", 7)) { print("machtest: FALHA pai nao ve a memoria do filho\n"); return 1; }
    for (int i = 0; i < 8; i++) shm[100 + i] = "resposta"[i];
    print("machtest: pai mapeou a memoria do filho e leu '%s'\n", "compart");
    int ok_ool = ooladdr && !strncmp((const char *)(u64)ooladdr, "ola-mach", 8);
    print("machtest: pai recebeu id=0x%x porta_transf=%d ool='%s'\n", (i64)id,
          (i64)(int)portname, ok_ool ? "ola-mach" : "?");
    if (id != 0xA1 || !portname || !ok_ool || !reply) {
        print("machtest: FALHA conteudo (reply=%d)\n", (i64)(int)reply);
        return 1;
    }
    /* pedidos de aviso, ANTES de o filho sair:
     *   - nome morto na porta que o filho transferiu (ele tem a recepção; ao sair, ela morre);
     *   - sem remetentes no serviço (o único envio é o do look_up do filho). */
    u32 np_dead = nova_porta(), np_ns = nova_porta();
    if (!np_dead || !np_ns) { print("machtest: FALHA alocar portas de aviso\n"); return 1; }
    u64 n1[8] = {portname, NOTE_DEAD_NAME, 0, np_dead, MAKE_SEND_ONCE, 0, 0, 0};
    u64 n2[8] = {svc, NOTE_NO_SENDERS, 0, np_ns, MAKE_SEND_ONCE, 0, 0, 0};
    if (mport(MACH_OP_NOTIFY, n1) || mport(MACH_OP_NOTIFY, n2)) { print("machtest: FALHA request_notification\n"); return 1; }
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

    /* o filho saiu: a porta dele morreu e o envio do serviço sumiu */
    u32 id1 = 0, arg1 = 0, id2 = 0, arg2 = 0;
    int k1 = espera_aviso(np_dead, &id1, &arg1), k2 = espera_aviso(np_ns, &id2, &arg2);
    if (k1 || id1 != NOTE_DEAD_NAME || arg1 != portname) {
        print("machtest: FALHA aviso de nome morto (kr=0x%x id=%d nome=%d, esperado %d)\n", (i64)(u32)k1, (i64)id1, (i64)arg1, (i64)portname);
        return 1;
    }
    u64 ty[8] = {portname, 0,0,0,0,0,0,0};
    mport(MACH_OP_TYPE, ty);
    u8 dm[32];
    for (int i = 0; i < 32; i++) dm[i] = 0;
    w32(dm, 0, (u32)COPY_SEND); w32(dm, 4, 24); w32(dm, 8, portname); w32(dm, 20, 1);
    mos_mach_msg_args_t ds = {(u64)dm, SEND, 24, 0, 0, 0, 0};
    i64 kd = mmsg(&ds);
    if (!(ty[1] & 0x100000u) || kd != 0x10000003) {
        print("machtest: FALHA nome morto (tipo 0x%x, envio kr=0x%x)\n", (i64)ty[1], (i64)(u32)kd);
        return 1;
    }
    print("machtest: aviso de nome morto OK (nome %d, tipo DEAD_NAME, envio -> SEND_INVALID_DEST)\n", (i64)portname);
    if (k2 || id2 != NOTE_NO_SENDERS) {
        print("machtest: FALHA sem remetentes entre processos (kr=0x%x id=%d)\n", (i64)(u32)k2, (i64)id2);
        return 1;
    }
    print("machtest: sem remetentes entre processos OK (o envio do filho sumiu ao sair)\n");

    /* sem remetentes no mesmo processo: make-send, pedido, deallocate do envio */
    u32 p = nova_porta(), np3 = nova_porta();
    u64 ir[8] = {p, p, MAKE_SEND, 0,0,0,0,0};
    u64 n3[8] = {p, NOTE_NO_SENDERS, 0, np3, MAKE_SEND_ONCE, 0, 0, 0};
    if (mport(MACH_OP_INSERT_RIGHT, ir) || mport(MACH_OP_NOTIFY, n3)) { print("machtest: FALHA preparar sem remetentes\n"); return 1; }
    u32 id3 = 0, arg3 = 0;
    if (espera_aviso_ms(np3, &id3, &arg3, 0) == 0) { print("machtest: FALHA aviso cedo demais (ainda havia envio)\n"); return 1; }
    u64 dl[8] = {p, 0,0,0,0,0,0,0};
    mport(MACH_OP_DEALLOCATE, dl);
    if (espera_aviso(np3, &id3, &arg3) || id3 != NOTE_NO_SENDERS || arg3 != 1) {
        print("machtest: FALHA sem remetentes local (id=%d mscount=%d)\n", (i64)id3, (i64)arg3);
        return 1;
    }
    print("machtest: sem remetentes local OK (mscount %d)\n", (i64)arg3);

    /* send-once não usado: o "servidor" destrói o pedido sem responder; o cliente bloqueado na porta de
     * resposta tem de receber MACH_NOTIFY_SEND_ONCE (71), como no rendezvous do Chromium */
    u32 srv = nova_porta(), rsp = nova_porta();
    u8 rq[32];
    for (int i = 0; i < 32; i++) rq[i] = 0;
    w32(rq, 0, (u32)MAKE_SEND | ((u32)MAKE_SEND_ONCE << 8)); w32(rq, 4, 24); w32(rq, 8, srv); w32(rq, 12, rsp); w32(rq, 20, 0x77);
    mos_mach_msg_args_t qs = {(u64)rq, SEND, 24, 0, 0, 0, 0};
    u8 got[96];
    mos_mach_msg_args_t qr = {(u64)got, RCV, 0, sizeof got, srv, 0, 0};
    if (mmsg(&qs) || mmsg(&qr)) { print("machtest: FALHA pedido para destruir\n"); return 1; }
    u64 dso[8] = {r32(got, 8), 0,0,0,0,0,0,0};       /* o send-once que veio no pedido */
    mport(MACH_OP_DEALLOCATE, dso);
    u32 id4 = 0, arg4 = 0;
    int k4 = espera_aviso(rsp, &id4, &arg4);
    if (k4 || id4 != 71) { print("machtest: FALHA aviso de send-once nao usado (kr=0x%x id=%d)\n", (i64)(u32)k4, (i64)id4); return 1; }
    print("machtest: send-once destruido sem resposta -> aviso 71 OK\n");

    /* servidor que morre: o serviço sai do registro (look_up -> 1102) e um novo check_in cria porta nova */
    int pid2 = fork();
    if (pid2 == 0) {
        u32 s2 = 0;
        exit(mbs(MACH_BS_CHECK_IN, "mOS.machtest.morto", &s2) ? 1 : 0);
    }
    int st2 = 0;
    waitany(&st2);
    u32 s3 = 0;
    i64 kl = mbs(MACH_BS_LOOK_UP, "mOS.machtest.morto", &s3);
    u32 s4 = 0;
    i64 kc = mbs(MACH_BS_CHECK_IN, "mOS.machtest.morto", &s4);
    u32 s5 = 0;
    i64 kl2 = mbs(MACH_BS_LOOK_UP, "mOS.machtest.morto", &s5);
    if (st2 != 0 || kl != 1102 || kc != 0 || !s4 || kl2 != 0) {
        print("machtest: FALHA servico morto (filho %d, look_up %d, check_in %d, look_up %d)\n", (i64)st2, kl, kc, kl2);
        return 1;
    }
    print("machtest: servico de processo morto -> 1102, novo check_in OK\n");

    /* largar a recepção com mod_refs(RECEIVE, -1): a porta morre, o aviso de nome morto sai e o nome
     * (que também tinha envio) vira DEAD_NAME */
    u32 q = nova_porta(), np4 = nova_porta();
    u64 iq[8] = {q, q, MAKE_SEND, 0,0,0,0,0};
    u64 n4[8] = {q, NOTE_DEAD_NAME, 0, np4, MAKE_SEND_ONCE, 0, 0, 0};
    u64 mr[8] = {q, RIGHT_RECEIVE, (u64)-1, 0,0,0,0,0};
    if (mport(MACH_OP_INSERT_RIGHT, iq) || mport(MACH_OP_NOTIFY, n4) || mport(MACH_OP_MOD_REFS, mr)) {
        print("machtest: FALHA preparar mod_refs\n"); return 1;
    }
    u32 id5 = 0, arg5 = 0;
    int k5 = espera_aviso(np4, &id5, &arg5);
    u64 tq[8] = {q, 0,0,0,0,0,0,0};
    mport(MACH_OP_TYPE, tq);
    if (k5 || id5 != NOTE_DEAD_NAME || arg5 != q || !(tq[1] & 0x100000u)) {
        print("machtest: FALHA mod_refs(RECEIVE,-1) (kr=0x%x id=%d nome=%d tipo=0x%x)\n", (i64)(u32)k5, (i64)id5, (i64)arg5, (i64)tq[1]);
        return 1;
    }
    print("machtest: mod_refs(RECEIVE,-1) mata a porta -> aviso 72 e DEAD_NAME OK\n");
    print("machtest: PASS\n");
    return 0;
}

void _start(const char *arg) {
    (void)arg;
    exit(pai());
}
