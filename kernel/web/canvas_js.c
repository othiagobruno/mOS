/* mOS — API JavaScript do Canvas 2D (HTML Living Standard): HTMLCanvasElement (width, height, getContext,
 * toDataURL), CanvasRenderingContext2D completo, CanvasGradient, CanvasPattern, Path2D, ImageData, TextMetrics,
 * DOMMatrix (2D) e DOMException. O desenho fica em canvas.c. */
#include "js_int.h"
#include "canvas.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

/* ============================================================ utilidades */
static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}

/* chave oculta do contexto 2D no invólucro do <canvas> (string internada: o coletor pode liberá-la, então não fica em cache) */
#define K_CTX js_intern("\x01" "canvas2d", -1)

jsv js_throw_dom(const char *name, const char *msg) {
    static const struct { const char *n; int c; } codes[] = {
        { "IndexSizeError", 1 }, { "HierarchyRequestError", 3 }, { "WrongDocumentError", 4 }, { "InvalidCharacterError", 5 },
        { "NoModificationAllowedError", 7 }, { "NotFoundError", 8 }, { "NotSupportedError", 9 }, { "InvalidStateError", 11 },
        { "SyntaxError", 12 }, { "InvalidModificationError", 13 }, { "NamespaceError", 14 }, { "InvalidAccessError", 15 },
        { "TypeMismatchError", 17 }, { "SecurityError", 18 }, { "NetworkError", 19 }, { "AbortError", 20 },
        { "URLMismatchError", 21 }, { "QuotaExceededError", 22 }, { "TimeoutError", 23 }, { "InvalidNodeTypeError", 24 },
        { "DataCloneError", 25 } };
    jsobj *e = js_newobj(J.realm->p_domexc ? J.realm->p_domexc : J.realm->p_err[E_ERROR]);
    int code = 0;
    for (u32 i = 0; i < ARRAY_LEN(codes); i++) if (!strcmp(codes[i].n, name)) code = codes[i].c;
    js_defn(e, "name", jv_str(js_strz(name)), PF_NOENUM);
    js_defn(e, "message", jv_str(js_strz(msg)), PF_NOENUM);
    js_defn(e, "code", jv_int(code), PF_NOENUM);
    return js_throw(jv_obj(e));
}

NATIVE(domexc_ctor) {
    UNUSED_ARGS;
    jsobj *e = js_newobj(J.realm->p_domexc);
    jsstr *m = jv_isnullish(ARG(0)) ? A[A_empty] : js_tostr(ARG(0));
    if (!m) return JS_EXC;
    jsstr *n = argc > 1 && ARG(1) != JS_UNDEF ? js_tostr(ARG(1)) : js_strz("Error");
    if (!n) return JS_EXC;
    js_defn(e, "message", jv_str(m), PF_NOENUM);
    js_defn(e, "name", jv_str(n), PF_NOENUM);
    js_defn(e, "code", jv_int(0), PF_NOENUM);
    return jv_obj(e);
}

/* converte n argumentos: -1 exceção, 0 algum não é finito, 1 todos finitos */
static int fargs(int argc, jsv *argv, int from, int n, float *f) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        double d = js_tonum(ARG(from + i));
        if (js_pending()) return -1;
        if (!(d == d) || d > 3.0e38 || d < -3.0e38) ok = 0;
        f[i] = (float)d;
    }
    return ok;
}

#define NEED(n, what) do { if (argc < (n)) return js_throw_err(E_TYPE, "%s: são necessários %d argumentos", what, n); } while (0)
#define FARGS(from, n, arr) do { int r_ = fargs(argc, argv, from, n, arr); if (r_ < 0) return JS_EXC; if (!r_) return JS_UNDEF; } while (0)

static bool str_is(jsv v, const char *s) { return jv_isstr(v) && !strcmp(jv_s(v)->d, s); }

/* ============================================================ contexto: acesso */
typedef struct { jsobj *o; canvas2d_t *c; js_page_t *p; jsobj *refs; } ctx_t;

static bool this_ctx(jsv thisv, ctx_t *X) {
    if (!jv_iscls(thisv, CL_HOST) || !has_proto(jv_o(thisv), J.realm->p_ctx2d)) {
        js_throw_err(E_TYPE, "Invocação ilegal: não é um CanvasRenderingContext2D");
        return false;
    }
    X->o = jv_o(thisv);
    X->p = X->o->u.host.page;
    X->c = canvas_of(js_page_doc(X->p), X->o->u.host.node, true);
    X->refs = jv_isobj(X->o->u.host.extra) ? jv_o(X->o->u.host.extra) : NULL;
    if (!X->c || !X->refs) { js_throw_err(E_TYPE, "Contexto 2D sem canvas"); return false; }
    return true;
}

static void *blob_of(jsv v) {
    if (!jv_iscls(v, CL_HOST)) return NULL;
    jsv b = jv_o(v)->u.host.extra;
    if (!jv_iscls(b, CL_ARRAYBUFFER)) return NULL;
    return jv_o(b)->u.buf.data;
}

/* gradientes e padrões ficam vivos no JS: o ponteiro é lido na hora de desenhar */
static void sync_paints(ctx_t *X) {
    cpaint_t *cp[2] = { &X->c->st.fill, &X->c->st.stroke };
    for (int i = 0; i < 2; i++) cp[i]->obj = cp[i]->kind == PS_COLOR ? NULL : blob_of(js_arr_get(X->refs, i));
}

static void repaint(ctx_t *X) { js_page_repaint(X->p); }

#define CTX() ctx_t X; if (!this_ctx(thisv, &X)) return JS_EXC; canvas2d_t *c = X.c; (void)c

/* ============================================================ Path2D: comandos num ArrayBuffer */
static void pc_load(jsobj *buf, pcmd_t *pc) {
    memset(pc, 0, sizeof *pc);
    float *h = (float *)buf->u.buf.data;
    if (buf->u.buf.len < sizeof(float) * PC_HDR) return;
    pc->base = h;
    pc->n = (int)h[0];
    pc->cap = (int)(buf->u.buf.len / sizeof(float)) - PC_HDR;
    pc->cx = h[1]; pc->cy = h[2]; pc->sx = h[3]; pc->sy = h[4]; pc->has = h[5] != 0; pc->last = (int)h[6];
}
static void pc_store(jsobj *buf, pcmd_t *pc) {
    if (!pc->base) return;
    float *h = pc->base;
    h[0] = (float)pc->n; h[1] = pc->cx; h[2] = pc->cy; h[3] = pc->sx; h[4] = pc->sy; h[5] = pc->has ? 1.0f : 0.0f; h[6] = (float)pc->last;
    buf->u.buf.data = (u8 *)pc->base;
    buf->u.buf.len = (u32)(sizeof(float) * (PC_HDR + pc->cap));
}
static jsobj *path_buf(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_path2d)) return NULL;
    jsv b = jv_o(v)->u.host.extra;
    return jv_iscls(b, CL_ARRAYBUFFER) ? jv_o(b) : NULL;
}

/* alvo dos métodos de caminho: o caminho atual do contexto (em pixels, com a matriz) ou um Path2D */
typedef struct { pcmd_t *pc; pcmd_t tmp; jsobj *buf; mat_t m; ctx_t X; bool is_ctx; } ptarget_t;

static bool path_target(jsv thisv, ptarget_t *T) {
    memset(T, 0, sizeof *T);
    if (jv_iscls(thisv, CL_HOST) && has_proto(jv_o(thisv), J.realm->p_ctx2d)) {
        if (!this_ctx(thisv, &T->X)) return false;
        T->pc = &T->X.c->path;
        T->m = T->X.c->st.m;
        T->is_ctx = true;
        return true;
    }
    T->buf = path_buf(thisv);
    if (!T->buf) { js_throw_err(E_TYPE, "Invocação ilegal: não é um Path2D nem um contexto 2D"); return false; }
    pc_load(T->buf, &T->tmp);
    T->pc = &T->tmp;
    T->m = MID;
    return true;
}
static void path_done(ptarget_t *T) { if (T->buf) pc_store(T->buf, &T->tmp); }

