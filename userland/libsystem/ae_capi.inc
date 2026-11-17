/* mOS libSystem — API C do Apple Event Manager sobre ae_core.inc: AEDesc com o layout da Apple (#pragma pack(2):
 * tipo de 4 bytes e o ponteiro logo depois, 12 bytes), AECreateDesc, listas e registros, parâmetros e atributos de
 * evento, coerções, AECreateAppleEvent e CreateObjSpecifier. Incluído por appleevents.c e tools/astest.c. */
/* ---- códigos */
enum {
    noErr = 0, paramErr = -50, memFullErr = -108, procNotFound = -600, errAECoercionFail = -1700,
    errAEDescNotFound = -1701, errAECorruptData = -1702, errAEWrongDataType = -1703, errAENotAEDesc = -1704,
    errAEBadListItem = -1705, errAEEventNotHandled = -1708, errAEUnknownSendMode = -1710, errAETimeout = -1712,
    errAEIllegalIndex = -1719, errAEEventNotPermitted = -1743, errAEEventWouldRequireUserConsent = -1744,
};
enum { kAENoReply = 1, kAEQueueReply = 2, kAEWaitReply = 3, kAENeverInteract = 0x10, kAECanInteract = 0x20,
       kAEAlwaysInteract = 0x30 };
#define kAEDefaultTimeout (-1)
#define kNoTimeOut (-2)
enum { K_SUBJ = AE_FCC('s', 'u', 'b', 'j'), K_ADDR = AE_FCC('a', 'd', 'd', 'r'), K_ERRN = AE_ERRN, K_ERRS = AE_ERRS };


/* ================================================================ AEDesc (layout da Apple) */
typedef struct __attribute__((packed, aligned(2))) { u32 descriptorType; void *dataHandle; } AEDesc;
static ae_node *desc_node(const AEDesc *d) { return d ? (ae_node *)d->dataHandle : 0; }
static void desc_set(AEDesc *d, ae_node *n) {
    if (!d) { ae_free(n); return; }
    d->descriptorType = n ? n->type : AE_NULL;
    d->dataHandle = n;
}

