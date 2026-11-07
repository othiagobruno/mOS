/*
 * mOS libSystem — CoreBluetooth.framework (papel central).
 *
 * O kernel (kernel/bt/ble.c) faz o rádio: varredura LE, a conexão, o pareamento (quando o periférico pede) e um
 * canal ATT cru (DEV_BT_* do GUI_DEVICE). Todo o cliente GATT fica aqui, como no bluetoothd do macOS:
 * troca de MTU, descoberta de serviços primários (Read By Group Type), de características (Read By Type 0x2803),
 * de descritores (Find Information), leitura com Read Blob para valores longos, escrita com e sem resposta,
 * notificações e indicações (CCCD 0x2902) e os erros ATT como NSError no CBATTErrorDomain.
 * Um timer de 10 ms no run loop principal lê o canal e o estado do enlace; os métodos do delegate são chamados na
 * fila passada ao CBCentralManager (a principal se nil).
 *
 * Limites do mOS: uma conexão LE por vez (o kernel tem um enlace), e o papel periférico (CBPeripheralManager,
 * anunciar e servir GATT) informa CBManagerStateUnsupported.
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
int strcmp(const char *a, const char *b);
void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
void *dispatch_get_main_queue(void);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
id CFDataCreate(void *alloc, const u8 *bytes, long n);

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static id ret(id o) { return o ? objc_retain(o) : 0; }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static u64 cnt(id a) { return a && resp(a, "count") ? MSG0(u64, a, "count") : 0; }
static id at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static id marr(void) { return MSG0(id, cls("NSMutableArray"), "array"); }
static id mdict(void) { return MSG0(id, cls("NSMutableDictionary"), "dictionary"); }
static id data_of(const u8 *p, u64 n) { return objc_autorelease(CFDataCreate(0, p, (long)n)); }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static void put16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }

/* ================================================================ constantes */
id CBAdvertisementDataLocalNameKey, CBAdvertisementDataManufacturerDataKey, CBAdvertisementDataOverflowServiceUUIDsKey,
   CBAdvertisementDataServiceDataKey, CBAdvertisementDataServiceUUIDsKey, CBAdvertisementDataTxPowerLevelKey,
   CBAdvertisementDataIsConnectable, CBAdvertisementDataSolicitedServiceUUIDsKey;
id CBCentralManagerScanOptionAllowDuplicatesKey, CBCentralManagerScanOptionSolicitedServiceUUIDsKey,
   CBCentralManagerOptionShowPowerAlertKey, CBCentralManagerOptionRestoreIdentifierKey,
   CBConnectPeripheralOptionNotifyOnConnectionKey, CBConnectPeripheralOptionNotifyOnDisconnectionKey,
   CBConnectPeripheralOptionNotifyOnNotificationKey, CBConnectPeripheralOptionStartDelayKey,
   CBConnectPeripheralOptionEnableTransportBridgingKey, CBConnectPeripheralOptionRequiresANCS,
   CBPeripheralManagerOptionShowPowerAlertKey, CBPeripheralManagerOptionRestoreIdentifierKey;
id CBErrorDomain, CBATTErrorDomain;
id CBUUIDCharacteristicExtendedPropertiesString, CBUUIDCharacteristicUserDescriptionString,
   CBUUIDClientCharacteristicConfigurationString, CBUUIDServerCharacteristicConfigurationString,
   CBUUIDCharacteristicFormatString, CBUUIDCharacteristicAggregateFormatString, CBUUIDCharacteristicValidRangeString,
   CBUUIDCharacteristicObservationScheduleString, CBUUIDL2CAPPSMCharacteristicString;

static id cb_error(id domain, i64 code, const char *msg) {
    id info = mdict();
    MSG(void, info, "setObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", domain, code, info);
}