/* magic: 0 moveTo 1 lineTo 2 quadraticCurveTo 3 bezierCurveTo 4 arcTo 5 rect 6 arc 7 ellipse 8 closePath 9 roundRect */
NATIVE(path_op) {
    UNUSED_ARGS;
    static const int need[] = { 2, 2, 4, 6, 5, 4, 5, 7, 0, 4 };
    static const char *names[] = { "moveTo", "lineTo", "quadraticCurveTo", "bezierCurveTo", "arcTo", "rect", "arc", "ellipse", "closePath", "roundRect" };
    int op = self->u.nat.magic;
    NEED(need[op], names[op]);
    ptarget_t T;
    if (!path_target(thisv, &T)) return JS_EXC;
    float f[8] = { 0 };
    int nf = op == 6 ? 5 : op == 7 ? 7 : need[op];
    FARGS(0, nf, f);
    bool ccw = false;
    if (op == 6 || op == 7) ccw = js_truthy(ARG(op == 6 ? 5 : 7));
    switch (op) {
    case 0: pc_move(T.pc, &T.m, f[0], f[1]); break;
    case 1: pc_line(T.pc, &T.m, f[0], f[1]); break;
    case 2: pc_quad(T.pc, &T.m, f[0], f[1], f[2], f[3]); break;
    case 3: pc_cubic(T.pc, &T.m, f[0], f[1], f[2], f[3], f[4], f[5]); break;
    case 4:
        if (f[4] < 0) { path_done(&T); return js_throw_dom("IndexSizeError", "arcTo: raio negativo"); }
        pc_arc_to(T.pc, &T.m, f[0], f[1], f[2], f[3], f[4]);
        break;
    case 5: pc_rect(T.pc, &T.m, f[0], f[1], f[2], f[3]); break;
    case 6:
        if (f[2] < 0) { path_done(&T); return js_throw_dom("IndexSizeError", "arc: raio negativo"); }
        pc_ellipse(T.pc, &T.m, f[0], f[1], f[2], f[2], 0, f[3], f[4], ccw);
        break;
    case 7:
        if (f[2] < 0 || f[3] < 0) { path_done(&T); return js_throw_dom("IndexSizeError", "ellipse: raio negativo"); }
        pc_ellipse(T.pc, &T.m, f[0], f[1], f[2], f[3], f[4], f[5], f[6], ccw);
        break;
    case 8: pc_close(T.pc); break;
    case 9: {
        /* raios: número, {x,y} ou lista de 1 a 4 */
        float r[8] = { 0 };
        jsv rv = ARG(4);
        jsv items[4];
        int ni = 0;
        if (rv == JS_UNDEF) { items[0] = jv_int(0); ni = 1; }
        else if (js_isarray(rv)) {
            u32 n = js_lengthof(rv);
            if (js_pending()) { path_done(&T); return JS_EXC; }
            if (n < 1 || n > 4) { path_done(&T); return js_throw_dom("IndexSizeError", "roundRect: de 1 a 4 raios"); }
            for (u32 i = 0; i < n; i++) items[ni++] = js_getidx(rv, i);
        } else { items[0] = rv; ni = 1; }
        float rx[4], ry[4];
        for (int i = 0; i < ni; i++) {
            if (jv_isobj(items[i])) {
                double x = js_tonum(js_getn(items[i], "x")), y = js_tonum(js_getn(items[i], "y"));
                if (js_pending()) { path_done(&T); return JS_EXC; }
                rx[i] = (float)x; ry[i] = (float)y;
            } else {
                double v = js_tonum(items[i]);
                if (js_pending()) { path_done(&T); return JS_EXC; }
                rx[i] = ry[i] = (float)v;
            }
            if (!(rx[i] == rx[i]) || !(ry[i] == ry[i]) || rx[i] > 3e38f || ry[i] > 3e38f) { path_done(&T); return JS_UNDEF; }
            if (rx[i] < 0 || ry[i] < 0) { path_done(&T); return js_throw_dom("IndexSizeError", "roundRect: raio negativo"); }
        }
        int map4[4][4] = { { 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { 0, 1, 2, 1 }, { 0, 1, 2, 3 } };   /* tl tr br bl */
        for (int k = 0; k < 4; k++) { int s = map4[ni - 1][k]; r[2 * k] = rx[s]; r[2 * k + 1] = ry[s]; }
        pc_round_rect(T.pc, &T.m, f[0], f[1], f[2], f[3], r);
        break;
    }
    }
    path_done(&T);
    return JS_UNDEF;
}

NATIVE(path2d_ctor) {
    UNUSED_ARGS;
    jsobj *o = js_newobj_cls(CL_HOST, J.realm->p_path2d);
    o->u.host.page = js_cur_page();
    jsobj *b = js_abuf_new(sizeof(float) * PC_HDR);
    if (!b) return JS_EXC;
    o->u.host.extra = jv_obj(b);
    jsv a = ARG(0);
    if (a == JS_UNDEF) return jv_obj(o);
    pcmd_t pc;
    pc_load(b, &pc);
    pc.base = NULL;   /* o buffer inicial é recriado por pc_push */
    pc.cap = 0;
    jsobj *src = path_buf(a);
    if (src) {
        pcmd_t s;
        pc_load(src, &s);
        pc_append(&pc, &s, &MID);
    } else {
        jsstr *d = js_tostr(a);
        if (!d) return JS_EXC;
        path_t p = { 0 };
        p.tol = 0.05f;
        rs_parse_path_d(&p, d->d);
        for (int ci = 0; ci < p.nc; ci++) {
            int s = p.start[ci], n = rs_cont_len(&p, ci);
            pc_move(&pc, &MID, p.xy[2 * s], p.xy[2 * s + 1]);
            for (int i = 1; i < n; i++) pc_line(&pc, &MID, p.xy[2 * (s + i)], p.xy[2 * (s + i) + 1]);
            if (p.closed[ci]) pc_close(&pc);
        }
        rs_path_free(&p);
    }
    if (pc.base) { kfree(b->u.buf.data); pc_store(b, &pc); }
    return jv_obj(o);
}

static bool read_matrix(jsv v, mat_t *m) {   /* DOMMatrix2DInit; false = exceção ou não finito */
    *m = MID;
    if (jv_isnullish(v)) return true;
    if (!jv_isobj(v)) { js_throw_err(E_TYPE, "matriz inválida"); return false; }
    static const char *k1[6] = { "a", "b", "c", "d", "e", "f" }, *k2[6] = { "m11", "m12", "m21", "m22", "m41", "m42" };
    float *dst[6] = { &m->a, &m->b, &m->c, &m->d, &m->e, &m->f };
    for (int i = 0; i < 6; i++) {
        jsv x = js_getn(v, k1[i]);
        if (js_pending()) return false;
        if (x == JS_UNDEF) { x = js_getn(v, k2[i]); if (js_pending()) return false; }
        if (x == JS_UNDEF) continue;
        double d = js_tonum(x);
        if (js_pending()) return false;
        if (!(d == d) || d > 3e38 || d < -3e38) return false;
        *dst[i] = (float)d;
    }
    return true;
}

NATIVE(path2d_addpath) {
    UNUSED_ARGS;
    NEED(1, "addPath");
    jsobj *dst = path_buf(thisv), *src = path_buf(ARG(0));
    if (!dst || !src) return js_throw_err(E_TYPE, "addPath: Path2D esperado");
    mat_t m;
    if (!read_matrix(ARG(1), &m)) return js_pending() ? JS_EXC : JS_UNDEF;
    pcmd_t a, b;
    pc_load(dst, &a);
    pc_load(src, &b);
    pcmd_t copy = { 0 };
    pc_append(&copy, &b, &MID);   /* src pode ser o próprio dst */
    pc_append(&a, &copy, &m);
    pc_free(&copy);
    pc_store(dst, &a);
    return JS_UNDEF;
}

/* ============================================================ estado e propriedades do contexto */
enum { P_CANVAS, P_ALPHA, P_OP, P_FILL, P_STROKE, P_LW, P_CAP, P_JOIN, P_MITER, P_DASHOFF, P_SHX, P_SHY, P_SHBLUR, P_SHCOLOR,
       P_FONT, P_ALIGN, P_BASELINE, P_DIR, P_SMOOTH, P_SMOOTHQ, P_LETTER, P_COUNT };
static const char *prop_names[P_COUNT] = { "canvas", "globalAlpha", "globalCompositeOperation", "fillStyle", "strokeStyle",
    "lineWidth", "lineCap", "lineJoin", "miterLimit", "lineDashOffset", "shadowOffsetX", "shadowOffsetY", "shadowBlur",
    "shadowColor", "font", "textAlign", "textBaseline", "direction", "imageSmoothingEnabled", "imageSmoothingQuality",
    "letterSpacing" };
static const char *caps[] = { "butt", "round", "square" }, *joins[] = { "miter", "round", "bevel" };
static const char *aligns[] = { "start", "end", "left", "right", "center" };
static const char *baselines[] = { "alphabetic", "top", "hanging", "middle", "ideographic", "bottom" };
static const char *dirs[] = { "inherit", "ltr", "rtl" }, *quals[] = { "low", "medium", "high" };

static int pick(jsv v, const char **names, int n) {
    if (!jv_isstr(v)) return -1;
    for (int i = 0; i < n; i++) if (!strcmp(jv_s(v)->d, names[i])) return i;
    return -1;
}

static u32 elem_color(ctx_t *X) { node_t *n = X->o->u.host.node; return n && n->st ? n->st->color | 0xFF000000u : 0xFF000000u; }

static jsv fmt_num(float v) { return jv_num((double)v); }

NATIVE(ctx_get) {
    UNUSED_ARGS;
    CTX();
    cstate_t *st = &c->st;
    char buf[64];
    switch (self->u.nat.magic) {
    case P_CANVAS: return js_dom_wrapv(X.p, X.o->u.host.node);
    case P_ALPHA: return fmt_num(st->alpha);
    case P_OP: return jv_str(js_strz(canvas_op_names[st->op]));
    case P_FILL: case P_STROKE: {
        cpaint_t *cp = self->u.nat.magic == P_FILL ? &st->fill : &st->stroke;
        if (cp->kind != PS_COLOR) return js_arr_get(X.refs, self->u.nat.magic == P_FILL ? 0 : 1);
        canvas_color_str(cp->color, buf, sizeof buf);
        return jv_str(js_strz(buf));
    }
    case P_LW: return fmt_num(st->lw);
    case P_CAP: return jv_str(js_strz(caps[st->cap]));
    case P_JOIN: return jv_str(js_strz(joins[st->join]));
    case P_MITER: return fmt_num(st->miter);
    case P_DASHOFF: return fmt_num(st->dashoff);
    case P_SHX: return fmt_num(st->shx);
    case P_SHY: return fmt_num(st->shy);
    case P_SHBLUR: return fmt_num(st->shblur);
    case P_SHCOLOR: canvas_color_str(st->shcolor, buf, sizeof buf); return jv_str(js_strz(buf));
    case P_FONT: return jv_str(js_strz(st->font));
    case P_ALIGN: return jv_str(js_strz(aligns[st->align]));
    case P_BASELINE: return jv_str(js_strz(baselines[st->baseline]));
    case P_DIR: return jv_str(js_strz(dirs[st->rtl]));
    case P_SMOOTH: return jv_bool(st->smooth);
    case P_SMOOTHQ: return jv_str(js_strz(quals[st->smoothq]));
    case P_LETTER: {
        snprintf(buf, sizeof buf, "%.4f", (double)st->letter);
        int L = (int)strlen(buf);
        while (L && buf[L - 1] == '0') buf[--L] = 0;
        if (L && buf[L - 1] == '.') buf[--L] = 0;
        strcat(buf, "px");
        return jv_str(js_strz(buf));
    }
    }
    return JS_UNDEF;
}

NATIVE(ctx_set) {
    UNUSED_ARGS;
    CTX();
    cstate_t *st = &c->st;
    jsv v = ARG(0);
    int m = self->u.nat.magic;
    if (m == P_FILL || m == P_STROKE) {
        cpaint_t *cp = m == P_FILL ? &st->fill : &st->stroke;
        if (jv_iscls(v, CL_HOST) && (has_proto(jv_o(v), J.realm->p_cgrad) || has_proto(jv_o(v), J.realm->p_cpat))) {
            bool pat = has_proto(jv_o(v), J.realm->p_cpat);
            if (pat) cp->kind = PS_PATTERN;
            else { cgrad_t *g = blob_of(v); cp->kind = g ? g->kind : PS_LINEAR; }
            js_arr_set(X.refs, m == P_FILL ? 0 : 1, v);
            return JS_UNDEF;
        }
        jsstr *s = js_tostr(v);
        if (!s) return JS_EXC;
        u32 col;
        if (!canvas_parse_color(s->d, elem_color(&X), &col)) return JS_UNDEF;
        cp->kind = PS_COLOR;
        cp->color = col;
        js_arr_set(X.refs, m == P_FILL ? 0 : 1, JS_UNDEF);
        return JS_UNDEF;
    }
    if (m == P_SMOOTH) { st->smooth = js_truthy(v); return JS_UNDEF; }
    if (m == P_ALPHA || m == P_LW || m == P_MITER || m == P_DASHOFF || m == P_SHX || m == P_SHY || m == P_SHBLUR) {
        double d = js_tonum(v);
        if (js_pending()) return JS_EXC;
        if (!(d == d) || d > 3e38 || d < -3e38) return JS_UNDEF;
        float f = (float)d;
        switch (m) {
        case P_ALPHA: if (f >= 0 && f <= 1) st->alpha = f; break;
        case P_LW: if (f > 0) st->lw = f; break;
        case P_MITER: if (f > 0) st->miter = f; break;
        case P_DASHOFF: st->dashoff = f; break;
        case P_SHX: st->shx = f; break;
        case P_SHY: st->shy = f; break;
        case P_SHBLUR: if (f >= 0) st->shblur = f; break;
        }
        return JS_UNDEF;
    }
    jsstr *s = js_tostr(v);
    if (!s) return JS_EXC;
    jsv sv = jv_str(s);
    int k;
    switch (m) {
    case P_OP: for (k = 0; k < OP_COUNT; k++) if (!strcmp(s->d, canvas_op_names[k])) { st->op = k; break; } break;
    case P_CAP: if ((k = pick(sv, caps, 3)) >= 0) st->cap = (u8)k; break;
    case P_JOIN: if ((k = pick(sv, joins, 3)) >= 0) st->join = (u8)k; break;
    case P_SHCOLOR: { u32 col; if (canvas_parse_color(s->d, elem_color(&X), &col)) st->shcolor = col; break; }
    case P_FONT: canvas_set_font(st, s->d); break;
    case P_ALIGN: if ((k = pick(sv, aligns, 5)) >= 0) st->align = (u8)k; break;
    case P_BASELINE: if ((k = pick(sv, baselines, 6)) >= 0) st->baseline = (u8)k; break;
    case P_DIR: if ((k = pick(sv, dirs, 3)) >= 0) st->rtl = (u8)k; break;
    case P_SMOOTHQ: if ((k = pick(sv, quals, 3)) >= 0) st->smoothq = (u8)k; break;
    case P_LETTER: {
        const char *q = s->d;
        float f;
        while (isspace((u8)*q)) q++;
        if (rs_parse_f(&q, &f) && (!strcmp(q, "px") || (f == 0 && !*q))) st->letter = f;
        else if (rs_parse_f(&q, &f) && !strcmp(q, "em")) st->letter = f * st->fsize;
        break;
    }
    }
    return JS_UNDEF;
}

/* magic: 0 save 1 restore 2 reset 3 beginPath 4 resetTransform 5 isContextLost */
NATIVE(ctx_misc) {
    UNUSED_ARGS;
    CTX();
    switch (self->u.nat.magic) {
    case 0:
        if (canvas_save(c)) { js_arr_push(X.refs, js_arr_get(X.refs, 0)); js_arr_push(X.refs, js_arr_get(X.refs, 1)); }
        break;
    case 1:
        if (canvas_restore(c) && X.refs->alen >= 4) {
            u32 n = X.refs->alen;
            js_arr_set(X.refs, 0, js_arr_get(X.refs, n - 2));
            js_arr_set(X.refs, 1, js_arr_get(X.refs, n - 1));
            js_arr_setlen(X.refs, n - 2);
        }
        break;
    case 2:
        canvas_resize(c, c->w, c->h);
        js_arr_setlen(X.refs, 2);
        js_arr_set(X.refs, 0, JS_UNDEF);
        js_arr_set(X.refs, 1, JS_UNDEF);
        repaint(&X);
        break;
    case 3: pc_reset(&c->path); break;
    case 4: c->st.m = MID; break;
    case 5: return JS_FALSE;
    }
    return JS_UNDEF;
}

/* magic: 0 scale 1 rotate 2 translate 3 transform 4 setTransform */
NATIVE(ctx_xform) {
    UNUSED_ARGS;
    static const int need[] = { 2, 1, 2, 6, 0 };
    int op = self->u.nat.magic;
    NEED(need[op], "transformação");
    CTX();
    float f[6];
    mat_t t = MID;
    if (op == 4 && argc < 6) {
        mat_t m;
        if (!read_matrix(ARG(0), &m)) return js_pending() ? JS_EXC : JS_UNDEF;
        c->st.m = m;
        return JS_UNDEF;
    }
    FARGS(0, op == 4 ? 6 : need[op], f);
    switch (op) {
    case 0: t.a = f[0]; t.d = f[1]; break;
    case 1: t.a = fcos(f[0]); t.b = fsin(f[0]); t.c = -t.b; t.d = t.a; break;
    case 2: t.e = f[0]; t.f = f[1]; break;
    case 3: case 4: t.a = f[0]; t.b = f[1]; t.c = f[2]; t.d = f[3]; t.e = f[4]; t.f = f[5]; break;
    }
    c->st.m = op == 4 ? t : mmul(c->st.m, t);
    return JS_UNDEF;
}

/* ---------------------------------------------------------------- DOMMatrix (2D) */
static jsobj *new_matrix(mat_t m) {
    jsobj *o = js_newobj(J.realm->p_dommatrix);
    float v[16] = { m.a, m.b, 0, 0, m.c, m.d, 0, 0, 0, 0, 1, 0, m.e, m.f, 0, 1 };
    static const char *names[16] = { "m11", "m12", "m13", "m14", "m21", "m22", "m23", "m24", "m31", "m32", "m33", "m34", "m41", "m42", "m43", "m44" };
    for (int i = 0; i < 16; i++) js_defn(o, names[i], fmt_num(v[i]), 0);
    js_defn(o, "a", fmt_num(m.a), 0); js_defn(o, "b", fmt_num(m.b), 0); js_defn(o, "c", fmt_num(m.c), 0);
    js_defn(o, "d", fmt_num(m.d), 0); js_defn(o, "e", fmt_num(m.e), 0); js_defn(o, "f", fmt_num(m.f), 0);
    js_defn(o, "is2D", JS_TRUE, 0);
    js_defn(o, "isIdentity", jv_bool(m.a == 1 && m.b == 0 && m.c == 0 && m.d == 1 && m.e == 0 && m.f == 0), 0);
    return o;
}

NATIVE(ctx_gettransform) { UNUSED_ARGS; CTX(); return jv_obj(new_matrix(c->st.m)); }

NATIVE(matrix_ctor) {
    UNUSED_ARGS;
    mat_t m = MID;
    jsv a = ARG(0);
    if (js_isarray(a)) {
        u32 n = js_lengthof(a);
        if (js_pending()) return JS_EXC;
        if (n != 6) return js_throw_err(E_TYPE, "DOMMatrix: só a forma 2D de 6 números é suportada");
        float *d[6] = { &m.a, &m.b, &m.c, &m.d, &m.e, &m.f };
        for (u32 i = 0; i < 6; i++) { double x = js_tonum(js_getidx(a, i)); if (js_pending()) return JS_EXC; *d[i] = (float)x; }
    } else if (jv_isstr(a)) {
        const char *s = jv_s(a)->d;
        while (isspace((u8)*s)) s++;
        if (!*s || !strcmp(s, "none")) m = MID;
        else if (!strncmp(s, "matrix(", 7)) {
            const char *q = s + 7;
            float *d[6] = { &m.a, &m.b, &m.c, &m.d, &m.e, &m.f };
            for (int i = 0; i < 6; i++) if (!rs_parse_f(&q, d[i])) return js_throw_dom("SyntaxError", "DOMMatrix: texto inválido");
        } else return js_throw_dom("SyntaxError", "DOMMatrix: texto inválido");
    } else if (jv_isobj(a)) { if (!read_matrix(a, &m)) return js_pending() ? JS_EXC : js_throw_err(E_TYPE, "DOMMatrix: valores não finitos"); }
    return jv_obj(new_matrix(m));
}

/* magic: 0 multiply 1 inverse 2 translate 3 scale 4 rotate 5 transformPoint 6 toString */
NATIVE(matrix_method) {
    UNUSED_ARGS;
    mat_t m;
    if (!read_matrix(thisv, &m)) return js_pending() ? JS_EXC : JS_UNDEF;
    float f[3] = { 0, 0, 0 };
    switch (self->u.nat.magic) {
    case 0: { mat_t o; if (!read_matrix(ARG(0), &o)) return js_pending() ? JS_EXC : JS_UNDEF; return jv_obj(new_matrix(mmul(m, o))); }
    case 1: {
        mat_t inv;
        if (!minv(m, &inv)) {
            /* não inversível: todos os coeficientes viram NaN */
            jsobj *o = new_matrix(MID);
            static const char *k[] = { "a", "b", "c", "d", "e", "f", "m11", "m12", "m21", "m22", "m41", "m42" };
            for (int i = 0; i < 12; i++) js_defn(o, k[i], JS_NAN_BITS, 0);
            js_defn(o, "isIdentity", JS_FALSE, 0);
            return jv_obj(o);
        }
        return jv_obj(new_matrix(inv));
    }
    case 2: { if (fargs(argc, argv, 0, 2, f) < 0) return JS_EXC; mat_t t = MID; t.e = f[0]; t.f = f[1]; return jv_obj(new_matrix(mmul(m, t))); }
    case 3: {
        if (fargs(argc, argv, 0, 2, f) < 0) return JS_EXC;
        mat_t t = MID;
        t.a = argc > 0 ? f[0] : 1;
        t.d = argc > 1 ? f[1] : t.a;
        return jv_obj(new_matrix(mmul(m, t)));
    }
    case 4: {
        if (fargs(argc, argv, 0, 1, f) < 0) return JS_EXC;
        float r = f[0] * PI_F / 180;
        mat_t t = { fcos(r), fsin(r), -fsin(r), fcos(r), 0, 0 };
        return jv_obj(new_matrix(mmul(m, t)));
    }
    case 5: {
        double x = 0, y = 0;
        if (jv_isobj(ARG(0))) { x = js_tonum(js_getn(ARG(0), "x")); y = js_tonum(js_getn(ARG(0), "y")); if (js_pending()) return JS_EXC; }
        float ox, oy;
        mapp(&m, (float)(x == x ? x : 0), (float)(y == y ? y : 0), &ox, &oy);
        jsobj *o = js_newobj(J.realm->p_object);
        js_defn(o, "x", fmt_num(ox), 0); js_defn(o, "y", fmt_num(oy), 0); js_defn(o, "z", jv_int(0), 0); js_defn(o, "w", jv_int(1), 0);
        return jv_obj(o);
    }
    case 6: {
        jsstr *parts[6];
        float v[6] = { m.a, m.b, m.c, m.d, m.e, m.f };
        for (int i = 0; i < 6; i++) parts[i] = js_num_tostr(v[i]);
        return jv_str(js_str_fmt("matrix(%s, %s, %s, %s, %s, %s)", parts[0]->d, parts[1]->d, parts[2]->d, parts[3]->d, parts[4]->d, parts[5]->d));
    }
    }
    return JS_UNDEF;
}

/* ============================================================ gradientes e padrões */
static jsobj *new_blob_obj(jsobj *proto, u32 size, void **data) {
    jsobj *o = js_newobj_cls(CL_HOST, proto);
    o->u.host.page = js_cur_page();
    jsobj *b = js_abuf_new(size);
    if (!b) return NULL;
    o->u.host.extra = jv_obj(b);
    *data = b->u.buf.data;
    return o;
}

/* magic: 0 linear 1 radial 2 conic */
NATIVE(ctx_gradient) {
    UNUSED_ARGS;
    static const int need[] = { 4, 6, 3 };
    int k = self->u.nat.magic;
    NEED(need[k], "createGradient");
    CTX();
    float f[6];
    int r = fargs(argc, argv, 0, need[k], f);
    if (r < 0) return JS_EXC;
    if (!r) return js_throw_err(E_TYPE, "gradiente com coordenada não finita");
    if (k == 1 && (f[2] < 0 || f[5] < 0)) return js_throw_dom("IndexSizeError", "createRadialGradient: raio negativo");
    void *d;
    jsobj *o = new_blob_obj(J.realm->p_cgrad, sizeof(cgrad_t), &d);
    if (!o) return JS_EXC;
    cgrad_t *g = d;
    if (k == 0) { g->kind = PS_LINEAR; g->x0 = f[0]; g->y0 = f[1]; g->x1 = f[2]; g->y1 = f[3]; }
    else if (k == 1) { g->kind = PS_RADIAL; g->x0 = f[0]; g->y0 = f[1]; g->r0 = f[2]; g->x1 = f[3]; g->y1 = f[4]; g->r1 = f[5]; }
    else { g->kind = PS_CONIC; g->ang = f[0]; g->x0 = f[1]; g->y0 = f[2]; }
    return jv_obj(o);
}

NATIVE(grad_addstop) {
    UNUSED_ARGS;
    NEED(2, "addColorStop");
    if (!jv_iscls(thisv, CL_HOST) || !has_proto(jv_o(thisv), J.realm->p_cgrad)) return js_throw_err(E_TYPE, "Invocação ilegal: não é um CanvasGradient");
    double off = js_tonum(ARG(0));
    if (js_pending()) return JS_EXC;
    if (!(off == off) || off > 1e300 || off < -1e300) return js_throw_err(E_TYPE, "addColorStop: offset não finito");
    if (off < 0 || off > 1) return js_throw_dom("IndexSizeError", "addColorStop: offset fora de [0, 1]");
    jsstr *s = js_tostr(ARG(1));
    if (!s) return JS_EXC;
    u32 col;
    if (!canvas_parse_color(s->d, 0xFF000000u, &col)) return js_throw_dom("SyntaxError", "addColorStop: cor inválida");
    jsobj *b = jv_o(jv_o(thisv)->u.host.extra);
    cgrad_t *g = (cgrad_t *)b->u.buf.data;
    if (g->nstops >= 4096) return JS_UNDEF;
    u32 nl = (u32)(sizeof(cgrad_t) + sizeof(cstop_t) * (g->nstops + 1));
    cgrad_t *ng = krealloc(g, nl);
    if (!ng) return js_throw_err(E_RANGE, "sem memória");
    b->u.buf.data = (u8 *)ng;
    b->u.buf.len = nl;
    int i = ng->nstops;
    while (i > 0 && ng->stops[i - 1].off > (float)off) { ng->stops[i] = ng->stops[i - 1]; i--; }
    ng->stops[i].off = (float)off;
    ng->stops[i].c = col;
    ng->nstops++;
    return JS_UNDEF;
}

NATIVE(pat_settransform) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_HOST) || !has_proto(jv_o(thisv), J.realm->p_cpat)) return js_throw_err(E_TYPE, "Invocação ilegal: não é um CanvasPattern");
    mat_t m;
    if (!read_matrix(ARG(0), &m)) return js_pending() ? JS_EXC : JS_UNDEF;
    cpat_t *p = blob_of(thisv);
    if (p) p->m = m;
    return JS_UNDEF;
}

