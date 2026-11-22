/*
 * mOS — SystemConfiguration de rede: SCNetworkInterface, SCNetworkService, SCPreferences e as chaves kSC* que os apps
 * usam (stats, Firefox, Thunderbird, Blender, GIMP...). Interfaces vêm do kernel (netif.c: lo0 + en0, en1...); cada
 * placa vira um serviço com ID estável tirado do MAC. SCPreferences lê/grava o plist de preferências do sistema
 * (/Library/Preferences/SystemConfiguration/preferences.plist), com Lock/Commit/Apply.
 */
#include "foundation.h"
#include "libc.h"

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
K(kSCNetworkInterfaceTypeEthernet, "Ethernet") K(kSCNetworkInterfaceTypeIEEE80211, "IEEE80211")
K(kSCNetworkInterfaceTypeBluetooth, "Bluetooth") K(kSCNetworkInterfaceTypeWWAN, "WWAN") K(kSCNetworkInterfaceType6to4, "6to4")
K(kSCNetworkInterfaceTypeBond, "Bond") K(kSCNetworkInterfaceTypeBridge, "Bridge") K(kSCNetworkInterfaceTypeFireWire, "FireWire")
K(kSCNetworkInterfaceTypeIPSec, "IPSec") K(kSCNetworkInterfaceTypeIrDA, "IrDA") K(kSCNetworkInterfaceTypeL2TP, "L2TP")
K(kSCNetworkInterfaceTypeModem, "Modem") K(kSCNetworkInterfaceTypePPP, "PPP") K(kSCNetworkInterfaceTypePPTP, "PPTP")
K(kSCNetworkInterfaceTypeSerial, "Serial") K(kSCNetworkInterfaceTypeVLAN, "VLAN") K(kSCNetworkInterfaceTypeIPv4, "IPv4")
K(kSCPropNetEthernetMediaSubType, "MediaSubType") K(kSCPropNetEthernetMediaOptions, "MediaOptions") K(kSCPropNetEthernetMTU, "MTU")
K(kSCEntNetDNS, "DNS") K(kSCEntNetAirPort, "AirPort") K(kSCEntNetEthernet, "Ethernet") K(kSCEntNetLink, "Link")
K(kSCEntNetProxies, "Proxies") K(kSCEntNetDHCP, "DHCP") K(kSCEntNetPPP, "PPP") K(kSCEntNetSMB, "SMB")
K(kSCDynamicStoreDomainSetup, "Setup:") K(kSCDynamicStoreDomainFile, "File:") K(kSCDynamicStoreDomainPlugin, "Plugin:")
K(kSCDynamicStoreDomainPrefs, "Prefs:")
K(kSCCompAnyRegex, "[^/]+") K(kSCCompNetwork, "Network") K(kSCCompGlobal, "Global") K(kSCCompService, "Service")
K(kSCCompInterface, "Interface") K(kSCCompSystem, "System") K(kSCCompUsers, "Users") K(kSCCompHostNames, "HostNames")
K(kSCPropNetProxiesGopherEnable, "GopherEnable") K(kSCPropNetProxiesGopherPort, "GopherPort")
K(kSCPropNetProxiesGopherProxy, "GopherProxy") K(kSCPropNetProxiesFTPPassive, "FTPPassive")
K(kSCPropNetProxiesRTSPEnable, "RTSPEnable") K(kSCPropNetProxiesRTSPPort, "RTSPPort") K(kSCPropNetProxiesRTSPProxy, "RTSPProxy")
K(kSCPropNetProxiesProxyAutoConfigJavaScript, "ProxyAutoConfigJavaScript")
K(kSCPropNetDNSServerAddresses, "ServerAddresses") K(kSCPropNetDNSSearchDomains, "SearchDomains")
K(kSCPropNetDNSDomainName, "DomainName") K(kSCPropNetDNSServerPort, "ServerPort")
K(kSCPropNetIPv4Addresses, "Addresses") K(kSCPropNetIPv4SubnetMasks, "SubnetMasks") K(kSCPropNetIPv4Router, "Router")
K(kSCPropNetIPv4ConfigMethod, "ConfigMethod") K(kSCValNetIPv4ConfigMethodDHCP, "DHCP")
K(kSCPropNetIPv6Addresses, "Addresses6") K(kSCPropNetIPv6Router, "Router6") K(kSCPropNetIPv6PrefixLength, "PrefixLength6")
K(kSCDynamicStoreKeyConsoleUser_s, "State:/Users/ConsoleUser")
K(kSCPropInterfaceName, "InterfaceName") K(kSCPropNetInterfaceType, "Type") K(kSCPropNetInterfaceDeviceName, "DeviceName")
K(kSCPropNetInterfaceHardware, "Hardware") K(kSCPropUserDefinedName, "UserDefinedName")
K(kSCPropNetLinkActive, "Active") K(kSCPropNetServiceOrder, "ServiceOrder")
K(kSCPrefNetworkServices, "NetworkServices") K(kSCPrefSets, "Sets") K(kSCPrefCurrentSet, "CurrentSet") K(kSCPrefSystem, "System")

