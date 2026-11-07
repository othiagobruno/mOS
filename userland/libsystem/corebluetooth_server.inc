/* incluído por corebluetooth.c — papel periférico: CBPeripheralManager anuncia (DEV_BT_ADVERTISE) e, quando um central
 * conecta, serve GATT pelo canal ATT cru: banco de atributos montado pelos addService:, Read By Group Type, Read By Type,
 * Find Information, Read/Read Blob, Write Request/Command, CCCD (assinaturas) e notificações/indicações. Leituras e
 * escritas sem valor fixo vão ao delegate (peripheralManager:didReceiveReadRequest:/didReceiveWriteRequests:), que
 * responde com respondToRequest:withResult:. */

cx_class_t CBMutableService_c __asm__("OBJC_CLASS_$_CBMutableService"), CBMutableService_m __asm__("OBJC_METACLASS_$_CBMutableService");
cx_class_t CBMutableCharacteristic_c __asm__("OBJC_CLASS_$_CBMutableCharacteristic"), CBMutableCharacteristic_m __asm__("OBJC_METACLASS_$_CBMutableCharacteristic");
cx_class_t CBMutableDescriptor_c __asm__("OBJC_CLASS_$_CBMutableDescriptor"), CBMutableDescriptor_m __asm__("OBJC_METACLASS_$_CBMutableDescriptor");
cx_class_t CBATTRequest_c __asm__("OBJC_CLASS_$_CBATTRequest"), CBATTRequest_m __asm__("OBJC_METACLASS_$_CBATTRequest");
cx_class_t CBCentral_c __asm__("OBJC_CLASS_$_CBCentral"), CBCentral_m __asm__("OBJC_METACLASS_$_CBCentral");

typedef struct { void *isa; id identifier; u16 mtu; } cbcentral_t;
typedef struct { void *isa; id delegate; void *queue; BOOL sent; } cbpm_t;
typedef struct { void *isa; id central, chr, value; i64 offset; u8 op; u16 handle; BOOL answered; } cbreq_t;
enum { AT_SVC, AT_DECL, AT_VALUE, AT_CCCD, AT_DESC };
typedef struct { u16 h; int kind; id obj; } attr_t;
static attr_t g_attr[512];
static int g_nattr;
static struct { id mgr; id services; BOOL adv, link; id central; u16 cccd_val[512]; } gs;

static id svc_mut_init(id s, SEL c, id uuid, BOOL primary) {
    (void)c;
    cbsvc_t *v = SVC(s);
    v->uuid = ret(uuid); v->primary = primary; v->chars = ret(marr()); v->included = ret(marr());
    return s;
}
static void svc_set_chars(id s, SEL c, id a) { (void)c; setobj(&SVC(s)->chars, a ? MSG(id, cls("NSMutableArray"), "arrayWithArray:", a) : marr()); }
static void svc_set_included(id s, SEL c, id a) { (void)c; setobj(&SVC(s)->included, a); }
static id chr_mut_init(id s, SEL c, id uuid, u64 props, id value, u64 perms) {
    (void)c;
    cbchr_t *x = CHR(s);
    x->uuid = ret(uuid); x->props = (u8)props; x->value = value ? ret(MSG(id, cls("NSData"), "dataWithData:", value)) : 0;
    x->descs = ret(marr()); x->perms = perms; x->fixed = value != 0;
    return s;
}
static void chr_set_value(id s, SEL c, id v) { (void)c; setobj(&CHR(s)->value, v); }
static void chr_set_props(id s, SEL c, u64 p) { (void)c; CHR(s)->props = (u8)p; }
static u64 chr_perms(id s, SEL c) { (void)c; return CHR(s)->perms; }
static void chr_set_perms(id s, SEL c, u64 p) { (void)c; CHR(s)->perms = p; }
static void chr_set_descs(id s, SEL c, id a) { (void)c; setobj(&CHR(s)->descs, a ? MSG(id, cls("NSMutableArray"), "arrayWithArray:", a) : marr()); }
static id chr_subscribed(id s, SEL c) {
    (void)c;
    id a = marr();
    if (CHR(s)->subscribed && gs.central) MSG(void, a, "addObject:", gs.central);
    return a;
}
static id dsc_mut_init(id s, SEL c, id uuid, id value) { (void)c; DSC(s)->uuid = ret(uuid); DSC(s)->value = ret(value); return s; }
static id cen_id(id s, SEL c) { (void)c; return ((cbcentral_t *)s)->identifier; }
static u64 cen_max(id s, SEL c) { (void)c; return (u64)(((cbcentral_t *)s)->mtu - 3); }
static id rq_central(id s, SEL c) { (void)c; return ((cbreq_t *)s)->central; }
static id rq_chr(id s, SEL c) { (void)c; return ((cbreq_t *)s)->chr; }
static i64 rq_offset(id s, SEL c) { (void)c; return ((cbreq_t *)s)->offset; }
static id rq_value(id s, SEL c) { (void)c; return ((cbreq_t *)s)->value; }
static void rq_set_value(id s, SEL c, id v) { (void)c; setobj(&((cbreq_t *)s)->value, v); }

