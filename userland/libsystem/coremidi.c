/*
 * mOS libSystem — CoreMIDI.
 *
 * Servidor MIDI dentro do processo: clientes, portas de entrada e de saída, fontes e destinos virtuais
 * (MIDISourceCreate/MIDIDestinationCreate), ligações porta→fonte, MIDIReceived/MIDISend entregando na hora aos
 * leitores (como o MIDIServer faz com endpoints virtuais: o carimbo de tempo vai junto, quem recebe agenda),
 * propriedades (nome, uniqueID, privado e as do app), notificações de objeto adicionado/removido e de setup.
 * MIDI 1.0 em MIDIPacketList e UMP em MIDIEventList (protocolos 1.0 e 2.0): cada leitor recebe no formato e no
 * protocolo em que se registrou, com a tradução padrão da especificação (M2-104, escala mínimo-centro-máximo).
 * Dispositivos de hardware (USB MIDI) e endpoints de outros processos não existem ainda: a enumeração só mostra os
 * endpoints virtuais deste processo.
 */
#include "sys.h"

typedef i32 OSStatus;
typedef void *id;
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strncpy(char *d, const char *s, size_t n);
id cx_str(const char *s);
const char *cx_cstr(id o);
id objc_retain(id o);
void objc_release(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);

#define kMIDIInvalidClient (-10830)
#define kMIDIInvalidPort (-10831)
#define kMIDIWrongEndpointType (-10832)
#define kMIDINoConnection (-10833)
#define kMIDIUnknownEndpoint (-10834)
#define kMIDIUnknownProperty (-10835)
#define kMIDIWrongPropertyType (-10836)
#define kMIDIObjectNotFound (-10842)
#define kMIDIIDNotUnique (-10843)
#define kMIDINotPermitted (-10844)
#define kParamErr (-50)

/* constantes CFString das propriedades, com os valores do macOS */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } midi_const_t;
#define MK(var, text) static midi_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
MK(kMIDIPropertyName, "name")
MK(kMIDIPropertyManufacturer, "manufacturer")
MK(kMIDIPropertyModel, "model")
MK(kMIDIPropertyUniqueID, "uniqueID")
MK(kMIDIPropertyDeviceID, "deviceID")
MK(kMIDIPropertyReceiveChannels, "receiveChannels")
MK(kMIDIPropertyTransmitChannels, "transmitChannels")
MK(kMIDIPropertyMaxSysExSpeed, "maxSysExSpeed")
MK(kMIDIPropertyAdvanceScheduleTimeMuSec, "scheduleAheadMuSec")
MK(kMIDIPropertyIsEmbeddedEntity, "embedded")
MK(kMIDIPropertyIsBroadcast, "broadcast")
MK(kMIDIPropertySingleRealtimeEntity, "realtimeEntity")
MK(kMIDIPropertyConnectionUniqueID, "connUniqueID")
MK(kMIDIPropertyOffline, "offline")
MK(kMIDIPropertyPrivate, "private")
MK(kMIDIPropertyDriverOwner, "driver")
MK(kMIDIPropertyDriverVersion, "driverVersion")
MK(kMIDIPropertyDisplayName, "displayName")
MK(kMIDIPropertyImage, "image")
MK(kMIDIPropertyProtocolID, "protocol")
MK(kMIDIPropertyUMPActiveGroupBitmap, "UMPActiveGroupBitmap")
MK(kMIDIPropertyUMPCanTransmitGroupless, "UMPCanTransmitGroupless")

/* ---- formatos (pack 4, como no MIDIServices.h) */
#define PKT_HDR 10                      /* MIDIPacket: u64 timeStamp, u16 length, u8 data[] */
#define EVT_HDR 12                      /* MIDIEventPacket: u64 timeStamp, u32 wordCount, u32 words[64] */
static u8 *pkt_next(u8 *p) {
    u16 len;
    memcpy(&len, p + 8, 2);
    uintptr_t n = (uintptr_t)(p + PKT_HDR + len);
#if defined(__aarch64__) || defined(__arm__)
    n = (n + 3) & ~(uintptr_t)3;        /* MIDIPacketNext alinha em 4 bytes no ARM */
#endif
    return (u8 *)n;
}
static u8 *evt_next(u8 *p) { u32 wc; memcpy(&wc, p + 8, 4); return p + EVT_HDR + 4 * (uintptr_t)wc; }

