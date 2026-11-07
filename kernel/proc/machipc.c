/*
 * mOS — portas Mach entre processos (IPC dos apps Mach-O; porte do VS Code/Electron,
 * docs/porte-vscode.md, seção "IPC Mach").
 *
 * Modelo (o do XNU, em escala menor):
 *   - kport_t é a porta: fila de mensagens, contadores de direitos, conjunto (port set) a que
 *     pertence e pedidos de notificação. Também representa conjuntos (KP_SET), objetos do kernel sem
 *     receptor (KP_KOBJ: task/host/thread e o bootstrap) e entradas de memória compartilhada (KP_MEM).
 *   - Cada processo tem um espaço de nomes (mspace_t): nome -> porta + direitos (receive, send com
 *     urefs, send-once, conjunto, nome morto). Envios para a mesma porta ficam num nome só.
 *   - mach_msg copia a mensagem para o kernel trocando nomes por portas (header, descritores de porta,
 *     de memória out-of-line e de vetores de portas). Na recepção os direitos entram no espaço de quem
 *     recebe, a memória OOL é mapeada e o trailer é acrescentado (o audit token leva o pid de quem
 *     enviou: o rendezvous do Chromium usa isso).
 *   - Quem recebe sem mensagem bloqueia (TH_WAITING, wait_pid = -7, campos mach_* de thread_t); quem
 *     envia entrega direto no buffer da thread que espera, como os pipes. O prazo é do escalonador.
 *   - Bootstrap: registro global de nomes de serviço. Todo processo tem o envio do bootstrap no nome
 *     0x303; filhos herdam o do pai ou a porta dada a posix_spawnattr_setspecialport_np.
 *
 * Syscalls 80..95 (syscall.h). Devolvem kern_return_t (ou -4 = interrompida por sinal). Syscalls
 * rodam com interrupções desligadas num núcleo só: não há travas aqui.
 */
#include "proc.h"
#include "syscall.h"

#define KERN_SUCCESS            0
#define KERN_INVALID_ADDRESS    1
#define KERN_NO_SPACE           3
#define KERN_INVALID_ARGUMENT   4
#define KERN_FAILURE            5
#define KERN_RESOURCE_SHORTAGE  6
#define KERN_NAME_EXISTS        13
#define KERN_INVALID_NAME       15
#define KERN_INVALID_RIGHT      17
#define KERN_INVALID_VALUE      18

#define SEND_INVALID_DATA   0x10000002
#define SEND_INVALID_DEST   0x10000003
#define SEND_TIMED_OUT      0x10000004
#define SEND_MSG_TOO_SMALL  0x10000008
#define SEND_INVALID_REPLY  0x10000009
#define SEND_INVALID_RIGHT  0x1000000a
#define SEND_TOO_LARGE      0x1000000e
#define SEND_INVALID_TYPE   0x1000000f
#define RCV_INVALID_NAME    0x10004002
#define RCV_TIMED_OUT       0x10004003
#define RCV_TOO_LARGE       0x10004004
#define RCV_PORT_CHANGED    0x10004006
#define RCV_INVALID_DATA    0x10004008
#define RCV_PORT_DIED       0x10004009

#define BS_NAME_IN_USE      1101
#define BS_UNKNOWN_SERVICE  1102
#define BS_SERVICE_ACTIVE   1103
#define BS_NO_MEMORY        1105

#define OPT_SEND      0x1
#define OPT_RCV       0x2
#define OPT_RCV_LARGE 0x4
#define OPT_SEND_TO   0x10
#define OPT_RCV_TO    0x100
#define MSGH_COMPLEX  0x80000000u

/* disposições (16..21) e tipos de direito recebidos (16 receive, 17 send, 18 send-once) */
#define MT_MOVE_RECEIVE   16
#define MT_MOVE_SEND      17
#define MT_MOVE_SEND_ONCE 18
#define MT_COPY_SEND      19
#define MT_MAKE_SEND      20
#define MT_MAKE_SEND_ONCE 21

#define RIGHT_SEND      0
#define RIGHT_RECEIVE   1
#define RIGHT_SEND_ONCE 2
#define RIGHT_PORT_SET  3
#define RIGHT_DEAD_NAME 4

#define T_SEND    0x10000u
#define T_RECEIVE 0x20000u
#define T_SO      0x40000u
#define T_PSET    0x80000u
#define T_DEAD    0x100000u

/* descritores no layout de 64 bits: o tipo fica sempre no byte 11 */
#define D_PORT      0
#define D_OOL       1
#define D_OOL_PORTS 2
#define D_OOL_VOL   3
#define D_GUARDED   4

#define NOTE_NO_SENDERS 70
#define NOTE_SEND_ONCE  71
#define NOTE_DEAD_NAME  72

#define BOOTSTRAP_NAME 0x303u    /* nome fixo do bootstrap em todo processo (variável bootstrap_port) */
#define WAIT_MACH      (-7)
#define MAX_MSG        (16u << 20)
#define MAX_OOL        (256u << 20)

enum { KP_PORT, KP_SET, KP_KOBJ, KP_MEM, KP_FILE };

/* fileports: cópias de descritores guardadas pelo process.c (proc_fd_snapshot) */
void *proc_fd_snapshot(process_t *p, int fd);
int   proc_fd_from_snapshot(process_t *p, const void *snap);
void  proc_fd_snapshot_free(void *snap);

typedef struct kmem { u64 *pages; u64 npages; int refs; } kmem_t;

struct mspace;
typedef struct dnreq { struct mspace *sp; u32 name; struct kport *notify; struct dnreq *next; } dnreq_t;

typedef struct kport {
    int refs;                 /* nomes, mensagens e registros que apontam para ela */
    u8 kind;
    bool dead;
    struct mspace *recv;      /* dono do direito de receber (NULL: em trânsito, morta ou objeto do kernel) */
    struct kmsg *qh, *qt;
    u32 qn, qlimit, seqno;
    u32 srights, sorights, mscount;
    u64 context;
    struct kport *pset;       /* conjunto ao qual pertence */
    struct kport *ns_notify;  /* pedido de "sem remetentes" (send-once guardado) */
    u32 ns_sync;
    dnreq_t *dnreqs;          /* pedidos de "nome morto" */
    kmem_t *mem;              /* KP_MEM */
    u64 moff, msize;
    u8 mprot;                 /* KP_MEM: proteção máxima (VM_PROT_READ 1 | WRITE 2 | EXECUTE 4) */
    void *file;               /* KP_FILE: cópia do descritor (fileport_makeport) */
    struct kport *gnext;      /* lista global (conjuntos procuram seus membros nela) */
} kport_t;

typedef struct {
    u8 kind, type;            /* D_*; tipo do direito em trânsito */
    u32 off;                  /* posição do descritor na mensagem */
    kport_t *port;
    u8 *buf; u32 size;        /* OOL */
    kport_t **ports; u8 ptype; u32 count;   /* vetor de portas */
} kitem_t;

typedef struct kmsg {
    struct kmsg *next;
    u8 *data; u32 size;       /* header + corpo como enviados */
    kport_t *dest; u8 dest_type;
    kport_t *reply; u8 reply_type;
    kitem_t *items; u32 nitems;
    int sender_pid;
    u32 seqno;
} kmsg_t;

typedef struct { u32 name, type; kport_t *port; u32 srefs, drefs; } ment_t;

typedef struct mspace {
    process_t *proc;
    int pid;
    ment_t *e;
    u32 n, cap;
    u32 next_idx;
    kport_t *spawn_special;   /* bootstrap do próximo filho */
    /* proteção REGISTRADA por faixa (initprot dos segmentos Mach-O, mprotect, mach_vm_protect), para
     * mach_vm_region responder como o macOS. Não muda a PTE: o V8 escreve em código marcado RX pelo
     * caminho MAP_JIT (pthread_jit_write_protect_np é vazio aqui), e uma PTE só-leitura o derrubaria. */
    struct prange { u64 lo, hi; u32 prot; } *pr;
    u32 npr, cappr;
} mspace_t;

static void prot_set(mspace_t *sp, u64 lo, u64 hi, u32 prot);

static kport_t *ports_all;
static kport_t *g_bootstrap;  /* o "launchd" do mOS: objeto do kernel, nunca liberado */
static struct { char name[128]; kport_t *port; } bs_reg[256];
static int log_left = 300;    /* diagnóstico no log serial, limitado */

static void mlog(const char *fmt, const char *s, u64 a, u64 b) {
    if (log_left <= 0) return;
    log_left--;
    kprintf(fmt, s ? s : "", a, b);
}

/* ------------------------------------------------------------ portas e memória */
static void kmem_unref(kmem_t *m) {
    if (!m || --m->refs > 0) return;
    for (u64 i = 0; i < m->npages; i++) if (m->pages[i]) uvm_page_put(m->pages[i]);
    kfree(m->pages);
    kfree(m);
}

static kport_t *port_new(u8 kind) {
    kport_t *p = kcalloc(1, sizeof *p);
    if (!p) return NULL;
    p->kind = kind;
    p->qlimit = 5;                         /* MACH_PORT_QLIMIT_DEFAULT */
    p->gnext = ports_all;
    ports_all = p;
    return p;
}

static void port_kill(kport_t *p);

static void port_ref(kport_t *p) { if (p) p->refs++; }
static void port_unref(kport_t *p) {
    if (!p || --p->refs > 0) return;
    if (p == g_bootstrap) { p->refs = 1; return; }
    if (!p->dead) port_kill(p);
    if (p->refs > 0) return;               /* port_kill pode ter criado referências (notificações) */
    for (kport_t **pp = &ports_all; *pp; pp = &(*pp)->gnext)
        if (*pp == p) { *pp = p->gnext; break; }
    kmem_unref(p->mem);
    if (p->file) proc_fd_snapshot_free(p->file);
    kfree(p);
}

/* ------------------------------------------------------------ espaços de nomes */
static ment_t *ent_at(mspace_t *sp, u32 name) {
    for (u32 i = 0; i < sp->n; i++) if (sp->e[i].name == name) return &sp->e[i];
    return NULL;
}