/* origem de imagem: <img> carregada ou <canvas>. -1 exceção, 0 nada a desenhar, 1 ok */
typedef struct { const u32 *px; int w, h, stride; surface_t *own; } isrc_t;

static int image_source(ctx_t *X, jsv v, isrc_t *S) {
    memset(S, 0, sizeof *S);
    node_t *n = jv_iscls(v, CL_NODE) ? jv_o(v)->u.host.node : NULL;
    if (!n || n->type != N_ELEM || (n->tag != T_IMG && n->tag != T_CANVAS && n->tag != T_SVG)) {
        js_throw_err(E_TYPE, "origem de imagem inválida (esperado <img> ou <canvas>)");
        return -1;
    }
    if (n->tag == T_CANVAS) {
        canvas2d_t *src = canvas_of(js_page_doc(X->p), n, true);
        if (!src || !src->s || !src->w || !src->h) { js_throw_dom("InvalidStateError", "canvas de origem com tamanho zero"); return -1; }
        if (src == X->c) {
            /* desenhar o canvas em si mesmo: copia antes */
            S->own = surface_new(src->w, src->h);
            if (!S->own || !S->own->px) { surface_free(S->own); S->own = NULL; return 0; }
            for (int y = 0; y < src->h; y++) memcpy(S->own->px + (usize)y * src->w, src->s->px + (usize)y * src->s->stride, (usize)src->w * 4);
            S->px = S->own->px; S->stride = src->w;
        } else { S->px = src->s->px; S->stride = src->s->stride; }
        S->w = src->w; S->h = src->h;
        return 1;
    }
    img_t *im = n->img;
    if (!im || im->state == IMG_PENDING || im->state == IMG_LOADING) return 0;
    if (im->state == IMG_ERROR || !im->surf) { js_throw_dom("InvalidStateError", "a imagem não pôde ser decodificada"); return -1; }
    S->px = im->surf->px; S->w = im->surf->w; S->h = im->surf->h; S->stride = im->surf->stride;
    return 1;
}