void *MIDIPacketListInit(void *list) { u8 *l = list; *(u32 *)l = 0; return l + 4; }
/* Acrescenta depois de cur; mesmo carimbo e nenhum dos dois SysEx: junta no pacote atual (como o macOS). */
void *MIDIPacketListAdd(void *list, unsigned long size, void *cur, u64 time, unsigned long n, const u8 *data) {
    u8 *l = list, *p = cur;
    u32 np = *(u32 *)l;
    if (!l || !p || (n && !data) || n > 65535) return 0;
    if (np) {
        u64 t; u16 len;
        memcpy(&t, p, 8);
        memcpy(&len, p + 8, 2);
        if (t == time && n && len && data[0] != 0xF0 && p[PKT_HDR] != 0xF0 && (unsigned long)len + n <= 65535 &&
            p + PKT_HDR + len + n <= l + size) {
            memcpy(p + PKT_HDR + len, data, n);
            len = (u16)(len + n);
            memcpy(p + 8, &len, 2);
            return p;
        }
        p = pkt_next(p);
    }
    if (p + PKT_HDR + n > l + size) return 0;
    memcpy(p, &time, 8);
    u16 len = (u16)n;
    memcpy(p + 8, &len, 2);
    memcpy(p + PKT_HDR, data, n);
    *(u32 *)l = np + 1;
    return p;
}
/* MIDIEventList: i32 protocol, u32 numPackets, MIDIEventPacket packet[] */
void *MIDIEventListInit(void *list, i32 proto) {
    u8 *l = list;
    if (!l) return 0;
    memcpy(l, &proto, 4);
    *(u32 *)(l + 4) = 0;
    return l + 8;
}
void *MIDIEventListAdd(void *list, size_t size, void *cur, u64 time, size_t words, const u32 *data) {
    u8 *l = list, *p = cur;
    if (!l || !p || (words && !data) || words > 64) return 0;
    u32 np = *(u32 *)(l + 4);
    if (np) {
        u64 t; u32 wc;
        memcpy(&t, p, 8);
        memcpy(&wc, p + 8, 4);
        if (t == time && wc + words <= 64 && p + EVT_HDR + 4 * (wc + words) <= l + size) {
            memcpy(p + EVT_HDR + 4 * wc, data, 4 * words);
            wc += (u32)words;
            memcpy(p + 8, &wc, 4);
            return p;
        }
        p = evt_next(p);
    }
    if (p + EVT_HDR + 4 * words > l + size) return 0;
    memcpy(p, &time, 8);
    u32 wc = (u32)words;
    memcpy(p + 8, &wc, 4);
    memcpy(p + EVT_HDR, data, 4 * words);
    *(u32 *)(l + 4) = np + 1;
    return p;
}