/* um envio para porta morta vira nome morto (como o XNU faz ao consultar o nome) */
static void ent_fix(ment_t *e) {
    if (e->port && e->port->dead && (e->type & (T_SEND | T_SO)) && !(e->type & T_RECEIVE)) {
        e->drefs = (e->type & T_SEND) ? (e->srefs ? e->srefs : 1) : 1;
        e->srefs = 0;
        e->type = T_DEAD;
    }
}

static ment_t *ent_add(mspace_t *sp, u32 name, u32 type, kport_t *port) {
    if (sp->n == sp->cap) {
        u32 nc = sp->cap ? sp->cap * 2 : 32;
        ment_t *ne = krealloc(sp->e, nc * sizeof *ne);
        if (!ne) return NULL;
        sp->e = ne;
        sp->cap = nc;
    }
    if (!name) {
        do name = (sp->next_idx++ << 8) | 3u; while (ent_at(sp, name));
    }
    ment_t *e = &sp->e[sp->n++];
    e->name = name;
    e->type = type;
    e->port = port;
    e->srefs = (type & T_SEND) ? 1 : 0;
    e->drefs = (type & T_DEAD) ? 1 : 0;
    return e;
}

/* tira a entrada (a referência dela à porta sai junto) */
static void ent_del(mspace_t *sp, ment_t *e) {
    kport_t *p = e->port;
    *e = sp->e[--sp->n];
    port_unref(p);
}

static ment_t *ent_find(mspace_t *sp, u32 name);

/* nomes "especiais" do libSystem (task 0x103, host 0x203, threads 0x1000+tid) não passam pelo
 * kernel até alguém enviá-los numa mensagem: aí viram um objeto do kernel com esse mesmo nome */
static ment_t *ent_find(mspace_t *sp, u32 name) {
    ment_t *e = ent_at(sp, name);
    if (e) { ent_fix(e); return e; }
    if (name >= 0x100 && name < 0x10000 && name != BOOTSTRAP_NAME) {
        kport_t *k = port_new(KP_KOBJ);
        if (!k) return NULL;
        port_ref(k);
        k->srights = 1;
        e = ent_add(sp, name, T_SEND, k);
        if (e) e->srefs = 1u << 24;
        return e;
    }
    return NULL;
}

/* entrada que já nomeia esta porta com envio ou recepção (para juntar direitos no mesmo nome) */
static ment_t *ent_of_port(mspace_t *sp, kport_t *p) {
    for (u32 i = 0; i < sp->n; i++)
        if (sp->e[i].port == p && (sp->e[i].type & (T_SEND | T_RECEIVE))) return &sp->e[i];
    return NULL;
}

static u32 name_of_recv(mspace_t *sp, kport_t *p) {
    for (u32 i = 0; i < sp->n; i++)
        if (sp->e[i].port == p && (sp->e[i].type & (T_RECEIVE | T_PSET))) return sp->e[i].name;
    return 0;
}

static mspace_t *sp_new(process_t *p) {
    mspace_t *sp = kcalloc(1, sizeof *sp);
    if (!sp) return NULL;
    sp->proc = p;
    sp->pid = p->pid;
    sp->next_idx = 0x1000;                 /* nomes a partir de 0x100003: longe dos nomes do libSystem */
    return sp;
}

static kport_t *bootstrap_port(void) {
    if (!g_bootstrap) { g_bootstrap = port_new(KP_KOBJ); if (g_bootstrap) g_bootstrap->refs = 1; }
    return g_bootstrap;
}

/* dá a sp um envio para port no nome fixo do bootstrap (a referência e o envio já são de quem chama) */
static void set_bootstrap(mspace_t *sp, kport_t *port) {
    ment_t *old = ent_at(sp, BOOTSTRAP_NAME);
    if (old) {
        if (old->port && (old->type & T_SEND)) old->port->srights--;
        ent_del(sp, old);
    }
    if (port) ent_add(sp, BOOTSTRAP_NAME, T_SEND, port);
}

/* diagnóstico (despejo de threads, process.c): portas deste processo com mensagens esperando na fila. Se a
 * navegação de uma janela para no meio, isto diz se a mensagem chegou e ninguém recebeu (fila cheia) ou se
 * nunca saiu do outro processo (filas vazias). Só lê: não cria o espaço de nomes. */
void machipc_dump_queues(process_t *p) {
    mspace_t *sp = p ? (mspace_t *)p->mach : NULL;
    if (!sp) return;
    int shown = 0;
    for (kport_t *k = ports_all; k && shown < 12; k = k->gnext) {
        if (k->recv != sp || !k->qn) continue;
        u32 name = name_of_recv(sp, k);
        u32 set = k->pset ? name_of_recv(sp, k->pset) : 0;
        u32 id = 0;
        if (k->qh && k->qh->data && k->qh->size >= 24) memcpy(&id, k->qh->data + 20, 4);
        kprintf("despejo:   %s pid %d: porta 0x%x com %u mensagem(ns) na fila (conjunto 0x%x, limite %u); a primeira: id %u, de pid %d\n",
                p->name, p->pid, name, k->qn, set, k->qlimit, id, k->qh ? k->qh->sender_pid : -1);
        shown++;
    }
}

static mspace_t *sp_of(process_t *p) {
    if (!p) return NULL;
    if (!p->mach) {
        mspace_t *sp = sp_new(p);
        if (!sp) return NULL;
        kport_t *b = bootstrap_port();
        if (b) { port_ref(b); b->srights++; set_bootstrap(sp, b); }
        p->mach = sp;
    }
    return p->mach;
}

/* ------------------------------------------------------------ cabeçalho e descritores do Mach
 * mach_msg_header_t (XNU): bits, size, remote(porta), local(porta), voucher, id. Em seguida, se
 * MSGH_COMPLEX, um u32 com a contagem de descritores. Lemos e escrevemos os campos por deslocamento
 * para não depender do alinhamento. */
#define H_BITS 0
#define H_SIZE 4
#define H_REMOTE 8
#define H_LOCAL 12
#define H_VOUCHER 16
#define H_ID 20
#define H_SIZE_BYTES 24

static u32 rd32(const u8 *b, u32 o) { return (u32)b[o] | (u32)b[o+1] << 8 | (u32)b[o+2] << 16 | (u32)b[o+3] << 24; }
static void wr32(u8 *b, u32 o, u32 v) { b[o] = v; b[o+1] = v >> 8; b[o+2] = v >> 16; b[o+3] = v >> 24; }
static u64 rd64(const u8 *b, u32 o) { return (u64)rd32(b, o) | (u64)rd32(b, o+4) << 32; }
static void wr64(u8 *b, u32 o, u64 v) { wr32(b, o, (u32)v); wr32(b, o+4, (u32)(v >> 32)); }
/* MACH_MSGH_BITS(remote, local) = remote | (local << 8): a disposição do destino fica no byte baixo */
static u8 bits_remote(u32 bits) { return bits & 0x1f; }
static u8 bits_local(u32 bits) { return (bits >> 8) & 0x1f; }

/* ------------------------------------------------------------ enfileirar e acordar */
static void send_notify(kport_t *note, int id, u32 arg);
static void so_drop_unused(kport_t *p);   /* send-once destruído sem uso: avisa quem recebe */

static void kmsg_free(kmsg_t *m) {
    if (!m) return;
    if (m->dest) port_unref(m->dest);
    if (m->reply) {
        if (m->reply_type == MT_MOVE_SEND_ONCE) so_drop_unused(m->reply);
        else port_unref(m->reply);
    }
    for (u32 i = 0; i < m->nitems; i++) {
        kitem_t *it = &m->items[i];
        if (it->kind == D_PORT && it->port) {
            if (it->type == MT_MOVE_SEND_ONCE) so_drop_unused(it->port);
            else port_unref(it->port);
        }
        else if ((it->kind == D_OOL || it->kind == D_OOL_VOL) && it->buf) kfree(it->buf);
        else if (it->kind == D_OOL_PORTS && it->ports) {
            for (u32 j = 0; j < it->count; j++) if (it->ports[j]) port_unref(it->ports[j]);
            kfree(it->ports);
        }
    }
    kfree(m->items);
    kfree(m->data);
    kfree(m);
}

static thread_t *waiter_of(kport_t *p) {
    for (thread_t *t = sched_threads(); t; t = t->next)
        if (t->state == TH_WAITING && t->wait_pid == WAIT_MACH && t->proc == p->recv->proc) {
            if (t->mach_wait == p) return t;
            if (t->mach_wait && ((kport_t *)t->mach_wait)->kind == KP_SET && p->pset == t->mach_wait) return t;
        }
    return NULL;
}

static u32 do_rcv(thread_t *t, kport_t *rcv, u8 *ubuf, u32 cap, u32 opt);

/* põe a mensagem na fila da porta e, se houver thread esperando por ela, entrega já */
static int enqueue(kport_t *dest, kmsg_t *m) {
    m->seqno = dest->seqno++;
    m->next = NULL;
    if (dest->qt) dest->qt->next = m; else dest->qh = m;
    dest->qt = m;
    dest->qn++;
    thread_t *w = dest->recv ? waiter_of(dest) : NULL;
    if (w) {
        int_frame_t *f = (int_frame_t *)w->frame;
        u32 r = w->mach_peek ? KERN_SUCCESS
                             : do_rcv(w, (kport_t *)w->mach_wait, (u8 *)w->mach_buf, w->mach_rsize, w->mach_opt);
        f->rax = r;
        w->mach_wait = NULL;
        w->mach_peek = false;
        w->mach_deadline = 0;
        w->state = TH_READY;
    }
    return KERN_SUCCESS;
}

/* send-once que ninguém usou (mach_msg_destroy, deallocate, mensagem descartada): o XNU entrega
 * MACH_NOTIFY_SEND_ONCE na porta de recepção. O cliente do rendezvous do Chromium está bloqueado
 * nela; sem este aviso ele espera para sempre, ou o processo trata a falta de portas como
 * "o pai morreu" e sai com código 0. `p` é a referência de quem solta o direito. */
