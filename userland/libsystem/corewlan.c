/*
 * mOS libSystem — CoreWLAN: CWWiFiClient, CWInterface e CWChannel com o estado real do Wi-Fi do kernel
 * (GUI_DEVICE, DEV_WIFI_INFO: kernel/wifi/wifi.c). Reimplementação própria da API pública.
 *
 * Cada pergunta ao CWInterface lê o estado na hora (um app como o Stats consulta rssiValue e transmitRate a
 * cada segundo). Sem placa, +interface devolve nil (como o macOS sem Wi-Fi). Desconectado: ssid/bssid nil,
 * rssi/ruído/taxa 0, wlanChannel nil, serviceActive NO. O ruído vem 0 porque o RTL8821CE não o mede.
 * Constantes: CWChannelBand (1 = 2,4 GHz, 2 = 5 GHz), CWChannelWidth (1/2/3 = 20/40/80 MHz), CWPHYMode
 * (1 11a, 2 11b, 3 11g, 4 11n, 5 11ac), CWSecurity (0 nenhuma, 1 WEP, 4 WPA2 Pessoal, 9 WPA2 Empresarial,
 * 11 WPA3 Pessoal), CWInterfaceMode (0 nenhum, 1 estação).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
id objc_autorelease(id o);
int strcmp(const char *a, const char *b);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);

#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n); cx_class_t n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(CWWiFiClient) CLS(CWInterface) CLS(CWChannel)

static BOOL info(gui_wifi_info_t *o) {
    memset(o, 0, sizeof *o);
    return SYS2(GUI_DEVICE, DEV_WIFI_INFO, o) == 1 && o->present;
}
static BOOL linked(gui_wifi_info_t *o) { return info(o) && o->state == 6; }

/* ---------------------------------------------------------------- CWChannel: valor imutável */
typedef struct { cx_class_t *isa; long number, band, width; } cwch_t;
static id ch_new(long number, long band, long width) {
    cwch_t *c = (cwch_t *)cx_alloc_instance(&CWChannel_c);
    if (!c) return 0;
    c->number = number; c->band = band; c->width = width;
    return objc_autorelease((id)c);
}
static long ch_number(id s, SEL c) { (void)c; return ((cwch_t *)s)->number; }
static long ch_band(id s, SEL c) { (void)c; return ((cwch_t *)s)->band; }
static long ch_width(id s, SEL c) { (void)c; return ((cwch_t *)s)->width; }
static BOOL ch_equal(id s, SEL c, id o) {
    (void)c;
    if (!o || object_getClass(o) != &CWChannel_c) return NO;
    cwch_t *a = (cwch_t *)s, *b = (cwch_t *)o;
    return a->number == b->number && a->band == b->band && a->width == b->width;
}
static BOOL ch_isequal(id s, SEL c, id o) { return ch_equal(s, c, o); }
static u64 ch_hash(id s, SEL c) { (void)c; cwch_t *a = (cwch_t *)s; return (u64)(a->number << 8 | a->band << 4 | a->width); }
static id ch_copy(id s, SEL c, void *zone) { (void)c; (void)zone; return objc_retain(s); }
static id ch_desc(id s, SEL c) {
    (void)c;
    cwch_t *a = (cwch_t *)s;
    char b[96];
    snprintf(b, sizeof b, "[channelNumber=%ld(%s), channelWidth={%d MHz}]", a->number, a->band == 2 ? "5GHz" : "2GHz",
             a->width == 3 ? 80 : a->width == 2 ? 40 : 20);
    return cx_str(b);
}

/* ---------------------------------------------------------------- CWInterface */
typedef struct { cx_class_t *isa; char name[16]; } cwif_t;
static id if_make(const char *name) {
    gui_wifi_info_t o;
    if (!info(&o)) return 0;
    if (name && strcmp(name, o.ifname)) return 0;                 /* só existe a en1 */
    cwif_t *i = (cwif_t *)cx_alloc_instance(&CWInterface_c);
    if (!i) return 0;
    snprintf(i->name, sizeof i->name, "%s", o.ifname);
    return (id)i;
}
static id if_class_interface(id cls, SEL c) { (void)cls; (void)c; id i = if_make(0); return i ? objc_autorelease(i) : 0; }
static id if_class_with_name(id cls, SEL c, id name) {
    (void)cls; (void)c;
    const char *n = name ? MSG0(const char *, name, "UTF8String") : 0;
    id i = if_make(n);
    return i ? objc_autorelease(i) : 0;
}
static id if_init(id s, SEL c) {
    (void)c;
    gui_wifi_info_t o;
    if (!info(&o)) return 0;
    snprintf(((cwif_t *)s)->name, 16, "%s", o.ifname);
    return s;
}
static id if_init_name(id s, SEL c, id name) {
    (void)c;
    gui_wifi_info_t o;
    const char *n = name ? MSG0(const char *, name, "UTF8String") : 0;
    if (!info(&o) || (n && strcmp(n, o.ifname))) return 0;
    snprintf(((cwif_t *)s)->name, 16, "%s", o.ifname);
    return s;
}
static id if_name(id s, SEL c) { (void)c; return cx_str(((cwif_t *)s)->name); }
static id if_ssid(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) && o.ssid[0] ? cx_str(o.ssid) : 0; }
static id if_ssid_data(id s, SEL c) {
    (void)s; (void)c;
    gui_wifi_info_t o;
    if (!linked(&o) || !o.ssid[0]) return 0;
    return MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", (const void *)o.ssid, (u64)strlen(o.ssid));
}
static id if_bssid(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? cx_str(o.bssid) : 0; }
static long if_rssi(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? o.rssi : 0; }
static long if_noise(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? o.noise : 0; }
static double if_rate(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? o.tx_kbps / 1000.0 : 0.0; }
static long if_tx_power(id s, SEL c) { (void)s; (void)c; return 0; }       /* o driver não informa mW de saída */
static id if_channel(id s, SEL c) {
    (void)s; (void)c;
    gui_wifi_info_t o;
    return linked(&o) && o.channel ? ch_new(o.channel, o.band, o.width ? o.width : 1) : 0;
}
static BOOL if_power(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return info(&o) && o.power ? YES : NO; }
static BOOL if_active(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? YES : NO; }
static long if_security(id s, SEL c) {
    (void)s; (void)c;
    gui_wifi_info_t o;
    if (!linked(&o)) return 0;
    switch (o.security) { case 0: return 0; case 1: return 1; case 2: return 4; case 3: return 11; case 4: return 9; default: return 0x7FFFFFFFFFFFFFFFL; }
}
static long if_phy(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? o.phy : 0; }
static long if_mode(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return linked(&o) ? 1 : 0; }
static id if_country(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return info(&o) && o.country[0] ? cx_str(o.country) : 0; }
static id if_hwaddr(id s, SEL c) { (void)s; (void)c; gui_wifi_info_t o; return info(&o) ? cx_str(o.mac) : 0; }
static id if_hwaddr_cfg(id s, SEL c) { return if_hwaddr(s, c); }
static id if_desc(id s, SEL c) {
    (void)c;
    gui_wifi_info_t o;
    char b[160];
    if (linked(&o)) snprintf(b, sizeof b, "<CWInterface: %s ssid=%s rssi=%d canal=%d>", ((cwif_t *)s)->name, o.ssid, o.rssi, o.channel);
    else snprintf(b, sizeof b, "<CWInterface: %s desconectada>", ((cwif_t *)s)->name);
    return cx_str(b);
}