/* ---- tradução MIDI 1.0 (bytes) <-> UMP */
static u32 scale_up(u32 v, int sb, int db) {
    int scale = db - sb;
    u32 shifted = v << scale, center = 1u << (sb - 1);
    if (v <= center) return shifted;
    int rep = sb - 1;
    u32 rv = v & ((1u << rep) - 1);
    rv = scale > rep ? rv << (scale - rep) : rv >> (rep - scale);
    while (rv) { shifted |= rv; rv >>= rep; }
    return shifted;
}
static int msg_len(u8 st) {
    if (st < 0xF0) return (st & 0xE0) == 0xC0 ? 2 : 3;
    switch (st) { case 0xF1: case 0xF3: return 2; case 0xF2: return 3; default: return 1; }
}
/* um pacote de bytes -> palavras UMP no protocolo proto; devolve quantas palavras (cap em palavras) */
static u32 bytes_to_ump(const u8 *b, u32 n, int proto, u32 *w, u32 cap) {
    u32 o = 0, i = 0;
    u8 run = 0;
    while (i < n && o + 2 <= cap) {
        u8 c = b[i];
        if (c == 0xF0) {   /* SysEx: até 6 bytes por UMP de 64 bits (tipo 3) */
            u32 j = i + 1, start = j;
            while (j < n && b[j] != 0xF7 && b[j] < 0x80) j++;
            u32 len = j - start, off = 0;
            int first = 1;
            do {
                u32 k = len - off > 6 ? 6 : len - off;
                int last = off + k >= len;
                u32 stt = first && last ? 0 : first ? 1 : last ? 3 : 2;
                u8 d[6] = {0};
                memcpy(d, b + start + off, k);
                if (o + 2 > cap) return o;
                w[o++] = 0x30000000u | stt << 20 | k << 16 | (u32)d[0] << 8 | d[1];
                w[o++] = (u32)d[2] << 24 | (u32)d[3] << 16 | (u32)d[4] << 8 | d[5];
                off += k;
                first = 0;
            } while (off < len);
            i = j < n && b[j] == 0xF7 ? j + 1 : j;
            continue;
        }
        u8 st = c;
        if (c < 0x80) { if (!run) { i++; continue; } st = run; }
        else { i++; if (st < 0xF0) run = st; else if (st < 0xF8) run = 0; }
        int need = msg_len(st) - 1;
        u8 d1 = 0, d2 = 0;
        if (c < 0x80) { d1 = b[i++]; if (need == 2) { if (i >= n) break; d2 = b[i++]; } }
        else {
            if (need >= 1) { if (i >= n) break; d1 = b[i++]; }
            if (need == 2) { if (i >= n) break; d2 = b[i++]; }
        }
        if (st >= 0xF0) { w[o++] = 0x10000000u | (u32)st << 16 | (u32)d1 << 8 | d2; continue; }
        if (proto != 2) { w[o++] = 0x20000000u | (u32)st << 16 | (u32)d1 << 8 | d2; continue; }
        u8 hi = st & 0xF0;
        u32 w0 = 0x40000000u | (u32)st << 16, w1 = 0;
        switch (hi) {
        case 0x90: if (d2 == 0) { w0 = 0x40000000u | (u32)(0x80 | (st & 15)) << 16; w0 |= (u32)d1 << 8; w1 = 0; break; }
                   /* fallthrough */
        case 0x80: w0 |= (u32)d1 << 8; w1 = scale_up(d2, 7, 16) << 16; break;
        case 0xA0: case 0xB0: w0 |= (u32)d1 << 8; w1 = scale_up(d2, 7, 32); break;
        case 0xC0: w1 = (u32)d1 << 24; break;
        case 0xD0: w1 = scale_up(d1, 7, 32); break;
        case 0xE0: w1 = scale_up((u32)d2 << 7 | d1, 14, 32); break;
        }
        w[o++] = w0;
        w[o++] = w1;
    }
    return o;
}
static int ump_words(u32 w0) {
    switch (w0 >> 28) { case 0: case 1: case 2: case 6: case 7: return 1; case 3: case 4: case 8: case 9: case 10: return 2; case 11: case 12: return 3; default: return 4; }
}
/* palavras UMP -> bytes MIDI 1.0; devolve quantos bytes (cap em bytes) */
static u32 ump_to_bytes(const u32 *w, u32 nw, u8 *b, u32 cap) {
    u32 o = 0;
    for (u32 i = 0; i < nw;) {
        u32 w0 = w[i];
        int k = ump_words(w0);
        if (i + (u32)k > nw || o + 12 > cap) break;
        u32 t = w0 >> 28;
        u8 st = (u8)(w0 >> 16), d1 = (u8)(w0 >> 8) & 0x7F, d2 = (u8)w0 & 0x7F;
        if (t == 1 || t == 2) {
            b[o++] = st;
            int len = msg_len(st);
            if (len > 1) b[o++] = d1;
            if (len > 2) b[o++] = d2;
        } else if (t == 3) {
            u32 stt = (w0 >> 20) & 15, cnt = (w0 >> 16) & 15, w1 = w[i + 1];
            u8 d[6] = {(u8)(w0 >> 8), (u8)w0, (u8)(w1 >> 24), (u8)(w1 >> 16), (u8)(w1 >> 8), (u8)w1};
            if (cnt > 6) cnt = 6;
            if (stt == 0 || stt == 1) b[o++] = 0xF0;
            for (u32 j = 0; j < cnt; j++) b[o++] = d[j] & 0x7F;
            if (stt == 0 || stt == 3) b[o++] = 0xF7;
        } else if (t == 4) {
            u32 w1 = w[i + 1];
            u8 hi = st & 0xF0, ch = st & 15;
            switch (hi) {
            case 0x80: case 0x90: {
                u8 v = (u8)(w1 >> 25);
                if (hi == 0x90 && v == 0) v = 1;   /* nota com velocidade 0 em MIDI 2.0 não é note off em 1.0 */
                b[o++] = st; b[o++] = d1; b[o++] = v;
                break;
            }
            case 0xA0: case 0xB0: b[o++] = st; b[o++] = d1; b[o++] = (u8)(w1 >> 25); break;
            case 0xC0:
                if (w0 & 1) { b[o++] = 0xB0 | ch; b[o++] = 0; b[o++] = (u8)(w1 >> 8) & 0x7F; b[o++] = 0xB0 | ch; b[o++] = 32; b[o++] = (u8)w1 & 0x7F; }
                b[o++] = st; b[o++] = (u8)(w1 >> 24) & 0x7F;
                break;
            case 0xD0: b[o++] = st; b[o++] = (u8)(w1 >> 25); break;
            case 0xE0: { u32 v = w1 >> 18; b[o++] = st; b[o++] = v & 0x7F; b[o++] = (v >> 7) & 0x7F; break; }
            default: break;   /* por nota, RPN/NRPN de 32 bits: sem equivalente direto em 1.0 */
            }
        }
        i += (u32)k;
    }
    return o;
}

