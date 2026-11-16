/* Teste de host do CoreMIDI (userland/libsystem/coremidi.c compilado de verdade).
 * uso: gcc -I userland/libsystem tools/coremidi_test.c userland/libsystem/coremidi.c -lpthread && ./a.out
 * Os valores UMP de MIDI 2.0 esperados vêm da tradução padrão (M2-104, escala mínimo-centro-máximo). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int OSStatus;
typedef void *id;
typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned long long u64;

/* o mínimo do runtime: strings no mesmo formato das constantes (isa, flags, ptr, len) e blocos sem cópia */
char cfisa[8] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } str_t;
id cx_str(const char *s) { str_t *o = calloc(1, sizeof *o); o->s = strdup(s); o->n = strlen(s); return o; }
const char *cx_cstr(id o) { return o ? ((str_t *)o)->s : 0; }
static int retains;
id objc_retain(id o) { retains++; return o; }
void objc_release(id o) { (void)o; retains--; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }

extern const void *kMIDIPropertyName, *kMIDIPropertyUniqueID, *kMIDIPropertyPrivate, *kMIDIPropertyDisplayName, *kMIDIPropertyManufacturer;
OSStatus MIDIClientCreate(id name, void *proc, void *ref, u32 *out);
OSStatus MIDIClientDispose(u32 c);
OSStatus MIDIInputPortCreate(u32 c, id name, void *proc, void *ref, u32 *out);
OSStatus MIDIInputPortCreateWithBlock(u32 c, id name, u32 *out, void *block);
OSStatus MIDIInputPortCreateWithProtocol(u32 c, id name, int proto, u32 *out, void *block);
OSStatus MIDIOutputPortCreate(u32 c, id name, u32 *out);
OSStatus MIDIPortConnectSource(u32 p, u32 s, void *ref);
OSStatus MIDIPortDisconnectSource(u32 p, u32 s);
OSStatus MIDISourceCreate(u32 c, id name, u32 *out);
OSStatus MIDISourceCreateWithProtocol(u32 c, id name, int proto, u32 *out);
OSStatus MIDIDestinationCreate(u32 c, id name, void *proc, void *ref, u32 *out);
OSStatus MIDIEndpointDispose(u32 e);
unsigned long MIDIGetNumberOfSources(void);
unsigned long MIDIGetNumberOfDestinations(void);
u32 MIDIGetSource(unsigned long i);
OSStatus MIDIReceived(u32 src, const void *list);
OSStatus MIDIReceivedEventList(u32 src, const void *list);
OSStatus MIDISend(u32 port, u32 dest, const void *list);
OSStatus MIDIObjectGetIntegerProperty(u32 o, const void *k, int *v);
OSStatus MIDIObjectSetIntegerProperty(u32 o, const void *k, int v);
OSStatus MIDIObjectGetStringProperty(u32 o, const void *k, id *v);
OSStatus MIDIObjectFindByUniqueID(int u, u32 *o, int *t);
void *MIDIPacketListInit(void *l);
void *MIDIPacketListAdd(void *l, unsigned long size, void *cur, u64 t, unsigned long n, const u8 *d);
void *MIDIEventListInit(void *l, int proto);
void *MIDIEventListAdd(void *l, unsigned long size, void *cur, u64 t, unsigned long n, const u32 *w);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static int notes_added, notes_removed, notes_setup;
static void notify(const void *msg, void *ref) {
    (void)ref;
    int idm = *(const int *)msg;
    if (idm == 2) notes_added++;
    else if (idm == 3) notes_removed++;
    else if (idm == 1) notes_setup++;
}
/* captura: bytes de MIDIPacketList ou palavras de MIDIEventList recebidas */
typedef struct { u8 b[256]; int nb, npk; u32 w[64]; int nw; void *conn; u64 ts; } cap_t;
static void read_pkts(const void *list, cap_t *c, void *conn) {
    const u8 *l = list, *p = l + 4;
    u32 np = *(const u32 *)l;
    c->conn = conn;
    c->npk += (int)np;
    for (u32 i = 0; i < np; i++) {
        unsigned short len;
        memcpy(&c->ts, p, 8);
        memcpy(&len, p + 8, 2);
        memcpy(c->b + c->nb, p + 10, len);
        c->nb += len;
        p += 10 + len;
    }
}
static void read_proc(const void *list, void *pref, void *conn) { read_pkts(list, pref, conn); }
static void read_evts(const void *list, cap_t *c, void *conn) {
    const u8 *l = list, *p = l + 8;
    u32 np = *(const u32 *)(l + 4);
    c->conn = conn;
    c->npk += (int)np;
    for (u32 i = 0; i < np; i++) {
        u32 wc;
        memcpy(&wc, p + 8, 4);
        memcpy(c->w + c->nw, p + 12, 4 * wc);
        c->nw += (int)wc;
        p += 12 + 4 * wc;
    }
}
/* blocos: {isa, flags, reserved, invoke, cap_t*} */
typedef struct blk { void *isa; int flags, res; void (*invoke)(struct blk *, const void *, void *); cap_t *c; } blk_t;
static void inv_pkts(blk_t *b, const void *l, void *conn) { read_pkts(l, b->c, conn); }
static void inv_evts(blk_t *b, const void *l, void *conn) { read_evts(l, b->c, conn); }