/* bytes do UUID no ATT (2 ou 16, little-endian) */
static int att_uuid(id u, u8 *out) {
    cbuuid_t *x = UU(u);
    if (uuid_is_short(x) && !x->b[0] && !x->b[1]) { out[0] = x->b[3]; out[1] = x->b[2]; return 2; }
    for (int i = 0; i < 16; i++) out[i] = x->b[15 - i];
    return 16;
}
static u16 uuid16(id u) { cbuuid_t *x = UU(u); return uuid_is_short(x) && !x->b[0] && !x->b[1] ? (u16)(x->b[2] << 8 | x->b[3]) : 0; }
static void rebuild_db(void) {
    g_nattr = 0;
    u16 h = 1;
    for (u64 i = 0; i < cnt(gs.services); i++) {
        cbsvc_t *s = SVC(at(gs.services, i));
        s->start = h;
        g_attr[g_nattr++] = (attr_t){h++, AT_SVC, (id)s};
        for (u64 k = 0; k < cnt(s->chars) && g_nattr < 500; k++) {
            cbchr_t *c = CHR(at(s->chars, k));
            c->service = (id)s;
            c->decl = h; g_attr[g_nattr++] = (attr_t){h++, AT_DECL, (id)c};
            c->vh = h; g_attr[g_nattr++] = (attr_t){h++, AT_VALUE, (id)c};
            c->cccd = 0;
            if (c->props & 0x30) { c->cccd = h; g_attr[g_nattr++] = (attr_t){h++, AT_CCCD, (id)c}; }
            for (u64 d = 0; d < cnt(c->descs) && g_nattr < 510; d++) { DSC(at(c->descs, d))->h = h; DSC(at(c->descs, d))->chr = (id)c; g_attr[g_nattr++] = (attr_t){h++, AT_DESC, at(c->descs, d)}; }
            c->end = (u16)(h - 1);
        }
        s->end = (u16)(h - 1);
    }
}
static attr_t *attr_at(u16 h) { for (int i = 0; i < g_nattr; i++) if (g_attr[i].h == h) return &g_attr[i]; return 0; }
static void att_err(u8 op, u16 h, u8 code) { u8 r[5] = {0x01, op}; put16(r + 2, h); r[4] = code; att_send(r, 5); }
static void *pm_queue(void) { return gs.mgr ? ((cbpm_t *)gs.mgr)->queue : 0; }
static void pm_deliver(const char *sel, int n, id a1, id a2, id a3) {
    if (!gs.mgr) return;
    id d = ((cbpm_t *)gs.mgr)->delegate;
    if (!d) return;
    dcall_t *x = malloc(sizeof *x);
    x->obj = ret(d); x->sel = sel_registerName(sel); x->nargs = n;
    x->a1 = ret(a1); x->a2 = ret(a2); x->a3 = ret(a3); x->a4 = 0;
    void *q = pm_queue();
    if (!q || q == dispatch_get_main_queue()) dcall_run(x); else dispatch_async_f(q, x, dcall_run);
}
static void send_value(u8 op, id value, i64 off, u16 h) {
    u8 r[260];
    u16 mtu = gs.central ? ((cbcentral_t *)gs.central)->mtu : 23;
    u64 n = value ? MSG0(u64, value, "length") : 0;
    if ((u64)off > n) { att_err(op, h, 0x07); return; }
    u64 k = n - (u64)off;
    if (k > (u64)(mtu - 1)) k = mtu - 1;
    r[0] = op == 0x0A ? 0x0B : 0x0D;
    if (k) memcpy(r + 1, MSG0(const u8 *, value, "bytes") + off, k);
    att_send(r, (u32)(1 + k));
}
static void server_rx(const u8 *p, int n) {
    if (n < 1) return;
    u8 op = p[0], r[260];
    cbcentral_t *cen = (cbcentral_t *)gs.central;
    u16 mtu = cen ? cen->mtu : 23;
    switch (op) {
    case 0x02: {                                                      /* Exchange MTU */
        u16 want = n >= 3 ? le16(p + 1) : 23;
        if (cen) cen->mtu = want < 23 ? 23 : want > 247 ? 247 : want;
        r[0] = 0x03; put16(r + 1, 247); att_send(r, 3);
        return;
    }
    case 0x10: case 0x08: case 0x04: {
        if (n < 5) { att_err(op, 0, 0x04); return; }
        u16 s = le16(p + 1), e = le16(p + 3);
        u16 type = op == 0x04 ? 0 : (n == 7 ? le16(p + 5) : 0xFFFF);
        int len = 2, el = 0;
        for (int i = 0; i < g_nattr; i++) {
            attr_t *a = &g_attr[i];
            if (a->h < s || a->h > e) continue;
            u8 item[24];
            int il = 0;
            if (op == 0x10) {                                         /* serviços primários */
                if (a->kind != AT_SVC || type != 0x2800 || !SVC(a->obj)->primary) continue;
                put16(item, a->h); put16(item + 2, SVC(a->obj)->end);
                il = 4 + att_uuid(SVC(a->obj)->uuid, item + 4);
            } else if (op == 0x08) {
                if (type == 0x2803 && a->kind == AT_DECL) {
                    cbchr_t *c = CHR(a->obj);
                    put16(item, a->h); item[2] = c->props; put16(item + 3, c->vh);
                    il = 5 + att_uuid(c->uuid, item + 5);
                } else if (a->kind == AT_VALUE && type != 0xFFFF && uuid16(CHR(a->obj)->uuid) == type && CHR(a->obj)->value) {
                    u64 vl = MSG0(u64, CHR(a->obj)->value, "length");
                    if (vl > 20) vl = 20;
                    put16(item, a->h); memcpy(item + 2, MSG0(const u8 *, CHR(a->obj)->value, "bytes"), vl);
                    il = 2 + (int)vl;
                } else continue;
            } else {                                                  /* Find Information: tipo de cada atributo */
                u16 t = a->kind == AT_SVC ? (SVC(a->obj)->primary ? 0x2800 : 0x2801) : a->kind == AT_DECL ? 0x2803 : a->kind == AT_CCCD ? 0x2902 : 0;
                put16(item, a->h);
                if (t) { put16(item + 2, t); il = 4; }
                else il = 2 + att_uuid(a->kind == AT_VALUE ? CHR(a->obj)->uuid : DSC(a->obj)->uuid, item + 2);
                if (el == 0) r[1] = il == 4 ? 1 : 2;
                else if ((il == 4) != (r[1] == 1)) break;
            }
            if (el && il != el) break;                                /* todas as entradas de uma resposta com o mesmo tamanho */
            if (len + il > mtu) break;
            el = il;
            memcpy(r + len, item, (size_t)il);
            len += il;
        }
        if (!el) { att_err(op, s, 0x0A); return; }
        r[0] = (u8)(op + 1);
        if (op != 0x04) r[1] = (u8)el;
        att_send(r, (u32)len);
        return;
    }
    case 0x0A: case 0x0C: {                                           /* Read / Read Blob */
        u16 h = n >= 3 ? le16(p + 1) : 0;
        i64 off = op == 0x0C && n >= 5 ? le16(p + 3) : 0;
        attr_t *a = attr_at(h);
        if (!a) { att_err(op, h, 0x01); return; }
        if (a->kind == AT_SVC) { u8 u[16]; int ul = att_uuid(SVC(a->obj)->uuid, u); r[0] = 0x0B; memcpy(r + 1, u, (size_t)ul); att_send(r, (u32)(1 + ul)); return; }
        if (a->kind == AT_DECL) { cbchr_t *c = CHR(a->obj); r[0] = 0x0B; r[1] = c->props; put16(r + 2, c->vh); int ul = att_uuid(c->uuid, r + 4); att_send(r, (u32)(4 + ul)); return; }
        if (a->kind == AT_CCCD) { r[0] = 0x0B; put16(r + 1, gs.cccd_val[h & 511]); att_send(r, 3); return; }
        if (a->kind == AT_DESC) { send_value(op, DSC(a->obj)->value, off, h); return; }
        cbchr_t *c = CHR(a->obj);
        if (!(c->props & 0x02)) { att_err(op, h, 0x02); return; }
        if (c->fixed) { send_value(op, c->value, off, h); return; }
        cbreq_t *q = (cbreq_t *)cx_alloc_instance(&CBATTRequest_c);
        q->central = gs.central; q->chr = (id)c; q->offset = off; q->op = op; q->handle = h;
        pm_deliver("peripheralManager:didReceiveReadRequest:", 2, gs.mgr, (id)q, 0);
        objc_release((id)q);
        return;
    }
    case 0x12: case 0x52: {                                           /* Write Request / Write Command */
        u16 h = n >= 3 ? le16(p + 1) : 0;
        attr_t *a = attr_at(h);
        if (!a) { if (op == 0x12) att_err(op, h, 0x01); return; }
        if (a->kind == AT_CCCD) {
            u16 v = n >= 5 ? le16(p + 3) : 0;
            cbchr_t *c = CHR(a->obj);
            gs.cccd_val[h & 511] = v;
            BOOL was = c->subscribed;
            c->subscribed = v != 0;
            if (op == 0x12) { u8 ok = 0x13; att_send(&ok, 1); }
            if (c->subscribed != was)
                pm_deliver(c->subscribed ? "peripheralManager:central:didSubscribeToCharacteristic:" : "peripheralManager:central:didUnsubscribeFromCharacteristic:",
                           3, gs.mgr, gs.central, (id)c);
            return;
        }
        if (a->kind != AT_VALUE || !(CHR(a->obj)->props & (op == 0x12 ? 0x08 : 0x04))) { if (op == 0x12) att_err(op, h, 0x03); return; }
        cbreq_t *q = (cbreq_t *)cx_alloc_instance(&CBATTRequest_c);
        q->central = gs.central; q->chr = a->obj; q->op = op; q->handle = h;
        q->value = ret(data_of(p + 3, (u64)(n > 3 ? n - 3 : 0)));
        q->answered = op == 0x52;
        id list = MSG(id, cls("NSArray"), "arrayWithObject:", (id)q);
        pm_deliver("peripheralManager:didReceiveWriteRequests:", 2, gs.mgr, list, 0);
        objc_release((id)q);
        return;
    }
    case 0x1E: return;                                                /* confirmação de indicação */
    default: if (op & 0x40) return; att_err(op, 0, 0x06); return;   /* comandos sem resposta são ignorados */
    }
}