/* ---- objetos */
enum { O_CLIENT = 1, O_INPORT, O_OUTPORT, O_SOURCE, O_DEST };
typedef struct { char key[48]; int is_str; i32 v; id s; } mprop_t;
typedef struct { u32 src; void *ref; } mconn_t;
typedef struct mobj {
    u32 ref;
    int type;
    u32 owner;
    id name;
    i32 uniq;
    void *proc, *pref, *block;   /* leitor (porta de entrada, destino) ou notificação (cliente) */
    int proto;                   /* 0: MIDIPacketList; 1/2: MIDIEventList no protocolo */
    mconn_t *conn;
    u32 nconn, cconn;
    mprop_t *props;
    u32 nprops;
} mobj_t;
static mobj_t **g_obj;
static u32 g_nobj, g_cobj, g_next_ref = 0x10000, g_next_uniq;
static u8 g_mlock[64];
static int g_mlock_init;
static void mlock(void) { if (!g_mlock_init) { pthread_mutex_init(g_mlock, 0); g_mlock_init = 1; } pthread_mutex_lock(g_mlock); }
static void munlock(void) { pthread_mutex_unlock(g_mlock); }
static mobj_t *find(u32 ref) { for (u32 i = 0; i < g_nobj; i++) if (g_obj[i]->ref == ref) return g_obj[i]; return 0; }
static mobj_t *find_t(u32 ref, int type) { mobj_t *o = find(ref); return o && o->type == type ? o : 0; }
static mobj_t *find_uniq(i32 u) { for (u32 i = 0; i < g_nobj; i++) if (g_obj[i]->uniq == u && g_obj[i]->type >= O_SOURCE) return g_obj[i]; return 0; }
static mobj_t *add(int type, u32 owner, id name) {
    if (g_nobj == g_cobj) {
        u32 nc = g_cobj ? g_cobj * 2 : 32;
        mobj_t **n = realloc(g_obj, nc * sizeof *n);
        if (!n) return 0;
        g_obj = n;
        g_cobj = nc;
    }
    mobj_t *o = calloc(1, sizeof *o);
    if (!o) return 0;
    o->ref = g_next_ref++;
    o->type = type;
    o->owner = owner;
    o->name = name ? objc_retain(name) : 0;
    if (type >= O_SOURCE) {
        /* uniqueID: inteiro de 32 bits com sinal, diferente de zero e único entre os endpoints */
        do { g_next_uniq = g_next_uniq * 1103515245u + 12345u + o->ref; o->uniq = (i32)g_next_uniq; } while (!o->uniq || find_uniq(o->uniq));
    }
    g_obj[g_nobj++] = o;
    return o;
}
static void destroy(mobj_t *o) {
    for (u32 i = 0; i < g_nobj; i++) if (g_obj[i] == o) { g_obj[i] = g_obj[--g_nobj]; break; }
    if (o->type >= O_SOURCE)   /* tira a fonte das ligações das portas */
        for (u32 i = 0; i < g_nobj; i++) {
            mobj_t *p = g_obj[i];
            for (u32 j = 0; j < p->nconn; j++) if (p->conn[j].src == o->ref) { p->conn[j] = p->conn[--p->nconn]; j--; }
        }
    if (o->name) objc_release(o->name);
    for (u32 i = 0; i < o->nprops; i++) if (o->props[i].s) objc_release(o->props[i].s);
    if (o->block) _Block_release(o->block);
    free(o->props);
    free(o->conn);
    free(o);
}

/* ---- notificações: MIDIObjectAddRemoveNotification e kMIDIMsgSetupChanged para todos os clientes */
typedef struct { i32 id; u32 size; u32 parent; i32 ptype; u32 child; i32 ctype; } madd_t;
typedef void (*notify_fn)(const void *msg, void *ref);
static void notify_all(int added, u32 child, int ctype) {
    madd_t m = {added ? 2 : 3, sizeof(madd_t), 0, -1, child, ctype};
    struct { i32 id; u32 size; } setup = {1, 8};
    struct { void *proc, *ref, *block; } t[64];
    u32 n = 0;
    mlock();
    for (u32 i = 0; i < g_nobj && n < 64; i++)
        if (g_obj[i]->type == O_CLIENT && (g_obj[i]->proc || g_obj[i]->block)) {
            t[n].proc = g_obj[i]->proc; t[n].ref = g_obj[i]->pref; t[n].block = g_obj[i]->block ? _Block_copy(g_obj[i]->block) : 0; n++;
        }
    munlock();
    for (u32 i = 0; i < n; i++) {
        for (int k = 0; k < 2; k++) {
            const void *msg = k == 0 ? (const void *)&m : (const void *)&setup;
            if (t[i].proc) ((notify_fn)t[i].proc)(msg, t[i].ref);
            else { void (*inv)(void *, const void *) = *(void **)((u8 *)t[i].block + 16); inv(t[i].block, msg); }
        }
        if (t[i].block) _Block_release(t[i].block);
    }
}

