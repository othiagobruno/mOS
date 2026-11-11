/*
 * mOS libSystem — IOHIDEventSystemClient (IOKit, SPI que o Chromium/Electron usa para ler sensores).
 *
 * O Electron (Cursor, VS Code) cria um cliente, filtra com IOHIDEventSystemClientSetMatching
 * ({PrimaryUsagePage, PrimaryUsage}) e, para cada serviço, pede o último evento de temperatura
 * (IOHIDServiceClientCopyEvent + IOHIDEventGetFloatValue). No mOS os serviços do sistema de eventos são os
 * dispositivos de entrada que o window server já tem: o teclado (página 1, uso 6) e o apontador (página 1,
 * uso 2). Não há sensores de temperatura, acelerômetro nem luz ambiente: o filtro desses não acha
 * serviço, CopyServices devolve um array vazio e o app segue sem a leitura, como num Mac sem o sensor.
 * Um serviço só entrega evento do tipo que produz; pedir outro tipo devolve NULL, como no macOS.
 *
 * Objetos CF do mOS são objetos ObjC (CFRetain/CFRelease/CFGetTypeID por mensagem); os três tipos aqui
 * são classes com o estado nas palavras extras da instância.
 */
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);
int strcmp(const char *a, const char *b);

enum { HID_PAGE_GENERIC_DESKTOP = 1, HID_USAGE_POINTER = 1, HID_USAGE_MOUSE = 2, HID_USAGE_KEYBOARD = 6 };
enum { HID_EVENT_FIELDS = 8 };

typedef struct {
    const char *product;
    int page, usage;
    int event_type;                 /* tipo de IOHIDEvent que o serviço produz (kIOHIDEventType*) */
    u64 registry_id;
} hid_service_desc_t;

/* kIOHIDEventTypeKeyboard = 3, kIOHIDEventTypePointer = 17 */
static const hid_service_desc_t k_services[] = {
    {"mOS Keyboard", HID_PAGE_GENERIC_DESKTOP, HID_USAGE_KEYBOARD, 3, 0x100000501ULL},
    {"mOS Pointer", HID_PAGE_GENERIC_DESKTOP, HID_USAGE_MOUSE, 17, 0x100000502ULL},
};
#define N_SERVICES ((int)(sizeof k_services / sizeof k_services[0]))

static cx_class_t HIDClient_c, HIDClient_m, HIDService_c, HIDService_m, HIDEvent_c, HIDEvent_m;
static int hid_ready;

typedef struct { id matching; } hid_client_t;                               /* +8 */
typedef struct { const hid_service_desc_t *d; } hid_service_t;              /* +8 */
typedef struct { u32 type; u32 nfields; u64 timestamp; double f[HID_EVENT_FIELDS]; } hid_event_t;

static void *ivars(id o) { return (u8 *)o + sizeof(void *); }

static void client_dealloc(id self, SEL c) {
    hid_client_t *s = ivars(self);
    if (s->matching) objc_release(s->matching);
    s->matching = 0;
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

static void hid_init(void) {
    if (hid_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&HIDClient_c, &HIDClient_m, "__IOHIDEventSystemClient", ns, (u32)sizeof(hid_client_t));
    cx_add_method(&HIDClient_c, "dealloc", (void *)client_dealloc);
    cx_define_class(&HIDService_c, &HIDService_m, "__IOHIDServiceClient", ns, (u32)sizeof(hid_service_t));
    cx_define_class(&HIDEvent_c, &HIDEvent_m, "__IOHIDEvent", ns, (u32)sizeof(hid_event_t));
    hid_ready = 1;
}

static int is_kind(id o, cx_class_t *k) { return o && hid_ready && object_getClass(o) == k; }

/* um serviço novo a cada cópia, como o IOHIDServiceClientRef que a Apple devolve no array */
static id service_new(const hid_service_desc_t *d) {
    id o = cx_alloc_instance(&HIDService_c);
    if (o) ((hid_service_t *)ivars(o))->d = d;
    return o;
}

static long num_of(id v, int *ok) {
    *ok = 0;
    if (!v || !cx_responds(object_getClass(v), sel_registerName("longLongValue"))) return 0;
    *ok = 1;
    return (long)MSG0(long long, v, "longLongValue");
}

static id service_property(const hid_service_desc_t *d, const char *k) {
    if (!strcmp(k, "PrimaryUsagePage") || !strcmp(k, "DeviceUsagePage")) return cx_num_i(d->page);
    if (!strcmp(k, "PrimaryUsage") || !strcmp(k, "DeviceUsage")) return cx_num_i(d->usage);
    if (!strcmp(k, "Product")) return cx_str(d->product);
    if (!strcmp(k, "Manufacturer")) return cx_str("mOS");
    if (!strcmp(k, "Transport")) return cx_str("Virtual");
    if (!strcmp(k, "Built-In")) return MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (BOOL)1);
    if (!strcmp(k, "RegistryID")) return cx_num_i((i64)d->registry_id);
    if (!strcmp(k, "LocationID")) return cx_num_i(0);
    return 0;
}