typedef struct {
    char bsd[16]; char driver[48]; unsigned index, flags; int loopback, wifi, link, active;
    unsigned char mac[6]; unsigned ip, mask; unsigned char ll6[16], global6[16]; int has_ll6, has_g6, prefix6;
    unsigned long long rx_bytes, tx_bytes, rx_packets, tx_packets;
} cx_netif_t;                                              /* netif.c */
int cx_netif_list(cx_netif_t *out, int max);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id cx_plist_parse(const u8 *b, u64 n);

#define CLASS(N) static cx_class_t N##_c, N##_m;
CLASS(MOSSCNetworkInterface) CLASS(MOSSCNetworkService) CLASS(MOSSCPreferences) CLASS(MOSSCNetworkSet)
typedef struct { cx_class_t *isa; cx_netif_t f; } MOSSCNetworkInterface_t;
typedef struct { cx_class_t *isa; id iface; char sid[40]; char name[64]; int enabled; } MOSSCNetworkService_t;
typedef struct { cx_class_t *isa; id name, prefs_id; id dict; int locked, dirty; char path[256]; } MOSSCPreferences_t;
#define IF(o) ((MOSSCNetworkInterface_t *)(o))
#define SV(o) ((MOSSCNetworkService_t *)(o))
#define PF(o) ((MOSSCPreferences_t *)(o))
static id cls(const char *n) { return (id)(void *)cx_class_named(n); }
static int sc_ready;