int AEInitializeDesc(AEDesc *d) { if (!d) return paramErr; d->descriptorType = AE_NULL; d->dataHandle = 0; return noErr; }
int AECreateDesc(u32 type, const void *ptr, long size, AEDesc *result) {
    if (!result || size < 0 || (size && !ptr)) return paramErr;
    AEInitializeDesc(result);
    ae_node *n = type == AE_LIST ? ae_list() : type == AE_RECO ? ae_record() : ae_scalar(type, ptr, (u32)size);
    if (!n) return memFullErr;
    desc_set(result, n);
    return noErr;
}
int AEDisposeDesc(AEDesc *d) {
    if (!d) return paramErr;
    ae_free(desc_node(d));
    AEInitializeDesc(d);
    return noErr;
}
int AEDuplicateDesc(const AEDesc *src, AEDesc *result) {
    if (!result) return paramErr;
    AEInitializeDesc(result);
    if (!src || !desc_node(src)) return noErr;
    ae_node *n = ae_copy(desc_node(src));
    if (!n) return memFullErr;
    desc_set(result, n);
    return noErr;
}
long AEGetDescDataSize(const AEDesc *d) { ae_node *n = desc_node(d); return n && n->kind == AEK_SCALAR ? (long)n->n : 0; }
int AEGetDescData(const AEDesc *d, void *ptr, long max) {
    ae_node *n = desc_node(d);
    if (!n || !ptr || max < 0) return paramErr;
    if (n->kind != AEK_SCALAR) return errAEWrongDataType;
    memcpy(ptr, n->data, (size_t)(max < (long)n->n ? max : (long)n->n));
    return noErr;
}
int AEReplaceDescData(u32 type, const void *ptr, long size, AEDesc *d) {
    if (!d) return paramErr;
    ae_free(desc_node(d));
    AEInitializeDesc(d);
    return AECreateDesc(type, ptr, size, d);
}
u8 AECheckIsRecord(const AEDesc *d) { ae_node *n = desc_node(d); return n && (n->kind == AEK_RECORD || n->kind == AEK_EVENT); }
/* handlers de coerção instalados pelo app (AEInstallCoercionHandler, ae_build.inc) têm prioridade */
static int (*ae_user_coerce)(const AEDesc *d, u32 to, AEDesc *result);
int AECoerceDesc(const AEDesc *d, u32 to, AEDesc *result) {
    if (ae_user_coerce && d) { int ue = ae_user_coerce(d, to, result); if (ue != errAECoercionFail) return ue; }
    if (!result) return paramErr;
    AEInitializeDesc(result);
    ae_node *n = desc_node(d);
    if (!n) return errAENotAEDesc;
    ae_node *c = ae_coerce(n, to);
    if (!c) return errAECoercionFail;
    desc_set(result, c);
    return noErr;
}
int AECoercePtr(u32 type, const void *ptr, long size, u32 to, AEDesc *result) {
    AEDesc tmp;
    int r = AECreateDesc(type, ptr, size, &tmp);
    if (r) return r;
    r = AECoerceDesc(&tmp, to, result);
    AEDisposeDesc(&tmp);
    return r;
}
int AECreateList(const void *factor, long fsize, u8 is_record, AEDesc *result) {
    (void)factor; (void)fsize;
    if (!result) return paramErr;
    AEInitializeDesc(result);
    ae_node *n = is_record ? ae_record() : ae_list();
    if (!n) return memFullErr;
    desc_set(result, n);
    return noErr;
}
int AECountItems(const AEDesc *list, long *count) {
    ae_node *n = desc_node(list);
    if (!count) return paramErr;
    if (!n || n->kind == AEK_SCALAR) { *count = 0; return n ? errAEWrongDataType : errAENotAEDesc; }
    *count = n->count;
    return noErr;
}
int AEPutDesc(AEDesc *list, long index, const AEDesc *d) {
    ae_node *n = desc_node(list), *it = desc_node(d);
    if (!n || n->kind != AEK_LIST || !it) return errAEWrongDataType;
    if (index < 0 || index > (long)n->count + 1) return errAEIllegalIndex;
    return ae_list_put(n, (u32)index, ae_copy(it)) ? memFullErr : noErr;
}
int AEPutPtr(AEDesc *list, long index, u32 type, const void *ptr, long size) {
    AEDesc tmp;
    int r = AECreateDesc(type, ptr, size, &tmp);
    if (r) return r;
    r = AEPutDesc(list, index, &tmp);
    AEDisposeDesc(&tmp);
    return r;
}
static int nth(const AEDesc *list, long index, ae_node **it, u32 *kw) {
    ae_node *n = desc_node(list);
    if (!n || n->kind == AEK_SCALAR) return errAEWrongDataType;
    if (index < 1 || index > (long)n->count) return errAEIllegalIndex;
    *it = n->items[index - 1];
    if (kw) *kw = n->keys ? n->keys[index - 1] : 0;
    return noErr;
}
int AEGetNthDesc(const AEDesc *list, long index, u32 desired, u32 *kw, AEDesc *result) {
    ae_node *it;
    u32 k = 0;
    if (!result) return paramErr;
    AEInitializeDesc(result);
    int r = nth(list, index, &it, &k);
    if (r) return r;
    if (kw) *kw = k;
    ae_node *c = ae_coerce(it, desired);
    if (!c) return errAECoercionFail;
    desc_set(result, c);
    return noErr;
}
static int copy_out(ae_node *it, u32 desired, u32 *type, void *ptr, long max, long *actual) {
    ae_node *c = ae_coerce(it, desired);
    if (!c) return errAECoercionFail;
    if (type) *type = c->type;
    if (actual) *actual = c->n;
    if (ptr && max > 0) memcpy(ptr, c->data, (size_t)(max < (long)c->n ? max : (long)c->n));
    ae_free(c);
    return noErr;
}
int AEGetNthPtr(const AEDesc *list, long index, u32 desired, u32 *kw, u32 *type, void *ptr, long max, long *actual) {
    ae_node *it;
    int r = nth(list, index, &it, kw);
    return r ? r : copy_out(it, desired, type, ptr, max, actual);
}
int AESizeOfNthItem(const AEDesc *list, long index, u32 *type, long *size) {
    ae_node *it;
    int r = nth(list, index, &it, 0);
    if (r) return r;
    if (type) *type = it->type;
    if (size) *size = it->n;
    return noErr;
}
int AEDeleteItem(AEDesc *list, long index) {
    ae_node *n = desc_node(list);
    if (!n || n->kind != AEK_LIST) return errAEWrongDataType;
    return ae_delete_nth(n, (u32)index) ? errAEIllegalIndex : noErr;
}
/* ---- registros e parâmetros */
int AEPutKeyDesc(AEDesc *rec, u32 key, const AEDesc *d) {
    ae_node *n = desc_node(rec), *it = desc_node(d);
    if (!n || !it) return errAENotAEDesc;
    if (n->kind != AEK_RECORD && n->kind != AEK_EVENT) return errAEWrongDataType;
    return ae_put_key(n, key, ae_copy(it)) ? memFullErr : noErr;
}
int AEPutKeyPtr(AEDesc *rec, u32 key, u32 type, const void *ptr, long size) {
    AEDesc tmp;
    int r = AECreateDesc(type, ptr, size, &tmp);
    if (r) return r;
    r = AEPutKeyDesc(rec, key, &tmp);
    AEDisposeDesc(&tmp);
    return r;
}
int AEGetKeyDesc(const AEDesc *rec, u32 key, u32 desired, AEDesc *result) {
    if (!result) return paramErr;
    AEInitializeDesc(result);
    ae_node *it = ae_get_key(desc_node(rec), key);
    if (!it) return errAEDescNotFound;
    ae_node *c = ae_coerce(it, desired);
    if (!c) return errAECoercionFail;
    desc_set(result, c);
    return noErr;
}
int AEGetKeyPtr(const AEDesc *rec, u32 key, u32 desired, u32 *type, void *ptr, long max, long *actual) {
    ae_node *it = ae_get_key(desc_node(rec), key);
    return it ? copy_out(it, desired, type, ptr, max, actual) : errAEDescNotFound;
}
int AESizeOfKeyDesc(const AEDesc *rec, u32 key, u32 *type, long *size) {
    ae_node *it = ae_get_key(desc_node(rec), key);
    if (!it) return errAEDescNotFound;
    if (type) *type = it->type;
    if (size) *size = it->n;
    return noErr;
}
int AEDeleteKeyDesc(AEDesc *rec, u32 key) { return ae_del_key(desc_node(rec), key) ? errAEDescNotFound : noErr; }
int AEPutParamDesc(AEDesc *ev, u32 key, const AEDesc *d) { return AEPutKeyDesc(ev, key, d); }
int AEPutParamPtr(AEDesc *ev, u32 key, u32 type, const void *ptr, long size) { return AEPutKeyPtr(ev, key, type, ptr, size); }
int AEGetParamDesc(const AEDesc *ev, u32 key, u32 desired, AEDesc *result) { return AEGetKeyDesc(ev, key, desired, result); }
int AEGetParamPtr(const AEDesc *ev, u32 key, u32 desired, u32 *type, void *ptr, long max, long *actual) {
    return AEGetKeyPtr(ev, key, desired, type, ptr, max, actual);
}
int AESizeOfParam(const AEDesc *ev, u32 key, u32 *type, long *size) { return AESizeOfKeyDesc(ev, key, type, size); }
int AEDeleteParam(AEDesc *ev, u32 key) { return AEDeleteKeyDesc(ev, key); }