/* ---------------------------------------------------------------- CWWiFiClient */
static id client_shared;
static id cl_shared(id cls, SEL c) {
    (void)cls; (void)c;
    if (!client_shared) client_shared = objc_retain(cx_alloc_instance(&CWWiFiClient_c));
    return client_shared;
}
static id cl_init(id s, SEL c) { (void)c; return s; }
static id cl_interface(id s, SEL c) { (void)s; (void)c; id i = if_make(0); return i ? objc_autorelease(i) : 0; }
static id cl_interface_named(id s, SEL c, id name) { return if_class_with_name(s, c, name); }
static id cl_interfaces(id s, SEL c) {
    (void)s; (void)c;
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    id i = if_make(0);
    if (i) MSG(void, arr, "addObject:", objc_autorelease(i));
    return arr;
}
static id cl_names(id cls, SEL c) {
    (void)cls; (void)c;
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    gui_wifi_info_t o;
    if (info(&o)) MSG(void, arr, "addObject:", cx_str(o.ifname));
    return arr;
}

void cx_corewlan_init(void) {
    if (cx_class_named("CWInterface")) return;
    cx_class_t *obj = cx_class_named("NSObject");
    cx_define_class(&CWChannel_c, &CWChannel_m, "CWChannel", obj, (u32)(sizeof(cwch_t) - 8));
    cx_class_t *k = &CWChannel_c;
    cx_add_method(k, "channelNumber", ch_number); cx_add_method(k, "channelBand", ch_band); cx_add_method(k, "channelWidth", ch_width);
    cx_add_method(k, "isEqualToChannel:", ch_equal); cx_add_method(k, "isEqual:", ch_isequal); cx_add_method(k, "hash", ch_hash);
    cx_add_method(k, "copyWithZone:", ch_copy); cx_add_method(k, "description", ch_desc);

    cx_define_class(&CWInterface_c, &CWInterface_m, "CWInterface", obj, (u32)(sizeof(cwif_t) - 8));
    k = &CWInterface_c;
    cx_add_cmethod(k, "interface", if_class_interface);
    cx_add_cmethod(k, "interfaceWithName:", if_class_with_name);
    cx_add_cmethod(k, "interfaceNames", cl_names);
    cx_add_method(k, "init", if_init); cx_add_method(k, "initWithInterfaceName:", if_init_name);
    cx_add_method(k, "interfaceName", if_name); cx_add_method(k, "ssid", if_ssid); cx_add_method(k, "ssidData", if_ssid_data);
    cx_add_method(k, "bssid", if_bssid); cx_add_method(k, "rssiValue", if_rssi); cx_add_method(k, "noiseMeasurement", if_noise);
    cx_add_method(k, "transmitRate", if_rate); cx_add_method(k, "transmitPower", if_tx_power); cx_add_method(k, "wlanChannel", if_channel);
    cx_add_method(k, "powerOn", if_power); cx_add_method(k, "serviceActive", if_active); cx_add_method(k, "security", if_security);
    cx_add_method(k, "activePHYMode", if_phy); cx_add_method(k, "interfaceMode", if_mode); cx_add_method(k, "countryCode", if_country);
    cx_add_method(k, "hardwareAddress", if_hwaddr); cx_add_method(k, "configuredHardwareAddress", if_hwaddr_cfg);
    cx_add_method(k, "description", if_desc);

    cx_define_class(&CWWiFiClient_c, &CWWiFiClient_m, "CWWiFiClient", obj, 0);
    k = &CWWiFiClient_c;
    cx_add_cmethod(k, "sharedWiFiClient", cl_shared);
    cx_add_cmethod(k, "interfaceNames", cl_names);
    cx_add_method(k, "init", cl_init); cx_add_method(k, "interface", cl_interface);
    cx_add_method(k, "interfaceWithName:", cl_interface_named); cx_add_method(k, "interfaces", cl_interfaces);
}