static void so_drop_unused(kport_t *p) {
    if (!p) return;
    if (p->sorights) p->sorights--;
    if (!p->dead && p->recv) {
        port_ref(p);
        send_notify(p, NOTE_SEND_ONCE, 0);
    }
    port_unref(p);
}

/* notificação do kernel: mensagem simples (id) para a porta de notificação, com send-once consumido */
static void send_notify(kport_t *note, int id, u32 arg) {
    if (!note || note->dead || !note->recv) { port_unref(note); return; }
    kmsg_t *m = kcalloc(1, sizeof *m);
    u8 *d = kcalloc(1, H_SIZE_BYTES + 16);
    if (!m || !d) { kfree(m); kfree(d); port_unref(note); return; }
    wr32(d, H_BITS, MT_MOVE_SEND_ONCE);
    wr32(d, H_SIZE, H_SIZE_BYTES + 4);
    wr32(d, H_ID, id);
    wr32(d, H_SIZE_BYTES, arg);        /* corpo mínimo: nome morto / contagem */
    m->data = d;
    m->size = H_SIZE_BYTES + 4;
    m->dest = note;                    /* já é a nossa referência */
    enqueue(note, m);
}

/* ------------------------------------------------------------ destruir uma porta */
static void dead_name_notify(kport_t *p) {
    dnreq_t *r = p->dnreqs;
    p->dnreqs = NULL;
    while (r) {
        dnreq_t *nx = r->next;
        ment_t *e = r->sp ? ent_at(r->sp, r->name) : NULL;   /* nome morto do remetente */
        if (e) { e->type = T_DEAD; e->drefs = e->srefs ? e->srefs : 1; e->srefs = 0; }
        send_notify(r->notify, NOTE_DEAD_NAME, r->name);
        kfree(r);
        r = nx;
    }
}

/* a porta perdeu o receptor (dono saiu, ou moveu o direito e ela ficou sem dono): vira morta */
static void port_kill(kport_t *p) {
    if (p->dead || p->kind == KP_KOBJ) return;
    p->dead = true;
    p->recv = NULL;
    while (p->qh) { kmsg_t *m = p->qh; p->qh = m->next; kmsg_free(m); }
    p->qt = NULL;
    p->qn = 0;
    if (p->ns_notify) { send_notify(p->ns_notify, NOTE_NO_SENDERS, 0); p->ns_notify = NULL; }
    dead_name_notify(p);
    /* tira dos conjuntos: só zera o ponteiro, o conjunto varre a lista global ao receber */
    p->pset = NULL;
}

/* ------------------------------------------------------------ envio (copyin)
 * Resolve o direito de destino e de resposta pelos bits do header, consome/transforma os direitos no
 * espaço de quem envia e, se MSGH_COMPLEX, cada descritor: porta (como o header), memória OOL (copiada
 * para o kernel) e vetor de portas OOL. */

/* resolve o nome do destino/resposta num direito que viaja com a mensagem; devolve o tipo recebido
 * (16/17/18) ou 0 e um erro em *kr */
static kport_t *copyin_right(mspace_t *sp, u32 name, u8 disp, u8 *out_type, u32 *kr) {
    *kr = KERN_SUCCESS;
    *out_type = 0;
    if (!name) return NULL;
    ment_t *e = ent_find(sp, name);
    if (!e) { *kr = disp == MT_MAKE_SEND || disp == MT_MAKE_SEND_ONCE ? SEND_INVALID_RIGHT : SEND_INVALID_DEST; return NULL; }
    if (e->type & T_DEAD) { *kr = SEND_INVALID_DEST; return NULL; }
    kport_t *p = e->port;
    switch (disp) {
    case MT_COPY_SEND:
    case MT_MAKE_SEND:
        if (disp == MT_MAKE_SEND && !(e->type & T_RECEIVE)) { *kr = SEND_INVALID_RIGHT; return NULL; }
        if (disp == MT_COPY_SEND && !(e->type & T_SEND)) { *kr = SEND_INVALID_RIGHT; return NULL; }
        port_ref(p);
        p->srights++;
        *out_type = MT_MOVE_SEND;
        return p;
    case MT_MOVE_SEND:
        if (!(e->type & T_SEND)) { *kr = SEND_INVALID_RIGHT; return NULL; }
        port_ref(p);
        p->srights++;                       /* a referência do remetente vira a da mensagem */
        if (--e->srefs == 0) { e->type &= ~T_SEND; p->srights--; if (!e->type) ent_del(sp, e); }
        *out_type = MT_MOVE_SEND;
        return p;
    case MT_MAKE_SEND_ONCE:
        if (!(e->type & T_RECEIVE)) { *kr = SEND_INVALID_RIGHT; return NULL; }
        port_ref(p);
        p->sorights++;
        *out_type = MT_MOVE_SEND_ONCE;
        return p;
    case MT_MOVE_SEND_ONCE:
        if (!(e->type & T_SO)) { *kr = SEND_INVALID_RIGHT; return NULL; }
        port_ref(p);
        e->type &= ~T_SO;
        if (!e->type) ent_del(sp, e);
        *out_type = MT_MOVE_SEND_ONCE;
        return p;
    case MT_MOVE_RECEIVE:
        if (!(e->type & T_RECEIVE)) { *kr = SEND_INVALID_RIGHT; return NULL; }
        port_ref(p);
        p->recv = NULL;                     /* em trânsito: sem receptor até ser entregue */
        if (p->pset) p->pset = NULL;
        e->type &= ~(T_RECEIVE | T_PSET);
        if (!e->type) ent_del(sp, e);
        *out_type = MT_MOVE_RECEIVE;
        return p;
    default:
        *kr = SEND_INVALID_TYPE;
        return NULL;
    }
}

/* copia [addr, addr+size) para páginas novas, com os bytes no começo da primeira página.
 * O XNU entrega OOL assim: o endereço recebido é alinhado e os `size` bytes começam nele.
 * O canal Mojo (channel_mac) lê o payload em descriptor->address; se o deslocamento original
 * dentro da página fosse preservado, a mensagem chegaria zerada e o Ping nunca voltava. */
static kmem_t *ool_copyin(process_t *p, u64 addr, u64 size) {
    u64 npages = size ? (size + 0xFFF) / 4096 : 0;
    kmem_t *m = kcalloc(1, sizeof *m);
    if (!m) return NULL;
    m->refs = 1;
    m->npages = npages;
    m->pages = npages ? kcalloc(npages, sizeof *m->pages) : NULL;
    if (npages && !m->pages) { kfree(m); return NULL; }
    u64 done = 0;
    for (u64 i = 0; i < npages; i++) {
        u64 ph = pmm_alloc(1);
        if (!ph) { kmem_unref(m); return NULL; }
        memset((void *)ph, 0, 4096);
        u64 n = MIN(4096, size - done);
        if (n) uvm_copy_in(p, (void *)ph, addr + done, n);
        done += n;
        m->pages[i] = ph;
    }
    return m;
}

#define MAX_ITEMS 64

/* monta o kmsg a partir do buffer já copiado do usuário (data/size). Devolve 0 ou um erro de envio. */
static u32 build_kmsg(mspace_t *sp, u8 *data, u32 size, kmsg_t **out) {
    u32 bits = rd32(data, H_BITS);
    kmsg_t *m = kcalloc(1, sizeof *m);
    if (!m) return SEND_INVALID_DATA;
    m->data = data;
    m->size = size;
    m->sender_pid = sp->pid;
    u32 kr;
    m->dest = copyin_right(sp, rd32(data, H_REMOTE), bits_remote(bits), &m->dest_type, &kr);
    if (kr) { kfree(m); return kr; }
    if (!m->dest) { kfree(m); return SEND_INVALID_DEST; }
    u32 lbits = bits_local(bits);
    if (rd32(data, H_LOCAL) && lbits) {
        m->reply = copyin_right(sp, rd32(data, H_LOCAL), lbits, &m->reply_type, &kr);
        if (kr) { port_unref(m->dest); kfree(m); return kr == SEND_INVALID_DEST ? SEND_INVALID_REPLY : kr; }
    }
    if (bits & MSGH_COMPLEX) {
        u32 nd = rd32(data, H_SIZE_BYTES);
        if (nd > MAX_ITEMS) { port_unref(m->dest); port_unref(m->reply); kfree(m); return SEND_TOO_LARGE; }
        m->items = nd ? kcalloc(nd, sizeof(kitem_t)) : NULL;
        u32 off = H_SIZE_BYTES + 4;
        for (u32 i = 0; i < nd; i++) {
            if (off + 12 > size) break;
            u8 dtype = data[off + 11];
            kitem_t *it = &m->items[m->nitems++];
            it->kind = dtype;
            it->off = off;
            /* tamanhos reais do XNU (64 bits): descritor de porta = 12 bytes; OOL e OOL de portas = 16.
             * disposição no byte off+10, tipo no off+11; OOL guarda endereço em off..7 e tamanho em off+12 */
            if (dtype == D_PORT) {
                u32 nm = rd32(data, off);
                u8 disp = data[off + 10];
                it->port = copyin_right(sp, nm, disp, &it->type, &kr);
                if (kr) { m->nitems--; }    /* porta inválida: o descritor vira nulo na entrega */
                off += 12;
            } else if (dtype == D_OOL || dtype == D_OOL_VOL) {
                if (off + 16 > size) { m->nitems--; break; }
                u64 oaddr = rd64(data, off);
                u32 osize = rd32(data, off + 12);
                if (osize > MAX_OOL) { it->kind = 0xFF; off += 16; continue; }
                kmem_t *km = ool_copyin(sp->proc, oaddr, osize);
                if (km) { it->size = osize;
                          /* guardamos as páginas copiadas; a entrega mapeia no destino */
                          it->buf = (u8 *)km; }
                off += 16;
            } else if (dtype == D_OOL_PORTS) {
                if (off + 16 > size) { m->nitems--; break; }
                u64 oaddr = rd64(data, off);
                u32 cnt = rd32(data, off + 12);
                u8 disp = data[off + 10];
                if (cnt > 4096) { it->kind = 0xFF; off += 16; continue; }
                it->count = cnt;
                it->ptype = disp;
                it->ports = cnt ? kcalloc(cnt, sizeof(kport_t *)) : NULL;
                for (u32 j = 0; j < cnt; j++) {
                    u32 nm = 0;
                    uvm_copy_in(sp->proc, &nm, oaddr + j * 4, 4);
                    u8 t;
                    u32 kr2;
                    it->ports[j] = copyin_right(sp, nm, disp, &t, &kr2);
                }
                off += 16;
            } else { off += 16; }
        }
    }
    *out = m;
    return KERN_SUCCESS;
}