/* CF mínimo para o MIDIObjectGetProperties (dicionário de chaves cx_str) */
typedef struct { const char *k[32]; void *v[32]; int n; } fdict;
typedef struct { int v; } fnum;
const char kCFTypeDictionaryKeyCallBacks[1], kCFTypeDictionaryValueCallBacks[1];
void *CFDictionaryCreateMutable(void *a, long c, const void *k, const void *v) { (void)a; (void)c; (void)k; (void)v; return calloc(1, sizeof(fdict)); }
void CFDictionarySetValue(void *d, const void *k, const void *v) { fdict *f = d; f->k[f->n] = cx_cstr((id)k); f->v[f->n++] = (void *)v; }
void *CFNumberCreate(void *a, long t, const void *v) { (void)a; (void)t; fnum *n = malloc(sizeof *n); n->v = *(const int *)v; return n; }
void CFRelease(const void *o) { (void)o; }
static void *fget(fdict *d, const char *k) { for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i], k)) return d->v[i]; return 0; }
unsigned long MIDIDeviceGetNumberOfEntities(u32), MIDIEntityGetNumberOfSources(u32);
u32 MIDIDeviceGetEntity(u32, unsigned long), MIDIEntityGetSource(u32, unsigned long);
OSStatus MIDIEntityGetDevice(u32, u32 *), MIDIObjectGetProperties(u32, id *, unsigned char);
int main(void) {
    u32 ca, cb, src, src2, pin, pblk, pu1, pu2, pout, dst;
    CHECK(MIDIClientCreate(cx_str("A"), notify, 0, &ca) == 0, "cliente A");
    CHECK(MIDIClientCreate(cx_str("B"), notify, 0, &cb) == 0, "cliente B");
    CHECK(MIDISourceCreate(ca, cx_str("Teclado"), &src) == 0, "fonte virtual");
    CHECK(notes_added == 2 && notes_setup == 2, "notificações de adição %d/%d (esperado 2/2: os dois clientes)", notes_added, notes_setup);
    CHECK(MIDIGetNumberOfSources() == 1 && MIDIGetSource(0) == src, "enumeração de fontes");
    id nm = 0;
    CHECK(MIDIObjectGetStringProperty(src, kMIDIPropertyName, &nm) == 0 && nm && !strcmp(cx_cstr(nm), "Teclado"), "nome");
    CHECK(MIDIObjectGetStringProperty(src, cx_str("displayName"), &nm) == 0 && !strcmp(cx_cstr(nm), "Teclado"), "displayName pelo literal");
    CHECK(MIDIObjectGetStringProperty(src, kMIDIPropertyManufacturer, &nm) == -10835, "propriedade ausente devolve kMIDIUnknownProperty");
    int uid = 0, ty = 0;
    u32 found = 0;
    CHECK(MIDIObjectGetIntegerProperty(src, kMIDIPropertyUniqueID, &uid) == 0 && uid != 0, "uniqueID");
    CHECK(MIDIObjectFindByUniqueID(uid, &found, &ty) == 0 && found == src && ty == 2, "FindByUniqueID");
    {
        id props = 0;
        MIDIObjectSetIntegerProperty(src, cx_str("receiveChannels"), 3);
        CHECK(MIDIObjectGetProperties(src, &props, 1) == 0 && props && !strcmp(cx_cstr(fget(props, "name")), "Teclado") &&
              ((fnum *)fget(props, "uniqueID"))->v == uid && ((fnum *)fget(props, "receiveChannels"))->v == 3, "MIDIObjectGetProperties");
        u32 dv = 1;
        CHECK(MIDIDeviceGetNumberOfEntities(0) == 0 && MIDIDeviceGetEntity(0, 0) == 0 && MIDIEntityGetNumberOfSources(0) == 0 &&
              MIDIEntityGetSource(0, 0) == 0 && MIDIEntityGetDevice(0, &dv) == -10842 && dv == 0, "sem dispositivo de hardware: sem entidades");
    }
    printf("coremidi ok fonte virtual: enumeração, nome, uniqueID %d, notificações\n", uid);

    cap_t c_in = {0}, c_blk = {0}, c_u1 = {0}, c_u2 = {0}, c_dst = {0};
    blk_t b1 = {0, 0, 0, inv_pkts, &c_blk}, b2 = {0, 0, 0, inv_evts, &c_u1}, b3 = {0, 0, 0, inv_evts, &c_u2};
    MIDIInputPortCreate(cb, cx_str("in"), read_proc, &c_in, &pin);
    MIDIInputPortCreateWithBlock(cb, cx_str("blk"), &pblk, &b1);
    MIDIInputPortCreateWithProtocol(cb, cx_str("u1"), 1, &pu1, &b2);
    MIDIInputPortCreateWithProtocol(cb, cx_str("u2"), 2, &pu2, &b3);
    void *conn = (void *)0x1234;
    CHECK(MIDIPortConnectSource(pin, src, conn) == 0 && MIDIPortConnectSource(pblk, src, conn) == 0 &&
          MIDIPortConnectSource(pu1, src, 0) == 0 && MIDIPortConnectSource(pu2, src, 0) == 0, "ligações");
    /* nota 60 vel 100, running status nota 62 vel 0, CC7 127, pitch bend no centro, SysEx, program 5 */
    const u8 msg[] = {0x90, 60, 100, 62, 0, 0xB0, 7, 127, 0xE0, 0x00, 0x40, 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7, 0xC0, 5};
    u8 lst[256];
    void *cur = MIDIPacketListInit(lst);
    cur = MIDIPacketListAdd(lst, sizeof lst, cur, 777, 11, msg);
    cur = MIDIPacketListAdd(lst, sizeof lst, cur, 777, 6, msg + 11);   /* SysEx: pacote próprio */
    cur = MIDIPacketListAdd(lst, sizeof lst, cur, 777, 2, msg + 17);
    CHECK(*(u32 *)lst == 3, "MIDIPacketListAdd: %u pacotes (esperado 3: SysEx não junta)", *(u32 *)lst);
    CHECK(MIDIReceived(src, lst) == 0, "MIDIReceived");
    CHECK(c_in.nb == (int)sizeof msg && !memcmp(c_in.b, msg, sizeof msg) && c_in.conn == conn && c_in.ts == 777, "porta clássica recebeu %d bytes", c_in.nb);
    CHECK(c_blk.nb == (int)sizeof msg && !memcmp(c_blk.b, msg, sizeof msg), "porta com bloco");
    const u32 e1[] = {0x20903C64, 0x20903E00, 0x20B0077F, 0x20E00040, 0x30047E7F, 0x06010000, 0x20C00500};
    CHECK(c_u1.nw == 7 && !memcmp(c_u1.w, e1, sizeof e1), "UMP MIDI 1.0: %d palavras %08x %08x %08x %08x %08x %08x %08x", c_u1.nw,
          c_u1.w[0], c_u1.w[1], c_u1.w[2], c_u1.w[3], c_u1.w[4], c_u1.w[5], c_u1.w[6]);
    const u32 e2[] = {0x40903C00, 0xC9240000, 0x40803E00, 0x00000000, 0x40B00700, 0xFFFFFFFF, 0x40E00000, 0x80000000,
                      0x30047E7F, 0x06010000, 0x40C00000, 0x05000000};
    CHECK(c_u2.nw == 12 && !memcmp(c_u2.w, e2, sizeof e2), "UMP MIDI 2.0: %d palavras %08x %08x %08x %08x %08x %08x", c_u2.nw,
          c_u2.w[0], c_u2.w[1], c_u2.w[2], c_u2.w[3], c_u2.w[6], c_u2.w[7]);
    printf("coremidi ok MIDIReceived: bytes idênticos, UMP 1.0 e 2.0 (vel 100 -> 0xC924, CC 127 -> 0xFFFFFFFF, bend centro -> 0x80000000)\n");

    /* fonte MIDI 2.0: a mesma lista UMP 2.0 volta a bytes MIDI 1.0 iguais na porta clássica */
    MIDISourceCreateWithProtocol(ca, cx_str("MIDI2"), 2, &src2);
    MIDIPortConnectSource(pin, src2, 0);
    u8 el[1024];
    void *ec = MIDIEventListInit(el, 2);
    ec = MIDIEventListAdd(el, sizeof el, ec, 5, 4, e2);
    ec = MIDIEventListAdd(el, sizeof el, ec, 5, 4, e2 + 4);
    ec = MIDIEventListAdd(el, sizeof el, ec, 6, 4, e2 + 8);
    CHECK(*(u32 *)(el + 4) == 2, "MIDIEventListAdd juntou por carimbo: %u pacotes", *(u32 *)(el + 4));
    memset(&c_in, 0, sizeof c_in);
    CHECK(MIDIReceivedEventList(src2, el) == 0, "MIDIReceivedEventList");
    CHECK(c_in.nb == 20 && !memcmp(c_in.b, msg, 3) && c_in.b[3] == 0x80 && c_in.b[4] == 62 && !memcmp(c_in.b + 6, msg + 5, 14),
          "UMP 2.0 -> bytes: %d bytes", c_in.nb);
    printf("coremidi ok UMP 2.0 -> MIDI 1.0 de volta (%d bytes)\n", c_in.nb);

    /* destino virtual e MIDISend */
    MIDIDestinationCreate(ca, cx_str("Sintetizador"), read_proc, &c_dst, &dst);
    MIDIOutputPortCreate(cb, cx_str("out"), &pout);
    CHECK(MIDIGetNumberOfDestinations() == 1, "enumeração de destinos");
    CHECK(MIDISend(pout, dst, lst) == 0 && c_dst.nb == (int)sizeof msg, "MIDISend para destino virtual");
    CHECK(MIDISend(pout, src, lst) == -10832, "MIDISend para uma fonte devolve kMIDIWrongEndpointType");
    CHECK(MIDISend(pin, dst, lst) == -10831, "MIDISend por porta de entrada devolve kMIDIInvalidPort");
    CHECK(MIDIReceived(dst, lst) == -10832, "MIDIReceived num destino");
    printf("coremidi ok MIDISend e os erros do macOS\n");

    /* desligar, privado, uniqueID repetido, descarte */
    memset(&c_in, 0, sizeof c_in);
    MIDIPortDisconnectSource(pin, src);
    MIDIReceived(src, lst);
    CHECK(c_in.nb == 0, "porta desligada ainda recebeu");
    CHECK(MIDIPortDisconnectSource(pin, src) == -10833, "desligar de novo devolve kMIDINoConnection");
    MIDIObjectSetIntegerProperty(src2, kMIDIPropertyPrivate, 1);
    CHECK(MIDIGetNumberOfSources() == 1, "fonte privada apareceu na enumeração");
    int uid2 = 0;
    MIDIObjectGetIntegerProperty(src2, kMIDIPropertyUniqueID, &uid2);
    CHECK(MIDIObjectSetIntegerProperty(src2, kMIDIPropertyUniqueID, uid) == -10843, "uniqueID repetido devolve kMIDIIDNotUnique");
    CHECK(uid2 != uid && uid2 != 0, "uniqueIDs");
    int before = notes_removed;
    CHECK(MIDIEndpointDispose(src) == 0 && MIDIGetNumberOfSources() == 0 && notes_removed == before + 2, "descarte do endpoint");
    CHECK(MIDIClientDispose(ca) == 0 && MIDIGetNumberOfDestinations() == 0, "MIDIClientDispose leva os endpoints do cliente");
    CHECK(MIDIPortConnectSource(pin, src, 0) == -10834, "ligar a endpoint descartado devolve kMIDIUnknownEndpoint");
    MIDIClientDispose(cb);
    printf("coremidi ok desligar, privado, uniqueID único, descarte\n");
    printf("%s\n", fails ? "coremidi FALHA" : "coremidi: tudo ok");
    return fails != 0;
}