/* ---- clientes, portas e endpoints */
static OSStatus new_client(id name, void *proc, void *ref, void *block, u32 *out) {
    if (!out) return kParamErr;
    mlock();
    mobj_t *o = add(O_CLIENT, 0, name);
    if (o) { o->proc = proc; o->pref = ref; o->block = block ? _Block_copy(block) : 0; *out = o->ref; }
    munlock();
    return o ? 0 : -108;
}
OSStatus MIDIClientCreate(id name, void *proc, void *ref, u32 *out) { return new_client(name, proc, ref, 0, out); }
OSStatus MIDIClientCreateWithBlock(id name, u32 *out, void *block) { return new_client(name, 0, 0, block, out); }
static OSStatus new_obj(u32 client, int type, id name, void *proc, void *ref, void *block, int proto, u32 *out) {
    if (!out) return kParamErr;
    mlock();
    if (!find_t(client, O_CLIENT)) { munlock(); return kMIDIInvalidClient; }
    mobj_t *o = add(type, client, name);
    if (o) { o->proc = proc; o->pref = ref; o->block = block ? _Block_copy(block) : 0; o->proto = proto; *out = o->ref; }
    munlock();
    if (!o) return -108;
    if (type >= O_SOURCE) notify_all(1, o->ref, type == O_SOURCE ? 2 : 3);
    return 0;
}
OSStatus MIDIInputPortCreate(u32 c, id name, void *proc, void *ref, u32 *out) { if (!proc) return kParamErr; return new_obj(c, O_INPORT, name, proc, ref, 0, 0, out); }
OSStatus MIDIInputPortCreateWithBlock(u32 c, id name, u32 *out, void *block) { if (!block) return kParamErr; return new_obj(c, O_INPORT, name, 0, 0, block, 0, out); }
OSStatus MIDIInputPortCreateWithProtocol(u32 c, id name, i32 proto, u32 *out, void *block) {
    if (!block || (proto != 1 && proto != 2)) return kParamErr;
    return new_obj(c, O_INPORT, name, 0, 0, block, proto, out);
}
OSStatus MIDIOutputPortCreate(u32 c, id name, u32 *out) { return new_obj(c, O_OUTPORT, name, 0, 0, 0, 0, out); }
OSStatus MIDISourceCreate(u32 c, id name, u32 *out) { return new_obj(c, O_SOURCE, name, 0, 0, 0, 0, out); }
OSStatus MIDISourceCreateWithProtocol(u32 c, id name, i32 proto, u32 *out) {
    if (proto != 1 && proto != 2) return kParamErr;
    return new_obj(c, O_SOURCE, name, 0, 0, 0, proto, out);
}
OSStatus MIDIDestinationCreate(u32 c, id name, void *proc, void *ref, u32 *out) { if (!proc) return kParamErr; return new_obj(c, O_DEST, name, proc, ref, 0, 0, out); }
OSStatus MIDIDestinationCreateWithBlock(u32 c, id name, u32 *out, void *block) { if (!block) return kParamErr; return new_obj(c, O_DEST, name, 0, 0, block, 0, out); }
OSStatus MIDIDestinationCreateWithProtocol(u32 c, id name, i32 proto, u32 *out, void *block) {
    if (!block || (proto != 1 && proto != 2)) return kParamErr;
    return new_obj(c, O_DEST, name, 0, 0, block, proto, out);
}
OSStatus MIDIPortDispose(u32 p) {
    mlock();
    mobj_t *o = find(p);
    if (!o || (o->type != O_INPORT && o->type != O_OUTPORT)) { munlock(); return kMIDIInvalidPort; }
    destroy(o);
    munlock();
    return 0;
}
OSStatus MIDIEndpointDispose(u32 e) {
    mlock();
    mobj_t *o = find(e);
    if (!o || o->type < O_SOURCE) { munlock(); return kMIDIUnknownEndpoint; }
    int ct = o->type == O_SOURCE ? 2 : 3;
    destroy(o);
    munlock();
    notify_all(0, e, ct);
    return 0;
}
OSStatus MIDIClientDispose(u32 c) {
    u32 gone[256];
    int gt[256];
    u32 ng = 0;
    mlock();
    mobj_t *cl = find_t(c, O_CLIENT);
    if (!cl) { munlock(); return kMIDIInvalidClient; }
    for (u32 i = 0; i < g_nobj; i++) {
        mobj_t *o = g_obj[i];
        if (o->owner != c) continue;
        if (o->type >= O_SOURCE && ng < 256) { gone[ng] = o->ref; gt[ng] = o->type == O_SOURCE ? 2 : 3; ng++; }
        destroy(o);
        i = (u32)-1;   /* a tabela mudou: recomeça */
    }
    destroy(cl);
    munlock();
    for (u32 i = 0; i < ng; i++) notify_all(0, gone[i], gt[i]);
    return 0;
}
OSStatus MIDIPortConnectSource(u32 p, u32 src, void *connref) {
    mlock();
    mobj_t *port = find(p), *s = find(src);
    OSStatus r = 0;
    if (!port || port->type != O_INPORT) r = kMIDIInvalidPort;
    else if (!s) r = kMIDIUnknownEndpoint;
    else if (s->type != O_SOURCE) r = kMIDIWrongEndpointType;
    else {
        u32 i = 0;
        while (i < port->nconn && port->conn[i].src != src) i++;
        if (i == port->nconn) {
            if (port->nconn == port->cconn) {
                u32 nc = port->cconn ? port->cconn * 2 : 4;
                mconn_t *n = realloc(port->conn, nc * sizeof *n);
                if (!n) { munlock(); return -108; }
                port->conn = n;
                port->cconn = nc;
            }
            port->nconn++;
        }
        port->conn[i].src = src;
        port->conn[i].ref = connref;
    }
    munlock();
    return r;
}
OSStatus MIDIPortDisconnectSource(u32 p, u32 src) {
    mlock();
    mobj_t *port = find(p);
    OSStatus r = kMIDINoConnection;
    if (!port || port->type != O_INPORT) r = kMIDIInvalidPort;
    else for (u32 i = 0; i < port->nconn; i++) if (port->conn[i].src == src) { port->conn[i] = port->conn[--port->nconn]; r = 0; break; }
    munlock();
    return r;
}