NATIVE(ctx_pattern) {
    UNUSED_ARGS;
    NEED(2, "createPattern");
    CTX();
    isrc_t S;
    int r = image_source(&X, ARG(0), &S);
    if (r < 0) return JS_EXC;
    if (!r) return JS_NULL;
    int rep = REP_REPEAT;
    jsv rv = ARG(1);
    if (rv != JS_NULL) {
        jsstr *s = js_tostr(rv);
        if (!s) { surface_free(S.own); return JS_EXC; }
        if (!strcmp(s->d, "repeat") || !*s->d) rep = REP_REPEAT;
        else if (!strcmp(s->d, "repeat-x")) rep = REP_X;
        else if (!strcmp(s->d, "repeat-y")) rep = REP_Y;
        else if (!strcmp(s->d, "no-repeat")) rep = REP_NONE;
        else { surface_free(S.own); return js_throw_dom("SyntaxError", "createPattern: repetição inválida"); }
    }
    void *d;
    jsobj *o = new_blob_obj(J.realm->p_cpat, (u32)(sizeof(cpat_t) + (usize)S.w * S.h * 4), &d);
    if (!o) { surface_free(S.own); return JS_EXC; }
    cpat_t *p = d;
    p->w = S.w; p->h = S.h; p->rep = rep; p->m = MID;
    for (int y = 0; y < S.h; y++) memcpy(p->px + (usize)y * S.w, S.px + (usize)y * S.stride, (usize)S.w * 4);
    surface_free(S.own);
    return jv_obj(o);
}