/* ================================================================ CBUUID */
cx_class_t CBUUID_c __asm__("OBJC_CLASS_$_CBUUID"), CBUUID_m __asm__("OBJC_METACLASS_$_CBUUID");
typedef struct { void *isa; u8 b[16]; int bits; } cbuuid_t;     /* bits = 16, 32 ou 128 (como foi criado) */
static const u8 base_uuid[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
#define UU(o) ((cbuuid_t *)(o))
static id uuid_from16(u16 v) {
    cbuuid_t *u = (cbuuid_t *)cx_alloc_instance(&CBUUID_c);
    memcpy(u->b, base_uuid, 16);
    u->b[2] = (u8)(v >> 8); u->b[3] = (u8)v;
    u->bits = 16;
    return objc_autorelease((id)u);
}
/* 128 bits do ATT (little-endian) */
static id uuid_from_att128(const u8 *le) {
    cbuuid_t *u = (cbuuid_t *)cx_alloc_instance(&CBUUID_c);
    for (int i = 0; i < 16; i++) u->b[i] = le[15 - i];
    u->bits = memcmp(u->b + 4, base_uuid + 4, 12) ? 128 : (u->b[0] || u->b[1] ? 32 : 16);
    return objc_autorelease((id)u);
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static id uuid_with_string(id k, SEL c, id s) {
    (void)k; (void)c;
    const char *t = s ? cx_cstr(s) : "";
    u8 v[16];
    int n = 0;
    for (const char *p = t; *p && n < 32; p++) {
        if (*p == '-') continue;
        int h = hexv(*p);
        if (h < 0) return 0;
        if (n & 1) v[n / 2] |= (u8)h; else v[n / 2] = (u8)(h << 4);
        n++;
    }
    cbuuid_t *u = (cbuuid_t *)cx_alloc_instance(&CBUUID_c);
    memcpy(u->b, base_uuid, 16);
    if (n == 4) { u->b[2] = v[0]; u->b[3] = v[1]; u->bits = 16; }
    else if (n == 8) { memcpy(u->b, v, 4); u->bits = 32; }
    else if (n == 32) { memcpy(u->b, v, 16); u->bits = 128; }
    else { objc_release((id)u); return 0; }
    return objc_autorelease((id)u);
}
static id uuid_with_data(id k, SEL c, id d) {
    (void)k; (void)c;
    u64 n = d ? MSG0(u64, d, "length") : 0;
    const u8 *p = n ? MSG0(const u8 *, d, "bytes") : 0;
    if (n != 2 && n != 4 && n != 16) return 0;
    cbuuid_t *u = (cbuuid_t *)cx_alloc_instance(&CBUUID_c);
    memcpy(u->b, base_uuid, 16);
    if (n == 2) { u->b[2] = p[0]; u->b[3] = p[1]; u->bits = 16; }
    else if (n == 4) { memcpy(u->b, p, 4); u->bits = 32; }
    else { memcpy(u->b, p, 16); u->bits = 128; }
    return objc_autorelease((id)u);
}
static id uuid_with_nsuuid(id k, SEL c, id nsu) { return uuid_with_string(k, c, nsu ? MSG0(id, nsu, "UUIDString") : 0); }
static id uuid_with_cfuuid(id k, SEL c, id cf) { return uuid_with_nsuuid(k, c, cf); }
static BOOL uuid_is_short(cbuuid_t *u) { return u->bits != 128 && !memcmp(u->b + 4, base_uuid + 4, 12); }
static id uuid_string(id s, SEL c) {
    (void)c;
    cbuuid_t *u = UU(s);
    char o[40];
    if (uuid_is_short(u) && !u->b[0] && !u->b[1]) snprintf(o, sizeof o, "%02X%02X", u->b[2], u->b[3]);
    else if (uuid_is_short(u)) snprintf(o, sizeof o, "%02X%02X%02X%02X", u->b[0], u->b[1], u->b[2], u->b[3]);
    else snprintf(o, sizeof o, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", u->b[0], u->b[1], u->b[2], u->b[3],
                  u->b[4], u->b[5], u->b[6], u->b[7], u->b[8], u->b[9], u->b[10], u->b[11], u->b[12], u->b[13], u->b[14], u->b[15]);
    return cx_str(o);
}
static id uuid_data(id s, SEL c) {
    (void)c;
    cbuuid_t *u = UU(s);
    if (uuid_is_short(u) && !u->b[0] && !u->b[1]) return data_of(u->b + 2, 2);
    if (uuid_is_short(u)) return data_of(u->b, 4);
    return data_of(u->b, 16);
}
static BOOL uuid_equal(id s, SEL c, id o) {
    (void)c;
    if (!o || !cx_is_subclass(object_getClass(o), &CBUUID_c)) return NO;
    return !memcmp(UU(s)->b, UU(o)->b, 16);
}
static u64 uuid_hash(id s, SEL c) { (void)c; u64 h = 1469598103934665603ull; for (int i = 0; i < 16; i++) h = (h ^ UU(s)->b[i]) * 1099511628211ull; return h; }
static id uuid_copy(id s, SEL c) { (void)c; return objc_retain(s); }
/* nomes que o macOS mostra no -description dos UUIDs conhecidos */
static id uuid_desc(id s, SEL c) {
    cbuuid_t *u = UU(s);
    if (uuid_is_short(u) && !u->b[0] && !u->b[1]) {
        u16 v = (u16)(u->b[2] << 8 | u->b[3]);
        static const struct { u16 v; const char *n; } names[] = {
            {0x1800, "Generic Access Profile"}, {0x1801, "Generic Attribute Profile"}, {0x180A, "Device Information"},
            {0x180F, "Battery"}, {0x180D, "Heart Rate"}, {0x1812, "Human Interface Device"}, {0x2A00, "Device Name"},
            {0x2A01, "Appearance"}, {0x2A19, "Battery Level"}, {0x2A29, "Manufacturer Name String"}, {0x2A24, "Model Number String"},
            {0x2A37, "Heart Rate Measurement"}, {0x2902, "Client Characteristic Configuration"}, {0x2901, "Characteristic User Description"},
        };
        for (size_t i = 0; i < sizeof names / sizeof *names; i++) if (names[i].v == v) return cx_str(names[i].n);
    }
    return uuid_string(s, c);
}

/* ================================================================ objetos GATT */
cx_class_t CBAttribute_c __asm__("OBJC_CLASS_$_CBAttribute"), CBAttribute_m __asm__("OBJC_METACLASS_$_CBAttribute");
cx_class_t CBService_c __asm__("OBJC_CLASS_$_CBService"), CBService_m __asm__("OBJC_METACLASS_$_CBService");
cx_class_t CBCharacteristic_c __asm__("OBJC_CLASS_$_CBCharacteristic"), CBCharacteristic_m __asm__("OBJC_METACLASS_$_CBCharacteristic");
cx_class_t CBDescriptor_c __asm__("OBJC_CLASS_$_CBDescriptor"), CBDescriptor_m __asm__("OBJC_METACLASS_$_CBDescriptor");
cx_class_t CBPeripheral_c __asm__("OBJC_CLASS_$_CBPeripheral"), CBPeripheral_m __asm__("OBJC_METACLASS_$_CBPeripheral");
cx_class_t CBPeer_c __asm__("OBJC_CLASS_$_CBPeer"), CBPeer_m __asm__("OBJC_METACLASS_$_CBPeer");
cx_class_t CBManager_c __asm__("OBJC_CLASS_$_CBManager"), CBManager_m __asm__("OBJC_METACLASS_$_CBManager");
cx_class_t CBCentralManager_c __asm__("OBJC_CLASS_$_CBCentralManager"), CBCentralManager_m __asm__("OBJC_METACLASS_$_CBCentralManager");
cx_class_t CBPeripheralManager_c __asm__("OBJC_CLASS_$_CBPeripheralManager"), CBPeripheralManager_m __asm__("OBJC_METACLASS_$_CBPeripheralManager");
cx_class_t CBMOSTicker_c __asm__("OBJC_CLASS_$_CBMOSTicker"), CBMOSTicker_m __asm__("OBJC_METACLASS_$_CBMOSTicker");

typedef struct { void *isa; id uuid; id peripheral; BOOL primary; u16 start, end; id chars, included; } cbsvc_t;
typedef struct { void *isa; id uuid; id service; u8 props; u16 decl, vh, end; id value, descs; BOOL notifying; u16 cccd; } cbchr_t;
typedef struct { void *isa; id uuid; id chr; u16 h; id value; } cbdsc_t;
typedef struct {
    void *isa;
    id identifier, name;
    int state;                      /* CBPeripheralState: 0 desconectado, 1 conectando, 2 conectado, 3 desconectando */
    id delegate, services, central;
    int dev_index;                  /* índice da varredura do kernel */
    int rssi;
    char addr[18];
    u16 mtu;
} cbper_t;
#define SVC(o) ((cbsvc_t *)(o))
#define CHR(o) ((cbchr_t *)(o))
#define DSC(o) ((cbdsc_t *)(o))
#define PER(o) ((cbper_t *)(o))
static id at_uuid(id s, SEL c) { (void)c; return SVC(s)->uuid; }
static id svc_peripheral(id s, SEL c) { (void)c; return SVC(s)->peripheral; }
static BOOL svc_primary(id s, SEL c) { (void)c; return SVC(s)->primary; }
static id svc_chars(id s, SEL c) { (void)c; return SVC(s)->chars; }
static id svc_included(id s, SEL c) { (void)c; return SVC(s)->included; }
static id chr_service(id s, SEL c) { (void)c; return CHR(s)->service; }
static u64 chr_props(id s, SEL c) { (void)c; return CHR(s)->props; }
static id chr_value(id s, SEL c) { (void)c; return CHR(s)->value; }
static id chr_descs(id s, SEL c) { (void)c; return CHR(s)->descs; }
static BOOL chr_notifying(id s, SEL c) { (void)c; return CHR(s)->notifying; }
static BOOL chr_broadcasted(id s, SEL c) { (void)s; (void)c; return NO; }
static id dsc_chr(id s, SEL c) { (void)c; return DSC(s)->chr; }
static id dsc_value(id s, SEL c) { (void)c; return DSC(s)->value; }

/* ================================================================ fila de operações GATT */
enum { OP_MTU = 1, OP_SERVICES, OP_CHARS, OP_DESCS, OP_READ, OP_WRITE, OP_NOTIFY, OP_READ_DESC, OP_WRITE_DESC, OP_INCLUDED };
typedef struct gop {
    int kind;
    id target;                      /* serviço / característica / descritor */
    id filter;                      /* NSArray de CBUUID (descobertas) */
    u8 *data;
    u32 len;
    BOOL enable, sent, retried;
    u16 next;                       /* próximo handle (descobertas) / offset (Read Blob) */
    id acc;                         /* valor acumulado (Read Blob) */
    struct gop *nx;
} gop_t;
static struct {
    id central;                     /* o CBCentralManager com a conexão */
    id per;                         /* periférico conectado (ou conectando) */
    gop_t *head, *tail;
    BOOL link_up;
    u64 sent_at;
} g;
static id g_centrals;               /* todos os CBCentralManager vivos (estado, varredura) */
static id g_known;                  /* periféricos vistos: endereço → CBPeripheral */

typedef struct { void *q; id obj; SEL sel; id a1, a2, a3, a4; int nargs; } dcall_t;
static void dcall_run(void *p) {
    dcall_t *d = p;
    if (d->obj && cx_responds(object_getClass(d->obj), d->sel)) {
        switch (d->nargs) {
        case 1: ((void (*)(id, SEL, id))objc_msgSend)(d->obj, d->sel, d->a1); break;
        case 2: ((void (*)(id, SEL, id, id))objc_msgSend)(d->obj, d->sel, d->a1, d->a2); break;
        case 3: ((void (*)(id, SEL, id, id, id))objc_msgSend)(d->obj, d->sel, d->a1, d->a2, d->a3); break;
        case 4: ((void (*)(id, SEL, id, id, id, id))objc_msgSend)(d->obj, d->sel, d->a1, d->a2, d->a3, d->a4); break;
        }
    }
    id o[5] = {d->obj, d->a1, d->a2, d->a3, d->a4};
    for (int i = 0; i < 5; i++) if (o[i]) objc_release(o[i]);
    free(d);
}
/* delegate da central ou do periférico, na fila escolhida */
static void *central_queue(id central);
static void foundation_gaps(void);
static void deliver(id central, id obj, const char *sel, int nargs, id a1, id a2, id a3, id a4) {
    if (!obj) return;
    dcall_t *d = malloc(sizeof *d);
    d->obj = ret(obj); d->sel = sel_registerName(sel); d->nargs = nargs;
    d->a1 = ret(a1); d->a2 = ret(a2); d->a3 = ret(a3); d->a4 = ret(a4);
    void *q = central ? central_queue(central) : 0;
    if (!q || q == dispatch_get_main_queue()) dcall_run(d);
    else dispatch_async_f(q, d, dcall_run);
}
static id per_delegate(id p) { return p ? PER(p)->delegate : 0; }

static void att_send(const u8 *p, u32 n) { SYS3(GUI_DEVICE, DEV_BT_ATT_SEND, p, n); g.sent_at = (u64)SYS0(SYS_TIME); }
static void op_push(gop_t *o) {
    if (!g.tail) g.head = g.tail = o; else { g.tail->nx = o; g.tail = o; }
}
static void op_free(gop_t *o) {
    if (o->target) objc_release(o->target);
    if (o->filter) objc_release(o->filter);
    if (o->acc) objc_release(o->acc);
    free(o->data);
    free(o);
}
static void *calloc_(size_t n);
static gop_t *op_new(int kind, id target) { gop_t *o = calloc_(sizeof *o); o->kind = kind; o->target = ret(target); return o; }
static void *calloc_(size_t n) { void *p = malloc(n); if (p) memset(p, 0, n); return p; }

/* envia o pedido da operação da frente (se ainda não foi) */
static void op_kick(void) {
    gop_t *o = g.head;
    if (!o || o->sent || !g.link_up) return;
    u8 b[260];
    o->sent = YES;
    switch (o->kind) {
    case OP_MTU: b[0] = 0x02; put16(b + 1, 247); att_send(b, 3); return;
    case OP_SERVICES: b[0] = 0x10; put16(b + 1, o->next ? o->next : 1); put16(b + 3, 0xFFFF); put16(b + 5, 0x2800); att_send(b, 7); return;
    case OP_INCLUDED: { cbsvc_t *s = SVC(o->target); b[0] = 0x08; put16(b + 1, o->next ? o->next : s->start); put16(b + 3, s->end); put16(b + 5, 0x2802); att_send(b, 7); return; }
    case OP_CHARS: { cbsvc_t *s = SVC(o->target); b[0] = 0x08; put16(b + 1, o->next ? o->next : s->start); put16(b + 3, s->end); put16(b + 5, 0x2803); att_send(b, 7); return; }
    case OP_DESCS: {
        cbchr_t *c = CHR(o->target);
        u16 s = o->next ? o->next : (u16)(c->vh + 1);
        if (s > c->end || !c->vh) { o->sent = NO; o->kind = -OP_DESCS; return; }   /* sem descritores: termina na hora */
        b[0] = 0x04; put16(b + 1, s); put16(b + 3, c->end); att_send(b, 5); return;
    }
    case OP_READ: case OP_READ_DESC: {
        u16 h = o->kind == OP_READ ? CHR(o->target)->vh : DSC(o->target)->h;
        if (o->next) { b[0] = 0x0C; put16(b + 1, h); put16(b + 3, o->next); att_send(b, 5); }
        else { b[0] = 0x0A; put16(b + 1, h); att_send(b, 3); }
        return;
    }
    case OP_WRITE: case OP_WRITE_DESC: {
        u16 h = o->kind == OP_WRITE ? CHR(o->target)->vh : DSC(o->target)->h;
        u32 n = o->len > 250 ? 250 : o->len;
        b[0] = 0x12; put16(b + 1, h); memcpy(b + 3, o->data, n); att_send(b, 3 + n);
        return;
    }
    case OP_NOTIFY: {
        cbchr_t *c = CHR(o->target);
        u16 v = o->enable ? ((c->props & 0x10) ? 0x0001 : 0x0002) : 0x0000;
        b[0] = 0x12; put16(b + 1, c->cccd); put16(b + 3, v); att_send(b, 5);
        return;
    }
    }
}
static BOOL uuid_in(id filter, id u) {
    if (!filter || !cnt(filter)) return YES;
    for (u64 i = 0; i < cnt(filter); i++) if (MSG(BOOL, at(filter, i), "isEqual:", u)) return YES;
    return NO;
}
static id att_error(u8 code) {
    static const char *msg[] = {"", "Handle inválido", "Leitura não permitida", "Escrita não permitida", "PDU inválida",
                                "Autenticação insuficiente", "Pedido não suportado", "Offset inválido", "Autorização insuficiente",
                                "Fila de preparo cheia", "Atributo não encontrado", "Atributo não é longo", "Chave de cifra insuficiente",
                                "Tamanho de valor inválido", "Erro improvável", "Cifra insuficiente", "Tipo de grupo não suportado",
                                "Recursos insuficientes"};
    return cb_error(CBATTErrorDomain, code, code < sizeof msg / sizeof *msg ? msg[code] : "Erro ATT");
}
/* operação terminou: avisa o delegate e passa à próxima */
static void op_done(gop_t *o, id err) {
    id per = g.per, d = per_delegate(per);
    g.head = o->nx;
    if (!g.head) g.tail = 0;
    switch (o->kind < 0 ? -o->kind : o->kind) {
    case OP_MTU: break;
    case OP_SERVICES: {
        if (o->filter && cnt(o->filter) && !err) {             /* só os pedidos ficam visíveis */
            id keep = marr();
            for (u64 i = 0; i < cnt(PER(per)->services); i++) if (uuid_in(o->filter, SVC(at(PER(per)->services, i))->uuid)) MSG(void, keep, "addObject:", at(PER(per)->services, i));
            setobj(&PER(per)->services, keep);
        }
        deliver(g.central, d, "peripheral:didDiscoverServices:", 2, per, err, 0, 0);
        break;
    }
    case OP_INCLUDED: deliver(g.central, d, "peripheral:didDiscoverIncludedServicesForService:error:", 3, per, o->target, err, 0); break;
    case OP_CHARS: {
        cbsvc_t *s = SVC(o->target);
        /* fim de cada característica: o handle antes da próxima declaração */
        u64 n = cnt(s->chars);
        for (u64 i = 0; i < n; i++) CHR(at(s->chars, i))->end = i + 1 < n ? (u16)(CHR(at(s->chars, i + 1))->decl - 1) : s->end;
        if (o->filter && cnt(o->filter)) {
            id keep = marr();
            for (u64 i = 0; i < n; i++) if (uuid_in(o->filter, CHR(at(s->chars, i))->uuid)) MSG(void, keep, "addObject:", at(s->chars, i));
            setobj(&s->chars, keep);
        }
        deliver(g.central, d, "peripheral:didDiscoverCharacteristicsForService:error:", 3, per, o->target, err, 0);
        break;
    }
    case OP_DESCS: if (!o->enable) deliver(g.central, d, "peripheral:didDiscoverDescriptorsForCharacteristic:error:", 3, per, o->target, err, 0); break;
    case OP_READ:
        if (!err) setobj(&CHR(o->target)->value, MSG(id, cls("NSData"), "dataWithData:", o->acc ? o->acc : data_of(0, 0)));
        deliver(g.central, d, "peripheral:didUpdateValueForCharacteristic:error:", 3, per, o->target, err, 0);
        break;
    case OP_READ_DESC:
        if (!err) setobj(&DSC(o->target)->value, MSG(id, cls("NSData"), "dataWithData:", o->acc ? o->acc : data_of(0, 0)));
        deliver(g.central, d, "peripheral:didUpdateValueForDescriptor:error:", 3, per, o->target, err, 0);
        break;
    case OP_WRITE: deliver(g.central, d, "peripheral:didWriteValueForCharacteristic:error:", 3, per, o->target, err, 0); break;
    case OP_WRITE_DESC: deliver(g.central, d, "peripheral:didWriteValueForDescriptor:error:", 3, per, o->target, err, 0); break;
    case OP_NOTIFY:
        if (!err) CHR(o->target)->notifying = o->enable;
        deliver(g.central, d, "peripheral:didUpdateNotificationStateForCharacteristic:error:", 3, per, o->target, err, 0);
        break;
    }
    op_free(o);
    op_kick();
}
static cbchr_t *chr_for_handle(u16 h) {
    id per = g.per;
    for (u64 i = 0; per && i < cnt(PER(per)->services); i++) {
        cbsvc_t *s = SVC(at(PER(per)->services, i));
        for (u64 k = 0; k < cnt(s->chars); k++) if (CHR(at(s->chars, k))->vh == h) return CHR(at(s->chars, k));
    }
    return 0;
}
static void att_rx(const u8 *p, int n) {
    if (n < 1) return;
    u8 op = p[0];
    if ((op == 0x1B || op == 0x1D) && n >= 3) {                     /* notificação / indicação */
        if (op == 0x1D) { u8 cf = 0x1E; att_send(&cf, 1); }
        cbchr_t *c = chr_for_handle(le16(p + 1));
        if (c) {
            setobj(&c->value, data_of(p + 3, (u64)(n - 3)));
            deliver(g.central, per_delegate(g.per), "peripheral:didUpdateValueForCharacteristic:error:", 3, g.per, (id)c, 0, 0);
        }
        return;
    }
    if (op == 0x02 && n >= 3) { u8 r[3] = {0x03}; put16(r + 1, 247); att_send(r, 3); return; }   /* MTU pedido pelo periférico */
    if (op == 0x0A || op == 0x08 || op == 0x10 || op == 0x04 || op == 0x12) {                       /* não somos servidor */
        u8 r[5] = {0x01, op, p[1], n > 2 ? p[2] : 0, 0x06};
        att_send(r, 5);
        return;
    }
    gop_t *o = g.head;
    if (!o || !o->sent) return;
    if (op == 0x01 && n >= 5) {
        u8 code = p[4];
        /* "atributo não encontrado" encerra uma descoberta normalmente */
        BOOL end_of_disc = code == 0x0A && (o->kind == OP_SERVICES || o->kind == OP_CHARS || o->kind == OP_DESCS || o->kind == OP_INCLUDED);
        if ((code == 0x05 || code == 0x0F) && !o->retried) { o->sent = NO; o->retried = YES; return; }   /* o kernel pareia; repete depois */
        op_done(o, end_of_disc ? 0 : att_error(code));
        return;
    }
    id per = g.per;
    switch (o->kind) {
    case OP_MTU:
        if (op == 0x03 && n >= 3) { u16 m = le16(p + 1); PER(per)->mtu = m < 247 ? (m < 23 ? 23 : m) : 247; }
        op_done(o, 0);
        return;
    case OP_SERVICES: {
        if (op != 0x11 || n < 2) { op_done(o, 0); return; }
        int el = p[1];
        u16 last = 0;
        for (int i = 2; i + el <= n; i += el) {
            cbsvc_t *s = (cbsvc_t *)cx_alloc_instance(&CBService_c);
            s->start = le16(p + i); s->end = le16(p + i + 2);
            s->uuid = ret(el == 6 ? uuid_from16(le16(p + i + 4)) : uuid_from_att128(p + i + 4));
            s->peripheral = per; s->primary = YES;
            s->chars = ret(marr()); s->included = ret(marr());
            MSG(void, PER(per)->services, "addObject:", (id)s);
            objc_release((id)s);
            last = s->end;
        }
        if (last && last != 0xFFFF) { o->next = (u16)(last + 1); o->sent = NO; op_kick(); return; }
        op_done(o, 0);
        return;
    }
    case OP_INCLUDED: {
        if (op != 0x09 || n < 2) { op_done(o, 0); return; }
        int el = p[1];
        u16 last = 0;
        for (int i = 2; i + el <= n; i += el) {
            last = le16(p + i);
            u16 st = le16(p + i + 2);
            for (u64 k = 0; k < cnt(PER(per)->services); k++) if (SVC(at(PER(per)->services, k))->start == st) MSG(void, SVC(o->target)->included, "addObject:", at(PER(per)->services, k));
        }
        if (last && last < SVC(o->target)->end) { o->next = (u16)(last + 1); o->sent = NO; op_kick(); return; }
        op_done(o, 0);
        return;
    }
    case OP_CHARS: {
        if (op != 0x09 || n < 2) { op_done(o, 0); return; }
        int el = p[1];
        u16 last = 0;
        cbsvc_t *s = SVC(o->target);
        for (int i = 2; i + el <= n; i += el) {
            cbchr_t *c = (cbchr_t *)cx_alloc_instance(&CBCharacteristic_c);
            c->decl = le16(p + i); c->props = p[i + 2]; c->vh = le16(p + i + 3);
            c->uuid = ret(el == 7 ? uuid_from16(le16(p + i + 5)) : uuid_from_att128(p + i + 5));
            c->service = (id)s;
            c->descs = ret(marr());
            BOOL dup = NO;
            for (u64 k = 0; k < cnt(s->chars); k++) if (CHR(at(s->chars, k))->decl == c->decl) dup = YES;
            if (!dup) MSG(void, s->chars, "addObject:", (id)c);
            objc_release((id)c);
            last = c->decl;
        }
        if (last && last < s->end) { o->next = (u16)(last + 1); o->sent = NO; op_kick(); return; }
        op_done(o, 0);
        return;
    }
    case OP_DESCS: {
        if (op != 0x05 || n < 2) { op_done(o, 0); return; }
        int fmt = p[1], el = fmt == 1 ? 4 : 18;
        u16 last = 0;
        cbchr_t *c = CHR(o->target);
        for (int i = 2; i + el <= n; i += el) {
            u16 h = le16(p + i);
            last = h;
            cbdsc_t *d = (cbdsc_t *)cx_alloc_instance(&CBDescriptor_c);
            d->h = h;
            d->uuid = ret(fmt == 1 ? uuid_from16(le16(p + i + 2)) : uuid_from_att128(p + i + 2));
            d->chr = (id)c;
            if (fmt == 1 && le16(p + i + 2) == 0x2902) c->cccd = h;
            BOOL dup = NO;
            for (u64 k = 0; k < cnt(c->descs); k++) if (DSC(at(c->descs, k))->h == h) dup = YES;
            if (!dup) MSG(void, c->descs, "addObject:", (id)d);
            objc_release((id)d);
        }
        if (last && last < c->end) { o->next = (u16)(last + 1); o->sent = NO; op_kick(); return; }
        op_done(o, 0);
        return;
    }
    case OP_READ: case OP_READ_DESC: {
        if (op != 0x0B && op != 0x0D) { op_done(o, 0); return; }
        if (!o->acc) o->acc = ret(MSG0(id, cls("NSMutableData"), "data"));
        MSG(void, o->acc, "appendBytes:length:", p + 1, (u64)(n - 1));
        u16 mtu = PER(per)->mtu ? PER(per)->mtu : 23;
        if (n - 1 >= mtu - 1 && o->next < 512) { o->next = (u16)MSG0(u64, o->acc, "length"); o->sent = NO; op_kick(); return; }   /* valor longo: Read Blob */
        op_done(o, 0);
        return;
    }
    case OP_WRITE: case OP_WRITE_DESC: case OP_NOTIFY:
        op_done(o, op == 0x13 ? 0 : att_error(0x0E));
        return;
    }
}

/* ================================================================ CBPeripheral */
static id per_identifier(id s, SEL c) { (void)c; return PER(s)->identifier; }
static id per_name(id s, SEL c) { (void)c; return PER(s)->name; }
static i64 per_state(id s, SEL c) { (void)c; return PER(s)->state; }
static id per_get_delegate(id s, SEL c) { (void)c; return PER(s)->delegate; }
static void per_set_delegate(id s, SEL c, id d) { (void)c; PER(s)->delegate = d; }
static id per_services(id s, SEL c) { (void)c; return PER(s)->state == 2 ? PER(s)->services : 0; }
static id per_rssi(id s, SEL c) { (void)c; return MSG(id, cls("NSNumber"), "numberWithInt:", PER(s)->rssi); }
static BOOL per_can_send(id s, SEL c) { (void)c; return PER(s)->state == 2; }
static u64 per_max_write(id s, SEL c, i64 type) { (void)c; (void)type; return (u64)((PER(s)->mtu ? PER(s)->mtu : 23) - 3); }
static BOOL per_ancs(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL connected(id s) { return s && g.per == s && PER(s)->state == 2 && g.link_up; }
static id not_connected(void) { return cb_error(CBErrorDomain, 7 /* CBErrorPeripheralDisconnected */, "O periférico não está conectado"); }
static void per_discover_services(id s, SEL c, id uuids) {
    (void)c;
    if (!connected(s)) { deliver(g.central, PER(s)->delegate, "peripheral:didDiscoverServices:", 2, s, not_connected(), 0, 0); return; }
    setobj(&PER(s)->services, marr());
    gop_t *o = op_new(OP_SERVICES, 0);
    o->filter = ret(uuids);
    op_push(o);
    op_kick();
}
static void per_discover_included(id s, SEL c, id uuids, id svc) {
    (void)c; (void)uuids;
    if (!connected(s) || !svc) return;
    op_push(op_new(OP_INCLUDED, svc));
    op_kick();
}
static void per_discover_chars(id s, SEL c, id uuids, id svc) {
    (void)c;
    if (!connected(s) || !svc) { deliver(g.central, PER(s)->delegate, "peripheral:didDiscoverCharacteristicsForService:error:", 3, s, svc, not_connected(), 0); return; }
    setobj(&SVC(svc)->chars, marr());
    gop_t *o = op_new(OP_CHARS, svc);
    o->filter = ret(uuids);
    op_push(o);
    op_kick();
}
static void per_discover_descs(id s, SEL c, id chr) {
    (void)c;
    if (!connected(s) || !chr) return;
    op_push(op_new(OP_DESCS, chr));
    op_kick();
}
static void per_read(id s, SEL c, id chr) {
    (void)c;
    if (!connected(s) || !chr) return;
    op_push(op_new(OP_READ, chr));
    op_kick();
}
static void per_read_desc(id s, SEL c, id dsc) {
    (void)c;
    if (!connected(s) || !dsc) return;
    op_push(op_new(OP_READ_DESC, dsc));
    op_kick();
}
static gop_t *write_op(int kind, id target, id data) {
    gop_t *o = op_new(kind, target);
    o->len = data ? (u32)MSG0(u64, data, "length") : 0;
    o->data = malloc(o->len ? o->len : 1);
    if (o->len) memcpy(o->data, MSG0(const void *, data, "bytes"), o->len);
    return o;
}
/* type: 0 = CBCharacteristicWriteWithResponse, 1 = WithoutResponse */
static void per_write(id s, SEL c, id data, id chr, i64 type) {
    (void)c;
    if (!connected(s) || !chr) return;
    if (type == 1) {
        u8 b[260];
        u32 n = data ? (u32)MSG0(u64, data, "length") : 0;
        if (n > 250) n = 250;
        b[0] = 0x52; put16(b + 1, CHR(chr)->vh);
        if (n) memcpy(b + 3, MSG0(const void *, data, "bytes"), n);
        att_send(b, 3 + n);
        deliver(g.central, PER(s)->delegate, "peripheralIsReadyToSendWriteWithoutResponse:", 1, s, 0, 0, 0);
        return;
    }
    op_push(write_op(OP_WRITE, chr, data));
    op_kick();
}
static void per_write_desc(id s, SEL c, id data, id dsc) {
    (void)c;
    if (!connected(s) || !dsc) return;
    op_push(write_op(OP_WRITE_DESC, dsc, data));
    op_kick();
}
static void per_set_notify(id s, SEL c, BOOL on, id chr) {
    (void)c;
    if (!connected(s) || !chr) return;
    if (!(CHR(chr)->props & 0x30)) {
        deliver(g.central, PER(s)->delegate, "peripheral:didUpdateNotificationStateForCharacteristic:error:", 3, s, chr,
                cb_error(CBATTErrorDomain, 6, "A característica não aceita notificações"), 0);
        return;
    }
    if (!CHR(chr)->cccd) {                                    /* acha o CCCD antes (sem avisar o delegate) */
        gop_t *d = op_new(OP_DESCS, chr);
        d->enable = YES;
        op_push(d);
    }
    gop_t *o = op_new(OP_NOTIFY, chr);
    o->enable = on;
    op_push(o);
    op_kick();
}
static void per_read_rssi(id s, SEL c) {
    (void)c;
    deliver(g.central, PER(s)->delegate, "peripheral:didReadRSSI:error:", 3, s, per_rssi(s, 0), 0, 0);
}
static id per_desc(id s, SEL c) {
    (void)c;
    char b[160];
    static const char *st[] = {"disconnected", "connecting", "connected", "disconnecting"};
    snprintf(b, sizeof b, "<CBPeripheral: %p, identifier = %s, name = %s, mtu = %u, state = %s>", (void *)s,
             cx_cstr(MSG0(id, PER(s)->identifier, "UUIDString")), PER(s)->name ? cx_cstr(PER(s)->name) : "(null)", PER(s)->mtu ? PER(s)->mtu : 23,
             st[PER(s)->state & 3]);
    return cx_str(b);
}
static id peripheral_for(const gui_bt_device_t *d, int index) {
    if (!g_known) g_known = ret(mdict());
    id key = cx_str(d->addr);
    id p = MSG(id, g_known, "objectForKey:", key);
    if (!p) {
        cbper_t *o = (cbper_t *)cx_alloc_instance(&CBPeripheral_c);
        /* identificador estável por endereço */
        char u[40];
        const char *a = d->addr;
        snprintf(u, sizeof u, "6D4F5300-0000-4000-8000-%c%c%c%c%c%c%c%c%c%c%c%c", a[0], a[1], a[3], a[4], a[6], a[7], a[9], a[10], a[12], a[13], a[15], a[16]);
        o->identifier = ret(objc_autorelease(MSG(id, MSG0(id, cls("NSUUID"), "alloc"), "initWithUUIDString:", cx_str(u))));
        o->services = ret(marr());
        memcpy(o->addr, d->addr, 18);
        MSG(void, g_known, "setObject:forKey:", (id)o, key);
        objc_release((id)o);
        p = (id)o;
    }
    cbper_t *o = PER(p);
    o->dev_index = index;
    o->rssi = d->rssi;
    if (d->name[0]) setobj(&o->name, cx_str(d->name));
    return p;
}

/* ================================================================ CBCentralManager */
typedef struct {
    void *isa;
    id delegate;
    void *queue;
    BOOL scanning, dup, state_sent;
    id scan_filter;
    id seen;                        /* endereços já anunciados nesta varredura */
    i64 state;
} cbcm_t;
#define CM(o) ((cbcm_t *)(o))
static void *central_queue(id central) { return CM(central)->queue; }
static i64 radio_state(void) {
    long s = SYS1(GUI_DEVICE, DEV_BT_STATE);
    return s == 2 ? 5 /* PoweredOn */ : s == 1 ? 4 /* PoweredOff */ : s == 0 ? 2 /* Unsupported */ : 0;
}
static id g_ticker;
static void cb_tick(id s, SEL c, id t);
static void ensure_ticker(void) {
    if (g_ticker) return;
    g_ticker = cx_alloc_instance(&CBMOSTicker_c);
    MSG(id, cls("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 0.01, g_ticker, sel_registerName("tick:"), (id)0, YES);
}
static id cm_init3(id s, SEL c, id delegate, void *queue, id opts) {
    foundation_gaps();                             /* o NSUUID do foundation_more.c pode ter vindo depois do init */
    (void)c; (void)opts;
    cbcm_t *m = CM(s);
    m->delegate = delegate; m->queue = queue;
    m->state = radio_state();
    if (!g_centrals) g_centrals = ret(marr());
    MSG(void, g_centrals, "addObject:", s);
    ensure_ticker();
    return s;
}
static id cm_init2(id s, SEL c, id delegate, void *queue) { return cm_init3(s, c, delegate, queue, 0); }
static id cm_init(id s, SEL c) { return cm_init3(s, c, 0, 0, 0); }
static i64 cm_state(id s, SEL c) { (void)c; return CM(s)->state; }
static i64 cm_authorization(id s, SEL c) { (void)s; (void)c; return 3; }   /* CBManagerAuthorizationAllowedAlways */
static i64 cm_class_authorization(id k, SEL c) { (void)k; (void)c; return 3; }
static id cm_delegate(id s, SEL c) { (void)c; return CM(s)->delegate; }
static void cm_set_delegate(id s, SEL c, id d) { (void)c; CM(s)->delegate = d; }
static BOOL cm_is_scanning(id s, SEL c) { (void)c; return CM(s)->scanning; }
static BOOL cm_supports(id k, SEL c, u64 f) { (void)k; (void)c; (void)f; return NO; }
static void cm_scan(id s, SEL c, id services, id opts) {
    (void)c;
    cbcm_t *m = CM(s);
    if (m->state != 5) return;
    setobj(&m->scan_filter, services);
    id dup = opts ? MSG(id, opts, "objectForKey:", CBCentralManagerScanOptionAllowDuplicatesKey) : 0;
    m->dup = dup && MSG0(BOOL, dup, "boolValue");
    setobj(&m->seen, MSG0(id, cls("NSMutableSet"), "set"));
    m->scanning = YES;
    SYS2(GUI_DEVICE, DEV_BT_SCAN, 1);
}
static void cm_stop_scan(id s, SEL c) {
    (void)c;
    CM(s)->scanning = NO;
    BOOL any = NO;
    for (u64 i = 0; i < cnt(g_centrals); i++) if (CM(at(g_centrals, i))->scanning) any = YES;
    if (!any) SYS2(GUI_DEVICE, DEV_BT_SCAN, 0);
}
static void cm_connect(id s, SEL c, id per, id opts) {
    (void)c; (void)opts;
    if (!per) return;
    if (g.per && g.per != per) {
        deliver(s, CM(s)->delegate, "centralManager:didFailToConnectPeripheral:error:", 3, s, per,
                cb_error(CBErrorDomain, 9 /* CBErrorTooManyLEPairedDevices: o mOS tem um enlace LE */, "Outro periférico já está conectado"), 0);
        return;
    }
    if (PER(per)->state == 2) return;
    g.central = s;
    setobj(&g.per, per);
    PER(per)->central = s;
    PER(per)->state = 1;
    g.link_up = NO;
    if (SYS2(GUI_DEVICE, DEV_BT_CONNECT, PER(per)->dev_index) < 0) {
        PER(per)->state = 0;
        setobj(&g.per, 0);
        deliver(s, CM(s)->delegate, "centralManager:didFailToConnectPeripheral:error:", 3, s, per,
                cb_error(CBErrorDomain, 10 /* ConnectionFailed */, "O rádio Bluetooth recusou a conexão"), 0);
    }
}
static void link_down(id err) {
    id per = g.per, central = g.central;
    if (!per) return;
    while (g.head) { gop_t *o = g.head; g.head = o->nx; op_free(o); }
    g.tail = 0;
    g.link_up = NO;
    int was = PER(per)->state;
    PER(per)->state = 0;
    for (u64 i = 0; i < cnt(PER(per)->services); i++) {
        cbsvc_t *sv = SVC(at(PER(per)->services, i));
        for (u64 k = 0; k < cnt(sv->chars); k++) CHR(at(sv->chars, k))->notifying = NO;
    }
    objc_retain(per);
    setobj(&g.per, 0);
    g.central = 0;
    if (was == 1) deliver(central, CM(central)->delegate, "centralManager:didFailToConnectPeripheral:error:", 3, central, per,
                          err ? err : cb_error(CBErrorDomain, 10, "Falha ao conectar"), 0);
    else deliver(central, CM(central)->delegate, "centralManager:didDisconnectPeripheral:error:", 3, central, per, err, 0);
    objc_release(per);
}
static void cm_cancel(id s, SEL c, id per) {
    (void)s; (void)c;
    if (!per || per != g.per) return;
    PER(per)->state = 3;
    SYS1(GUI_DEVICE, DEV_BT_DISCONNECT);
    link_down(0);
}
static id cm_retrieve(id s, SEL c, id ids) {
    (void)s; (void)c;
    id out = marr();
    if (!g_known) return out;
    id all = MSG0(id, g_known, "allValues");
    for (u64 i = 0; i < cnt(all); i++)
        for (u64 k = 0; k < cnt(ids); k++)
            if (MSG(BOOL, PER(at(all, i))->identifier, "isEqual:", at(ids, k))) MSG(void, out, "addObject:", at(all, i));
    return out;
}
static id cm_retrieve_connected(id s, SEL c, id uuids) {
    (void)s; (void)c; (void)uuids;
    id out = marr();
    if (g.per && PER(g.per)->state == 2) MSG(void, out, "addObject:", g.per);
    return out;
}
/* o laço: estado do rádio, varredura, enlace e PDUs */
static void cb_tick(id s, SEL c, id t) {
    (void)s; (void)c; (void)t;
    static u64 last_scan;
    u64 now = (u64)SYS0(SYS_TIME);
    i64 st = radio_state();
    for (u64 i = 0; i < cnt(g_centrals); i++) {
        id m = at(g_centrals, i);
        if (!CM(m)->state_sent || CM(m)->state != st) {
            CM(m)->state = st;
            CM(m)->state_sent = YES;
            deliver(m, CM(m)->delegate, "centralManagerDidUpdateState:", 1, m, 0, 0, 0);
        }
    }
    if (now - last_scan >= 100) {
        last_scan = now;
        for (int i = 0; i < 64; i++) {
            gui_bt_device_t d;
            if (SYS3(GUI_DEVICE, DEV_BT_DEVICE, i, &d) != 1) break;
            id per = 0;
            for (u64 k = 0; k < cnt(g_centrals); k++) {
                id m = at(g_centrals, k);
                cbcm_t *cm = CM(m);
                if (!cm->scanning) continue;
                id key = cx_str(d.addr);
                if (!cm->dup && MSG(BOOL, cm->seen, "containsObject:", key)) continue;
                /* filtro por serviço: o kernel só sabe se o anúncio traz o HID (0x1812) */
                if (cm->scan_filter && cnt(cm->scan_filter)) {
                    BOOL ok = NO;
                    for (u64 f = 0; f < cnt(cm->scan_filter); f++) {
                        id u = at(cm->scan_filter, f);
                        if (MSG(BOOL, u, "isEqual:", uuid_from16(0x1812))) { if (d.hid) ok = YES; }
                        else ok = YES;                         /* serviço desconhecido no anúncio: não dá para descartar */
                    }
                    if (!ok) continue;
                }
                MSG(void, cm->seen, "addObject:", key);
                if (!per) per = peripheral_for(&d, i);
                id adv = mdict();
                if (d.name[0]) MSG(void, adv, "setObject:forKey:", cx_str(d.name), CBAdvertisementDataLocalNameKey);
                id conn = MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)(d.connectable != 0));
                MSG(void, adv, "setObject:forKey:", conn, CBAdvertisementDataIsConnectable);
                if (d.hid) { id svcs = MSG(id, cls("NSArray"), "arrayWithObject:", uuid_from16(0x1812)); MSG(void, adv, "setObject:forKey:", svcs, CBAdvertisementDataServiceUUIDsKey); }
                deliver(m, cm->delegate, "centralManager:didDiscoverPeripheral:advertisementData:RSSI:", 4, m, per, adv,
                        MSG(id, cls("NSNumber"), "numberWithInt:", d.rssi));
            }
        }
    }
    if (g.per) {
        gui_bt_link_t l;
        memset(&l, 0, sizeof l);
        SYS2(GUI_DEVICE, DEV_BT_LINK, &l);
        if (!g.link_up && l.state == 2) {
            g.link_up = YES;
            PER(g.per)->state = 2;
            PER(g.per)->mtu = (u16)(l.mtu ? l.mtu : 23);
            op_push(op_new(OP_MTU, 0));
            op_kick();
            deliver(g.central, CM(g.central)->delegate, "centralManager:didConnectPeripheral:", 2, g.central, g.per, 0, 0);
        } else if (l.state == 3 || (g.link_up && l.state == 0)) {
            link_down(l.state == 3 ? cb_error(CBErrorDomain, 10, l.err[0] ? l.err : "Falha ao conectar") : cb_error(CBErrorDomain, 7, "O periférico desconectou"));
        }
        for (int k = 0; k < 32 && g.link_up; k++) {
            u8 b[260];
            long n = SYS3(GUI_DEVICE, DEV_BT_ATT_RECV, b, sizeof b);
            if (n < 0) { link_down(cb_error(CBErrorDomain, 7, "O periférico desconectou")); break; }
            if (!n) break;
            att_rx(b, (int)n);
        }
        /* periférico mudo há 30 s numa operação: erro de tempo esgotado (como o CBErrorConnectionTimeout) */
        if (g.head && g.head->sent && now - g.sent_at > 30000) op_done(g.head, cb_error(CBErrorDomain, 6, "O periférico não respondeu"));
        if (g.head && !g.head->sent && g.head->kind == -OP_DESCS) op_done(g.head, 0);
        op_kick();
    }
}

/* ================================================================ CBPeripheralManager (sem papel periférico no mOS) */
typedef struct { void *isa; id delegate; void *queue; BOOL sent; } cbpm_t;
static id pm_init3(id s, SEL c, id d, void *q, id o) {
    (void)c; (void)o;
    ((cbpm_t *)s)->delegate = d; ((cbpm_t *)s)->queue = q;
    if (d) deliver(0, d, "peripheralManagerDidUpdateState:", 1, s, 0, 0, 0);
    return s;
}
static id pm_init2(id s, SEL c, id d, void *q) { return pm_init3(s, c, d, q, 0); }
static i64 pm_state(id s, SEL c) { (void)s; (void)c; return 2; }            /* CBManagerStateUnsupported */
static BOOL pm_advertising(id s, SEL c) { (void)s; (void)c; return NO; }
static void pm_noop1(id s, SEL c, id a) { (void)s; (void)c; (void)a; }
static void pm_noop0(id s, SEL c) { (void)s; (void)c; }
static i64 pm_auth(id k, SEL c) { (void)k; (void)c; return 3; }

/* ================================================================ lacunas do Foundation que estes frameworks usam
 * Só entram se ninguém mais definiu (o NSUUID mora em foundation_more.c: bytes em box_t.p, offset 72). */
typedef struct { void *isa; id a, b, c, e; i64 i, j; double d, f; void *p; int fl, fl2; } nsuuid_box_t;
static id gap_uuid_init_string(id s, SEL c, id str) {
    (void)c;
    const char *t = str ? cx_cstr(str) : "";
    u8 *b = malloc(16);
    int n = 0;
    for (const char *q = t; *q && n < 32; q++) {
        if (*q == '-') continue;
        int h = hexv(*q);
        if (h < 0) { free(b); objc_release(s); return 0; }
        if (n & 1) b[n / 2] |= (u8)h; else b[n / 2] = (u8)(h << 4);
        n++;
    }
    if (n != 32 || strlen(t) != 36) { free(b); objc_release(s); return 0; }
    ((nsuuid_box_t *)s)->p = b;
    return s;
}
static id gap_uuid_init_bytes(id s, SEL c, const u8 *bytes) { (void)c; u8 *b = malloc(16); memcpy(b, bytes, 16); ((nsuuid_box_t *)s)->p = b; return s; }
static BOOL gap_uuid_equal(id s, SEL c, id o) {
    (void)c;
    if (o == s) return YES;
    if (!o || object_getClass(o) != object_getClass(s)) return NO;
    const u8 *a = ((nsuuid_box_t *)s)->p, *b = ((nsuuid_box_t *)o)->p;
    return a && b && !memcmp(a, b, 16);
}
static u64 gap_uuid_hash(id s, SEL c) { (void)c; const u8 *a = ((nsuuid_box_t *)s)->p; u64 h = 0; for (int i = 0; a && i < 16; i++) h = h * 31 + a[i]; return h; }
static id gap_uuid_copy(id s, SEL c) { (void)c; return objc_retain(s); }
static i64 gap_uuid_compare(id s, SEL c, id o) {
    (void)c;
    const u8 *a = ((nsuuid_box_t *)s)->p, *b = o ? ((nsuuid_box_t *)o)->p : 0;
    int r = a && b ? memcmp(a, b, 16) : 0;
    return r < 0 ? -1 : r > 0 ? 1 : 0;
}
static id gap_mset_set(id k, SEL c) { (void)c; return objc_autorelease(MSG0(id, MSG0(id, k, "alloc"), "init")); }
static id gap_data_with_data(id k, SEL c, id d) {
    (void)c;
    u64 n = d ? MSG0(u64, d, "length") : 0;
    return MSG(id, k, "dataWithBytes:length:", n ? MSG0(const void *, d, "bytes") : (const void *)"", n);
}
static id gap_str_init_data(id s, SEL c, id d, u64 enc) {
    (void)c;
    u64 n = d ? MSG0(u64, d, "length") : 0;
    return ((id (*)(id, SEL, const void *, u64, u64))objc_msgSend)(s, sel_registerName("initWithBytes:length:encoding:"), n ? MSG0(const void *, d, "bytes") : (const void *)"", n, enc);
}
static void gap(const char *cname, const char *sel, void *imp, BOOL meta) {
    cx_class_t *k = cx_class_named(cname);
    if (!k) return;
    SEL s = sel_registerName(sel);
    if (meta) { if (!cx_responds(k->isa, s)) cx_add_cmethod(k, sel, imp); }
    else if (!cx_responds(k, s)) cx_add_method(k, sel, imp);
}
static void foundation_gaps(void) {
    gap("NSUUID", "initWithUUIDString:", (void *)gap_uuid_init_string, NO);
    gap("NSUUID", "initWithUUIDBytes:", (void *)gap_uuid_init_bytes, NO);
    gap("NSUUID", "compare:", (void *)gap_uuid_compare, NO);
    gap("NSUUID", "copyWithZone:", (void *)gap_uuid_copy, NO);
    /* isEqual:/hash herdados do NSObject comparam ponteiros: troca pelos do valor */
    cx_class_t *u = cx_class_named("NSUUID");
    if (u) { cx_add_method(u, "isEqual:", (void *)gap_uuid_equal); cx_add_method(u, "hash", (void *)gap_uuid_hash); }
    gap("NSMutableSet", "set", (void *)gap_mset_set, YES);
    gap("NSData", "dataWithData:", (void *)gap_data_with_data, YES);
    gap("NSMutableData", "dataWithData:", (void *)gap_data_with_data, YES);
    gap("NSString", "initWithData:encoding:", (void *)gap_str_init_data, NO);
}

/* ================================================================ registro */
static void K(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *super, u32 size) { if (!cx_class_named(name)) cx_define_class(k, m, name, super, size); }
#define A(k, s, f) cx_add_method(k, s, (void *)(f))
#define AC(k, s, f) cx_add_cmethod(k, s, (void *)(f))
static id S(const char *s) { return ret(cx_str(s)); }
void cx_corebluetooth_init(void) {
    foundation_gaps();
    cx_class_t *obj = cx_class_named("NSObject");
    CBAdvertisementDataLocalNameKey = S("kCBAdvDataLocalName");
    CBAdvertisementDataManufacturerDataKey = S("kCBAdvDataManufacturerData");
    CBAdvertisementDataOverflowServiceUUIDsKey = S("kCBAdvDataHashedServiceUUIDs");
    CBAdvertisementDataServiceDataKey = S("kCBAdvDataServiceData");
    CBAdvertisementDataServiceUUIDsKey = S("kCBAdvDataServiceUUIDs");
    CBAdvertisementDataTxPowerLevelKey = S("kCBAdvDataTxPowerLevel");
    CBAdvertisementDataIsConnectable = S("kCBAdvDataIsConnectable");
    CBAdvertisementDataSolicitedServiceUUIDsKey = S("kCBAdvDataSolicitedServiceUUIDs");
    CBCentralManagerScanOptionAllowDuplicatesKey = S("kCBScanOptionAllowDuplicates");
    CBCentralManagerScanOptionSolicitedServiceUUIDsKey = S("kCBScanOptionSolicitedServiceUUIDs");
    CBCentralManagerOptionShowPowerAlertKey = S("kCBInitOptionShowPowerAlert");
    CBCentralManagerOptionRestoreIdentifierKey = S("kCBRestoreIdentifierKey");
    CBConnectPeripheralOptionNotifyOnConnectionKey = S("kCBConnectOptionNotifyOnConnection");
    CBConnectPeripheralOptionNotifyOnDisconnectionKey = S("kCBConnectOptionNotifyOnDisconnection");
    CBConnectPeripheralOptionNotifyOnNotificationKey = S("kCBConnectOptionNotifyOnNotification");
    CBConnectPeripheralOptionStartDelayKey = S("kCBConnectOptionStartDelay");
    CBConnectPeripheralOptionEnableTransportBridgingKey = S("kCBConnectOptionEnableTransportBridging");
    CBConnectPeripheralOptionRequiresANCS = S("kCBConnectOptionRequiresANCS");
    CBPeripheralManagerOptionShowPowerAlertKey = S("kCBInitOptionShowPowerAlert");
    CBPeripheralManagerOptionRestoreIdentifierKey = S("kCBRestoreIdentifierKey");
    CBErrorDomain = S("CBErrorDomain");
    CBATTErrorDomain = S("CBATTErrorDomain");
    CBUUIDCharacteristicExtendedPropertiesString = S("2900");
    CBUUIDCharacteristicUserDescriptionString = S("2901");
    CBUUIDClientCharacteristicConfigurationString = S("2902");
    CBUUIDServerCharacteristicConfigurationString = S("2903");
    CBUUIDCharacteristicFormatString = S("2904");
    CBUUIDCharacteristicAggregateFormatString = S("2905");
    CBUUIDCharacteristicValidRangeString = S("2906");
    CBUUIDCharacteristicObservationScheduleString = S("2909");
    CBUUIDL2CAPPSMCharacteristicString = S("ABDD3056-28FA-441D-A470-55A75A52553A");

    K(&CBUUID_c, &CBUUID_m, "CBUUID", obj, (u32)(sizeof(cbuuid_t) - 8));
    AC(&CBUUID_c, "UUIDWithString:", uuid_with_string); AC(&CBUUID_c, "UUIDWithData:", uuid_with_data);
    AC(&CBUUID_c, "UUIDWithNSUUID:", uuid_with_nsuuid); AC(&CBUUID_c, "UUIDWithCFUUID:", uuid_with_cfuuid);
    A(&CBUUID_c, "UUIDString", uuid_string); A(&CBUUID_c, "data", uuid_data); A(&CBUUID_c, "isEqual:", uuid_equal);
    A(&CBUUID_c, "hash", uuid_hash); A(&CBUUID_c, "description", uuid_desc); A(&CBUUID_c, "copyWithZone:", uuid_copy); A(&CBUUID_c, "copy", uuid_copy);

    K(&CBAttribute_c, &CBAttribute_m, "CBAttribute", obj, 0);
    K(&CBService_c, &CBService_m, "CBService", &CBAttribute_c, (u32)(sizeof(cbsvc_t) - 8));
    A(&CBService_c, "UUID", at_uuid); A(&CBService_c, "peripheral", svc_peripheral); A(&CBService_c, "isPrimary", svc_primary);
    A(&CBService_c, "characteristics", svc_chars); A(&CBService_c, "includedServices", svc_included);
    K(&CBCharacteristic_c, &CBCharacteristic_m, "CBCharacteristic", &CBAttribute_c, (u32)(sizeof(cbchr_t) - 8));
    A(&CBCharacteristic_c, "UUID", at_uuid); A(&CBCharacteristic_c, "service", chr_service); A(&CBCharacteristic_c, "properties", chr_props);
    A(&CBCharacteristic_c, "value", chr_value); A(&CBCharacteristic_c, "descriptors", chr_descs); A(&CBCharacteristic_c, "isNotifying", chr_notifying);
    A(&CBCharacteristic_c, "isBroadcasted", chr_broadcasted);
    K(&CBDescriptor_c, &CBDescriptor_m, "CBDescriptor", &CBAttribute_c, (u32)(sizeof(cbdsc_t) - 8));
    A(&CBDescriptor_c, "UUID", at_uuid); A(&CBDescriptor_c, "characteristic", dsc_chr); A(&CBDescriptor_c, "value", dsc_value);

    K(&CBPeer_c, &CBPeer_m, "CBPeer", obj, 0);
    K(&CBPeripheral_c, &CBPeripheral_m, "CBPeripheral", &CBPeer_c, (u32)(sizeof(cbper_t) - 8));
    cx_class_t *p = &CBPeripheral_c;
    A(p, "identifier", per_identifier); A(p, "name", per_name); A(p, "state", per_state); A(p, "delegate", per_get_delegate);
    A(p, "setDelegate:", per_set_delegate); A(p, "services", per_services); A(p, "RSSI", per_rssi);
    A(p, "canSendWriteWithoutResponse", per_can_send); A(p, "maximumWriteValueLengthForType:", per_max_write);
    A(p, "ancsAuthorized", per_ancs); A(p, "discoverServices:", per_discover_services);
    A(p, "discoverIncludedServices:forService:", per_discover_included); A(p, "discoverCharacteristics:forService:", per_discover_chars);
    A(p, "discoverDescriptorsForCharacteristic:", per_discover_descs); A(p, "readValueForCharacteristic:", per_read);
    A(p, "readValueForDescriptor:", per_read_desc); A(p, "writeValue:forCharacteristic:type:", per_write);
    A(p, "writeValue:forDescriptor:", per_write_desc); A(p, "setNotifyValue:forCharacteristic:", per_set_notify);
    A(p, "readRSSI", per_read_rssi); A(p, "description", per_desc);

    K(&CBManager_c, &CBManager_m, "CBManager", obj, 0);
    AC(&CBManager_c, "authorization", cm_class_authorization);
    K(&CBCentralManager_c, &CBCentralManager_m, "CBCentralManager", &CBManager_c, (u32)(sizeof(cbcm_t) - 8));
    cx_class_t *m = &CBCentralManager_c;
    A(m, "init", cm_init); A(m, "initWithDelegate:queue:", cm_init2); A(m, "initWithDelegate:queue:options:", cm_init3);
    A(m, "state", cm_state); A(m, "authorization", cm_authorization); A(m, "delegate", cm_delegate); A(m, "setDelegate:", cm_set_delegate);
    A(m, "isScanning", cm_is_scanning); A(m, "scanForPeripheralsWithServices:options:", cm_scan); A(m, "stopScan", cm_stop_scan);
    A(m, "connectPeripheral:options:", cm_connect); A(m, "cancelPeripheralConnection:", cm_cancel);
    A(m, "retrievePeripheralsWithIdentifiers:", cm_retrieve); A(m, "retrieveConnectedPeripheralsWithServices:", cm_retrieve_connected);
    AC(m, "supportsFeatures:", cm_supports);
    K(&CBPeripheralManager_c, &CBPeripheralManager_m, "CBPeripheralManager", &CBManager_c, (u32)(sizeof(cbpm_t) - 8));
    cx_class_t *pm = &CBPeripheralManager_c;
    A(pm, "initWithDelegate:queue:", pm_init2); A(pm, "initWithDelegate:queue:options:", pm_init3); A(pm, "state", pm_state);
    A(pm, "isAdvertising", pm_advertising); A(pm, "startAdvertising:", pm_noop1); A(pm, "stopAdvertising", pm_noop0);
    A(pm, "addService:", pm_noop1); A(pm, "removeService:", pm_noop1); A(pm, "removeAllServices", pm_noop0);
    AC(pm, "authorizationStatus", pm_auth);
    K(&CBMOSTicker_c, &CBMOSTicker_m, "CBMOSTicker", obj, 0);
    A(&CBMOSTicker_c, "tick:", cb_tick);
}