static void sc_init(void);
static id iface_new(const cx_netif_t *f) {
    sc_init();
    id o = cx_alloc_instance(&MOSSCNetworkInterface_c);
    IF(o)->f = *f;
    return o;
}
static id all_ifaces(int skip_loop) {
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    id a = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    for (int i = 0; i < n; i++) {
        if (skip_loop && f[i].loopback) continue;
        id o = iface_new(&f[i]);
        MSG(void, a, "addObject:", o);
        objc_release(o);
    }
    return a;
}
static int is_iface(void *i) { return i && object_getClass((id)i) == &MOSSCNetworkInterface_c; }
id SCNetworkInterfaceCopyAll(void) { return all_ifaces(1); }      /* o macOS não lista o lo0 */
unsigned long SCNetworkInterfaceGetTypeID(void) { sc_init(); return (unsigned long)(void *)&MOSSCNetworkInterface_c; }
id SCNetworkInterfaceGetBSDName(void *i) { return is_iface(i) ? objc_autorelease(cx_str(IF(i)->f.bsd)) : 0; }
id SCNetworkInterfaceGetInterfaceType(void *i) {
    if (!is_iface(i)) return 0;
    return IF(i)->f.wifi ? (id)kSCNetworkInterfaceTypeIEEE80211 : (id)kSCNetworkInterfaceTypeEthernet;
}
id SCNetworkInterfaceGetLocalizedDisplayName(void *i) {
    if (!is_iface(i)) return 0;
    return objc_autorelease(cx_str(IF(i)->f.wifi ? "Wi-Fi" : IF(i)->f.loopback ? "Loopback" : "Ethernet"));
}
id SCNetworkInterfaceGetHardwareAddressString(void *i) {
    if (!is_iface(i) || IF(i)->f.loopback) return 0;
    char b[18];
    static const char h[] = "0123456789abcdef";
    for (int k = 0; k < 6; k++) { b[3 * k] = h[IF(i)->f.mac[k] >> 4]; b[3 * k + 1] = h[IF(i)->f.mac[k] & 15]; b[3 * k + 2] = k < 5 ? ':' : 0; }
    return objc_autorelease(cx_str(b));
}
id SCNetworkInterfaceGetInterface(void *i) { (void)i; return 0; }                /* nenhuma camada por baixo (sem PPP/VLAN) */
id SCNetworkInterfaceGetSupportedInterfaceTypes(void *i) { (void)i; return 0; }
id SCNetworkInterfaceGetSupportedProtocolTypes(void *i) {
    if (!is_iface(i)) return 0;
    id a = MSG0(id, cls("NSMutableArray"), "array");
    const char *p[] = {"DNS", "IPv4", "IPv6", "Proxies", "SMB"};
    for (int k = 0; k < 5; k++) MSG(void, a, "addObject:", cx_str(p[k]));
    return a;
}
id SCNetworkInterfaceGetConfiguration(void *i) { (void)i; return 0; }
u8 SCNetworkInterfaceCopyMTU(void *i, int *cur, int *min, int *max) {
    if (!is_iface(i)) return 0;
    if (cur) *cur = IF(i)->f.loopback ? 16384 : 1500;
    if (min) *min = 1280;
    if (max) *max = IF(i)->f.loopback ? 16384 : 1500;
    return 1;
}
static id media_dict(const char *sub, int full) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str(sub), (id)kSCPropNetEthernetMediaSubType);
    id o = MSG0(id, cls("NSMutableArray"), "array");
    if (full) MSG(void, o, "addObject:", cx_str("full-duplex"));
    MSG(void, d, "setObject:forKey:", o, (id)kSCPropNetEthernetMediaOptions);
    return d;
}
/* mídia: o autoselect é o atual; o ativo é o que a placa negociou (com link) */
u8 SCNetworkInterfaceCopyMediaOptions(void *i, id *current, id *active, id *available, u8 filter) {
    (void)filter;
    if (!is_iface(i) || IF(i)->f.loopback) return 0;
    if (current) *current = objc_retain(media_dict("autoselect", 0));
    if (active) *active = IF(i)->f.link ? objc_retain(media_dict(IF(i)->f.wifi ? "autoselect" : "1000baseT", 1)) : 0;
    if (available) {
        id a = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
        MSG(void, a, "addObject:", media_dict("autoselect", 0));
        if (!IF(i)->f.wifi) {
            const char *s[] = {"10baseT/UTP", "100baseTX", "1000baseT"};
            for (int k = 0; k < 3; k++) { MSG(void, a, "addObject:", media_dict(s[k], 0)); MSG(void, a, "addObject:", media_dict(s[k], 1)); }
        }
        *available = a;
    }
    return 1;
}
id SCNetworkInterfaceCopyMediaSubTypes(id available) {
    id out = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    u64 n = available ? MSG0(u64, available, "count") : 0;
    for (u64 k = 0; k < n; k++) {
        id s = MSG(id, MSG(id, available, "objectAtIndex:", k), "objectForKey:", (id)kSCPropNetEthernetMediaSubType);
        if (s && !MSG(BOOL, out, "containsObject:", s)) MSG(void, out, "addObject:", s);
    }
    return out;
}