/* ============================================================ desenho */
/* magic: 0 fillRect 1 strokeRect 2 clearRect */
NATIVE(ctx_rect) {
    UNUSED_ARGS;
    NEED(4, "rect");
    CTX();
    float f[4];
    FARGS(0, 4, f);
    int op = self->u.nat.magic;
    if (op == 2) canvas_clear_rect(c, f[0], f[1], f[2], f[3]);
    else {
        sync_paints(&X);
        pcmd_t pc = { 0 };
        if (op == 0) {
            if (f[2] == 0 || f[3] == 0) return JS_UNDEF;
            pc_rect(&pc, &MID, f[0], f[1], f[2], f[3]);
            canvas_fill(c, &pc, &c->st.m, false);
        } else {
            if (f[2] == 0 && f[3] == 0) return JS_UNDEF;
            if (f[2] == 0 || f[3] == 0) { pc_move(&pc, &MID, f[0], f[1]); pc_line(&pc, &MID, f[0] + f[2], f[1] + f[3]); }
            else pc_rect(&pc, &MID, f[0], f[1], f[2], f[3]);
            canvas_stroke(c, &pc, false);
        }
        pc_free(&pc);
    }
    repaint(&X);
    return JS_UNDEF;
}

static int fill_rule(jsv v, bool *evenodd) {   /* -1 exceção */
    *evenodd = false;
    if (v == JS_UNDEF) return 0;
    jsstr *s = js_tostr(v);
    if (!s) return -1;
    if (!strcmp(s->d, "evenodd")) *evenodd = true;
    else if (strcmp(s->d, "nonzero")) { js_throw_err(E_TYPE, "regra de preenchimento inválida: %s", s->d); return -1; }
    return 0;
}

/* magic: 0 fill 1 stroke 2 clip */
NATIVE(ctx_paint) {
    UNUSED_ARGS;
    CTX();
    int op = self->u.nat.magic;
    jsobj *pb = path_buf(ARG(0));
    pcmd_t tmp;
    const pcmd_t *pc = &c->path;
    if (pb) { pc_load(pb, &tmp); pc = &tmp; }
    bool eo = false;
    if (op != 1 && fill_rule(ARG(pb ? 1 : 0), &eo) < 0) return JS_EXC;
    sync_paints(&X);
    if (op == 0) canvas_fill(c, pc, pb ? &c->st.m : &MID, eo);
    else if (op == 1) canvas_stroke(c, pc, !pb);
    else canvas_clip(c, pc, pb ? &c->st.m : &MID, eo);
    if (op != 2) repaint(&X);
    return JS_UNDEF;
}

/* magic: 0 isPointInPath 1 isPointInStroke */
NATIVE(ctx_hit) {
    UNUSED_ARGS;
    CTX();
    jsobj *pb = path_buf(ARG(0));
    int a = pb ? 1 : 0;
    NEED(a + 2, "isPointInPath");
    float f[2];
    int r = fargs(argc, argv, a, 2, f);
    if (r < 0) return JS_EXC;
    if (!r) return JS_FALSE;
    pcmd_t tmp;
    const pcmd_t *pc = &c->path;
    if (pb) { pc_load(pb, &tmp); pc = &tmp; }
    if (self->u.nat.magic == 1) return jv_bool(canvas_point_in_stroke(c, pc, !pb, f[0], f[1]));
    bool eo;
    if (fill_rule(ARG(a + 2), &eo) < 0) return JS_EXC;
    return jv_bool(canvas_point_in_path(c, pc, pb ? &c->st.m : &MID, f[0], f[1], eo));
}