/* ---- CBPeripheralManager */
static i64 pm_state_now(void) { return radio_state(); }
static id pm_init3(id s, SEL c, id d, void *q, id o) {
    (void)c; (void)o;
    foundation_gaps();
    ((cbpm_t *)s)->delegate = d; ((cbpm_t *)s)->queue = q;
    gs.mgr = s;
    if (!gs.services) gs.services = ret(marr());
    ensure_ticker();
    return s;
}
static id pm_init2(id s, SEL c, id d, void *q) { return pm_init3(s, c, d, q, 0); }
static i64 pm_state(id s, SEL c) { (void)s; (void)c; return pm_state_now(); }
static BOOL pm_advertising(id s, SEL c) { (void)s; (void)c; return gs.adv; }
static id pm_get_delegate(id s, SEL c) { (void)c; return ((cbpm_t *)s)->delegate; }
static void pm_set_delegate(id s, SEL c, id d) { (void)c; ((cbpm_t *)s)->delegate = d; }
static void pm_add_service(id s, SEL c, id svc) {
    (void)s; (void)c;
    if (!svc) return;
    MSG(void, gs.services, "addObject:", svc);
    rebuild_db();
    pm_deliver("peripheralManager:didAddService:error:", 3, gs.mgr, svc, 0);
}
static void pm_remove_service(id s, SEL c, id svc) { (void)s; (void)c; MSG(void, gs.services, "removeObject:", svc); rebuild_db(); }
static void pm_remove_all(id s, SEL c) { (void)s; (void)c; MSG0(void, gs.services, "removeAllObjects"); rebuild_db(); }
/* anúncio: flags, nome e UUIDs de 16 bits (o que couber nos 31 bytes) */
static void pm_start_adv(id s, SEL c, id dict) {
    (void)c;
    u8 ad[31];
    int n = 0;
    ad[n++] = 2; ad[n++] = 0x01; ad[n++] = 0x06;
    id uuids = dict ? MSG(id, dict, "objectForKey:", CBAdvertisementDataServiceUUIDsKey) : 0;
    int nu = 0;
    u16 us[8];
    for (u64 i = 0; i < cnt(uuids) && nu < 8; i++) { u16 v = uuid16(at(uuids, i)); if (v) us[nu++] = v; }
    if (nu && n + 2 + 2 * nu <= 31) { ad[n++] = (u8)(1 + 2 * nu); ad[n++] = 0x03; for (int i = 0; i < nu; i++) { put16(ad + n, us[i]); n += 2; } }
    id name = dict ? MSG(id, dict, "objectForKey:", CBAdvertisementDataLocalNameKey) : 0;
    if (name && cx_is_str(name)) {
        const char *t = cx_cstr(name);
        int tl = (int)strlen(t), room = 31 - n - 2;
        if (room > 0) { int k = tl < room ? tl : room; ad[n++] = (u8)(k + 1); ad[n++] = (u8)(k < tl ? 0x08 : 0x09); memcpy(ad + n, t, (size_t)k); n += k; }
    }
    long r = SYS4(GUI_DEVICE, DEV_BT_ADVERTISE, ad, n, 1);
    gs.adv = r == 0;
    pm_deliver("peripheralManagerDidStartAdvertising:error:", 2, s, r ? cb_error(CBErrorDomain, 1, "O rádio não aceitou o anúncio") : 0, 0);
}
static void pm_stop_adv(id s, SEL c) { (void)s; (void)c; SYS4(GUI_DEVICE, DEV_BT_ADVERTISE, 0, 0, 0); gs.adv = NO; }
/* respondToRequest:withResult: (CBATTError: 0 = sucesso) */
static void pm_respond(id s, SEL c, id req, i64 result) {
    (void)s; (void)c;
    cbreq_t *q = (cbreq_t *)req;
    if (!q || q->answered) return;
    q->answered = YES;
    if (result) { att_err(q->op, q->handle, (u8)result); return; }
    if (q->op == 0x12) { u8 ok = 0x13; att_send(&ok, 1); return; }
    send_value(q->op, q->value ? q->value : CHR(q->chr)->value, q->offset, q->handle);
}
/* updateValue:forCharacteristic:onSubscribedCentrals: → notificação (ou indicação) */
static BOOL pm_update(id s, SEL c, id value, id chr, id centrals) {
    (void)s; (void)c; (void)centrals;
    cbchr_t *x = CHR(chr);
    setobj(&x->value, value);
    if (!x->subscribed || !gs.link) return NO;
    u8 r[260];
    u16 mtu = gs.central ? ((cbcentral_t *)gs.central)->mtu : 23;
    u64 n = value ? MSG0(u64, value, "length") : 0;
    if (n > (u64)(mtu - 3)) n = mtu - 3;
    r[0] = (x->props & 0x10) ? 0x1B : 0x1D;
    put16(r + 1, x->vh);
    if (n) memcpy(r + 3, MSG0(const u8 *, value, "bytes"), n);
    att_send(r, (u32)(3 + n));
    return YES;
}
static void pm_set_latency(id s, SEL c, i64 l, id central) { (void)s; (void)c; (void)l; (void)central; }
static i64 pm_auth(id k, SEL c) { (void)k; (void)c; return 3; }