/* ---- serviços: um por placa */
static id service_new(id iface) {
    id s = cx_alloc_instance(&MOSSCNetworkService_c);
    SV(s)->iface = objc_retain(iface);
    const unsigned char *m = IF(iface)->f.mac;
    /* ID estável: formato de UUID com um prefixo fixo, o índice e o MAC da placa (o mesmo a cada boot) */
    static const char h[] = "0123456789ABCDEF";
    unsigned char u[16] = {0x6d, 0x4f, 0x53, 0x00, 0x00, 0x00, 0x40, 0x00, 0x80, 0x00, m[0], m[1], m[2], m[3], m[4], m[5]};
    u[5] = (unsigned char)IF(iface)->f.index;
    int k = 0;
    for (int b = 0; b < 16; b++) {
        if (b == 4 || b == 6 || b == 8 || b == 10) SV(s)->sid[k++] = '-';
        SV(s)->sid[k++] = h[u[b] >> 4]; SV(s)->sid[k++] = h[u[b] & 15];
    }
    SV(s)->sid[k] = 0;
    strcpy(SV(s)->name, IF(iface)->f.wifi ? "Wi-Fi" : "Ethernet");
    SV(s)->enabled = 1;
    return s;
}
id SCNetworkServiceCopyAll(void *prefs) {
    (void)prefs;
    sc_init();
    id ifs = all_ifaces(1);
    id out = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    u64 n = MSG0(u64, ifs, "count");
    int eth = 0, wifi = 0;
    for (u64 k = 0; k < n; k++) {
        id s = service_new(MSG(id, ifs, "objectAtIndex:", k));
        /* nomes repetidos ganham número, como no Ajustes */
        int *c = IF(SV(s)->iface)->f.wifi ? &wifi : &eth;
        if ((*c)++) { char b[64]; snprintf(b, sizeof b, "%s %d", SV(s)->name, *c); strcpy(SV(s)->name, b); }
        MSG(void, out, "addObject:", s);
        objc_release(s);
    }
    objc_release(ifs);
    return out;
}
static int is_svc(void *s) { return s && object_getClass((id)s) == &MOSSCNetworkService_c; }
id SCNetworkServiceGetInterface(void *s) { return is_svc(s) ? SV(s)->iface : 0; }
id SCNetworkServiceGetServiceID(void *s) { return is_svc(s) ? objc_autorelease(cx_str(SV(s)->sid)) : 0; }
id SCNetworkServiceGetName(void *s) { return is_svc(s) ? objc_autorelease(cx_str(SV(s)->name)) : 0; }
u8 SCNetworkServiceGetEnabled(void *s) { return is_svc(s) && SV(s)->enabled; }
unsigned long SCNetworkServiceGetTypeID(void) { sc_init(); return (unsigned long)(void *)&MOSSCNetworkService_c; }
id SCNetworkServiceCopy(void *prefs, id sid) {
    id all = SCNetworkServiceCopyAll(prefs), found = 0;
    u64 n = MSG0(u64, all, "count");
    for (u64 k = 0; k < n && !found; k++) {
        id s = MSG(id, all, "objectAtIndex:", k);
        if (sid && !strcmp(SV(s)->sid, cx_cstr(sid))) found = objc_retain(s);
    }
    objc_release(all);
    return found;
}