/* cada chave do dicionário de filtro tem de bater com a propriedade do serviço */
static int service_matches(const hid_service_desc_t *d, id matching) {
    if (!matching) return 1;
    if (!cx_responds(object_getClass(matching), sel_registerName("allKeys"))) return 1;
    id keys = MSG0(id, matching, "allKeys");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id key = MSG(id, keys, "objectAtIndex:", i);
        id want = MSG(id, matching, "objectForKey:", key);
        const char *k = key && cx_is_str(key) ? cx_cstr(key) : 0;
        id have = k ? service_property(d, k) : 0;
        if (!have) return 0;
        int ok1, ok2;
        long a = num_of(want, &ok1), b = num_of(have, &ok2);
        if (ok1 && ok2) { if (a != b) return 0; continue; }
        if (!MSG(BOOL, have, "isEqual:", want)) return 0;
    }
    return 1;
}

void *IOHIDEventSystemClientCreate(void *allocator) {
    (void)allocator;
    hid_init();
    if (!hid_ready) return 0;
    return cx_alloc_instance(&HIDClient_c);
}

void IOHIDEventSystemClientSetMatching(void *client, id matching) {
    if (!is_kind((id)client, &HIDClient_c)) return;
    hid_client_t *s = ivars((id)client);
    if (matching) objc_retain(matching);
    if (s->matching) objc_release(s->matching);
    s->matching = matching;
}

id IOHIDEventSystemClientCopyServices(void *client) {
    if (!is_kind((id)client, &HIDClient_c)) return 0;
    hid_client_t *s = ivars((id)client);
    id arr = objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array"));
    for (int i = 0; arr && i < N_SERVICES; i++) {
        if (!service_matches(&k_services[i], s->matching)) continue;
        id sv = service_new(&k_services[i]);
        if (!sv) continue;
        MSG(void, arr, "addObject:", sv);
        objc_release(sv);
    }
    return arr;
}

id IOHIDServiceClientCopyProperty(void *service, id key) {
    if (!is_kind((id)service, &HIDService_c) || !key || !cx_is_str(key)) return 0;
    const hid_service_desc_t *d = ((hid_service_t *)ivars((id)service))->d;
    id v = d ? service_property(d, cx_cstr(key)) : 0;
    return v ? objc_retain(v) : 0;
}

/* O window server do mOS não guarda um "último evento" por dispositivo para consulta: teclado e apontador
 * entregam os eventos pela fila de cada app. Um tipo que o serviço não produz (temperatura, luz...) é NULL
 * no macOS também. Pedido do tipo do próprio serviço: evento sem campos preenchidos e marcado com o
 * instante do pedido, que é o que o serviço sabe dizer. */
void *IOHIDServiceClientCopyEvent(void *service, long long type, long long options, u64 timestamp) {
    (void)options;
    if (!is_kind((id)service, &HIDService_c)) return 0;
    const hid_service_desc_t *d = ((hid_service_t *)ivars((id)service))->d;
    if (!d || d->event_type != (int)type) return 0;
    id e = cx_alloc_instance(&HIDEvent_c);
    if (!e) return 0;
    hid_event_t *ev = ivars(e);
    ev->type = (u32)type;
    ev->nfields = HID_EVENT_FIELDS;
    ev->timestamp = timestamp;
    return e;
}

/* campo = (tipo << 16) | índice (IOHIDEventFieldBase). Campo de outro tipo de evento é 0, como na Apple. */
double IOHIDEventGetFloatValue(void *event, u32 field) {
    if (!is_kind((id)event, &HIDEvent_c)) return 0.0;
    hid_event_t *ev = ivars((id)event);
    u32 type = field >> 16, idx = field & 0xffff;
    if (type != ev->type || idx >= ev->nfields) return 0.0;
    return ev->f[idx];
}