/* ------------------------------------------------------------ recepção (copyout)
 * Insere no espaço de quem recebe os direitos que vieram na mensagem (a referência da mensagem passa
 * a ser a da entrada), mapeia a memória OOL e escreve o trailer com o audit token do remetente. */

/* insere um direito recebido (rtype = 16 receive / 17 send / 18 send-once); a referência de `port`
 * (uma por direito na mensagem) é consumida. Devolve o nome no espaço do receptor. */
static u32 insert_recv_right(mspace_t *sp, kport_t *port, u8 rtype) {
    if (!port) return 0;
    if (rtype == MT_MOVE_RECEIVE) {
        ment_t *e = ent_of_port(sp, port);
        if (e) { e->type |= T_RECEIVE; port_unref(port); }
        else e = ent_add(sp, 0, T_RECEIVE, port);
        if (!e) { port_unref(port); return 0; }
        port->recv = sp;
        port->dead = false;
        return e->name;
    }
    if (rtype == MT_MOVE_SEND_ONCE) {
        ment_t *e = ent_add(sp, 0, T_SO, port);        /* send-once nunca junta com outro nome */
        if (!e) { port->sorights--; port_unref(port); return 0; }
        return e->name;
    }
    /* send: junta no nome que já existir para a porta */
    if (port->dead) {
        ment_t *e = ent_add(sp, 0, T_DEAD, port);
        return e ? e->name : 0;
    }
    ment_t *e = ent_of_port(sp, port);
    if (e) {
        e->type |= T_SEND;
        e->srefs++;
        port_unref(port);                               /* já tínhamos uma referência */
        return e->name;
    }
    e = ent_add(sp, 0, T_SEND, port);
    if (!e) { port->srights--; port_unref(port); return 0; }
    return e->name;
}

/* reserva um intervalo virtual no receptor (como o SYS_MMAP com b=1) e mapeia as páginas físicas */
static u64 map_ool(process_t *p, kmem_t *m) {
    if (!m || !m->npages) return 0;
    u64 len = m->npages * 4096;
    if (!p->mmap_next) p->mmap_next = USER_MMAP_BASE;
    u64 at = p->mmap_next;
    if (at + len > USER_END) return 0;
    for (u64 i = 0; i < m->npages; i++)
        if (!uvm_map_phys(p, at + i * 4096, m->pages[i], true)) return 0;
    p->mmap_next += len + 4096;
    return at;
}

/* trailer de auditoria (52 bytes): o rendezvous do Chromium lê o pid de audit_token_to_pid(msgh_audit).
 * mach_msg_audit_trailer_t: type(4) size(4) seqno(4) security_token(8) audit_token(32) */
#define TRAILER_SIZE 52
/* tamanho do trailer pedido pelo receptor (como no XNU): MACH_RCV_TRAILER_ELEMENTS(n) nos bits 24..27
 * das opções. 0 = só tipo e tamanho (8 bytes, mach_msg_trailer_t: é o que mensagens vazias como as
 * do WaitableEvent do Chromium esperam); 1 = + seqno (12); 2 = + security token (20); 3+ = + audit token (52). */
static u32 trailer_size(u32 opt) {
    u32 el = (opt >> 24) & 0xF;
    return el == 0 ? 8 : el == 1 ? 12 : el == 2 ? 20 : TRAILER_SIZE;
}
static void write_trailer(u8 *d, u32 body, u32 seqno, int pid, u32 tsize) {
    wr32(d, body, 0);                   /* MACH_MSG_TRAILER_FORMAT_0 */
    wr32(d, body + 4, tsize);           /* msgh_trailer_size */
    if (tsize >= 12) wr32(d, body + 8, seqno);
    if (tsize >= 20) wr64(d, body + 12, 0);              /* security token (uid, gid) */
    if (tsize >= TRAILER_SIZE) {
        for (int i = 0; i < 32; i++) d[body + 20 + i] = 0;
        wr32(d, body + 20, (u32)pid);       /* audit_token.val[0] = pid (uso do mOS) */
        wr32(d, body + 20 + 20, (u32)pid);  /* val[5] = pid (campo que audit_token_to_pid lê no XNU) */
    }
}

/* entrega a mensagem m (já tirada da fila) ao receptor sp, no buffer de usuário ubuf/cap. */
static u32 deliver(mspace_t *sp, kport_t *rcv, kmsg_t *m, u8 *ubuf, u32 cap, u32 opt) {
    process_t *p = sp->proc;
    u32 body = (m->size + 3) & ~3u;
    u32 trailer = trailer_size(opt);
    u32 need = body + trailer;
    if (need > cap) {
        if (opt & OPT_RCV_LARGE) {
            u8 hdr[H_SIZE_BYTES];
            memcpy(hdr, m->data, H_SIZE_BYTES);
            wr32(hdr, H_SIZE, need);             /* devolve só o tamanho; a mensagem fica na fila (requeue) */
            uvm_copy_out(p, (u64)ubuf, hdr, H_SIZE_BYTES);
            m->next = rcv->qh; rcv->qh = m; if (!rcv->qt) rcv->qt = m; rcv->qn++;
            return RCV_TOO_LARGE;
        }
        kmsg_free(m);                            /* sem RCV_LARGE: a mensagem é descartada */
        return RCV_TOO_LARGE;
    }
    u8 *d = m->data;
    u32 bits = rd32(d, H_BITS);
    /* reescreve o header: remote = direito de resposta (agora do receptor), local = nome da recepção */
    u32 rname = insert_recv_right(sp, m->reply, m->reply_type);
    m->reply = NULL;
    u32 lname = rcv ? name_of_recv(sp, rcv) : 0;
    u8 nremote = m->reply_type ? m->reply_type : 0;
    wr32(d, H_BITS, (bits & MSGH_COMPLEX) | (u32)nremote | ((u32)(lname ? MT_MOVE_RECEIVE : 0) << 8));
    wr32(d, H_REMOTE, rname);
    wr32(d, H_LOCAL, lname);
    /* o destino já foi consumido na recepção; libera-o */
    if (m->dest) { port_unref(m->dest); m->dest = NULL; }
    /* descritores */
    for (u32 i = 0; i < m->nitems; i++) {
        kitem_t *it = &m->items[i];
        if (it->kind == D_PORT) {
            u8 got = it->type;
            u32 nm = insert_recv_right(sp, it->port, got);
            it->port = NULL;
            wr32(d, it->off, nm);
            /* o receptor vê o tipo do direito que ganhou (PORT_SEND/RECEIVE/SEND_ONCE = 17/16/18).
             * COPY_SEND (19) e MAKE_SEND (20) deixados no descritor derrubam o canal Mojo. */
            if (got) { d[it->off + 10] = got; d[it->off + 11] = D_PORT; }
        } else if (it->kind == D_OOL || it->kind == D_OOL_VOL) {
            kmem_t *km = (kmem_t *)it->buf;
            u64 at = map_ool(p, km);
            wr64(d, it->off, at);
            wr32(d, it->off + 12, it->size);     /* tamanho fica no byte off+12 do descritor OOL */
            kmem_unref(km);
            it->buf = NULL;
        } else if (it->kind == D_OOL_PORTS) {
            u64 at = 0;
            u64 len = (it->count * 4 + 0xFFF) & ~0xFFFULL;
            if (it->count) {
                if (!p->mmap_next) p->mmap_next = USER_MMAP_BASE;
                at = p->mmap_next;
                if (uvm_map(p, at, len)) p->mmap_next += len + 4096; else at = 0;
            }
            for (u32 j = 0; j < it->count; j++) {
                u32 nm = insert_recv_right(sp, it->ports[j], it->ptype == MT_MAKE_SEND || it->ptype == MT_COPY_SEND || it->ptype == MT_MOVE_SEND ? MT_MOVE_SEND : it->ptype);
                it->ports[j] = NULL;
                if (at) uvm_copy_out(p, at + j * 4, &nm, 4);
            }
            wr64(d, it->off, at);
            kfree(it->ports);
            it->ports = NULL;
        }
    }
    /* o buffer da mensagem foi alocado com send_size; o trailer vem depois do corpo (body),
     * então aumenta-o para caber header+corpo+trailer antes de escrever */
    if (body + trailer > m->size) {
        u8 *nd = krealloc(m->data, body + trailer);
        if (!nd) { kmsg_free(m); return RCV_INVALID_DATA; }
        d = m->data = nd;
    }
    wr32(d, H_SIZE, m->size);                    /* msgh_size: só header+corpo, sem o trailer (como no XNU) */
    write_trailer(d, body, m->seqno, m->sender_pid, trailer);
    /* 'mrzv' / 'MRZV': pedido e resposta do MachPortRendezvous. Sem as portas o helper sai com exit(0). */
    {
        u32 id = rd32(d, H_ID);
        if (id == 0x6d727a76u || id == 0x4d525a56u)
            kprintf("mach: rendezvous id 0x%x de pid %d, %u porta(s), %u bytes\n", id, m->sender_pid, m->nitems, m->size);
    }
    if (!uvm_copy_out(p, (u64)ubuf, d, need)) { kmsg_free(m); return RCV_INVALID_DATA; }
    kmsg_free(m);
    return KERN_SUCCESS;
}

