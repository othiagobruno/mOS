/* mOS libSystem — AEBuild, AEPrint e AEStream (AEHelpers.h), Object Support Library (AEResolve, acessores), handlers
 * especiais e de coerção, AEPutArray, CreateOffsetDescriptor/CreateRangeDescriptor. Sobre a API C de ae_capi.inc;
 * incluído por appleevents.c e tools/astest.c (que provê NewHandle/GetHandleSize).
 *
 * Gramática do AEBuild: descritor = dado | 'tipo'(dado) | ident(dado) | [d, d...] | {chave: d, ...} | 'tipo'{...}.
 * Dados: inteiro (typeSInt32), "texto" ou “texto” (typeChar), «hex» (precisa de coerção), 'abcd' ou ident sozinho
 * (typeType), @ sozinho = um AEDesc* dos varargs, 'tipo'(@) = tamanho (long) e ponteiro, 'tipo'(@@) = um Handle. */
typedef struct { u32 fError, fErrorPos; } AEBuildError;
enum { aeBuildSyntaxNoErr, aeBuildSyntaxBadToken, aeBuildSyntaxBadEOF, aeBuildSyntaxNoEOF, aeBuildSyntaxBadNegative,
       aeBuildSyntaxMissingQuote, aeBuildSyntaxBadHex, aeBuildSyntaxOddHex, aeBuildSyntaxNoCloseHex, aeBuildSyntaxUncoercedHex,
       aeBuildSyntaxNoCloseString, aeBuildSyntaxBadDesc, aeBuildSyntaxBadData, aeBuildSyntaxNoCloseParen,
       aeBuildSyntaxNoCloseBracket, aeBuildSyntaxNoCloseBrace, aeBuildSyntaxNoKey, aeBuildSyntaxNoColon, aeBuildSyntaxCoercedList,
       aeBuildSyntaxUncoercedDoubleAt };
#define errAEBuildSyntaxError (-1740)
long GetHandleSize(char **h);
char **NewHandle(long size);