/* ---- enumeração: endpoints virtuais não privados deste processo */
static int is_private(mobj_t *o) {
    for (u32 i = 0; i < o->nprops; i++) if (!strcmp(o->props[i].key, "private") && !o->props[i].is_str) return o->props[i].v != 0;
    return 0;
}
static u32 nth(int type, unsigned long idx, unsigned long *count) {
    unsigned long k = 0;
    u32 r = 0;
    mlock();
    for (u32 i = 0; i < g_nobj; i++) {
        mobj_t *o = g_obj[i];
        if (o->type != type || is_private(o)) continue;
        if (k == idx) r = o->ref;
        k++;
    }
    munlock();
    if (count) *count = k;
    return r;
}
unsigned long MIDIGetNumberOfSources(void) { unsigned long n; nth(O_SOURCE, (unsigned long)-1, &n); return n; }
unsigned long MIDIGetNumberOfDestinations(void) { unsigned long n; nth(O_DEST, (unsigned long)-1, &n); return n; }
u32 MIDIGetSource(unsigned long i) { return nth(O_SOURCE, i, 0); }
u32 MIDIGetDestination(unsigned long i) { return nth(O_DEST, i, 0); }
unsigned long MIDIGetNumberOfDevices(void) { return 0; }
u32 MIDIGetDevice(unsigned long i) { (void)i; return 0; }
unsigned long MIDIGetNumberOfExternalDevices(void) { return 0; }
u32 MIDIGetExternalDevice(unsigned long i) { (void)i; return 0; }
OSStatus MIDIEndpointGetEntity(u32 e, u32 *ent) {
    if (ent) *ent = 0;
    mlock();
    mobj_t *o = find(e);
    munlock();
    if (!o || o->type < O_SOURCE) return kMIDIUnknownEndpoint;
    return kMIDIObjectNotFound;   /* endpoint virtual: não pertence a entidade nenhuma */
}
OSStatus MIDIObjectFindByUniqueID(i32 u, u32 *obj, i32 *type) {
    mlock();
    mobj_t *o = find_uniq(u);
    if (obj) *obj = o ? o->ref : 0;
    if (type) *type = o ? (o->type == O_SOURCE ? 2 : 3) : -1;
    munlock();
    return o ? 0 : kMIDIObjectNotFound;
}