/* ---- SCPreferences */
static const char *PREFS = "/Library/Preferences/SystemConfiguration/preferences.plist";
id SCPreferencesCreate(void *alloc, id name, id prefs_id) {
    (void)alloc;
    sc_init();
    id p = cx_alloc_instance(&MOSSCPreferences_c);
    PF(p)->name = objc_retain(name);
    PF(p)->prefs_id = objc_retain(prefs_id);
    const char *path = prefs_id ? cx_cstr(prefs_id) : 0;
    snprintf(PF(p)->path, sizeof PF(p)->path, "%s", path && path[0] == '/' ? path : PREFS);
    id url = MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(PF(p)->path));
    id data = MSG(id, cls("NSData"), "dataWithContentsOfURL:", url);
    id d = data ? cx_plist_parse(MSG0(const u8 *, data, "bytes"), MSG0(u64, data, "length")) : 0;
    PF(p)->dict = d && MSG(BOOL, d, "isKindOfClass:", cls("NSDictionary")) ? MSG0(id, d, "mutableCopy")
                                                                             : MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    return p;
}
id SCPreferencesCreateWithAuthorization(void *alloc, id name, id prefs_id, void *auth) { (void)auth; return SCPreferencesCreate(alloc, name, prefs_id); }
static int is_pref(void *p) { return p && object_getClass((id)p) == &MOSSCPreferences_c; }
unsigned long SCPreferencesGetTypeID(void) { sc_init(); return (unsigned long)(void *)&MOSSCPreferences_c; }
id SCPreferencesGetValue(void *p, id key) { return is_pref(p) && key ? MSG(id, PF(p)->dict, "objectForKey:", key) : 0; }
id SCPreferencesCopyKeyList(void *p) { return is_pref(p) ? objc_retain(MSG0(id, PF(p)->dict, "allKeys")) : 0; }
u8 SCPreferencesSetValue(void *p, id key, id v) { if (!is_pref(p) || !key || !v) return 0; MSG(void, PF(p)->dict, "setObject:forKey:", v, key); PF(p)->dirty = 1; return 1; }
u8 SCPreferencesAddValue(void *p, id key, id v) { if (!is_pref(p) || !key || SCPreferencesGetValue(p, key)) return 0; return SCPreferencesSetValue(p, key, v); }
u8 SCPreferencesRemoveValue(void *p, id key) { if (!is_pref(p) || !key || !SCPreferencesGetValue(p, key)) return 0; MSG(void, PF(p)->dict, "removeObjectForKey:", key); PF(p)->dirty = 1; return 1; }
u8 SCPreferencesLock(void *p, u8 wait) { (void)wait; if (!is_pref(p) || PF(p)->locked) return 0; PF(p)->locked = 1; return 1; }
u8 SCPreferencesUnlock(void *p) { if (!is_pref(p) || !PF(p)->locked) return 0; PF(p)->locked = 0; return 1; }
u8 SCPreferencesCommitChanges(void *p) {
    if (!is_pref(p)) return 0;
    if (!PF(p)->dirty) return 1;
    id url = MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(PF(p)->path));
    id data = MSG(id, cls("NSPropertyListSerialization"), "dataWithPropertyList:format:options:error:", PF(p)->dict, (u64)100, (u64)0, (id *)0);
    u8 ok = data && MSG(BOOL, data, "writeToURL:atomically:", url, (BOOL)YES);
    if (ok) PF(p)->dirty = 0;
    return ok;
}
u8 SCPreferencesApplyChanges(void *p) { return is_pref(p); }       /* o configd relê o arquivo; não há outro estado */
void SCPreferencesSynchronize(void *p) { (void)p; }
id SCPreferencesGetSignature(void *p) {
    if (!is_pref(p)) return 0;
    char b[64]; snprintf(b, sizeof b, "%lx", (unsigned long)MSG0(u64, PF(p)->dict, "hash"));
    return MSG(id, cls("NSData"), "dataWithBytes:length:", b, (u64)strlen(b));
}