/* chamado pelo cb_tick: enlace como periférico */
static void server_tick(void) {
    if (!gs.mgr) return;
    gui_bt_link_t l;
    memset(&l, 0, sizeof l);
    SYS2(GUI_DEVICE, DEV_BT_LINK, &l);
    BOOL up = l.state == 2 && l.role == 1;
    if (up && !gs.link) {
        gs.link = YES;
        gs.adv = NO;
        cbcentral_t *cn = (cbcentral_t *)cx_alloc_instance(&CBCentral_c);
        char u[40];
        const char *a = l.addr;
        snprintf(u, sizeof u, "6D4F5300-0000-4000-8001-%c%c%c%c%c%c%c%c%c%c%c%c", a[0], a[1], a[3], a[4], a[6], a[7], a[9], a[10], a[12], a[13], a[15], a[16]);
        cn->identifier = ret(objc_autorelease(MSG(id, MSG0(id, cls("NSUUID"), "alloc"), "initWithUUIDString:", cx_str(u))));
        cn->mtu = 23;
        setobj(&gs.central, (id)cn);
        objc_release((id)cn);
        memset(gs.cccd_val, 0, sizeof gs.cccd_val);
    } else if (!up && gs.link) {
        gs.link = NO;
        for (int i = 0; i < g_nattr; i++) if (g_attr[i].kind == AT_CCCD && CHR(g_attr[i].obj)->subscribed) {
            CHR(g_attr[i].obj)->subscribed = NO;
            pm_deliver("peripheralManager:central:didUnsubscribeFromCharacteristic:", 3, gs.mgr, gs.central, g_attr[i].obj);
        }
        setobj(&gs.central, 0);
    }
    for (int k = 0; k < 32 && gs.link; k++) {
        u8 b[260];
        long n = SYS3(GUI_DEVICE, DEV_BT_ATT_RECV, b, sizeof b);
        if (n <= 0) break;
        server_rx(b, (int)n);
    }
    static i64 last_state = -1;
    i64 st = pm_state_now();
    if (st != last_state) { last_state = st; pm_deliver("peripheralManagerDidUpdateState:", 1, gs.mgr, 0, 0); }
}