/* ---- atributos: os do cabeçalho do evento ('evcl', 'evid', 'rtid', 'tran', 'addr') e os livres */
static ae_node *attr_of(ae_node *e, u32 key, int *owned) {
    *owned = 1;
    if (!e || e->kind != AEK_EVENT) return 0;
    switch (key) {
    case AE_FCC('e', 'v', 'c', 'l'): return ae_code(AE_TYPE, e->ev_class);
    case AE_FCC('e', 'v', 'i', 'd'): return ae_code(AE_TYPE, e->ev_id);
    case AE_FCC('r', 't', 'i', 'd'): return ae_i32(e->return_id);
    case AE_FCC('t', 'r', 'a', 'n'): return ae_i32(e->transaction_id);
    case K_ADDR: return e->target ? ae_copy(e->target) : 0;
    }
    *owned = 0;
    return ae_get_attr(e, key);
}
int AEGetAttributeDesc(const AEDesc *ev, u32 key, u32 desired, AEDesc *result) {
    if (!result) return paramErr;
    AEInitializeDesc(result);
    int owned;
    ae_node *a = attr_of(desc_node(ev), key, &owned);
    if (!a) return errAEDescNotFound;
    ae_node *c = ae_coerce(a, desired);
    if (owned) ae_free(a);
    if (!c) return errAECoercionFail;
    desc_set(result, c);
    return noErr;
}
int AEGetAttributePtr(const AEDesc *ev, u32 key, u32 desired, u32 *type, void *ptr, long max, long *actual) {
    int owned;
    ae_node *a = attr_of(desc_node(ev), key, &owned);
    if (!a) return errAEDescNotFound;
    int r = copy_out(a, desired, type, ptr, max, actual);
    if (owned) ae_free(a);
    return r;
}
int AESizeOfAttribute(const AEDesc *ev, u32 key, u32 *type, long *size) {
    int owned;
    ae_node *a = attr_of(desc_node(ev), key, &owned);
    if (!a) return errAEDescNotFound;
    if (type) *type = a->type;
    if (size) *size = a->n;
    if (owned) ae_free(a);
    return noErr;
}
int AEPutAttributeDesc(AEDesc *ev, u32 key, const AEDesc *d) {
    ae_node *e = desc_node(ev), *it = desc_node(d);
    if (!e || e->kind != AEK_EVENT || !it) return errAEWrongDataType;
    if (key == K_ADDR) { ae_free(e->target); e->target = ae_copy(it); return noErr; }
    return ae_put_attr(e, key, ae_copy(it)) ? memFullErr : noErr;
}
int AEPutAttributePtr(AEDesc *ev, u32 key, u32 type, const void *ptr, long size) {
    AEDesc tmp;
    int r = AECreateDesc(type, ptr, size, &tmp);
    if (r) return r;
    r = AEPutAttributeDesc(ev, key, &tmp);
    AEDisposeDesc(&tmp);
    return r;
}
static i32 g_next_rid = 1;
int AECreateAppleEvent(u32 cls, u32 id_, const AEDesc *target, i16 rid, i32 tid, AEDesc *result) {
    if (!result) return paramErr;
    AEInitializeDesc(result);
    i32 r = rid == -1 ? __sync_fetch_and_add(&g_next_rid, 1) : rid;    /* kAutoGenerateReturnID */
    ae_node *e = ae_event(cls, id_, target && desc_node(target) ? ae_copy(desc_node(target)) : 0, r, tid);
    if (!e) return memFullErr;
    desc_set(result, e);
    return noErr;
}