/* magic: 0 fillText 1 strokeText */
NATIVE(ctx_text) {
    UNUSED_ARGS;
    NEED(3, "fillText");
    CTX();
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    float f[3] = { 0, 0, -1 };
    FARGS(1, 2, f);
    if (argc > 3 && ARG(3) != JS_UNDEF) {
        double m = js_tonum(ARG(3));
        if (js_pending()) return JS_EXC;
        if (!(m == m) || m <= 0) return JS_UNDEF;
        f[2] = m > 3e38 ? -1 : (float)m;
    }
    sync_paints(&X);
    canvas_text(c, s->d, (int)s->len, f[0], f[1], f[2], self->u.nat.magic == 1);
    repaint(&X);
    return JS_UNDEF;
}

NATIVE(ctx_measure) {
    UNUSED_ARGS;
    NEED(1, "measureText");
    CTX();
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    cmetrics_t m;
    canvas_measure(c, s->d, (int)s->len, &m);
    jsobj *o = js_newobj(J.realm->p_tmetrics);
    js_defn(o, "width", fmt_num(m.width), 0);
    js_defn(o, "actualBoundingBoxLeft", fmt_num(m.l), 0);
    js_defn(o, "actualBoundingBoxRight", fmt_num(m.r), 0);
    js_defn(o, "actualBoundingBoxAscent", fmt_num(m.asc), 0);
    js_defn(o, "actualBoundingBoxDescent", fmt_num(m.desc), 0);
    js_defn(o, "fontBoundingBoxAscent", fmt_num(m.fasc), 0);
    js_defn(o, "fontBoundingBoxDescent", fmt_num(m.fdesc), 0);
    js_defn(o, "emHeightAscent", fmt_num(m.fasc), 0);
    js_defn(o, "emHeightDescent", fmt_num(m.fdesc), 0);
    js_defn(o, "hangingBaseline", fmt_num(m.hanging), 0);
    js_defn(o, "alphabeticBaseline", fmt_num(m.alpha), 0);
    js_defn(o, "ideographicBaseline", fmt_num(m.ideo), 0);
    return jv_obj(o);
}

NATIVE(ctx_drawimage) {
    UNUSED_ARGS;
    if (argc != 3 && argc != 5 && argc < 9) return js_throw_err(E_TYPE, "drawImage: use 3, 5 ou 9 argumentos");
    CTX();
    isrc_t S;
    int r = image_source(&X, ARG(0), &S);
    if (r <= 0) return r < 0 ? JS_EXC : JS_UNDEF;
    float f[8];
    int ok = fargs(argc, argv, 1, argc >= 9 ? 8 : argc - 1, f);
    if (ok <= 0) { surface_free(S.own); return ok < 0 ? JS_EXC : JS_UNDEF; }
    float sx = 0, sy = 0, sw = (float)S.w, sh = (float)S.h, dx, dy, dw, dh;
    if (argc == 3) { dx = f[0]; dy = f[1]; dw = sw; dh = sh; }
    else if (argc == 5) { dx = f[0]; dy = f[1]; dw = f[2]; dh = f[3]; }
    else { sx = f[0]; sy = f[1]; sw = f[2]; sh = f[3]; dx = f[4]; dy = f[5]; dw = f[6]; dh = f[7]; }
    if (sw < 0) { sx += sw; sw = -sw; }
    if (sh < 0) { sy += sh; sh = -sh; }
    if (dw < 0) { dx += dw; dw = -dw; }
    if (dh < 0) { dy += dh; dh = -dh; }
    sync_paints(&X);
    canvas_draw_image(c, S.px, S.w, S.h, S.stride, sx, sy, sw, sh, dx, dy, dw, dh);
    surface_free(S.own);
    repaint(&X);
    return JS_UNDEF;
}

/* ============================================================ ImageData */
static jsobj *new_imagedata(int w, int h, jsobj *data) {
    if (!data) {
        u64 n = (u64)w * h * 4;
        if (n > 0x7FFFFFFFu) { js_throw_err(E_RANGE, "ImageData grande demais"); return NULL; }
        jsobj *b = js_abuf_new((u32)n);
        if (!b) return NULL;
        data = js_typed_new(TA_UINT8C, b, 0, (u32)n);
    }
    jsobj *o = js_newobj(J.realm->p_imgdata);
    js_defn(o, "width", jv_int(w), PF_RO);
    js_defn(o, "height", jv_int(h), PF_RO);
    js_defn(o, "data", jv_obj(data), PF_RO);
    js_defn(o, "colorSpace", jv_str(js_strz("srgb")), PF_RO);
    return o;
}

static bool imagedata_of(jsv v, int *w, int *h, u8 **px) {
    if (!jv_isobj(v) || !has_proto(jv_o(v), J.realm->p_imgdata)) { js_throw_err(E_TYPE, "ImageData esperado"); return false; }
    jsv d = js_getn(v, "data");
    double W = js_tonum(js_getn(v, "width")), H = js_tonum(js_getn(v, "height"));
    if (js_pending()) return false;
    if (!jv_iscls(d, CL_TYPED)) { js_throw_err(E_TYPE, "ImageData sem dados"); return false; }
    jsobj *t = jv_o(d);
    jsobj *b = t->u.ta.buffer;
    *w = (int)W; *h = (int)H;
    if (!b || b->u.buf.detached || (u64)t->u.ta.len < (u64)*w * *h * 4) { js_throw_dom("InvalidStateError", "ImageData com dados inválidos"); return false; }
    *px = b->u.buf.data + t->u.ta.off;
    return true;
}

NATIVE(imagedata_ctor) {
    UNUSED_ARGS;
    NEED(2, "ImageData");
    if (jv_iscls(ARG(0), CL_TYPED)) {
        jsobj *t = jv_o(ARG(0));
        if (t->u.ta.kind != TA_UINT8C) return js_throw_err(E_TYPE, "ImageData: Uint8ClampedArray esperado");
        double W = js_tonum(ARG(1));
        if (js_pending()) return JS_EXC;
        u32 len = t->u.ta.len;
        if (!len || len % 4) return js_throw_dom("InvalidStateError", "ImageData: tamanho dos dados não é múltiplo de 4");
        if (!(W >= 1) || W > 1e9) return js_throw_dom("IndexSizeError", "ImageData: largura inválida");
        u32 w = (u32)W;
        if ((len / 4) % w) return js_throw_dom("IndexSizeError", "ImageData: largura não divide os dados");
        u32 h = len / 4 / w;
        if (argc > 2 && ARG(2) != JS_UNDEF) {
            double H = js_tonum(ARG(2));
            if (js_pending()) return JS_EXC;
            if ((u32)H != h) return js_throw_dom("IndexSizeError", "ImageData: altura não confere");
        }
        jsobj *o = new_imagedata((int)w, (int)h, t);
        return o ? jv_obj(o) : JS_EXC;
    }
    double W = js_tonum(ARG(0)), H = js_tonum(ARG(1));
    if (js_pending()) return JS_EXC;
    if (!(W >= 1) || !(H >= 1) || W > 1e9 || H > 1e9) return js_throw_dom("IndexSizeError", "ImageData: largura e altura devem ser positivas");
    jsobj *o = new_imagedata((int)W, (int)H, NULL);
    return o ? jv_obj(o) : JS_EXC;
}

NATIVE(ctx_createimagedata) {
    UNUSED_ARGS;
    NEED(1, "createImageData");
    CTX();
    int w, h;
    if (argc == 1) {
        u8 *px;
        if (!imagedata_of(ARG(0), &w, &h, &px)) return JS_EXC;
    } else {
        float f[2];
        int r = fargs(argc, argv, 0, 2, f);
        if (r < 0) return JS_EXC;
        if (!r) return js_throw_err(E_TYPE, "createImageData: tamanho não finito");
        w = (int)fabs_(f[0]); h = (int)fabs_(f[1]);
        if (!w || !h) return js_throw_dom("IndexSizeError", "createImageData: tamanho zero");
    }
    jsobj *o = new_imagedata(w, h, NULL);
    return o ? jv_obj(o) : JS_EXC;
}