/* tira a próxima mensagem de rcv (porta ou conjunto) e entrega; RCV_TIMED_OUT se não há nenhuma */
static u32 do_rcv(thread_t *t, kport_t *rcv, u8 *ubuf, u32 cap, u32 opt) {
    mspace_t *sp = sp_of(t->proc);
    kport_t *src = rcv;
    if (rcv->kind == KP_SET) {
        src = NULL;
        for (kport_t *q = ports_all; q; q = q->gnext)
            if (q->pset == rcv && q->qn) { src = q; break; }
        if (!src) return RCV_TIMED_OUT;
    }
    if (rcv->dead || (rcv->recv != sp && rcv->kind != KP_SET)) return RCV_PORT_DIED;
    if (!src->qh) return RCV_TIMED_OUT;
    kmsg_t *m = src->qh;
    src->qh = m->next;
    if (!src->qh) src->qt = NULL;
    src->qn--;
    return deliver(sp, src, m, ubuf, cap, opt);
}

/* ------------------------------------------------------------ mach_msg (SYS_MACH_MSG) */
static u32 do_send(mspace_t *sp, u64 uaddr, u32 send_size, u32 opt, u32 timeout) {
    if (send_size < H_SIZE_BYTES || send_size > MAX_MSG) return SEND_MSG_TOO_SMALL;
    u8 *data = kmalloc(send_size);
    if (!data) return SEND_INVALID_DATA;
    if (!uvm_copy_in(sp->proc, data, uaddr, send_size)) { kfree(data); return SEND_INVALID_DATA; }
    wr32(data, H_SIZE, send_size);          /* o kernel confia no send_size, não no campo do usuário */
    kmsg_t *m = NULL;
    u32 kr = build_kmsg(sp, data, send_size, &m);   /* consome `data` em caso de sucesso */
    if (kr) { kfree(data); return kr; }
    kport_t *dest = m->dest;
    /* recv nulo com a porta viva: o direito de receber está em trânsito (MOVE_RECEIVE na resposta
     * do rendezvous). No XNU a mensagem entra na fila e espera o receptor; descartar aqui mata o
     * handshake do Mojo e o renderer sai com exit(0) depois de 15s sem Ping. */
    if (dest->dead) {
        kmsg_free(m);
        return SEND_INVALID_DEST;
    }
    u32 cap = dest->qlimit < 16 ? 16 : dest->qlimit;
    if (dest->qn >= cap && !(dest->recv && waiter_of(dest))) {
        if ((opt & OPT_SEND_TO) && timeout == 0) { kmsg_free(m); return SEND_TIMED_OUT; }
        /* sem espaço e ninguém esperando: o mOS não bloqueia o remetente; descarta com erro */
        kmsg_free(m);
        return SEND_TIMED_OUT;
    }
    return enqueue(dest, m);
}

static u64 sys_mach_msg(int_frame_t *f, thread_t *t, mspace_t *sp) {
    mos_mach_msg_args_t a;
    if (!uvm_copy_in(sp->proc, &a, f->rdi, sizeof a)) { f->rax = SEND_INVALID_DATA; return (u64)f; }
    u32 r = KERN_SUCCESS;
    if (a.option & OPT_SEND) {
        r = do_send(sp, a.msg, a.send_size, a.option, a.timeout);
        if (r || !(a.option & OPT_RCV)) { f->rax = r; return (u64)f; }
    }
    if (a.option & OPT_RCV) {
        ment_t *e = ent_find(sp, a.rcv_name);
        if (!e || !(e->type & (T_RECEIVE | T_PSET))) { f->rax = RCV_INVALID_NAME; return (u64)f; }
        kport_t *rcv = e->port;
        /* há mensagem? entrega já */
        bool has = rcv->kind == KP_SET ? false : rcv->qn > 0;
        if (rcv->kind == KP_SET)
            for (kport_t *q = ports_all; q; q = q->gnext) if (q->pset == rcv && q->qn) { has = true; break; }
        if (has) { f->rax = do_rcv(t, rcv, (u8 *)a.msg, a.rcv_size, a.option); return (u64)f; }
        if ((a.option & OPT_RCV_TO) && a.timeout == 0) { f->rax = RCV_TIMED_OUT; return (u64)f; }
        /* bloqueia até chegar mensagem (enqueue entrega e acorda) ou o prazo vencer */
        t->frame = (u64)f;
        t->mach_wait = rcv;
        t->mach_buf = a.msg;
        t->mach_rsize = a.rcv_size;
        t->mach_opt = a.option;
        t->mach_peek = false;
        t->mach_deadline = (a.option & OPT_RCV_TO) ? timer_ticks() + a.timeout : 0;
        t->wait_pid = WAIT_MACH;
        t->state = TH_WAITING;
        f->rax = RCV_TIMED_OUT;             /* valor caso o escalonador acorde pelo prazo */
        return sched_switch(f);
    }
    f->rax = r;
    return (u64)f;
}

/* ------------------------------------------------------------ mach_port_* (SYS_MACH_PORT) */
static u32 right_type_flag(int right) {
    return right == RIGHT_RECEIVE ? T_RECEIVE : right == RIGHT_PORT_SET ? T_PSET :
           right == RIGHT_SEND_ONCE ? T_SO : right == RIGHT_DEAD_NAME ? T_DEAD : T_SEND;
}