typedef struct { const char *s, *p; va_list *ap; u32 err, pos; } aeb_t;
static void aeb_ws(aeb_t *b) { while (*b->p == ' ' || *b->p == '\t' || *b->p == '\n' || *b->p == '\r') b->p++; }
static int aeb_fail(aeb_t *b, u32 e) { if (!b->err) { b->err = e; b->pos = (u32)(b->p - b->s); } return -1; }
static int aeb_is_id(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
/* 'abcd' ou ident de até 4 letras (completado com espaços); 1 se leu */
static int aeb_code(aeb_t *b, u32 *out) {
    char c[4] = {' ', ' ', ' ', ' '};
    int n = 0;
    if (*b->p == '\'') {
        b->p++;
        while (*b->p && *b->p != '\'' && n < 4) c[n++] = *b->p++;
        if (*b->p != '\'') return aeb_fail(b, aeBuildSyntaxMissingQuote);
        b->p++;
    } else if (aeb_is_id(*b->p) && !(*b->p >= '0' && *b->p <= '9')) {
        while (aeb_is_id(*b->p) && n < 4) c[n++] = *b->p++;
        if (aeb_is_id(*b->p)) return aeb_fail(b, aeBuildSyntaxBadToken);
    } else return 0;
    *out = AE_FCC((u8)c[0], (u8)c[1], (u8)c[2], (u8)c[3]);
    return 1;
}
static int aeb_desc(aeb_t *b, AEDesc *out);
/* dado entre parênteses (ou sozinho); type = coerção pedida (0 = nenhuma) */
static int aeb_data(aeb_t *b, u32 type, AEDesc *out) {
    aeb_ws(b);
    const unsigned char *u = (const unsigned char *)b->p;
    if (*b->p == '@') {
        b->p++;
        if (*b->p == '@') {                                                  /* @@: um Handle */
            b->p++;
            if (!type) return aeb_fail(b, aeBuildSyntaxUncoercedDoubleAt);
            char **h = va_arg(*b->ap, char **);
            return AECreateDesc(type, h ? *h : 0, h ? GetHandleSize(h) : 0, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
        }
        if (!type) {                                                         /* @ sozinho: AEDesc* */
            const AEDesc *d = va_arg(*b->ap, const AEDesc *);
            return AEDuplicateDesc(d, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
        }
        long len = va_arg(*b->ap, long);
        const void *ptr = va_arg(*b->ap, const void *);
        return AECreateDesc(type, ptr, len, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
    }
    if (*b->p == '-' || (*b->p >= '0' && *b->p <= '9')) {
        int neg = *b->p == '-';
        if (neg) { b->p++; if (!(*b->p >= '0' && *b->p <= '9')) return aeb_fail(b, aeBuildSyntaxBadNegative); }
        long long v = 0;
        while (*b->p >= '0' && *b->p <= '9') v = v * 10 + (*b->p++ - '0');
        i32 x = (i32)(neg ? -v : v);
        if (!type || type == AE_LONG) return AECreateDesc(AE_LONG, &x, 4, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
        return AECoercePtr(AE_LONG, &x, 4, type, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
    }
    int curly = u[0] == 0xe2 && u[1] == 0x80 && u[2] == 0x9c, mac = u[0] == 0xd2;
    if (*b->p == '"' || curly || mac) {                                      /* texto */
        b->p += curly ? 3 : 1;
        const char *s = b->p;
        for (;;) {
            const unsigned char *q = (const unsigned char *)b->p;
            if (!*q) return aeb_fail(b, aeBuildSyntaxNoCloseString);
            if ((!curly && !mac && *q == '"') || (curly && q[0] == 0xe2 && q[1] == 0x80 && q[2] == 0x9d) || (mac && *q == 0xd3)) break;
            b->p++;
        }
        long n = (long)(b->p - s);
        b->p += curly ? 3 : 1;
        if (!type || type == AE_TEXT) return AECreateDesc(AE_TEXT, s, n, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
        if (AECoercePtr(AE_UTF8, s, n, type, out)) return AECreateDesc(type, s, n, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
        return 0;
    }
    int ghex = u[0] == 0xc2 && u[1] == 0xab, mhex = u[0] == 0xc7;
    if (ghex || mhex) {                                                      /* «hex» */
        b->p += ghex ? 2 : 1;
        if (!type) return aeb_fail(b, aeBuildSyntaxUncoercedHex);
        u8 buf[4096];
        long n = 0;
        int half = -1;
        for (;;) {
            const unsigned char *q = (const unsigned char *)b->p;
            if (!*q) return aeb_fail(b, aeBuildSyntaxNoCloseHex);
            if ((ghex && q[0] == 0xc2 && q[1] == 0xbb) || (mhex && q[0] == 0xc8)) { b->p += ghex ? 2 : 1; break; }
            char c = *b->p++;
            if (c == ' ') continue;
            int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (v < 0) return aeb_fail(b, aeBuildSyntaxBadHex);
            if (half < 0) half = v; else { if (n < (long)sizeof buf) buf[n++] = (u8)(half << 4 | v); half = -1; }
        }
        if (half >= 0) return aeb_fail(b, aeBuildSyntaxOddHex);
        return AECreateDesc(type, buf, n, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
    }
    u32 code;
    int r = aeb_code(b, &code);
    if (r < 0) return r;
    if (r) {                                                                 /* código sozinho: tipo (ou o da coerção) */
        u32 t = type ? type : AE_TYPE;
        return AECreateDesc(t, &code, 4, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0;
    }
    return aeb_fail(b, aeBuildSyntaxBadData);
}
/* {k: d, ...} ou o corpo de parâmetros de um evento (sem chaves) */
static int aeb_pairs(aeb_t *b, AEDesc *rec, char close) {
    aeb_ws(b);
    if (close && *b->p == close) { b->p++; return 0; }
    for (;;) {
        aeb_ws(b);
        if (!close && !*b->p) return 0;
        if (*b->p == '~') b->p++;                                            /* parâmetro opcional: mesma coisa */
        u32 key;
        int r = aeb_code(b, &key);
        if (r <= 0) return r < 0 ? r : aeb_fail(b, aeBuildSyntaxNoKey);
        aeb_ws(b);
        if (*b->p != ':') return aeb_fail(b, aeBuildSyntaxNoColon);
        b->p++;
        AEDesc v = {AE_NULL, 0};
        if (aeb_desc(b, &v)) return -1;
        AEPutKeyDesc(rec, key, &v);
        AEDisposeDesc(&v);
        aeb_ws(b);
        if (*b->p == ',') { b->p++; continue; }
        if (close && *b->p == close) { b->p++; return 0; }
        if (!close && !*b->p) return 0;
        return aeb_fail(b, close ? aeBuildSyntaxNoCloseBrace : aeBuildSyntaxNoEOF);
    }
}
static int aeb_desc(aeb_t *b, AEDesc *out) {
    aeb_ws(b);
    if (*b->p == '[') {
        b->p++;
        if (AECreateList(0, 0, 0, out)) return aeb_fail(b, aeBuildSyntaxBadDesc);
        aeb_ws(b);
        if (*b->p == ']') { b->p++; return 0; }
        for (;;) {
            AEDesc v = {AE_NULL, 0};
            if (aeb_desc(b, &v)) return -1;
            AEPutDesc(out, 0, &v);
            AEDisposeDesc(&v);
            aeb_ws(b);
            if (*b->p == ',') { b->p++; continue; }
            if (*b->p == ']') { b->p++; return 0; }
            return aeb_fail(b, aeBuildSyntaxNoCloseBracket);
        }
    }
    if (*b->p == '{') {
        b->p++;
        if (AECreateList(0, 0, 1, out)) return aeb_fail(b, aeBuildSyntaxBadDesc);
        return aeb_pairs(b, out, '}');
    }
    const char *save = b->p;
    u32 code;
    int r = aeb_code(b, &code);
    if (r < 0) return r;
    if (r) {
        aeb_ws(b);
        if (*b->p == '(') {                                                  /* coerção */
            b->p++;
            aeb_ws(b);
            if (*b->p == ')') { b->p++; return AECreateDesc(code, 0, 0, out) ? aeb_fail(b, aeBuildSyntaxBadData) : 0; }   /* 'null'() */
            if (*b->p == '[') return aeb_fail(b, aeBuildSyntaxCoercedList);
            if (*b->p == '{' || *b->p == '\'' || (aeb_is_id(*b->p) && !(*b->p >= '0' && *b->p <= '9'))) {
                /* descritor inteiro coagido: 'obj '{...} dentro de parênteses ou um código */
                if (*b->p == '{') {
                    AEDesc inner = {AE_NULL, 0};
                    if (aeb_desc(b, &inner)) return -1;
                    int e = AECoerceDesc(&inner, code, out);
                    if (e) { ae_node *n = desc_node(&inner); if (n) n->type = code; *out = inner; inner.dataHandle = 0; out->descriptorType = code; }
                    AEDisposeDesc(&inner);
                } else if (aeb_data(b, code, out)) return -1;
            } else if (aeb_data(b, code, out)) return -1;
            aeb_ws(b);
            if (*b->p != ')') return aeb_fail(b, aeBuildSyntaxNoCloseParen);
            b->p++;
            return 0;
        }
        if (*b->p == '{') {                                                  /* registro com tipo: 'obj '{...} */
            b->p++;
            if (AECreateList(0, 0, 1, out)) return aeb_fail(b, aeBuildSyntaxBadDesc);
            if (aeb_pairs(b, out, '}')) return -1;
            ae_node *n = desc_node(out);
            if (n) n->type = code;
            out->descriptorType = code;
            return 0;
        }
        if (*b->p == '[') return aeb_fail(b, aeBuildSyntaxCoercedList);
        b->p = save;
    }
    return aeb_data(b, 0, out);
}
static int aeb_run_desc(AEDesc *out, AEBuildError *err, const char *fmt, va_list *ap) {
    aeb_t b = {fmt, fmt, ap, 0, 0};
    if (out) { out->descriptorType = AE_NULL; out->dataHandle = 0; }
    int r = aeb_desc(&b, out);
    aeb_ws(&b);
    if (!r && *b.p) r = aeb_fail(&b, aeBuildSyntaxNoEOF);
    if (err) { err->fError = b.err; err->fErrorPos = b.pos; }
    if (r) { AEDisposeDesc(out); return errAEBuildSyntaxError; }
    return noErr;
}
static int aeb_run_params(AEDesc *ev, AEBuildError *err, const char *fmt, va_list *ap) {
    aeb_t b = {fmt, fmt, ap, 0, 0};
    int r = fmt ? aeb_pairs(&b, ev, 0) : 0;
    if (err) { err->fError = b.err; err->fErrorPos = b.pos; }
    return r ? errAEBuildSyntaxError : noErr;
}
int vAEBuildDesc(AEDesc *out, AEBuildError *err, const char *fmt, va_list args) { va_list ap; va_copy(ap, args); int r = aeb_run_desc(out, err, fmt, &ap); va_end(ap); return r; }
int AEBuildDesc(AEDesc *out, AEBuildError *err, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = aeb_run_desc(out, err, fmt, &ap); va_end(ap); return r; }
int vAEBuildParameters(AEDesc *ev, AEBuildError *err, const char *fmt, va_list args) { va_list ap; va_copy(ap, args); int r = aeb_run_params(ev, err, fmt, &ap); va_end(ap); return r; }
int AEBuildParameters(AEDesc *ev, AEBuildError *err, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = aeb_run_params(ev, err, fmt, &ap); va_end(ap); return r; }
int vAEBuildAppleEvent(u32 cls, u32 id_, u32 addrType, const void *addr, long addrLen, i16 rid, i32 tid, AEDesc *ev, AEBuildError *err,
                       const char *fmt, va_list args) {
    AEDesc target = {AE_NULL, 0};
    int e = AECreateDesc(addrType, addr, addrLen, &target);
    if (e) return e;
    e = AECreateAppleEvent(cls, id_, &target, rid, tid, ev);
    AEDisposeDesc(&target);
    if (e) return e;
    va_list ap;
    va_copy(ap, args);
    e = aeb_run_params(ev, err, fmt, &ap);
    va_end(ap);
    if (e) AEDisposeDesc(ev);
    return e;
}
int AEBuildAppleEvent(u32 cls, u32 id_, u32 addrType, const void *addr, long addrLen, i16 rid, i32 tid, AEDesc *ev, AEBuildError *err,
                      const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int e = vAEBuildAppleEvent(cls, id_, addrType, addr, addrLen, rid, tid, ev, err, fmt, ap);
    va_end(ap);
    return e;
}

/* ================================================================ AEPrint: o texto na mesma gramática */
typedef struct { char *b; long n, cap; } aep_t;
static void aep_put(aep_t *o, const char *s, long n) {
    if (o->n + n + 1 > o->cap) { long c = (o->n + n + 1) * 2 + 64; char *x = realloc(o->b, (size_t)c); if (!x) return; o->b = x; o->cap = c; }
    memcpy(o->b + o->n, s, (size_t)n); o->n += n; o->b[o->n] = 0;
}
static void aep_s(aep_t *o, const char *s) { aep_put(o, s, (long)strlen(s)); }
static void aep_code(aep_t *o, u32 c) {
    char t[7] = {'\'', (char)(c >> 24), (char)(c >> 16), (char)(c >> 8), (char)c, '\'', 0};
    int plain = 1;
    for (int i = 1; i < 5; i++) if (!(aeb_is_id(t[i]) || (t[i] == ' ' && i > 1))) plain = 0;
    if (plain) { int n = 4; while (n > 1 && t[n] == ' ') n--; aep_put(o, t + 1, n); }
    else aep_s(o, t);
}
static void aep_desc(aep_t *o, const AEDesc *d) {
    ae_node *n = desc_node(d);
    if (!n) { aep_s(o, "'null'()"); return; }
    if (n->kind == AEK_LIST) {
        aep_s(o, "[");
        long c = 0;
        AECountItems(d, &c);
        for (long i = 1; i <= c; i++) { AEDesc it = {AE_NULL, 0}; u32 kw; if (i > 1) aep_s(o, ", "); if (!AEGetNthDesc(d, i, AE_FCC('*', '*', '*', '*'), &kw, &it)) { aep_desc(o, &it); AEDisposeDesc(&it); } }
        aep_s(o, "]");
        return;
    }
    if (n->kind == AEK_RECORD) {
        if (n->type != AE_RECO) aep_code(o, n->type);
        aep_s(o, "{");
        long c = 0;
        AECountItems(d, &c);
        for (long i = 1; i <= c; i++) {
            AEDesc it = {AE_NULL, 0};
            u32 kw = 0;
            if (i > 1) aep_s(o, ", ");
            if (!AEGetNthDesc(d, i, AE_FCC('*', '*', '*', '*'), &kw, &it)) { aep_code(o, kw); aep_s(o, ":"); aep_desc(o, &it); AEDisposeDesc(&it); }
        }
        aep_s(o, "}");
        return;
    }
    long sz = AEGetDescDataSize(d);
    u8 *buf = malloc((size_t)(sz ? sz : 1));
    if (!buf) return;
    AEGetDescData(d, buf, sz);
    char t[64];
    if (n->type == AE_LONG && sz == 4) { i32 v; memcpy(&v, buf, 4); snprintf(t, sizeof t, "%d", v); aep_s(o, t); }
    else if ((n->type == AE_TYPE) && sz == 4) { u32 v; memcpy(&v, buf, 4); aep_code(o, v); }
    else if (n->type == AE_TEXT || n->type == AE_UTF8) { if (n->type != AE_TEXT) { aep_code(o, n->type); aep_s(o, "("); } aep_s(o, "\xe2\x80\x9c"); aep_put(o, (char *)buf, sz); aep_s(o, "\xe2\x80\x9d"); if (n->type != AE_TEXT) aep_s(o, ")"); }
    else {
        aep_code(o, n->type);
        aep_s(o, "(\xc2\xab");
        for (long i = 0; i < sz; i++) { snprintf(t, sizeof t, "%02X", buf[i]); aep_s(o, t); }
        aep_s(o, "\xc2\xbb)");
    }
    free(buf);
}
int AEPrintDescToHandle(const AEDesc *d, char ***out) {
    if (!d || !out) return paramErr;
    aep_t o = {0, 0, 0};
    if (d->descriptorType == AE_FCC('a', 'e', 'v', 't')) {
        u32 cls = 0, eid = 0, ty;
        long got;
        AEGetAttributePtr(d, AE_FCC('e', 'v', 'c', 'l'), AE_TYPE, &ty, &cls, 4, &got);
        AEGetAttributePtr(d, AE_FCC('e', 'v', 'i', 'd'), AE_TYPE, &ty, &eid, 4, &got);
        aep_code(&o, cls); aep_s(&o, "\\"); aep_code(&o, eid);
        aep_s(&o, "{");
        long c = 0;
        AECountItems(d, &c);
        for (long i = 1; i <= c; i++) {
            AEDesc it = {AE_NULL, 0};
            u32 kw = 0;
            if (i > 1) aep_s(&o, ", ");
            if (!AEGetNthDesc(d, i, AE_FCC('*', '*', '*', '*'), &kw, &it)) { aep_code(&o, kw); aep_s(&o, ":"); aep_desc(&o, &it); AEDisposeDesc(&it); }
        }
        aep_s(&o, "}");
    } else aep_desc(&o, d);
    char **h = NewHandle(o.n + 1);
    if (!h) { free(o.b); return memFullErr; }
    memcpy(*h, o.b ? o.b : "", (size_t)o.n + 1);
    free(o.b);
    *out = h;
    return noErr;
}

/* ================================================================ AEStream: monta descritores em sequência */
#define AES_DEPTH 32
typedef struct { AEDesc d; int kind; u32 key; int has_key; } aes_frame;   /* kind: 0 lista, 1 registro, 2 evento, 3 dados */
typedef struct { aes_frame f[AES_DEPTH]; int top; AEDesc root; int has_root; u32 dtype; u8 *data; long dn, dcap; u32 pending_key;
                 int has_pending; } AEStream;
static int aes_add(AEStream *s, AEDesc *v) {
    if (s->top < 0) {                                                        /* sem contêiner: vira a raiz */
        if (s->has_root) return paramErr;
        s->root = *v; s->has_root = 1; v->dataHandle = 0;
        return noErr;
    }
    aes_frame *f = &s->f[s->top];
    int e;
    if (f->kind == 0) e = AEPutDesc(&f->d, 0, v);
    else { if (!s->has_pending) return errAENotAEDesc; e = AEPutKeyDesc(&f->d, s->pending_key, v); s->has_pending = 0; }
    AEDisposeDesc(v);
    return e;
}
AEStream *AEStreamOpen(void) { AEStream *s = calloc(1, sizeof *s); if (s) s->top = -1; return s; }
AEStream *AEStreamOpenEvent(AEDesc *ev) {
    AEStream *s = AEStreamOpen();
    if (!s || !ev) return s;
    s->top = 0;
    s->f[0].kind = 2;
    s->f[0].d = *ev;                                                         /* o stream fica dono do evento */
    ev->dataHandle = 0; ev->descriptorType = AE_NULL;
    return s;
}
AEStream *AEStreamCreateEvent(u32 cls, u32 id_, u32 addrType, const void *addr, long addrLen, i16 rid, i32 tid) {
    AEDesc target = {AE_NULL, 0}, ev = {AE_NULL, 0};
    if (AECreateDesc(addrType, addr, addrLen, &target)) return 0;
    int e = AECreateAppleEvent(cls, id_, &target, rid, tid, &ev);
    AEDisposeDesc(&target);
    return e ? 0 : AEStreamOpenEvent(&ev);
}
int AEStreamClose(AEStream *s, AEDesc *out) {
    if (!s) return paramErr;
    int e = noErr;
    if (out) { out->descriptorType = AE_NULL; out->dataHandle = 0; }
    if (s->top == 0 && s->f[0].kind == 2) { if (out) *out = s->f[0].d; else AEDisposeDesc(&s->f[0].d); s->top = -1; }
    else if (s->top >= 0) e = errAECorruptData;                              /* algo aberto e não fechado */
    else if (s->has_root) { if (out) *out = s->root; else AEDisposeDesc(&s->root); }
    for (int i = s->top; i >= 0; i--) AEDisposeDesc(&s->f[i].d);
    free(s->data);
    free(s);
    return e;
}
int AEStreamOpenDesc(AEStream *s, u32 type) { if (!s || s->data) return paramErr; s->dtype = type; s->dcap = 64; s->dn = 0; s->data = malloc(64); return s->data ? noErr : memFullErr; }
int AEStreamWriteData(AEStream *s, const void *p, long n) {
    if (!s || !s->data || n < 0) return paramErr;
    if (s->dn + n > s->dcap) { long c = (s->dn + n) * 2; u8 *x = realloc(s->data, (size_t)c); if (!x) return memFullErr; s->data = x; s->dcap = c; }
    memcpy(s->data + s->dn, p, (size_t)n);
    s->dn += n;
    return noErr;
}
int AEStreamCloseDesc(AEStream *s) {
    if (!s || !s->data) return paramErr;
    AEDesc v = {AE_NULL, 0};
    int e = AECreateDesc(s->dtype, s->data, s->dn, &v);
    free(s->data); s->data = 0;
    return e ? e : aes_add(s, &v);
}
int AEStreamWriteDesc(AEStream *s, u32 type, const void *p, long n) { AEDesc v = {AE_NULL, 0}; int e = s ? AECreateDesc(type, p, n, &v) : paramErr; return e ? e : aes_add(s, &v); }
int AEStreamWriteAEDesc(AEStream *s, const AEDesc *d) { AEDesc v = {AE_NULL, 0}; int e = s ? AEDuplicateDesc(d, &v) : paramErr; return e ? e : aes_add(s, &v); }
static int aes_push(AEStream *s, int kind, u32 type) {
    if (!s || s->top + 1 >= AES_DEPTH) return paramErr;
    aes_frame *f = &s->f[++s->top];
    memset(f, 0, sizeof *f);
    f->kind = kind;
    f->key = s->pending_key; f->has_key = s->has_pending;                    /* a chave do contêiner de fora espera o pop */
    s->has_pending = 0;
    int e = AECreateList(0, 0, kind == 1, &f->d);
    if (!e && kind == 1 && type && type != AE_RECO) { ae_node *n = desc_node(&f->d); if (n) n->type = type; f->d.descriptorType = type; }
    return e;
}
static int aes_pop(AEStream *s, int kind) {
    if (!s || s->top < 0 || s->f[s->top].kind != kind) return paramErr;
    AEDesc v = s->f[s->top].d;
    s->pending_key = s->f[s->top].key; s->has_pending = s->f[s->top].has_key;   /* a chave guardada no push */
    s->top--;
    return aes_add(s, &v);
}
int AEStreamOpenList(AEStream *s) { return aes_push(s, 0, 0); }
int AEStreamCloseList(AEStream *s) { return aes_pop(s, 0); }
int AEStreamOpenRecord(AEStream *s, u32 type) { return aes_push(s, 1, type); }
int AEStreamSetRecordType(AEStream *s, u32 type) {
    if (!s || s->top < 0 || s->f[s->top].kind != 1) return paramErr;
    ae_node *n = desc_node(&s->f[s->top].d);
    if (n) n->type = type;
    s->f[s->top].d.descriptorType = type;
    return noErr;
}
int AEStreamCloseRecord(AEStream *s) { return aes_pop(s, 1); }
int AEStreamWriteKey(AEStream *s, u32 key) { if (!s || s->top < 0 || s->f[s->top].kind == 0) return paramErr; s->pending_key = key; s->has_pending = 1; return noErr; }
int AEStreamWriteKeyDesc(AEStream *s, u32 key, u32 type, const void *p, long n) { int e = AEStreamWriteKey(s, key); return e ? e : AEStreamWriteDesc(s, type, p, n); }
int AEStreamOpenKeyDesc(AEStream *s, u32 key, u32 type) { int e = AEStreamWriteKey(s, key); return e ? e : AEStreamOpenDesc(s, type); }
/* parâmetro opcional: a chave entra na lista keyOptionalKeywordAttr ('optk') do evento */
int AEStreamOptionalParam(AEStream *s, u32 key) {
    if (!s || s->top < 0 || s->f[0].kind != 2) return paramErr;
    AEDesc l = {AE_NULL, 0};
    if (AEGetAttributeDesc(&s->f[0].d, AE_FCC('o', 'p', 't', 'k'), AE_LIST, &l)) AECreateList(0, 0, 0, &l);
    AEPutPtr(&l, 0, AE_FCC('k', 'e', 'y', 'w'), &key, 4);
    int e = AEPutAttributeDesc(&s->f[0].d, AE_FCC('o', 'p', 't', 'k'), &l);
    AEDisposeDesc(&l);
    return e;
}

/* ================================================================ Object Support Library */
typedef int (*ae_accessor)(u32 desiredClass, const AEDesc *container, u32 containerClass, u32 form, const AEDesc *data, AEDesc *token, long refcon);
typedef struct { u32 cls, ctype; ae_accessor fn; long refcon; } ae_acc_t;
static ae_acc_t g_acc[64];
static int g_nacc;
static int (*g_dispose_token)(AEDesc *);
#define WILD AE_FCC('*', '*', '*', '*')
int AEObjectInit(void) { return noErr; }
int AEInstallObjectAccessor(u32 cls, u32 ctype, ae_accessor fn, long refcon, u8 isSys) {
    (void)isSys;
    for (int i = 0; i < g_nacc; i++) if (g_acc[i].cls == cls && g_acc[i].ctype == ctype) { g_acc[i].fn = fn; g_acc[i].refcon = refcon; return noErr; }
    if (g_nacc >= 64) return memFullErr;
    g_acc[g_nacc++] = (ae_acc_t){cls, ctype, fn, refcon};
    return noErr;
}
int AERemoveObjectAccessor(u32 cls, u32 ctype, ae_accessor fn, u8 isSys) {
    (void)isSys;
    for (int i = 0; i < g_nacc; i++)
        if (g_acc[i].cls == cls && g_acc[i].ctype == ctype && (!fn || g_acc[i].fn == fn)) { g_acc[i] = g_acc[--g_nacc]; return noErr; }
    return errAEDescNotFound - 22;                                            /* errAEAccessorNotFound (-1723) */
}
static ae_acc_t *acc_find(u32 cls, u32 ctype) {
    ae_acc_t *best = 0;
    for (int i = 0; i < g_nacc; i++) {
        ae_acc_t *a = &g_acc[i];
        if (a->cls == cls && a->ctype == ctype) return a;
        if ((a->cls == cls || a->cls == WILD) && (a->ctype == ctype || a->ctype == WILD) && !best) best = a;
    }
    return best;
}
int AEGetObjectAccessor(u32 cls, u32 ctype, ae_accessor *fn, long *refcon, u8 isSys) {
    (void)isSys;
    ae_acc_t *a = acc_find(cls, ctype);
    if (!a) return -1723;
    if (fn) *fn = a->fn;
    if (refcon) *refcon = a->refcon;
    return noErr;
}
int AECallObjectAccessor(u32 cls, const AEDesc *container, u32 ccls, u32 form, const AEDesc *data, AEDesc *token) {
    ae_acc_t *a = acc_find(cls, container ? container->descriptorType : AE_NULL);
    if (!a) return -1723;
    return a->fn(cls, container, ccls, form, data, token, a->refcon);
}
int AESetObjectCallbacks(void *cmp, void *count, int (*dispose)(AEDesc *), void *mark, void *markp, void *adjust, void *errd) {
    (void)cmp; (void)count; (void)mark; (void)markp; (void)adjust; (void)errd;
    g_dispose_token = dispose;
    return noErr;
}
int AEDisposeToken(AEDesc *t) { if (!t) return paramErr; if (g_dispose_token) { int e = g_dispose_token(t); if (!e) return noErr; } return AEDisposeDesc(t); }
/* resolve 'obj '{want, from, form, seld} de fora para dentro, chamando o acessor de cada nível */
static int ae_resolve(const AEDesc *spec, AEDesc *token) {
    if (!spec || spec->descriptorType != AE_FCC('o', 'b', 'j', ' ')) {           /* não é especificador: o próprio dado */
        return AEDuplicateDesc(spec, token);
    }
    AEDesc want = {AE_NULL, 0}, from = {AE_NULL, 0}, form = {AE_NULL, 0}, seld = {AE_NULL, 0}, ctok = {AE_NULL, 0};
    u32 cls = 0, fm = 0, ccls = AE_NULL, ty;
    long got;
    int e = AEGetKeyPtr(spec, AE_FCC('w', 'a', 'n', 't'), AE_TYPE, &ty, &cls, 4, &got);
    if (!e) e = AEGetKeyPtr(spec, AE_FCC('f', 'o', 'r', 'm'), AE_ENUM, &ty, &fm, 4, &got);
    if (!e) e = AEGetKeyDesc(spec, AE_FCC('s', 'e', 'l', 'd'), WILD, &seld);
    if (!e && !AEGetKeyDesc(spec, AE_FCC('f', 'r', 'o', 'm'), WILD, &from) && from.descriptorType == AE_FCC('o', 'b', 'j', ' ')) {
        AEGetKeyPtr(&from, AE_FCC('w', 'a', 'n', 't'), AE_TYPE, &ty, &ccls, 4, &got);
        e = ae_resolve(&from, &ctok);
    } else if (!e) {
        ccls = from.descriptorType;                                          /* null, ccnt ou exmn: o contêiner é ele */
        e = AEDuplicateDesc(&from, &ctok);
    }
    if (!e) e = AECallObjectAccessor(cls, &ctok, ccls, fm, &seld, token);
    AEDisposeToken(&ctok);
    AEDisposeDesc(&want); AEDisposeDesc(&from); AEDisposeDesc(&form); AEDisposeDesc(&seld);
    return e;
}
int AEResolve(const AEDesc *spec, i16 flags, AEDesc *token) {
    (void)flags;
    if (!spec || !token) return paramErr;
    token->descriptorType = AE_NULL; token->dataHandle = 0;
    return ae_resolve(spec, token);
}

/* ================================================================ handlers especiais e de coerção */
typedef struct { u32 a, b; void *fn; long refcon; u8 desc; } ae_hnd_t;
static ae_hnd_t g_special[16], g_coerce[64];
static int g_nspecial, g_ncoerce;
int AEInstallSpecialHandler(u32 fclass, void *fn, u8 isSys) {
    (void)isSys;
    for (int i = 0; i < g_nspecial; i++) if (g_special[i].a == fclass) { g_special[i].fn = fn; return noErr; }
    if (g_nspecial >= 16) return memFullErr;
    g_special[g_nspecial++] = (ae_hnd_t){fclass, 0, fn, 0, 0};
    return noErr;
}
int AERemoveSpecialHandler(u32 fclass, void *fn, u8 isSys) {
    (void)isSys;
    for (int i = 0; i < g_nspecial; i++) if (g_special[i].a == fclass && (!fn || g_special[i].fn == fn)) { g_special[i] = g_special[--g_nspecial]; return noErr; }
    return errAEDescNotFound - 16;                                            /* errAEHandlerNotFound (-1717) */
}
int AEGetSpecialHandler(u32 fclass, void **fn, u8 isSys) {
    (void)isSys;
    for (int i = 0; i < g_nspecial; i++) if (g_special[i].a == fclass) { if (fn) *fn = g_special[i].fn; return noErr; }
    return -1717;
}
/* AECoerceDesc pergunta primeiro aos handlers do app (ae_capi.inc chama ae_user_coerce) */
static int ae_user_coerce_impl(const AEDesc *d, u32 to, AEDesc *result);
int AEInstallCoercionHandler(u32 from, u32 to, void *fn, long refcon, u8 fromIsDesc, u8 isSys) {
    (void)isSys;
    ae_user_coerce = ae_user_coerce_impl;
    for (int i = 0; i < g_ncoerce; i++) if (g_coerce[i].a == from && g_coerce[i].b == to) { g_coerce[i] = (ae_hnd_t){from, to, fn, refcon, fromIsDesc}; return noErr; }
    if (g_ncoerce >= 64) return memFullErr;
    g_coerce[g_ncoerce++] = (ae_hnd_t){from, to, fn, refcon, fromIsDesc};
    return noErr;
}
int AERemoveCoercionHandler(u32 from, u32 to, void *fn, u8 isSys) {
    (void)isSys;
    for (int i = 0; i < g_ncoerce; i++) if (g_coerce[i].a == from && g_coerce[i].b == to && (!fn || g_coerce[i].fn == fn)) { g_coerce[i] = g_coerce[--g_ncoerce]; return noErr; }
    return -1717;
}
static int ae_user_coerce_impl(const AEDesc *d, u32 to, AEDesc *result) {
    for (int i = 0; i < g_ncoerce; i++) {
        ae_hnd_t *h = &g_coerce[i];
        if ((h->a != d->descriptorType && h->a != WILD) || (h->b != to && h->b != WILD)) continue;
        if (h->desc) return ((int (*)(const AEDesc *, u32, long, AEDesc *))h->fn)(d, to, h->refcon, result);
        long n = AEGetDescDataSize(d);
        void *p = malloc((size_t)(n ? n : 1));
        if (!p) return memFullErr;
        AEGetDescData(d, p, n);
        int e = ((int (*)(u32, const void *, long, u32, long, AEDesc *))h->fn)(d->descriptorType, p, n, to, h->refcon, result);
        free(p);
        return e;
    }
    return errAECoercionFail;
}
/* AEManagerInfo: o Apple Event Manager do mOS não publica contadores nem versão por esta chamada */
int AEManagerInfo(u32 key, long *result) { (void)key; if (result) *result = 0; return errAEDescNotFound; }
/* AEPutArray: kAEDataArray 0 / kAEPackedArray 1 (itens de tamanho fixo), kAEHandleArray 2, kAEDescArray 3, kAEKeyDescArray 4 */
typedef struct __attribute__((packed, aligned(2))) { u32 key; AEDesc d; } AEKeyDesc;
int AEPutArray(AEDesc *list, int kind, const void *arr, u32 itemType, long itemSize, long count) {
    if (!list || (!arr && count)) return paramErr;
    for (long i = 0; i < count; i++) {
        int e;
        switch (kind) {
        case 0: case 1: e = AEPutPtr(list, 0, itemType, (const u8 *)arr + i * itemSize, itemSize); break;
        case 2: { char **h = ((char ***)arr)[i]; e = AEPutPtr(list, 0, itemType, h ? *h : 0, h ? GetHandleSize(h) : 0); break; }
        case 3: e = AEPutDesc(list, 0, &((const AEDesc *)arr)[i]); break;
        case 4: { const AEKeyDesc *k = &((const AEKeyDesc *)arr)[i]; e = AEPutKeyDesc(list, k->key, &k->d); break; }
        default: return paramErr;
        }
        if (e) return e;
    }
    return noErr;
}
/* descritores de posição (formAbsolutePosition) e de intervalo (formRange) */
int CreateOffsetDescriptor(i32 off, AEDesc *out) { return AECreateDesc(AE_LONG, &off, 4, out); }
int CreateRangeDescriptor(AEDesc *start, AEDesc *stop, u8 dispose, AEDesc *out) {
    if (!start || !stop || !out) return paramErr;
    int e = AECreateList(0, 0, 1, out);
    if (!e) e = AEPutKeyDesc(out, AE_FCC('s', 't', 'a', 'r'), start);
    if (!e) e = AEPutKeyDesc(out, AE_FCC('s', 't', 'o', 'p'), stop);
    if (!e) { ae_node *n = desc_node(out); if (n) n->type = AE_FCC('r', 'a', 'n', 'g'); out->descriptorType = AE_FCC('r', 'a', 'n', 'g'); }
    if (dispose) { AEDisposeDesc(start); AEDisposeDesc(stop); }
    return e;
}