/* ---- propriedades */
static mprop_t *prop(mobj_t *o, const char *k, int create) {
    for (u32 i = 0; i < o->nprops; i++) if (!strcmp(o->props[i].key, k)) return &o->props[i];
    if (!create || strlen(k) >= sizeof o->props[0].key) return 0;
    mprop_t *n = realloc(o->props, (o->nprops + 1) * sizeof *n);
    if (!n) return 0;
    o->props = n;
    mprop_t *p = &n[o->nprops++];
    memset(p, 0, sizeof *p);
    strncpy(p->key, k, sizeof p->key - 1);
    return p;
}
OSStatus MIDIObjectGetIntegerProperty(u32 ref, id key, i32 *v) {
    const char *k = key ? cx_cstr(key) : 0;
    if (!k || !v) return kParamErr;
    mlock();
    mobj_t *o = find(ref);
    OSStatus r = 0;
    if (!o) r = kMIDIObjectNotFound;
    else if (!strcmp(k, "uniqueID") && o->type >= O_SOURCE) *v = o->uniq;
    else if (!strcmp(k, "protocol") && o->type >= O_SOURCE && o->proto) *v = o->proto;
    else {
        mprop_t *p = prop(o, k, 0);
        if (p && !p->is_str) *v = p->v;
        else if (p) r = kMIDIWrongPropertyType;
        else if (!strcmp(k, "offline") || !strcmp(k, "private")) *v = 0;
        else r = kMIDIUnknownProperty;
    }
    munlock();
    return r;
}
OSStatus MIDIObjectSetIntegerProperty(u32 ref, id key, i32 v) {
    const char *k = key ? cx_cstr(key) : 0;
    if (!k) return kParamErr;
    mlock();
    mobj_t *o = find(ref);
    OSStatus r = 0;
    if (!o) r = kMIDIObjectNotFound;
    else if (!strcmp(k, "uniqueID") && o->type >= O_SOURCE) {
        mobj_t *other = find_uniq(v);
        if (other && other != o) r = kMIDIIDNotUnique;
        else if (!v) r = kParamErr;
        else o->uniq = v;
    } else {
        mprop_t *p = prop(o, k, 1);
        if (!p) r = -108;
        else { if (p->s) objc_release(p->s); p->s = 0; p->is_str = 0; p->v = v; }
    }
    munlock();
    return r;
}
/* devolve com +1 (regra Copy/Create do CoreMIDI) */
OSStatus MIDIObjectGetStringProperty(u32 ref, id key, id *v) {
    const char *k = key ? cx_cstr(key) : 0;
    if (!k || !v) return kParamErr;
    *v = 0;
    mlock();
    mobj_t *o = find(ref);
    OSStatus r = 0;
    if (!o) r = kMIDIObjectNotFound;
    else {
        mprop_t *p = prop(o, k, 0);
        if (p && p->is_str) *v = p->s ? objc_retain(p->s) : 0;
        else if (p) r = kMIDIWrongPropertyType;
        else if ((!strcmp(k, "name") || !strcmp(k, "displayName")) && o->name) *v = objc_retain(o->name);
        else r = kMIDIUnknownProperty;
    }
    munlock();
    return r;
}
OSStatus MIDIObjectSetStringProperty(u32 ref, id key, id v) {
    const char *k = key ? cx_cstr(key) : 0;
    if (!k) return kParamErr;
    mlock();
    mobj_t *o = find(ref);
    OSStatus r = 0;
    if (!o) r = kMIDIObjectNotFound;
    else if (!strcmp(k, "name")) { if (o->name) objc_release(o->name); o->name = v ? objc_retain(v) : 0; }
    else {
        mprop_t *p = prop(o, k, 1);
        if (!p) r = -108;
        else { if (p->s) objc_release(p->s); p->s = v ? objc_retain(v) : 0; p->is_str = 1; p->v = 0; }
    }
    munlock();
    return r;
}
OSStatus MIDIObjectRemoveProperty(u32 ref, id key) {
    const char *k = key ? cx_cstr(key) : 0;
    if (!k) return kParamErr;
    mlock();
    mobj_t *o = find(ref);
    OSStatus r = kMIDIUnknownProperty;
    if (!o) r = kMIDIObjectNotFound;
    else for (u32 i = 0; i < o->nprops; i++)
        if (!strcmp(o->props[i].key, k)) { if (o->props[i].s) objc_release(o->props[i].s); o->props[i] = o->props[--o->nprops]; r = 0; break; }
    munlock();
    return r;
}