static u32 port_op(mspace_t *sp, u32 op, u64 *a) {
    switch (op) {
    case MACH_OP_ALLOCATE: {
        kport_t *p;
        u32 type;
        if (a[0] == RIGHT_PORT_SET) { p = port_new(KP_SET); type = T_PSET; }
        else if (a[0] == RIGHT_DEAD_NAME) { p = port_new(KP_PORT); if (p) p->dead = true; type = T_DEAD; }
        else { p = port_new(KP_PORT); type = T_RECEIVE; }
        if (!p) return KERN_RESOURCE_SHORTAGE;
        port_ref(p);
        if (type == T_RECEIVE) p->recv = sp;
        ment_t *e = ent_add(sp, (u32)a[1], type, p);
        if (!e) { port_unref(p); return KERN_NAME_EXISTS; }
        a[2] = e->name;
        return KERN_SUCCESS;
    }
    case MACH_OP_FILEPORT_MAKE: {
        void *s = proc_fd_snapshot(sp->proc, (int)a[0]);
        if (!s) return KERN_INVALID_ARGUMENT;
        kport_t *k = port_new(KP_FILE);
        if (!k) { proc_fd_snapshot_free(s); return KERN_RESOURCE_SHORTAGE; }
        k->file = s;
        port_ref(k);
        k->srights++;
        ment_t *e = ent_add(sp, 0, T_SEND, k);
        if (!e) { port_unref(k); return KERN_RESOURCE_SHORTAGE; }
        a[1] = e->name;
        return KERN_SUCCESS;
    }
    case MACH_OP_FILEPORT_FD: {
        ment_t *e = ent_find(sp, (u32)a[0]);
        if (!e || !e->port || e->port->kind != KP_FILE || !e->port->file || !(e->type & T_SEND)) return KERN_INVALID_ARGUMENT;
        int fd = proc_fd_from_snapshot(sp->proc, e->port->file);
        if (fd < 0) return KERN_RESOURCE_SHORTAGE;
        a[1] = (u64)fd;
        return KERN_SUCCESS;
    }
    case MACH_OP_CONSTRUCT: {
        kport_t *p = port_new(KP_PORT);
        if (!p) return KERN_RESOURCE_SHORTAGE;
        if (a[1]) p->qlimit = (u32)a[1];
        p->context = a[2];
        port_ref(p);
        p->recv = sp;
        ment_t *e = ent_add(sp, 0, T_RECEIVE, p);
        if (!e) { port_unref(p); return KERN_RESOURCE_SHORTAGE; }
        a[3] = e->name;
        return KERN_SUCCESS;
    }
    /* DEALLOCATE, MOD_REFS e GET_REFS aceitam os nomes especiais (task 0x103, host, threads) como o envio de
     * mensagem aceita (ent_find): o Chromium faz RetainMachSendRight(mach_task_self()) — mod_refs +1 — antes de
     * mandar a porta da task no ChildProcess.GetTaskPort; com KERN_INVALID_NAME o direito saía vazio e o
     * processo principal recusava a resposta (VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE) */
    case MACH_OP_DEALLOCATE:
    case MACH_OP_DESTRUCT: {
        ment_t *e = ent_find(sp, (u32)a[0]);
        if (!e) return KERN_INVALID_NAME;
        if (e->type & T_DEAD) { if (--e->drefs == 0) ent_del(sp, e); return KERN_SUCCESS; }
        if (e->type & T_SEND) {
            if (--e->srefs == 0) { if (e->port) e->port->srights--; e->type &= ~T_SEND; if (!e->type) ent_del(sp, e); }
            return KERN_SUCCESS;
        }
        if (op == MACH_OP_DESTRUCT && (e->type & T_RECEIVE)) { if (e->port) e->port->recv = NULL; ent_del(sp, e); return KERN_SUCCESS; }
        if (e->type & T_SO) {
            kport_t *p = e->port;
            port_ref(p);                 /* sobrevive ao ent_del para o aviso de send-once */
            ent_del(sp, e);
            so_drop_unused(p);
            return KERN_SUCCESS;
        }
        return KERN_INVALID_RIGHT;
    }
    case MACH_OP_MOD_REFS: {
        ment_t *e = ent_find(sp, (u32)a[0]);
        if (!e) return KERN_INVALID_NAME;
        i64 delta = (i64)a[2];
        if (a[1] == RIGHT_SEND && (e->type & T_SEND)) {
            if ((i64)e->srefs + delta < 0) return KERN_INVALID_VALUE;
            e->srefs += (u32)delta;
            if (e->srefs == 0) { if (e->port) e->port->srights--; e->type &= ~T_SEND; if (!e->type) ent_del(sp, e); }
        } else if (a[1] == RIGHT_DEAD_NAME && (e->type & T_DEAD)) {
            if ((i64)e->drefs + delta < 0) return KERN_INVALID_VALUE;
            e->drefs += (u32)delta;
            if (e->drefs == 0) ent_del(sp, e);
        } else if ((i64)delta < 0 && (e->type & (T_RECEIVE | T_SO | T_PSET))) {
            ent_del(sp, e);
        }
        return KERN_SUCCESS;
    }
    case MACH_OP_INSERT_RIGHT: {
        /* a0 = nome alvo, a1 = nome de origem (poly), a2 = disposição */
        ment_t *src = ent_find(sp, (u32)a[1]);
        if (!src) return KERN_INVALID_NAME;
        u8 t;
        u32 kr;
        kport_t *p = copyin_right(sp, (u32)a[1], (u8)a[2], &t, &kr);
        if (kr || !p) return KERN_INVALID_RIGHT;
        u8 rt = t == MT_MOVE_SEND_ONCE ? MT_MOVE_SEND_ONCE : t == MT_MOVE_RECEIVE ? MT_MOVE_RECEIVE : MT_MOVE_SEND;
        u32 want = (u32)a[0];
        if (want && !ent_at(sp, want)) {
            u32 ty = rt == MT_MOVE_RECEIVE ? T_RECEIVE : rt == MT_MOVE_SEND_ONCE ? T_SO : T_SEND;
            ment_t *e = ent_add(sp, want, ty, p);
            if (!e) { port_unref(p); return KERN_NAME_EXISTS; }
            if (rt == MT_MOVE_RECEIVE) p->recv = sp;
            return KERN_SUCCESS;
        }
        insert_recv_right(sp, p, rt);
        return KERN_SUCCESS;
    }
    case MACH_OP_EXTRACT_RIGHT: {
        u8 t;
        u32 kr;
        kport_t *p = copyin_right(sp, (u32)a[0], (u8)a[1], &t, &kr);
        if (kr || !p) return kr ? kr : KERN_INVALID_NAME;
        a[2] = insert_recv_right(sp, p, t == MT_MOVE_SEND_ONCE ? MT_MOVE_SEND_ONCE : t == MT_MOVE_RECEIVE ? MT_MOVE_RECEIVE : MT_MOVE_SEND);
        a[3] = t;
        return KERN_SUCCESS;
    }
    case MACH_OP_TYPE: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e) { if (ent_find(sp, (u32)a[0])) e = ent_at(sp, (u32)a[0]); }
        if (!e) return KERN_INVALID_NAME;
        a[1] = e->type;
        return KERN_SUCCESS;
    }
    case MACH_OP_GET_REFS: {
        ment_t *e = ent_find(sp, (u32)a[0]);
        if (!e) return KERN_INVALID_NAME;
        a[2] = a[1] == RIGHT_SEND ? e->srefs : a[1] == RIGHT_DEAD_NAME ? e->drefs :
               (e->type & right_type_flag((int)a[1])) ? 1 : 0;
        return KERN_SUCCESS;
    }
    case MACH_OP_INSERT_MEMBER:
    case MACH_OP_MOVE_MEMBER: {
        ment_t *pe = ent_at(sp, (u32)a[0]);
        if (!pe || !(pe->type & T_RECEIVE)) return KERN_INVALID_RIGHT;
        if (a[1] == 0) { pe->port->pset = NULL; return KERN_SUCCESS; }
        ment_t *se = ent_at(sp, (u32)a[1]);
        if (!se || !(se->type & T_PSET)) return KERN_INVALID_RIGHT;
        pe->port->pset = se->port;
        return KERN_SUCCESS;
    }
    case MACH_OP_EXTRACT_MEMBER: {
        ment_t *pe = ent_at(sp, (u32)a[0]);
        if (pe && pe->port) pe->port->pset = NULL;
        return KERN_SUCCESS;
    }
    case MACH_OP_NOTIFY: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port) return KERN_INVALID_NAME;
        u8 t;
        u32 kr;
        kport_t *note = a[3] ? copyin_right(sp, (u32)a[3], (u8)a[4], &t, &kr) : NULL;
        if (a[1] == NOTE_DEAD_NAME) {
            dnreq_t *r = kcalloc(1, sizeof *r);
            if (r) { r->sp = sp; r->name = (u32)a[0]; r->notify = note; r->next = e->port->dnreqs; e->port->dnreqs = r; }
            else port_unref(note);
            a[5] = 0;
        } else if (a[1] == NOTE_NO_SENDERS) {
            if (e->port->ns_notify) port_unref(e->port->ns_notify);
            e->port->ns_notify = note;
            e->port->ns_sync = (u32)a[2];
            a[5] = 0;
        } else port_unref(note);
        return KERN_SUCCESS;
    }
    case MACH_OP_SET_ATTR: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port) return KERN_INVALID_NAME;
        if (a[1] == 1 /* MACH_PORT_LIMITS_INFO */) e->port->qlimit = (u32)a[2];
        return KERN_SUCCESS;
    }
    case MACH_OP_STATUS: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port) return KERN_INVALID_NAME;
        kport_t *p = e->port;
        a[1] = p->qn; a[2] = p->seqno; a[3] = p->qlimit;
        a[4] = p->srights; a[5] = p->sorights; a[6] = p->mscount; a[7] = p->pset ? 1 : 0;
        return KERN_SUCCESS;
    }
    case MACH_OP_SET_CONTEXT: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port) return KERN_INVALID_NAME;
        e->port->context = a[1];
        return KERN_SUCCESS;
    }
    case MACH_OP_GET_CONTEXT: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port) return KERN_INVALID_NAME;
        a[1] = e->port->context;
        return KERN_SUCCESS;
    }
    case MACH_OP_PENDING: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port) { a[1] = 0; return KERN_SUCCESS; }
        kport_t *p = e->port;
        if (p->kind == KP_SET) {
            a[1] = 0; a[2] = 0;
            for (kport_t *q = ports_all; q; q = q->gnext)
                if (q->pset == p && q->qn) { a[1] = q->qn; a[2] = name_of_recv(sp, q); break; }
        } else { a[1] = p->qn; a[2] = (u32)a[0]; }
        return KERN_SUCCESS;
    }
    case MACH_OP_PEEK: {
        ment_t *e = ent_at(sp, (u32)a[0]);
        if (!e || !e->port || !e->port->qh) return KERN_FAILURE;
        kmsg_t *m = e->port->qh;
        a[1] = m->size; a[2] = m->seqno; a[3] = rd32(m->data, H_ID);
        return KERN_SUCCESS;
    }
    case MACH_OP_GET_SPECIAL: {
        if (a[0] == 4) {                      /* TASK_BOOTSTRAP_PORT */
            if (!ent_at(sp, BOOTSTRAP_NAME)) { kport_t *b = bootstrap_port(); if (b) { port_ref(b); b->srights++; set_bootstrap(sp, b); } }
            a[1] = BOOTSTRAP_NAME;
        } else a[1] = a[0] == 1 || a[0] == 2 ? 0x103 : 0x203;   /* kernel/host */
        return KERN_SUCCESS;
    }
    case MACH_OP_SET_SPECIAL: {
        if (a[0] == 4) {
            ment_t *e = ent_find(sp, (u32)a[1]);
            if (e && e->port && (e->type & T_SEND)) { port_ref(e->port); e->port->srights++; set_bootstrap(sp, e->port); }
        }
        return KERN_SUCCESS;
    }
    case MACH_OP_SPAWN_SPECIAL: {
        if (a[0] == 4) {
            if (sp->spawn_special) { sp->spawn_special->srights--; port_unref(sp->spawn_special); sp->spawn_special = NULL; }
            ment_t *e = ent_find(sp, (u32)a[1]);
            if (e && e->port && (e->type & T_SEND)) { port_ref(e->port); e->port->srights++; sp->spawn_special = e->port; }
        }
        return KERN_SUCCESS;
    }
    default:
        return KERN_INVALID_ARGUMENT;
    }
}

/* ------------------------------------------------------------ bootstrap (SYS_MACH_BOOTSTRAP)
 * O "launchd" do mOS: nomes de serviço globais. check_in cria/entrega o direito de RECEBER do serviço
 * a quem registra (o servidor); look_up entrega um direito de ENVIAR para quem procura. */
static int bs_find(const char *name) {
    for (int i = 0; i < 256; i++) if (bs_reg[i].port && !strcmp(bs_reg[i].name, name)) return i;
    return -1;
}