/* especificador de objeto: o registro 'obj ' com 'want' (classe), 'form' (forma da chave), 'seld' (chave) e 'from'
 * (o contêiner; 'null' = o aplicativo). dispose: as entradas passam para o descritor novo. */
int CreateObjSpecifier(u32 desired_class, AEDesc *container, u32 key_form, AEDesc *key, u8 dispose, AEDesc *result) {
    if (!result || !key || !desc_node(key)) return paramErr;
    AEInitializeDesc(result);
    ae_node *o = ae_record();
    if (!o) return memFullErr;
    o->type = AE_FCC('o', 'b', 'j', ' ');
    ae_put_key(o, AE_FCC('w', 'a', 'n', 't'), ae_code(AE_TYPE, desired_class));
    ae_put_key(o, AE_FCC('f', 'o', 'r', 'm'), ae_code(AE_ENUM, key_form));
    ae_put_key(o, AE_FCC('s', 'e', 'l', 'd'), ae_copy(desc_node(key)));
    ae_put_key(o, AE_FCC('f', 'r', 'o', 'm'), container && desc_node(container) ? ae_copy(desc_node(container)) : ae_scalar(AE_NULL, 0, 0));
    desc_set(result, o);
    if (dispose) { if (container) AEDisposeDesc(container); AEDisposeDesc(key); }
    return noErr;
}