static void svc_dealloc(id s, SEL c) { objc_release(SV(s)->iface); IMP sup = cx_lookup(cx_class_named("NSObject"), c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
static void pref_dealloc(id s, SEL c) {
    objc_release(PF(s)->name); objc_release(PF(s)->prefs_id); objc_release(PF(s)->dict);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void sc_init(void) {
    if (sc_ready) return;
    sc_ready = 1;
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&MOSSCNetworkInterface_c, &MOSSCNetworkInterface_m, "MOSSCNetworkInterface", ob, (u32)(sizeof(MOSSCNetworkInterface_t) - 8));
    cx_define_class(&MOSSCNetworkService_c, &MOSSCNetworkService_m, "MOSSCNetworkService", ob, (u32)(sizeof(MOSSCNetworkService_t) - 8));
    cx_define_class(&MOSSCPreferences_c, &MOSSCPreferences_m, "MOSSCPreferences", ob, (u32)(sizeof(MOSSCPreferences_t) - 8));
    cx_add_method(&MOSSCNetworkService_c, "dealloc", (void *)svc_dealloc);
    cx_add_method(&MOSSCPreferences_c, "dealloc", (void *)pref_dealloc);
    cx_define_class(&MOSSCNetworkSet_c, &MOSSCNetworkSet_m, "MOSSCNetworkSet", ob, 8);
}
/* conjuntos de rede ("locais" do Ajustes): o mOS tem um, o Automático, com todos os serviços */
static int is_set(void *s) { return s && object_getClass((id)s) == &MOSSCNetworkSet_c; }
id SCNetworkSetCopyCurrent(void *prefs) { (void)prefs; sc_init(); return cx_alloc_instance(&MOSSCNetworkSet_c); }
id SCNetworkSetCopyAll(void *prefs) {
    id s = SCNetworkSetCopyCurrent(prefs);
    id a = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    MSG(void, a, "addObject:", s);
    objc_release(s);
    return a;
}
id SCNetworkSetCopyServices(void *set) { return is_set(set) ? SCNetworkServiceCopyAll(0) : 0; }
id SCNetworkSetGetName(void *set) { return is_set(set) ? objc_autorelease(cx_str("Automático")) : 0; }
id SCNetworkSetGetSetID(void *set) { return is_set(set) ? objc_autorelease(cx_str("6D4F5300-0000-4000-8000-000000000001")) : 0; }
id SCNetworkSetGetServiceOrder(void *set) {
    if (!is_set(set)) return 0;
    id svcs = SCNetworkServiceCopyAll(0), ids = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = MSG0(u64, svcs, "count");
    for (u64 k = 0; k < n; k++) { id sv = MSG(id, svcs, "objectAtIndex:", k); MSG(void, ids, "addObject:", SCNetworkServiceGetServiceID(sv)); }
    objc_release(svcs);
    return ids;
}
u8 SCNetworkSetContainsInterface(void *set, void *iface) { (void)iface; return is_set(set); }
unsigned long SCNetworkSetGetTypeID(void) { sc_init(); return (unsigned long)(void *)&MOSSCNetworkSet_c; }
/* chaves e informações do configd */
id SCDynamicStoreKeyCreateConsoleUser(void *alloc) { (void)alloc; return objc_retain((id)kSCDynamicStoreKeyConsoleUser_s); }
id SCDynamicStoreCopyDHCPInfo(void *store, id service) { (void)store; (void)service; return 0; }   /* sem o lease do DHCP no espaço do app */
id DHCPInfoGetOptionData(id info, u8 code) { (void)code; return info ? MSG(id, info, "objectForKey:", cx_str("Option")) : 0; }
id DHCPInfoGetLeaseStartTime(id info) { (void)info; return 0; }
id DHCPInfoGetLeaseExpirationTime(id info) { (void)info; return 0; }
/* conexões PPP/VPN do SystemConfiguration: o mOS não tem serviço desse tipo */
void *SCNetworkConnectionCreateWithServiceID(void *alloc, id sid, void *cb, void *ctx) { (void)alloc; (void)sid; (void)cb; (void)ctx; return 0; }
u8 SCNetworkConnectionCopyUserPreferences(id opts, id *sid, id *uopts) { (void)opts; if (sid) *sid = 0; if (uopts) *uopts = 0; return 0; }