NATIVE(ctx_getimagedata) {
    UNUSED_ARGS;
    NEED(4, "getImageData");
    CTX();
    float f[4];
    int r = fargs(argc, argv, 0, 4, f);
    if (r < 0) return JS_EXC;
    if (!r) return js_throw_err(E_TYPE, "getImageData: argumento não finito");
    int sx = (int)ffloor(f[0]), sy = (int)ffloor(f[1]), sw = (int)f[2], sh = (int)f[3];
    if (!sw || !sh) return js_throw_dom("IndexSizeError", "getImageData: largura ou altura zero");
    if (sw < 0) { sx += sw; sw = -sw; }
    if (sh < 0) { sy += sh; sh = -sh; }
    jsobj *o = new_imagedata(sw, sh, NULL);
    if (!o) return JS_EXC;
    u8 *px;
    int w, h;
    if (!imagedata_of(jv_obj(o), &w, &h, &px)) return JS_EXC;
    canvas_get_rgba(c, sx, sy, sw, sh, px);
    return jv_obj(o);
}

NATIVE(ctx_putimagedata) {
    UNUSED_ARGS;
    NEED(3, "putImageData");
    CTX();
    int w, h;
    u8 *px;
    if (!imagedata_of(ARG(0), &w, &h, &px)) return JS_EXC;
    float f[6];
    int n = argc >= 7 ? 6 : 2;
    FARGS(1, n, f);
    int dx = (int)ffloor(f[0]), dy = (int)ffloor(f[1]);
    int x0 = 0, y0 = 0, x1 = w, y1 = h;
    if (n == 6) {
        float ax = f[2], ay = f[3], aw = f[4], ah = f[5];
        if (aw < 0) { ax += aw; aw = -aw; }
        if (ah < 0) { ay += ah; ah = -ah; }
        x0 = (int)ffloor(ax); y0 = (int)ffloor(ay); x1 = (int)ffloor(ax + aw); y1 = (int)ffloor(ay + ah);
    }
    canvas_put_rgba(c, px, w, h, dx, dy, x0, y0, x1, y1);
    repaint(&X);
    return JS_UNDEF;
}

/* ============================================================ tracejado e outros */
NATIVE(ctx_setlinedash) {
    UNUSED_ARGS;
    NEED(1, "setLineDash");
    CTX();
    jsv a = ARG(0);
    if (!jv_isobj(a)) return js_throw_err(E_TYPE, "setLineDash: sequência esperada");
    u32 n = js_lengthof(a);
    if (js_pending()) return JS_EXC;
    if (n > 1024) return JS_UNDEF;
    float *d = kmalloc(sizeof(float) * (n * 2 + 1));
    if (!d) return js_throw_err(E_RANGE, "sem memória");
    for (u32 i = 0; i < n; i++) {
        double v = js_tonum(js_getidx(a, i));
        if (js_pending()) { kfree(d); return JS_EXC; }
        if (!(v == v) || v < 0 || v > 3e38) { kfree(d); return JS_UNDEF; }
        d[i] = (float)v;
    }
    if (n & 1) { for (u32 i = 0; i < n; i++) d[n + i] = d[i]; n *= 2; }
    if (c->st.dash) kfree(c->st.dash);
    c->st.dash = n ? d : NULL;
    c->st.ndash = (int)n;
    if (!n) kfree(d);
    return JS_UNDEF;
}

NATIVE(ctx_getlinedash) {
    UNUSED_ARGS;
    CTX();
    jsobj *a = js_newarr(0);
    for (int i = 0; i < c->st.ndash; i++) js_arr_push(a, fmt_num(c->st.dash[i]));
    return jv_obj(a);
}

NATIVE(ctx_attrs) {
    UNUSED_ARGS;
    CTX();
    jsobj *o = js_newobj(J.realm->p_object);
    js_defn(o, "alpha", jv_bool(!c->opaque), 0);
    js_defn(o, "colorSpace", jv_str(js_strz("srgb")), 0);
    js_defn(o, "desynchronized", JS_FALSE, 0);
    js_defn(o, "willReadFrequently", JS_FALSE, 0);
    return jv_obj(o);
}

NATIVE(ctx_focus) {
    UNUSED_ARGS;
    NEED(1, "drawFocusIfNeeded");
    CTX();
    jsobj *pb = path_buf(ARG(0));
    jsv el = ARG(pb ? 1 : 0);
    node_t *n = jv_iscls(el, CL_NODE) ? jv_o(el)->u.host.node : NULL;
    if (!n || js_page_focus(X.p) != n) return JS_UNDEF;
    bool inside = false;
    for (node_t *a = n->parent; a; a = a->parent) if (a == X.o->u.host.node) inside = true;
    if (!inside) return JS_UNDEF;
    pcmd_t tmp;
    const pcmd_t *pc = &c->path;
    if (pb) { pc_load(pb, &tmp); pc = &tmp; }
    /* anel de foco do sistema: 2px azul, sem sombra, sem tracejado, sobre o estado atual */
    canvas_save(c);
    c->st.stroke.kind = PS_COLOR;
    c->st.stroke.color = 0xFF0A84FFu;
    c->st.lw = 2;
    c->st.alpha = 1;
    c->st.op = OP_SRC_OVER;
    c->st.shcolor = 0;
    if (c->st.dash) kfree(c->st.dash);
    c->st.dash = NULL;
    c->st.ndash = 0;
    canvas_stroke(c, pc, !pb);
    canvas_restore(c);
    repaint(&X);
    return JS_UNDEF;
}

/* ============================================================ HTMLCanvasElement */
static node_t *this_canvas(jsv thisv) {
    node_t *n = jv_iscls(thisv, CL_NODE) ? jv_o(thisv)->u.host.node : NULL;
    if (!n || n->type != N_ELEM || n->tag != T_CANVAS) { js_throw_err(E_TYPE, "Invocação ilegal: não é um HTMLCanvasElement"); return NULL; }
    return n;
}

NATIVE(cv_size_get) {
    UNUSED_ARGS;
    node_t *n = this_canvas(thisv);
    if (!n) return JS_EXC;
    int w, h;
    canvas_size_attrs(n, &w, &h);
    return jv_int(self->u.nat.magic ? h : w);
}

/* mudar width/height (mesmo para o mesmo valor) recria o bitmap e zera o estado do contexto */
void js_canvas_attr_changed(js_page_t *p, node_t *n) {
    canvas2d_t *c = n->canvas;
    if (!c) return;
    int w, h;
    canvas_size_attrs(n, &w, &h);
    canvas_resize(c, w, h);
    jsv wv = js_dom_wrapv(p, n);
    if (jv_isobj(wv)) {
        jsprop *pr = js_ownprop(jv_o(wv), K_CTX);
        if (pr && jv_isobj(pr->v)) {
            jsv refs = jv_o(pr->v)->u.host.extra;
            if (jv_isobj(refs)) { js_arr_setlen(jv_o(refs), 2); js_arr_set(jv_o(refs), 0, JS_UNDEF); js_arr_set(jv_o(refs), 1, JS_UNDEF); }
        }
    }
    js_page_repaint(p);
}

NATIVE(cv_size_set) {
    UNUSED_ARGS;
    node_t *n = this_canvas(thisv);
    if (!n) return JS_EXC;
    double d = js_tonum(ARG(0));
    if (js_pending()) return JS_EXC;
    u32 v = d == d && d >= 0 && d <= 2147483647.0 ? (u32)d : (self->u.nat.magic ? 150 : 300);
    char buf[16];
    snprintf(buf, sizeof buf, "%u", v);
    jsv args[2] = { jv_str(js_intern(self->u.nat.magic ? "height" : "width", -1)), jv_str(js_strz(buf)) };
    jsv r = js_invoke(thisv, js_intern("setAttribute", -1), 2, args);   /* o gancho de atributo recria o bitmap */
    return r == JS_EXC ? JS_EXC : JS_UNDEF;
}

NATIVE(cv_getcontext) {
    UNUSED_ARGS;
    NEED(1, "getContext");
    node_t *n = this_canvas(thisv);
    if (!n) return JS_EXC;
    jsstr *id = js_tostr(ARG(0));
    if (!id) return JS_EXC;
    jsobj *w = jv_o(thisv);
    jsprop *pr = js_ownprop(w, K_CTX);
    if (strcmp(id->d, "2d")) {
        /* outros tipos de contexto ainda não existem neste motor: null, como a especificação manda */
        return JS_NULL;
    }
    if (pr && jv_isobj(pr->v)) return pr->v;
    js_page_t *p = w->u.host.page;
    canvas2d_t *c = canvas_of(js_page_doc(p), n, true);
    if (!c) return js_throw_err(E_RANGE, "sem memória para o canvas");
    if (jv_isobj(ARG(1))) {
        jsv a = js_getn(ARG(1), "alpha");
        if (js_pending()) return JS_EXC;
        if (a != JS_UNDEF && !js_truthy(a)) { c->opaque = true; canvas_resize(c, c->w, c->h); }
    }
    jsobj *o = js_newobj_cls(CL_HOST, J.realm->p_ctx2d);
    o->u.host.page = p;
    o->u.host.node = n;
    jsobj *refs = js_newarr(0);
    js_arr_push(refs, JS_UNDEF);
    js_arr_push(refs, JS_UNDEF);
    o->u.host.extra = jv_obj(refs);
    js_defprop(w, K_CTX, jv_obj(o), PF_NOENUM);
    return jv_obj(o);
}

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