static u32 bootstrap_op(mspace_t *sp, u32 op, const char *name, u32 *out_name) {
    int i = bs_find(name);
    if (op == MACH_BS_CHECK_IN) {
        kport_t *p;
        if (i >= 0) {
            p = bs_reg[i].port;
            if (p->recv) return BS_SERVICE_ACTIVE;        /* já há um servidor */
        } else {
            for (i = 0; i < 256 && bs_reg[i].port; i++) {}
            if (i == 256) return BS_NO_MEMORY;
            p = port_new(KP_PORT);
            if (!p) return BS_NO_MEMORY;
            port_ref(p);                                  /* o registro global guarda uma referência */
            snprintf(bs_reg[i].name, sizeof bs_reg[i].name, "%s", name);
            bs_reg[i].port = p;
        }
        port_ref(p);
        p->recv = sp;
        p->dead = false;
        ment_t *e = ent_of_port(sp, p);
        if (e) { e->type |= T_RECEIVE; port_unref(p); } else e = ent_add(sp, 0, T_RECEIVE, p);
        if (!e) { port_unref(p); return BS_NO_MEMORY; }
        *out_name = e->name;
        mlog("mach: bootstrap check_in '%s' (pid %llu)\n", name, (u64)sp->pid, 0);
        return 0;
    }
    if (op == MACH_BS_LOOK_UP) {
        if (i < 0) { *out_name = 0; return BS_UNKNOWN_SERVICE; }
        kport_t *p = bs_reg[i].port;
        port_ref(p);
        p->srights++;
        *out_name = insert_recv_right(sp, p, MT_MOVE_SEND);
        mlog("mach: bootstrap look_up '%s' -> nome %llx (pid %llu)\n", name, (u64)*out_name, (u64)sp->pid);
        return 0;
    }
    if (op == MACH_BS_REGISTER) {
        /* registra um direito de envio que o chamador já tem (nome em *out_name, entrada) */
        ment_t *e = ent_find(sp, *out_name);
        if (!e || !(e->type & (T_SEND | T_RECEIVE))) return BS_NO_MEMORY;
        if (i < 0) { for (i = 0; i < 256 && bs_reg[i].port; i++) {} if (i == 256) return BS_NO_MEMORY; }
        else if (bs_reg[i].port) port_unref(bs_reg[i].port);
        port_ref(e->port);
        snprintf(bs_reg[i].name, sizeof bs_reg[i].name, "%s", name);
        bs_reg[i].port = e->port;
        if (e->type & T_RECEIVE) e->port->recv = sp;
        return 0;
    }
    return KERN_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------ CAContext entre processos
 * O Chromium (DisplayCALayerTree) cria um CAContext no processo da GPU e manda só o contextId ao
 * processo da janela, que o mostra num CALayerHost. O id é único no sistema. A publicação guarda a
 * entrada de memória do quadro achatado (cabeçalho + pixels); o outro processo pede a geração e,
 * quando ela muda, mapeia a mesma entrada. */
#define CA_SLOTS 64
typedef struct { u32 cid; int pid; u32 gen; kport_t *mem; } ca_slot_t;
static ca_slot_t ca_tab[CA_SLOTS];
static u32 ca_next_id = 0x5000;

static void ca_drop_pid(int pid) {
    for (int i = 0; i < CA_SLOTS; i++)
        if (ca_tab[i].mem && ca_tab[i].pid == pid) {
            port_unref(ca_tab[i].mem);
            ca_tab[i].mem = NULL;
            ca_tab[i].cid = 0;
            ca_tab[i].gen = 0;
        }
}
static int ca_find(u32 cid) {
    for (int i = 0; i < CA_SLOTS; i++) if (ca_tab[i].mem && ca_tab[i].cid == cid) return i;
    return -1;
}
static u32 ca_op(mspace_t *sp, u32 op, u64 *a) {
    if (op == MACH_VM_CA_ALLOC) {
        a[0] = ca_next_id++;
        if (ca_next_id < 0x5000) ca_next_id = 0x5000;
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_CA_GEN) {
        int i = ca_find((u32)a[0]);
        a[1] = i >= 0 ? ca_tab[i].gen : 0;
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_CA_DROP) {
        int i = ca_find((u32)a[0]);
        if (i >= 0 && ca_tab[i].pid == sp->pid) {
            port_unref(ca_tab[i].mem);
            ca_tab[i].mem = NULL;
            ca_tab[i].cid = 0;
            ca_tab[i].gen = 0;
        }
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_CA_PUBLISH) {
        ment_t *e = ent_at(sp, (u32)a[1]);
        if (!e || !e->port || e->port->kind != KP_MEM || !e->port->mem) return KERN_INVALID_ARGUMENT;
        int i = ca_find((u32)a[0]);
        if (i < 0) {
            for (int k = 0; k < CA_SLOTS; k++) if (!ca_tab[k].mem) { i = k; break; }
            if (i < 0) return KERN_RESOURCE_SHORTAGE;
        }
        if (ca_tab[i].mem != e->port) {
            if (ca_tab[i].mem) port_unref(ca_tab[i].mem);
            port_ref(e->port);
            ca_tab[i].mem = e->port;
        }
        ca_tab[i].cid = (u32)a[0];
        ca_tab[i].pid = sp->pid;
        if (!++ca_tab[i].gen) ca_tab[i].gen = 1;
        a[2] = ca_tab[i].gen;
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_CA_LOOKUP) {
        int i = ca_find((u32)a[0]);
        if (i < 0) return KERN_INVALID_ARGUMENT;
        kport_t *p = ca_tab[i].mem;
        port_ref(p);
        p->srights++;
        a[1] = insert_recv_right(sp, p, MT_MOVE_SEND);
        a[2] = p->msize;
        a[3] = ca_tab[i].gen;
        return a[1] ? KERN_SUCCESS : KERN_RESOURCE_SHORTAGE;
    }
    return KERN_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------ memória compartilhada (SYS_MACH_VM) */
static u32 vm_op(mspace_t *sp, u32 op, u64 *a) {
    if (op >= MACH_VM_CA_ALLOC && op <= MACH_VM_CA_DROP) return ca_op(sp, op, a);   /* só a faixa das CA (4..8) */
    if (op == MACH_VM_MAKE_ENTRY) {
        u64 size = (a[0] + 0xFFF) & ~0xFFFULL;
        kport_t *parent = NULL;
        if (a[3]) { ment_t *e = ent_at(sp, (u32)a[3]); if (e && e->port && e->port->kind == KP_MEM) parent = e->port; }
        kport_t *k = port_new(KP_MEM);
        if (!k) return KERN_RESOURCE_SHORTAGE;
        if (parent && parent->mem) { k->mem = parent->mem; k->mem->refs++; k->moff = parent->moff + a[1]; k->msize = size; k->mprot = parent->mprot & (a[2] & 7 ? (u8)(a[2] & 7) : 7); }
        else {
            /* MAP_MEM_NAMED_CREATE (0x20000): objeto NOVO, zerado, do tamanho pedido (o caso do
             * base::SharedMemory do Chromium). Sem ele: as páginas JÁ mapeadas em [offset, offset+size) do
             * chamador, compartilhadas (não copiadas): quem mapear a entrada vê os mesmos bytes. */
            bool create = (a[2] & 0x20000) != 0;
            u64 first = create ? 0 : (a[1] & ~0xFFFULL);
            u64 npages = create ? (size ? size : 4096) / 4096
                                : (((a[1] + (size ? size : 4096) + 0xFFF) & ~0xFFFULL) - first) / 4096;
            kmem_t *m = kcalloc(1, sizeof *m);
            u64 *pages = m && npages ? kcalloc(npages, sizeof *pages) : NULL;
            if (!m || (npages && !pages)) { kfree(m); kfree(pages); port_unref(k); return KERN_RESOURCE_SHORTAGE; }
            m->refs = 1;
            m->pages = pages;
            for (u64 i = 0; i < npages; i++, m->npages++) {
                u64 ph;
                if (create) {
                    ph = pmm_alloc(1);
                    if (!ph) break;
                    memset((void *)ph, 0, 4096);
                } else {
                    u64 va = first + i * 4096;
                    u64 *pte = uvm_pte(sp->proc, va);
                    if (!pte && uvm_map(sp->proc, va, 4096)) pte = uvm_pte(sp->proc, va);
                    if (!pte) break;
                    ph = *pte & 0x000FFFFFFFFFF000ULL;
                    uvm_page_get(ph);              /* o objeto passa a ser mais um dono da página */
                }
                pages[i] = ph;
            }
            if (m->npages != npages) { kmem_unref(m); port_unref(k); return create ? KERN_RESOURCE_SHORTAGE : KERN_INVALID_ADDRESS; }
            k->mem = m;
            k->moff = create ? 0 : (a[1] & 0xFFF);
            k->msize = size ? size : 4096;
            k->mprot = a[2] & 7 ? (u8)(a[2] & 7) : 7;   /* perm sem bits de proteção: tudo */
        }
        port_ref(k);
        k->srights++;
        ment_t *e = ent_add(sp, 0, T_SEND, k);
        if (!e) { port_unref(k); return KERN_RESOURCE_SHORTAGE; }
        a[4] = e->name;
        a[5] = size;
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_MAP) {
        ment_t *e = ent_at(sp, (u32)a[3]);
        if (!e || !e->port || e->port->kind != KP_MEM || !e->port->mem) {
            /* DIAGNÓSTICO TEMPORÁRIO (vm_map do Chromium no VS Code) */
            kprintf("mach[diag]: vm_map pid %d objeto 0x%x: %s (tipo de nome 0x%x, tipo de porta %d)\n", sp->pid, (u32)a[3],
                    !e ? "nome inexistente neste processo" : !e->port ? "nome sem porta" : e->port->kind != KP_MEM ? "não é entrada de memória" : "entrada sem páginas",
                    e ? e->type : 0, e && e->port ? e->port->kind : -1);
            return KERN_INVALID_ARGUMENT;
        }
        kmem_t *m = e->port->mem;
        /* a[1] = tamanho (0 = a entrada toda), a[2] = flags (bit 0 = VM_FLAGS_ANYWHERE), a[4] = deslocamento
         * dentro da entrada, a[5] = proteção atual (VM_PROT_WRITE = 2) */
        u64 start = e->port->moff + a[4];                 /* byte dentro do objeto */
        u64 len = a[1] ? a[1] : (e->port->msize > a[4] ? e->port->msize - a[4] : 0);
        u64 pfirst = start / 4096;
        u64 npages = ((start & 0xFFF) + len + 0xFFF) / 4096;
        if (!len || pfirst + npages > m->npages) {
            kprintf("mach[diag]: vm_map pid %d objeto 0x%x: fora da entrada (len 0x%llx, offset 0x%llx, moff 0x%llx, msize 0x%llx, %llu páginas)\n",
                    sp->pid, (u32)a[3], len, a[4], e->port->moff, e->port->msize, m->npages);   /* DIAGNÓSTICO TEMPORÁRIO */
            return KERN_INVALID_ARGUMENT;
        }
        u64 at = a[0] & ~0xFFFULL;
        if ((a[2] & 1) || !at) {                          /* VM_FLAGS_ANYWHERE: o kernel escolhe */
            if (!sp->proc->mmap_next) sp->proc->mmap_next = USER_MMAP_BASE;
            at = sp->proc->mmap_next;
            if (at + npages * 4096 > USER_END) return KERN_NO_SPACE;
            sp->proc->mmap_next += npages * 4096 + 4096;
        }
        /* proteção pedida (cur | max, a[5] | a[6]) além da que a entrada permite: KERN_INVALID_RIGHT, como no
         * XNU. O Chromium depende disso para conferir que uma região só-leitura é mesmo só-leitura
         * (PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode). */
        u8 want_prot = (u8)((a[5] | a[6]) & 7);
        if (want_prot & ~e->port->mprot) return KERN_INVALID_RIGHT;
        bool rw = (a[5] & 2) || (!a[5] && (e->port->mprot & 2));
        for (u64 i = 0; i < npages; i++)                  /* as MESMAS páginas físicas: memória compartilhada */
            if (!uvm_map_phys(sp->proc, at + i * 4096, m->pages[pfirst + i], rw)) return KERN_NO_SPACE;
        a[0] = at + (start & 0xFFF);
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_ENTRY_INFO) {
        ment_t *e = ent_at(sp, (u32)a[0]);
        a[1] = e && e->port && e->port->mem ? e->port->mem->npages * 4096 : 0;
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_PROTECT) {
        if (a[0] < USER_BASE || a[0] + a[1] > USER_END || a[0] + a[1] < a[0]) return KERN_INVALID_ADDRESS;
        prot_set(sp, a[0], a[0] + a[1], (u32)a[2]);
        return KERN_SUCCESS;
    }
    if (op == MACH_VM_REGION) {
        u64 addr = a[0];
        if (addr < USER_BASE || addr >= USER_END) return KERN_INVALID_ADDRESS;
        /* faixa registrada que contém o endereço; sem registro: a página, RWX (o NX está desligado) */
        for (u32 i = 0; i < sp->npr; i++)
            if (addr >= sp->pr[i].lo && addr < sp->pr[i].hi) {
                a[1] = sp->pr[i].lo;
                a[2] = sp->pr[i].hi - sp->pr[i].lo;
                a[3] = sp->pr[i].prot;
                a[4] = 7;
                return KERN_SUCCESS;
            }
        a[1] = addr & ~0xFFFULL;
        a[2] = 4096;
        a[3] = 7;
        a[4] = 7;
        return KERN_SUCCESS;
    }
    return KERN_INVALID_ARGUMENT;
}

/* ------------------------------------------------------------ proteção registrada (mach_vm_region) */
#define MAX_PRANGES 8192
static void prot_set(mspace_t *sp, u64 lo, u64 hi, u32 prot) {
    lo &= ~0xFFFULL;
    hi = (hi + 0xFFF) & ~0xFFFULL;
    if (hi <= lo) return;
    /* tira [lo, hi) das faixas existentes (cortando ou dividindo), depois acrescenta a nova */
    for (u32 i = 0; i < sp->npr; i++) {
        struct prange *r = &sp->pr[i];
        if (r->hi <= lo || r->lo >= hi) continue;
        if (r->lo < lo && r->hi > hi) {                   /* a nova cai no meio: divide em duas */
            if (sp->npr == sp->cappr) {                   /* garante espaço ANTES de mexer (r pode mudar de lugar) */
                if (sp->npr >= MAX_PRANGES) return;
                u32 nc = sp->cappr ? sp->cappr * 2 : 64;
                struct prange *n = krealloc(sp->pr, nc * sizeof *n);
                if (!n) return;
                sp->pr = n; sp->cappr = nc;
                r = &sp->pr[i];
            }
            struct prange tail = {hi, r->hi, r->prot};
            r->hi = lo;
            sp->pr[sp->npr++] = tail;
            continue;
        }
        if (r->lo < lo) r->hi = lo;                       /* corta a ponta direita */
        else if (r->hi > hi) r->lo = hi;                  /* corta a ponta esquerda */
        else { sp->pr[i--] = sp->pr[--sp->npr]; }         /* coberta por inteiro: sai */
    }
    if (sp->npr >= MAX_PRANGES) return;                   /* tabela cheia: fica o padrão (RWX) */
    if (sp->npr == sp->cappr) {
        u32 nc = sp->cappr ? sp->cappr * 2 : 64;
        struct prange *n = krealloc(sp->pr, nc * sizeof *n);
        if (!n) return;
        sp->pr = n; sp->cappr = nc;
    }
    sp->pr[sp->npr++] = (struct prange){lo, hi, prot & 7};
}

void machipc_set_prot(process_t *p, u64 lo, u64 hi, u32 prot) {
    mspace_t *sp = sp_of(p);
    if (sp) prot_set(sp, lo, hi, prot);
}

/* ------------------------------------------------------------ entrada das syscalls */
u64 machipc_syscall(int_frame_t *f) {
    thread_t *t = thread_current();
    process_t *proc = t ? t->proc : NULL;
    mspace_t *sp = sp_of(proc);
    if (!sp) { f->rax = (u64)KERN_FAILURE; return (u64)f; }
    switch (f->rax) {
    case SYS_MACH_MSG:
        return sys_mach_msg(f, t, sp);
    case SYS_MACH_PORT: {
        u64 a[8] = {0};
        if (!uvm_copy_in(proc, a, f->rsi, sizeof a)) { f->rax = KERN_INVALID_ADDRESS; return (u64)f; }
        /* espera haver mensagem, sem receber (a fonte MACH_RECV do libdispatch). Sem isto a thread
         * do servidor de rendezvous girava em vão e podia atender o pedido antes de o pai registrar
         * as portas do filho — o renderer do Electron então sai com código 0. */
        if ((u32)f->rdi == MACH_OP_WAIT) {
            ment_t *e = ent_find(sp, (u32)a[0]);
            if (!e || !e->port || !(e->type & (T_RECEIVE | T_PSET))) { f->rax = KERN_INVALID_NAME; return (u64)f; }
            kport_t *rcv = e->port;
            bool has = rcv->kind == KP_SET ? false : rcv->qn > 0;
            if (rcv->kind == KP_SET)
                for (kport_t *q = ports_all; q; q = q->gnext)
                    if (q->pset == rcv && q->qn) { has = true; break; }
            if (has) { f->rax = KERN_SUCCESS; return (u64)f; }
            u64 ms = a[1];
            if (!ms) { f->rax = KERN_SUCCESS; return (u64)f; }   /* prazo 0: só consulta */
            t->frame = (u64)f;
            t->mach_wait = rcv;
            t->mach_peek = true;
            t->mach_buf = 0;
            t->mach_rsize = 0;
            t->mach_opt = 0;
            t->mach_deadline = ms == ~0ULL ? 0 : timer_ticks() + ms;
            t->wait_pid = WAIT_MACH;
            t->state = TH_WAITING;
            f->rax = KERN_SUCCESS;           /* prazo ou mensagem: quem chamou olha cx_mach_pending */
            return sched_switch(f);
        }
        u32 r = port_op(sp, (u32)f->rdi, a);
        uvm_copy_out(proc, f->rsi, a, sizeof a);
        f->rax = r;
        return (u64)f;
    }
    case SYS_MACH_BOOTSTRAP: {
        char name[128];
        u32 out = 0;
        if (f->rdi == MACH_BS_REGISTER) uvm_copy_in(proc, &out, f->rdx, 4);
        /* o nome vem em rsi (string de usuário) */
        for (int i = 0; i < 127; i++) { if (!uvm_copy_in(proc, &name[i], f->rsi + i, 1)) { name[i] = 0; break; } if (!name[i]) break; }
        name[127] = 0;
        u32 r = bootstrap_op(sp, (u32)f->rdi, name, &out);
        if (f->rdx) uvm_copy_out(proc, f->rdx, &out, 4);
        f->rax = r;
        return (u64)f;
    }
    case SYS_MACH_VM: {
        u64 a[8] = {0};
        if (!uvm_copy_in(proc, a, f->rsi, sizeof a)) { f->rax = KERN_INVALID_ADDRESS; return (u64)f; }
        u32 r = vm_op(sp, (u32)f->rdi, a);
        uvm_copy_out(proc, f->rsi, a, sizeof a);
        f->rax = r;
        return (u64)f;
    }
    default:
        f->rax = (u64)KERN_INVALID_ARGUMENT;
        return (u64)f;
    }
}

/* ------------------------------------------------------------ ciclo de vida do processo */
void machipc_spawn(process_t *parent, process_t *child) {
    if (!child) return;
    bool fresh = !child->mach;                            /* fork: o carregador Mach-O não passou por aqui */
    mspace_t *sp = sp_of(child);
    if (!sp) return;
    mspace_t *psp = parent ? parent->mach : NULL;
    /* fork: mesma memória do pai, então a mesma proteção registrada. (Um filho Mach-O de posix_spawn já
     * registrou a dos próprios segmentos no macho_load, antes de chegar aqui.) */
    if (fresh && psp && psp->npr && !sp->npr) {
        sp->pr = kmalloc(psp->npr * sizeof *sp->pr);
        if (sp->pr) { memcpy(sp->pr, psp->pr, psp->npr * sizeof *sp->pr); sp->npr = sp->cappr = psp->npr; }
    }
    /* herda a porta de bootstrap: a escolhida por posix_spawnattr_setspecialport_np, senão a do pai */
    kport_t *inherit = NULL;
    if (psp) {
        if (psp->spawn_special) {
            inherit = psp->spawn_special;
            psp->spawn_special = NULL;                    /* vale para um filho só */
        } else {
            ment_t *e = ent_at(psp, BOOTSTRAP_NAME);
            if (e && e->port && (e->type & T_SEND)) { inherit = e->port; port_ref(inherit); inherit->srights++; }
        }
    }
    if (inherit) set_bootstrap(sp, inherit);              /* consome a referência */
}

void machipc_exit(process_t *p) {
    mspace_t *sp = p ? p->mach : NULL;
    if (!sp) return;
    ca_drop_pid(sp->pid);
    p->mach = NULL;
    if (sp->spawn_special) { sp->spawn_special->srights--; port_unref(sp->spawn_special); }
    /* tira os serviços que este processo servia do registro global (viram "serviço inativo") */
    for (int i = 0; i < 256; i++)
        if (bs_reg[i].port && bs_reg[i].port->recv == sp) bs_reg[i].port->recv = NULL;
    /* larga cada nome; as portas que ficam sem receptor morrem e notificam (dentro de port_unref) */
    for (u32 i = 0; i < sp->n; i++) {
        kport_t *port = sp->e[i].port;
        if (port && (sp->e[i].type & T_RECEIVE) && port->recv == sp) { port->recv = NULL; port_kill(port); }
    }
    while (sp->n) { kport_t *port = sp->e[sp->n - 1].port; sp->n--; port_unref(port); }
    kfree(sp->e);
    kfree(sp->pr);
    kfree(sp);
}