static void server_register(cx_class_t *obj) {
    cx_class_t *pm = &CBPeripheralManager_c;
    A(pm, "initWithDelegate:queue:", pm_init2); A(pm, "initWithDelegate:queue:options:", pm_init3); A(pm, "state", pm_state);
    A(pm, "delegate", pm_get_delegate); A(pm, "setDelegate:", pm_set_delegate);
    A(pm, "isAdvertising", pm_advertising); A(pm, "startAdvertising:", pm_start_adv); A(pm, "stopAdvertising", pm_stop_adv);
    A(pm, "addService:", pm_add_service); A(pm, "removeService:", pm_remove_service); A(pm, "removeAllServices", pm_remove_all);
    A(pm, "respondToRequest:withResult:", pm_respond); A(pm, "updateValue:forCharacteristic:onSubscribedCentrals:", pm_update);
    A(pm, "setDesiredConnectionLatency:forCentral:", pm_set_latency);
    AC(pm, "authorizationStatus", pm_auth); AC(pm, "authorization", pm_auth);
    K(&CBMutableService_c, &CBMutableService_m, "CBMutableService", &CBService_c, 0);
    A(&CBMutableService_c, "initWithType:primary:", svc_mut_init); A(&CBMutableService_c, "setCharacteristics:", svc_set_chars);
    A(&CBMutableService_c, "setIncludedServices:", svc_set_included);
    K(&CBMutableCharacteristic_c, &CBMutableCharacteristic_m, "CBMutableCharacteristic", &CBCharacteristic_c, 0);
    cx_class_t *mc = &CBMutableCharacteristic_c;
    A(mc, "initWithType:properties:value:permissions:", chr_mut_init); A(mc, "setValue:", chr_set_value); A(mc, "setProperties:", chr_set_props);
    A(mc, "permissions", chr_perms); A(mc, "setPermissions:", chr_set_perms); A(mc, "setDescriptors:", chr_set_descs);
    A(mc, "subscribedCentrals", chr_subscribed);
    K(&CBMutableDescriptor_c, &CBMutableDescriptor_m, "CBMutableDescriptor", &CBDescriptor_c, 0);
    A(&CBMutableDescriptor_c, "initWithType:value:", dsc_mut_init);
    K(&CBCentral_c, &CBCentral_m, "CBCentral", &CBPeer_c, (u32)(sizeof(cbcentral_t) - 8));
    A(&CBCentral_c, "identifier", cen_id); A(&CBCentral_c, "maximumUpdateValueLength", cen_max);
    K(&CBATTRequest_c, &CBATTRequest_m, "CBATTRequest", obj, (u32)(sizeof(cbreq_t) - 8));
    A(&CBATTRequest_c, "central", rq_central); A(&CBATTRequest_c, "characteristic", rq_chr); A(&CBATTRequest_c, "offset", rq_offset);
    A(&CBATTRequest_c, "value", rq_value); A(&CBATTRequest_c, "setValue:", rq_set_value);
}