NATIVE(cv_todataurl) {
    UNUSED_ARGS;
    node_t *n = this_canvas(thisv);
    if (!n) return JS_EXC;
    canvas2d_t *c = canvas_of(js_page_doc(jv_o(thisv)->u.host.page), n, true);
    if (!c || !c->s || !c->w || !c->h) return jv_str(js_strz("data:,"));
    /* só PNG é codificado aqui; a especificação manda cair em PNG para os outros tipos */
    usize len = 0;
    u8 *png = canvas_png(c, &len);
    if (!png) return js_throw_err(E_RANGE, "sem memória para codificar o PNG");
    static const char pre[] = "data:image/png;base64,";
    usize out = sizeof pre - 1 + (len + 2) / 3 * 4;
    char *s = kmalloc(out + 1);
    if (!s) { kfree(png); return js_throw_err(E_RANGE, "sem memória"); }
    memcpy(s, pre, sizeof pre - 1);
    usize k = sizeof pre - 1;
    for (usize i = 0; i < len; i += 3) {
        u32 v = (u32)png[i] << 16 | (i + 1 < len ? (u32)png[i + 1] << 8 : 0) | (i + 2 < len ? png[i + 2] : 0);
        s[k++] = b64[v >> 18];
        s[k++] = b64[(v >> 12) & 63];
        s[k++] = i + 1 < len ? b64[(v >> 6) & 63] : '=';
        s[k++] = i + 2 < len ? b64[v & 63] : '=';
    }
    s[k] = 0;
    kfree(png);
    jsstr *r = js_strn(s, (int)k);
    kfree(s);
    return jv_str(r);
}

NATIVE(illegal_ctor) { UNUSED_ARGS; return js_throw_err(E_TYPE, "Construtor ilegal"); }

/* ============================================================ registro */
static jsobj *iface(jsobj *g, const char *name, jsobj *proto, jsnative ctor, int nargs) {
    jsobj *c = js_newctor(ctor ? ctor : illegal_ctor, name, nargs, proto);
    js_defn(g, name, jv_obj(c), PF_NOENUM);
    js_defprop(proto, A[A_sym_toStringTag], jv_str(js_intern(name, -1)), PF_NOENUM | PF_RO);
    return c;
}

void js_canvas_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    /* DOMException */
    r->p_domexc = js_newobj(r->p_err[E_ERROR]);
    iface(g, "DOMException", r->p_domexc, domexc_ctor, 2);
    /* HTMLCanvasElement */
    r->p_canvas = js_newobj(r->p_elem);
    jsobj *cp = r->p_canvas;
    js_defaccessor(cp, js_intern("width", -1), jv_obj(js_newnative(cv_size_get, "width", 0, 0)), jv_obj(js_newnative(cv_size_set, "width", 1, 0)), 0);
    js_defaccessor(cp, js_intern("height", -1), jv_obj(js_newnative(cv_size_get, "height", 0, 1)), jv_obj(js_newnative(cv_size_set, "height", 1, 1)), 0);
    js_defnative(cp, "getContext", cv_getcontext, 1, 0);
    js_defnative(cp, "toDataURL", cv_todataurl, 0, 0);
    iface(g, "HTMLCanvasElement", cp, NULL, 0);
    /* CanvasRenderingContext2D */
    r->p_ctx2d = js_newobj(op);
    jsobj *xp = r->p_ctx2d;
    for (int i = 0; i < P_COUNT; i++)
        js_defaccessor(xp, js_intern(prop_names[i], -1), jv_obj(js_newnative(ctx_get, prop_names[i], 0, i)),
                       i == P_CANVAS ? JS_UNDEF : jv_obj(js_newnative(ctx_set, prop_names[i], 1, i)), 0);
    static const char *misc[] = { "save", "restore", "reset", "beginPath", "resetTransform", "isContextLost" };
    for (int i = 0; i < 6; i++) js_defnative(xp, misc[i], ctx_misc, 0, i);
    static const char *xf[] = { "scale", "rotate", "translate", "transform", "setTransform" };
    static const int xfn[] = { 2, 1, 2, 6, 0 };
    for (int i = 0; i < 5; i++) js_defnative(xp, xf[i], ctx_xform, xfn[i], i);
    js_defnative(xp, "getTransform", ctx_gettransform, 0, 0);
    static const char *po[] = { "moveTo", "lineTo", "quadraticCurveTo", "bezierCurveTo", "arcTo", "rect", "arc", "ellipse", "closePath", "roundRect" };
    static const int pon[] = { 2, 2, 4, 6, 5, 4, 5, 7, 0, 4 };
    for (int i = 0; i < 10; i++) js_defnative(xp, po[i], path_op, pon[i], i);
    js_defnative(xp, "fillRect", ctx_rect, 4, 0);
    js_defnative(xp, "strokeRect", ctx_rect, 4, 1);
    js_defnative(xp, "clearRect", ctx_rect, 4, 2);
    js_defnative(xp, "fill", ctx_paint, 0, 0);
    js_defnative(xp, "stroke", ctx_paint, 0, 1);
    js_defnative(xp, "clip", ctx_paint, 0, 2);
    js_defnative(xp, "isPointInPath", ctx_hit, 2, 0);
    js_defnative(xp, "isPointInStroke", ctx_hit, 2, 1);
    js_defnative(xp, "fillText", ctx_text, 3, 0);
    js_defnative(xp, "strokeText", ctx_text, 3, 1);
    js_defnative(xp, "measureText", ctx_measure, 1, 0);
    js_defnative(xp, "drawImage", ctx_drawimage, 3, 0);
    js_defnative(xp, "createLinearGradient", ctx_gradient, 4, 0);
    js_defnative(xp, "createRadialGradient", ctx_gradient, 6, 1);
    js_defnative(xp, "createConicGradient", ctx_gradient, 3, 2);
    js_defnative(xp, "createPattern", ctx_pattern, 2, 0);
    js_defnative(xp, "createImageData", ctx_createimagedata, 1, 0);
    js_defnative(xp, "getImageData", ctx_getimagedata, 4, 0);
    js_defnative(xp, "putImageData", ctx_putimagedata, 3, 0);
    js_defnative(xp, "setLineDash", ctx_setlinedash, 1, 0);
    js_defnative(xp, "getLineDash", ctx_getlinedash, 0, 0);
    js_defnative(xp, "getContextAttributes", ctx_attrs, 0, 0);
    js_defnative(xp, "drawFocusIfNeeded", ctx_focus, 1, 0);
    iface(g, "CanvasRenderingContext2D", xp, NULL, 0);
    /* CanvasGradient, CanvasPattern */
    r->p_cgrad = js_newobj(op);
    js_defnative(r->p_cgrad, "addColorStop", grad_addstop, 2, 0);
    iface(g, "CanvasGradient", r->p_cgrad, NULL, 0);
    r->p_cpat = js_newobj(op);
    js_defnative(r->p_cpat, "setTransform", pat_settransform, 0, 0);
    iface(g, "CanvasPattern", r->p_cpat, NULL, 0);
    /* Path2D */
    r->p_path2d = js_newobj(op);
    for (int i = 0; i < 10; i++) js_defnative(r->p_path2d, po[i], path_op, pon[i], i);
    js_defnative(r->p_path2d, "addPath", path2d_addpath, 1, 0);
    iface(g, "Path2D", r->p_path2d, path2d_ctor, 0);
    /* ImageData, TextMetrics, DOMMatrix */
    r->p_imgdata = js_newobj(op);
    iface(g, "ImageData", r->p_imgdata, imagedata_ctor, 2);
    r->p_tmetrics = js_newobj(op);
    iface(g, "TextMetrics", r->p_tmetrics, NULL, 0);
    r->p_dommatrix = js_newobj(op);
    static const char *mm[] = { "multiply", "inverse", "translate", "scale", "rotate", "transformPoint", "toString" };
    for (int i = 0; i < 7; i++) js_defnative(r->p_dommatrix, mm[i], matrix_method, i == 1 || i == 6 ? 0 : 1, i);
    jsobj *dm = iface(g, "DOMMatrix", r->p_dommatrix, matrix_ctor, 0);
    js_defn(g, "DOMMatrixReadOnly", jv_obj(dm), PF_NOENUM);
}