/* ---- entrega */
/* MIDIEventList -> MIDIPacketList nova (malloc) */
static void *evt_to_pkt(const void *list) {
    const u8 *l = list, *p = l + 8;
    u32 np = *(const u32 *)(l + 4);
    size_t cap = 4;
    for (u32 i = 0; i < np; i++) { u32 wc; memcpy(&wc, p + 8, 4); cap += PKT_HDR + 4 + 8u * wc + 16; p = evt_next((u8 *)p); }
    void *own = malloc(cap);
    if (!own) return 0;
    u8 *cur = MIDIPacketListInit(own), tmp[64 * 8 + 16];
    p = l + 8;
    for (u32 i = 0; i < np; i++) {
        u64 ts; u32 wc;
        memcpy(&ts, p, 8);
        memcpy(&wc, p + 8, 4);
        u32 nb = ump_to_bytes((const u32 *)(p + EVT_HDR), wc, tmp, sizeof tmp);
        if (nb) { u8 *n = MIDIPacketListAdd(own, cap, cur, ts, nb, tmp); if (n) cur = n; }
        p = evt_next((u8 *)p);
    }
    return own;
}
/* MIDIPacketList -> MIDIEventList nova no protocolo proto (até 64 palavras por pacote, sem partir mensagem) */
static void *pkt_to_evt(const void *bl, int proto) {
    u32 np = *(const u32 *)bl;
    const u8 *p = (const u8 *)bl + 4;
    size_t cap = 8;
    for (u32 i = 0; i < np; i++) { u16 len; memcpy(&len, p + 8, 2); cap += (EVT_HDR + 4u * 64) * (2u * len / 64 + 2); p = pkt_next((u8 *)p); }
    void *own = malloc(cap);
    if (!own) return 0;
    u8 *cur = MIDIEventListInit(own, proto);
    p = (const u8 *)bl + 4;
    for (u32 i = 0; i < np; i++) {
        u64 ts; u16 len;
        memcpy(&ts, p, 8);
        memcpy(&len, p + 8, 2);
        u32 cw = 2u * len + 4, *w = malloc(cw * 4);
        if (w) {
            u32 nw = bytes_to_ump(p + PKT_HDR, len, proto, w, cw);
            for (u32 off = 0; off < nw;) {
                u32 k = 0;
                while (off + k < nw) { u32 m = (u32)ump_words(w[off + k]); if (k + m > 64) break; k += m; }
                if (!k) break;
                u8 *n = MIDIEventListAdd(own, cap, cur, ts, k, w + off);
                if (!n) break;
                cur = n;
                off += k;
            }
            free(w);
        }
        p = pkt_next((u8 *)p);
    }
    return own;
}
typedef struct { void *proc, *pref, *block, *connref; int proto; } target_t;
typedef void (*read_fn)(const void *list, void *pref, void *connref);
/* entrada em MIDIPacketList (is_evt 0) ou MIDIEventList; o leitor recebe no formato e no protocolo dele */
static void deliver(const target_t *t, const void *list, int is_evt, u32 list_proto) {
    void *own = 0, *mid = 0;
    const void *out = list;
    if (!t->proto && is_evt) out = own = evt_to_pkt(list);
    else if (t->proto && !is_evt) out = own = pkt_to_evt(list, t->proto);
    else if (t->proto && is_evt && list_proto != (u32)t->proto) {
        mid = evt_to_pkt(list);
        out = own = mid ? pkt_to_evt(mid, t->proto) : 0;
    }
    if (out) {
        if (t->proc) ((read_fn)t->proc)(out, t->pref, t->connref);
        else if (t->block) { void (*inv)(void *, const void *, void *) = *(void **)((u8 *)t->block + 16); inv(t->block, out, t->connref); }
    }
    free(own);
    free(mid);
}
static OSStatus received(u32 src, const void *list, int is_evt) {
    if (!list) return kParamErr;
    target_t t[64];
    u32 n = 0;
    mlock();
    mobj_t *s = find(src);
    if (!s || s->type != O_SOURCE) { munlock(); return s ? kMIDIWrongEndpointType : kMIDIUnknownEndpoint; }
    for (u32 i = 0; i < g_nobj; i++) {
        mobj_t *p = g_obj[i];
        if (p->type != O_INPORT) continue;
        for (u32 j = 0; j < p->nconn && n < 64; j++)
            if (p->conn[j].src == src) {
                t[n] = (target_t){p->proc, p->pref, p->block ? _Block_copy(p->block) : 0, p->conn[j].ref, p->proto};
                n++;
            }
    }
    munlock();
    u32 lp = is_evt ? *(const u32 *)list : 0;
    for (u32 i = 0; i < n; i++) { deliver(&t[i], list, is_evt, lp); if (t[i].block) _Block_release(t[i].block); }
    return 0;
}
OSStatus MIDIReceived(u32 src, const void *list) { return received(src, list, 0); }
OSStatus MIDIReceivedEventList(u32 src, const void *list) { return received(src, list, 1); }
static OSStatus sent(u32 port, u32 dest, const void *list, int is_evt) {
    if (!list) return kParamErr;
    mlock();
    mobj_t *p = find(port), *d = find(dest);
    if (!p || p->type != O_OUTPORT) { munlock(); return kMIDIInvalidPort; }
    if (!d) { munlock(); return kMIDIUnknownEndpoint; }
    if (d->type != O_DEST) { munlock(); return kMIDIWrongEndpointType; }
    target_t t = {d->proc, d->pref, d->block ? _Block_copy(d->block) : 0, 0, d->proto};
    munlock();
    deliver(&t, list, is_evt, is_evt ? *(const u32 *)list : 0);
    if (t.block) _Block_release(t.block);
    return 0;
}
OSStatus MIDISend(u32 port, u32 dest, const void *list) { return sent(port, dest, list, 0); }
OSStatus MIDISendEventList(u32 port, u32 dest, const void *list) { return sent(port, dest, list, 1); }
/* a entrega é imediata: não há nada na fila para descartar */
OSStatus MIDIFlushOutput(u32 dest) {
    if (!dest) return 0;
    mlock();
    mobj_t *d = find(dest);
    munlock();
    return d && d->type == O_DEST ? 0 : kMIDIUnknownEndpoint;
}
OSStatus MIDIRestart(void) { return 0; }
