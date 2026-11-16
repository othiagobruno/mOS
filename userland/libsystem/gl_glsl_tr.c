/*
 * mOS libSystem — OpenGL por software: tradução de GLSL (1.10 a 4.10) para o dialeto do compilador de MSL.
 * O texto já vem do pré-processador (msl_preprocess). A tradução:
 *   - tira os qualificadores de armazenamento das globais (uniform, in, out, attribute, varying, layout,
 *     flat/smooth/noperspective, centroid, invariant, precisão) e guarda o que cada uma é (gl_decl_t);
 *   - blocos de interface (uniform/in/out Nome { ... } inst) viram struct + instância; membros de bloco
 *     sem instância são renomeados para a instância oculta;
 *   - parâmetros out/inout viram referências (thread T &);
 *   - discard -> discard_fragment(), ^^ -> !=, construtores de array T[N](...) -> lista {...},
 *     x.length() -> tamanho conhecido, sufixo lf de double some;
 *   - nomes que são palavras do MSL e não do GLSL (vertex, sampler, half, device...) ganham prefixo.
 */
#include "gl_glsl.h"

int snprintf(char *b, size_t n, const char *fmt, ...);
char *strcat(char *d, const char *s);
char *strncat(char *d, const char *s, size_t n);

typedef struct { int k; const char *p; int n, line; } gt_t;   /* k: 'i' id, 'n' número, 'p' pontuação */
typedef struct { gt_t *v; int n, cap; } gtv_t;
static int is_a(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_an(int c) { return is_a(c) || (c >= '0' && c <= '9'); }
static void gpush(gtv_t *v, gt_t t) {
    if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 1024; v->v = realloc(v->v, (size_t)v->cap * sizeof *v->v); }
    v->v[v->n++] = t;
}
static void glex(const char *s, gtv_t *out) {
    int line = 1;
    static const char *p3[] = {"<<=", ">>=", 0};
    static const char *p2[] = {"++", "--", "<=", ">=", "==", "!=", "&&", "||", "^^", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<", ">>", "::", "->", 0};
    while (*s) {
        if (*s == '\n') { line++; s++; continue; }
        if (*s == ' ' || *s == '\t' || *s == '\r') { s++; continue; }
        gt_t t = {0, s, 0, line};
        if (is_a(*s)) { while (is_an(*s)) s++; t.k = 'i'; }
        else if ((*s >= '0' && *s <= '9') || (*s == '.' && s[1] >= '0' && s[1] <= '9')) {
            while (is_an(*s) || *s == '.' || ((*s == '+' || *s == '-') && (s[-1] == 'e' || s[-1] == 'E') && !(s[-2] == '0' && (s[-1] == 'x' || s[-1] == 'X')))) s++;
            t.k = 'n';
        } else {
            t.k = 'p';
            int m = 1;
            for (int i = 0; p3[i]; i++) if (!strncmp(s, p3[i], 3)) m = 3;
            if (m == 1) for (int i = 0; p2[i]; i++) if (!strncmp(s, p2[i], 2)) m = 2;
            s += m;
        }
        t.n = (int)(s - t.p);
        gpush(out, t);
    }
}
static int teq(const gt_t *t, const char *s) { int n = (int)strlen(s); return t->n == n && !memcmp(t->p, s, n); }
static char *tdup(const gt_t *t) { char *r = malloc((size_t)t->n + 1); memcpy(r, t->p, (size_t)t->n); r[t->n] = 0; return r; }

/* ---- saída */
typedef struct { char *p; u64 n, cap; int line; } ob_t;
static void ob_put(ob_t *o, const char *s, u64 n) {
    if (o->n + n + 2 > o->cap) { u64 nc = o->cap ? o->cap * 2 : 65536; while (nc < o->n + n + 2) nc *= 2; o->p = realloc(o->p, nc); o->cap = nc; }
    memcpy(o->p + o->n, s, n);
    o->n += n;
    o->p[o->n] = 0;
}
static void ob_s(ob_t *o, const char *s) { ob_put(o, s, strlen(s)); ob_put(o, " ", 1); }
static void ob_tok(ob_t *o, const gt_t *t) {
    while (o->line < t->line) { ob_put(o, "\n", 1); o->line++; }
    ob_put(o, t->p, (u64)t->n);
    ob_put(o, " ", 1);
}

/* ---- palavras */
static const char *QUALS[] = {"uniform", "in", "out", "inout", "attribute", "varying", "buffer", "flat", "smooth", "noperspective",
                              "centroid", "sample", "patch", "invariant", "precise", "highp", "mediump", "lowp", "shared",
                              "coherent", "volatile", "restrict", "readonly", "writeonly", "bindable", 0};
static const char *RENAME[] = {"vertex", "fragment", "kernel", "device", "constant", "thread", "threadgroup", "sampler", "half", "using",
                               "namespace", "template", "typename", "static", "inline", "auto", "array", "atomic", "vec", "matrix",
                               "packed", "char", "uchar", "short", "ushort", "long", "ulong", "size_t", "ptrdiff_t", "constexpr",
                               "metal", "delete", "new", "operator", "private", "public", "protected", "class", "this", "throw",
                               "try", "catch", "friend", "virtual", "signed", "unsigned", "goto", "register", "extern", "union",
                               "simd", "fast", "precise_", "ray_data", "object_data", "visible", "stitchable", "uchar2", "half2",
                               "select", "saturate", "level", "bias", "depth2d", "texture2d", "texture1d", "texture3d", "texturecube",
                               "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t", "float2",
                               "float3", "float4", "int2", "int3", "int4", "uint2", "uint3", "uint4", "bool2", "bool3", "bool4", 0};
static const char *GTYPES[] = {"void", "bool", "int", "uint", "float", "double", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4",
                               "uvec2", "uvec3", "uvec4", "bvec2", "bvec3", "bvec4", "dvec2", "dvec3", "dvec4", "mat2", "mat3", "mat4",
                               "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3", "mat4x4", "dmat2",
                               "dmat3", "dmat4", "dmat2x2", "dmat2x3", "dmat2x4", "dmat3x2", "dmat3x3", "dmat3x4", "dmat4x2",
                               "dmat4x3", "dmat4x4", 0};
static int in_list(const gt_t *t, const char **l) { for (int i = 0; l[i]; i++) if (teq(t, l[i])) return 1; return 0; }

typedef struct {
    gtv_t T;
    int pos;
    ob_t o;
    gl_shinfo_t *I;
    char *structs[256]; int nstructs;
    char *arrn[256]; int arrv[256]; int narr;          /* tamanhos de array conhecidos (x.length()) */
    char *bmem[256]; char *bmap[256]; int nbmem;       /* membro de bloco sem instância -> "inst.membro" */
    int anon;
    int unsized;               /* "T x[]" de entrada/saída vira [N]: 6 no geometry shader, 32 na tesselação */
} tr_t;
static int is_type_tok(tr_t *R, const gt_t *t) {
    if (t->k != 'i') return 0;
    if (in_list(t, GTYPES)) return 1;
    for (int i = 0; i < R->nstructs; i++) if ((int)strlen(R->structs[i]) == t->n && !memcmp(R->structs[i], t->p, t->n)) return 1;
    if (t->n > 7 && (!memcmp(t->p, "sampler", 7) || !memcmp(t->p, "isampler", 8) || !memcmp(t->p, "usampler", 8))) return 1;
    return 0;
}
static int arr_len(tr_t *R, const gt_t *t) { for (int i = R->narr - 1; i >= 0; i--) if ((int)strlen(R->arrn[i]) == t->n && !memcmp(R->arrn[i], t->p, t->n)) return R->arrv[i]; return -1; }
static void arr_note(tr_t *R, const gt_t *name, int n) { if (R->narr < 256) { R->arrn[R->narr] = tdup(name); R->arrv[R->narr++] = n; } }
static const char *bmem_map(tr_t *R, const gt_t *t) { for (int i = 0; i < R->nbmem; i++) if ((int)strlen(R->bmem[i]) == t->n && !memcmp(R->bmem[i], t->p, t->n)) return R->bmap[i]; return 0; }

/* emite tokens [a, b) com as reescritas de corpo */
static void body(tr_t *R, int a, int b) {
    gt_t *T = R->T.v;
    int close_stack[64], ncs = 0;   /* profundidade de parêntese em que um construtor de array fecha com '}' */
    int pd = 0;
    for (int i = a; i < b; i++) {
        gt_t *t = &T[i];
        if (t->k == 'i') {
            if (teq(t, "highp") || teq(t, "mediump") || teq(t, "lowp") || teq(t, "precise") || teq(t, "invariant")) continue;
            if (teq(t, "discard")) { gt_t d = *t; d.p = "discard_fragment()"; d.n = 18; ob_tok(&R->o, &d); continue; }
            /* x.length() */
            if (i + 3 < b && teq(&T[i + 1], ".") && teq(&T[i + 2], "length") && teq(&T[i + 3], "(") && i + 4 < b && teq(&T[i + 4], ")")) {
                int n = arr_len(R, t);
                if (n > 0) { char num[16]; snprintf(num, sizeof num, "%d", n); gt_t d = *t; d.p = num; d.n = (int)strlen(num); ob_tok(&R->o, &d); i += 4; continue; }
            }
            /* construtor de array: T[N](...) / T[](...) */
            if (is_type_tok(R, t) && i + 2 < b && teq(&T[i + 1], "[")) {
                int j = i + 2;
                while (j < b && !teq(&T[j], "]")) j++;
                if (j + 1 < b && teq(&T[j + 1], "(") && (i == a || !(T[i - 1].k == 'i' && !teq(&T[i - 1], "return")))) {
                    gt_t d = T[j + 1];
                    d.p = "{"; d.n = 1;
                    ob_tok(&R->o, &d);
                    pd++;
                    if (ncs < 64) close_stack[ncs++] = pd;
                    i = j + 1;
                    continue;
                }
            }
            const char *bm = (i == a || !teq(&T[i - 1], ".")) ? bmem_map(R, t) : 0;
            if (bm) { gt_t d = *t; d.p = bm; d.n = (int)strlen(bm); ob_tok(&R->o, &d); continue; }
            if (in_list(t, RENAME) && (i == a || !teq(&T[i - 1], "."))) {
                char nm[80];
                snprintf(nm, sizeof nm, "_g_%.*s", t->n, t->p);
                gt_t d = *t; d.p = nm; d.n = (int)strlen(nm);
                ob_tok(&R->o, &d);
                continue;
            }
        } else if (t->k == 'n') {
            gt_t d = *t;
            if (d.n > 2 && (d.p[d.n - 1] == 'f' || d.p[d.n - 1] == 'F') && (d.p[d.n - 2] == 'l' || d.p[d.n - 2] == 'L')) d.n -= 2;
            ob_tok(&R->o, &d);
            continue;
        } else {
            if (R->unsized && teq(t, "[") && i + 1 < b && teq(&T[i + 1], "]")) { char us[8]; snprintf(us, sizeof us, "[%d]", R->unsized); gt_t d = *t; d.p = us; d.n = (int)strlen(us); ob_tok(&R->o, &d); i++; continue; }
            if (teq(t, "^^")) { gt_t d = *t; d.p = "!="; ob_tok(&R->o, &d); continue; }
            if (teq(t, "(")) pd++;
            if (teq(t, ")")) {
                if (ncs && close_stack[ncs - 1] == pd) { ncs--; pd--; gt_t d = *t; d.p = "}"; ob_tok(&R->o, &d); continue; }
                pd--;
            }
        }
        ob_tok(&R->o, t);
    }
}
/* qualificadores de uma declaração; devolve o índice do primeiro token depois deles */
typedef struct { int storage, loc, index, interp, rowmajor, centroid, origin_ul, pixel_int, isconst, maxv, inv, hasprim, patch, verts, cw, ccw, point, tes; GLenum prim, spacing; } qual_t;
static int quals(tr_t *R, int i, int e, qual_t *q) {
    gt_t *T = R->T.v;
    memset(q, 0, sizeof *q);
    q->loc = -1;
    q->index = -1;
    q->interp = MSL_INTERP_PERSP;
    for (; i < e; i++) {
        gt_t *t = &T[i];
        if (teq(t, "layout") && i + 1 < e && teq(&T[i + 1], "(")) {
            int j = i + 2;
            while (j < e && !teq(&T[j], ")")) {
                if (teq(&T[j], "location") && j + 2 < e && teq(&T[j + 1], "=")) q->loc = (int)strtol(T[j + 2].p, 0, 0);
                if (teq(&T[j], "index") && j + 2 < e && teq(&T[j + 1], "=")) q->index = (int)strtol(T[j + 2].p, 0, 0);
                if (teq(&T[j], "row_major")) q->rowmajor = 1;
                if (teq(&T[j], "column_major")) q->rowmajor = 0;
                if (teq(&T[j], "origin_upper_left")) q->origin_ul = 1;
                if (teq(&T[j], "pixel_center_integer")) q->pixel_int = 1;
                static const struct { const char *n; GLenum p; } GP[] = {{"points", GL_POINTS}, {"lines", GL_LINES},
                    {"lines_adjacency", GL_LINES_ADJACENCY}, {"triangles", GL_TRIANGLES}, {"triangles_adjacency", GL_TRIANGLES_ADJACENCY},
                    {"line_strip", GL_LINE_STRIP}, {"triangle_strip", GL_TRIANGLE_STRIP}, {0, 0}};
                for (int g = 0; GP[g].n; g++) if (teq(&T[j], GP[g].n)) { q->prim = GP[g].p; q->hasprim = 1; }
                if (teq(&T[j], "max_vertices") && j + 2 < e && teq(&T[j + 1], "=")) q->maxv = (int)strtol(T[j + 2].p, 0, 0);
                if (teq(&T[j], "invocations") && j + 2 < e && teq(&T[j + 1], "=")) q->inv = (int)strtol(T[j + 2].p, 0, 0);
                /* tesselação */
                if (teq(&T[j], "vertices") && j + 2 < e && teq(&T[j + 1], "=")) q->verts = (int)strtol(T[j + 2].p, 0, 0);
                if (teq(&T[j], "quads")) { q->prim = GL_QUADS; q->hasprim = 1; }
                if (teq(&T[j], "isolines")) { q->prim = GL_ISOLINES; q->hasprim = 1; }
                if (teq(&T[j], "equal_spacing")) { q->spacing = GL_EQUAL; q->tes = 1; }
                if (teq(&T[j], "fractional_even_spacing")) { q->spacing = GL_FRACTIONAL_EVEN; q->tes = 1; }
                if (teq(&T[j], "fractional_odd_spacing")) { q->spacing = GL_FRACTIONAL_ODD; q->tes = 1; }
                if (teq(&T[j], "cw")) { q->cw = 1; q->tes = 1; }
                if (teq(&T[j], "ccw")) { q->ccw = 1; q->tes = 1; }
                if (teq(&T[j], "point_mode")) { q->point = 1; q->tes = 1; }
                j++;
            }
            i = j;
            continue;
        }
        if (teq(t, "uniform")) { q->storage = GS_UNIFORM; continue; }
        if (teq(t, "in") || teq(t, "attribute")) { q->storage = teq(t, "in") ? GS_IN : GS_IN; continue; }
        if (teq(t, "varying")) { q->storage = R->I->stage == 0 ? GS_OUT : GS_IN; continue; }
        if (teq(t, "out")) { q->storage = GS_OUT; continue; }
        if (teq(t, "flat")) { q->interp = MSL_INTERP_FLAT; continue; }
        if (teq(t, "noperspective")) { q->interp = MSL_INTERP_NOPERSP; continue; }
        if (teq(t, "smooth")) { q->interp = MSL_INTERP_PERSP; continue; }
        if (teq(t, "centroid")) { if (!q->centroid) q->centroid = 1; continue; }
        if (teq(t, "sample")) { q->centroid = 2; continue; }
        if (teq(t, "const")) { q->isconst = 1; continue; }
        if (teq(t, "patch")) { q->patch = 1; continue; }
        if (in_list(t, QUALS)) continue;
        break;
    }
    return i;
}
static gl_decl_t *decl_new(tr_t *R, const gt_t *name, const qual_t *q) {
    gl_shinfo_t *I = R->I;
    if (I->ndecl == I->capdecl) { I->capdecl = I->capdecl ? I->capdecl * 2 : 32; I->decl = realloc(I->decl, (size_t)I->capdecl * sizeof *I->decl); }
    gl_decl_t *d = &I->decl[I->ndecl++];
    memset(d, 0, sizeof *d);
    d->name = tdup(name);
    d->storage = q->storage;
    d->loc = q->loc;
    d->index = q->index;
    d->interp = q->interp;
    d->aux = q->centroid;
    d->patch = q->patch;
    d->rowmajor = q->rowmajor;
    return d;
}
/* tamanho de "T x[]": entradas do GS (6), entradas e saídas por vértice da tesselação (gl_MaxPatchVertices = 32) */
static int unsized_of(tr_t *R, const qual_t *q) {
    int st = R->I->stage;
    if (st == 2 && q->storage == GS_IN) return 6;
    if (st == 3 && (q->storage == GS_IN || q->storage == GS_OUT) && !q->patch) return 32;
    if (st == 4 && q->storage == GS_IN && !q->patch) return 32;
    return 0;
}
/* declaração de topo (sem o ';'): tokens [a, e) */
static void top_decl(tr_t *R, int a, int e) {
    gt_t *T = R->T.v;
    if (a >= e) return;
    if (teq(&T[a], "precision")) return;
    if (teq(&T[a], "struct")) {
        if (a + 1 < e && T[a + 1].k == 'i' && R->nstructs < 256) R->structs[R->nstructs++] = tdup(&T[a + 1]);
        body(R, a, e);
        ob_s(&R->o, ";");
        return;
    }
    qual_t q;
    int i = quals(R, a, e, &q);
    if (i >= e) {
        /* tesselação: "layout(vertices = 4) out;" e "layout(quads, fractional_odd_spacing, cw, point_mode) in;" */
        if (R->I->stage == 3 && q.storage == GS_OUT && q.verts) {
            R->I->tcs_vertices = q.verts;
            gt_t g = {'i', "gl_out", 6, 0};
            arr_note(R, &g, q.verts);
            return;
        }
        if (R->I->stage == 4 && q.storage == GS_IN && (q.hasprim || q.tes)) {
            if (q.hasprim) { R->I->tes_mode = q.prim; R->I->tes_set = 1; }
            if (q.spacing) R->I->tes_spacing = q.spacing;
            if (q.cw) R->I->tes_cw = 1;
            if (q.ccw) R->I->tes_cw = 0;
            if (q.point) R->I->tes_point = 1;
            return;
        }
        /* "layout(triangles) in;" / "layout(triangle_strip, max_vertices = 3) out;" */
        if (q.storage == GS_IN && q.hasprim) {
            R->I->gs_in = q.prim;
            R->I->gs_in_set = 1;
            if (q.inv) R->I->gs_inv = q.inv;
            int n = q.prim == GL_POINTS ? 1 : q.prim == GL_LINES ? 2 : q.prim == GL_LINES_ADJACENCY ? 4 : q.prim == GL_TRIANGLES ? 3 : 6;
            gt_t g = {'i', "gl_in", 5, 0};
            arr_note(R, &g, n);
        }
        if (q.storage == GS_OUT && q.hasprim) { R->I->gs_out = q.prim; R->I->gs_out_set = 1; if (q.maxv) R->I->gs_max = q.maxv; }
        if (q.storage == GS_OUT && q.maxv) R->I->gs_max = q.maxv;
        return;
    }
    /* invariant gl_Position; / redeclaração de embutida */
    if (T[i].k == 'i' && (i + 1 == e || teq(&T[i + 1], ",")) && !is_type_tok(R, &T[i])) return;
    if (!q.storage) {
        /* global comum: só as reescritas de corpo; as mutáveis voltam ao valor inicial a cada invocação */
        if (q.isconst) ob_s(&R->o, "const");
        body(R, i, e);
        ob_s(&R->o, ";");
        for (int k = i; k + 3 < e; k++) if (T[k].k == 'i' && teq(&T[k + 1], "[") && T[k + 2].k == 'n' && teq(&T[k + 3], "]")) arr_note(R, &T[k], (int)strtol(T[k + 2].p, 0, 0));
        if (!q.isconst && is_type_tok(R, &T[i])) {
            int depth = 0;
            for (int k = i + 1; k < e; k++) {
                if (teq(&T[k], "(") || teq(&T[k], "[") || teq(&T[k], "{")) depth++;
                if (teq(&T[k], ")") || teq(&T[k], "]") || teq(&T[k], "}")) depth--;
                if (depth || T[k].k != 'i') continue;
                if (k == i + 1 || teq(&T[k - 1], ",") || (teq(&T[k - 1], "]") && k == i + 4)) { qual_t z = q; z.storage = GS_PLAIN; decl_new(R, &T[k], &z); }
            }
        }
        return;
    }
    /* bloco de interface */
    if (T[i].k == 'i' && i + 1 < e && teq(&T[i + 1], "{")) {
        gt_t *bname = &T[i];
        int j = i + 2, d = 1;
        while (j < e && d) { if (teq(&T[j], "{")) d++; if (teq(&T[j], "}")) d--; j++; }
        int mb = i + 2, me = j - 1;   /* membros */
        int has_inst = j < e && T[j].k == 'i';
        char sname[128], iname[128];
        snprintf(sname, sizeof sname, "__blk_%.*s", bname->n, bname->p);
        if (has_inst) snprintf(iname, sizeof iname, "%.*s", T[j].n, T[j].p);
        else snprintf(iname, sizeof iname, "__blki_%.*s", bname->n, bname->p);
        if (bname->n == 10 && !memcmp(bname->p, "gl_PerVertex", 10)) return;
        if (bname->n >= 3 && !memcmp(bname->p, "gl_", 3)) return;
        ob_s(&R->o, "struct");
        ob_s(&R->o, sname);
        ob_s(&R->o, "{");
        /* membros sem qualificadores */
        int k = mb;
        while (k < me) {
            int s = k;
            while (k < me && !teq(&T[k], ";")) k++;
            qual_t mq;
            int f = quals(R, s, k, &mq);
            body(R, f, k);
            ob_s(&R->o, ";");
            if (!has_inst) {
                /* nomes dos membros para o acesso direto */
                for (int m = f + 1; m < k; m++)
                    if (T[m].k == 'i' && (teq(&T[m - 1], ",") || m == f + 1 || is_type_tok(R, &T[m - 1])) && R->nbmem < 256 && !is_type_tok(R, &T[m])) {
                        char full[256];
                        snprintf(full, sizeof full, "%s.%.*s", iname, T[m].n, T[m].p);
                        R->bmem[R->nbmem] = tdup(&T[m]);
                        R->bmap[R->nbmem++] = strdup(full);
                    }
            }
            k++;
        }
        ob_s(&R->o, "};");
        ob_s(&R->o, sname);
        ob_s(&R->o, iname);
        int arr = 0;
        if (has_inst && j + 1 < e && teq(&T[j + 1], "[")) {
            arr = j + 2 < e && T[j + 2].k == 'n' ? (int)strtol(T[j + 2].p, 0, 0) : unsized_of(R, &q);
            R->unsized = unsized_of(R, &q);
            body(R, j + 1, e);
            R->unsized = 0;
        }
        ob_s(&R->o, ";");
        gt_t nm = {'i', iname, (int)strlen(iname), 0};
        gl_decl_t *dd = decl_new(R, &nm, &q);
        dd->block = strdup(sname + 6);
        dd->arr = arr;
        dd->has_inst = has_inst;
        dd->type = strdup(sname);
        if (arr) { char dm[16]; snprintf(dm, sizeof dm, "[%d]", arr); dd->dims = strdup(dm); }
        return;
    }
    /* uniform/in/out comum: emite sem os qualificadores e anota cada nome */
    if (T[i].k == 'i' && T[i].n > 3 && !memcmp(T[i].p, "gl_", 3)) return;
    int ti = i;
    if (!is_type_tok(R, &T[ti])) { body(R, i, e); ob_s(&R->o, ";"); return; }
    for (int k = ti + 1; k < e; k++) if (T[k].n > 3 && !memcmp(T[k].p, "gl_", 3) && T[k].k == 'i') return;   /* "out vec4 gl_FragColor;" */
    R->unsized = unsized_of(R, &q);
    body(R, i, e);
    R->unsized = 0;
    ob_s(&R->o, ";");
    int depth = 0;
    for (int k = ti + 1; k < e; k++) {
        if (teq(&T[k], "(") || teq(&T[k], "[") || teq(&T[k], "{")) depth++;
        if (teq(&T[k], ")") || teq(&T[k], "]") || teq(&T[k], "}")) depth--;
        if (depth || T[k].k != 'i') continue;
        gt_t *pv = &T[k - 1];
        if (k == ti + 1 || teq(pv, ",") || teq(pv, "]")) {
            if (k > ti + 1 && teq(pv, "]")) {
                /* "vec4 x[2]" — o ] é do tipo: o nome vem agora */
            }
            gl_decl_t *dd = decl_new(R, &T[k], &q);
            dd->type = tdup(&T[ti]);
            if (k + 1 < e && teq(&T[k + 1], "[")) {
                char dm[32] = "";
                int m = k + 1;
                while (m < e && !teq(&T[m], ",") && !teq(&T[m], "=") && (int)strlen(dm) + T[m].n + 2 < (int)sizeof dm) {
                    if (teq(&T[m], "]") && teq(&T[m - 1], "[")) { char us[8]; snprintf(us, sizeof us, "%d", unsized_of(R, &q)); strcat(dm, us); }
                    strncat(dm, T[m].p, (size_t)T[m].n);
                    m++;
                }
                dd->dims = strdup(dm);
            }
            if (k + 3 < e && teq(&T[k + 1], "[") && T[k + 2].k == 'n') arr_note(R, &T[k], (int)strtol(T[k + 2].p, 0, 0));
            else if (k + 2 < e && teq(&T[k + 1], "[") && teq(&T[k + 2], "]") && unsized_of(R, &q)) arr_note(R, &T[k], unsized_of(R, &q));
        }
    }
}
/* cabeçalho de função [a, e) até o ')' */
static void fn_header(tr_t *R, int a, int e) {
    gt_t *T = R->T.v;
    int p = a;
    while (p < e && !teq(&T[p], "(")) p++;
    body(R, a, p + 1);
    int k = p + 1;
    int end = e - 1;   /* ')' */
    while (k < end) {
        int s = k, d = 0;
        while (k < end && !(d == 0 && teq(&T[k], ","))) { if (teq(&T[k], "(") || teq(&T[k], "[")) d++; if (teq(&T[k], ")") || teq(&T[k], "]")) d--; k++; }
        /* parâmetro [s, k) */
        int ref = 0, cst = 0, j = s;
        for (; j < k; j++) {
            if (teq(&T[j], "out") || teq(&T[j], "inout")) { ref = 1; continue; }
            if (teq(&T[j], "in") || teq(&T[j], "highp") || teq(&T[j], "mediump") || teq(&T[j], "lowp") || teq(&T[j], "precise")) continue;
            if (teq(&T[j], "const")) { cst = 1; continue; }
            break;
        }
        (void)cst;
        if (j < k && teq(&T[j], "void") && k - j == 1) { ob_s(&R->o, "void"); }
        else if (j < k) {
            if (ref) ob_s(&R->o, "thread");
            /* tipo */
            body(R, j, j + 1);
            int n = j + 1;
            if (n < k && teq(&T[n], "[")) { /* "float[3] a" */ while (n < k && !teq(&T[n], "]")) n++; n++; }
            if (ref) ob_s(&R->o, "&");
            body(R, n, k);
            if (n < k && T[n].k == 'i' && n + 3 < k && teq(&T[n + 1], "[") && T[n + 2].k == 'n') arr_note(R, &T[n], (int)strtol(T[n + 2].p, 0, 0));
        }
        if (k < end) { ob_s(&R->o, ","); k++; }
    }
    ob_s(&R->o, ")");
}
/* corpo de função a partir do '{' em i; devolve o índice depois do '}' */
static int fn_body(tr_t *R, int i) {
    gt_t *T = R->T.v;
    int d = 0, s = i;
    for (; i < R->T.n; i++) {
        if (teq(&T[i], "{")) d++;
        if (teq(&T[i], "}")) { d--; if (!d) { i++; break; } }
    }
    /* tamanhos de arrays locais */
    for (int k = s; k + 3 < i; k++) if (T[k].k == 'i' && teq(&T[k + 1], "[") && T[k + 2].k == 'n' && teq(&T[k + 3], "]") && k > s && (is_type_tok(R, &T[k - 1]) || teq(&T[k - 1], ","))) arr_note(R, &T[k], (int)strtol(T[k + 2].p, 0, 0));
    body(R, s, i);
    return i;
}
char *gl_glsl_translate(const char *pp_text, gl_shinfo_t *I) {
    tr_t *R = calloc(1, sizeof *R);
    if (!R) return 0;
    R->I = I;
    R->o.line = 1;
    glex(pp_text, &R->T);
    if (I->stage == 3 || I->stage == 4) { gt_t g = {'i', "gl_in", 5, 0}; arr_note(R, &g, 32); }   /* gl_in[gl_MaxPatchVertices] */
    gt_t *T = R->T.v;
    int n = R->T.n, i = 0;
    while (i < n) {
        int s = i, pd = 0, bd = 0, func = 0;
        for (; i < n; i++) {
            gt_t *t = &T[i];
            if (teq(t, "(")) pd++;
            else if (teq(t, ")")) pd--;
            else if (teq(t, "{")) {
                if (!bd && !pd && i > s && teq(&T[i - 1], ")")) { func = 1; break; }
                bd++;
            } else if (teq(t, "}")) bd--;
            else if (teq(t, ";") && !pd && !bd) break;
        }
        if (func) {
            fn_header(R, s, i);
            i = fn_body(R, i);
            continue;
        }
        /* protótipo "T f(...);" */
        int is_proto = 0;
        for (int k = s; k < i; k++) if (teq(&T[k], "(") && k > s && T[k - 1].k == 'i' && !is_type_tok(R, &T[k - 1]) && k - 1 > s && (is_type_tok(R, &T[k - 2]) || T[k - 2].k == 'i')) {
            /* "vec3 v = vec3(1)" tem '=' antes do parêntese */
            int eq = 0;
            for (int m = s; m < k; m++) if (teq(&T[m], "=")) eq = 1;
            if (!eq) is_proto = 1;
            break;
        }
        if (is_proto && i > s && teq(&T[i - 1], ")")) { fn_header(R, s, i); ob_s(&R->o, ";"); i++; continue; }
        top_decl(R, s, i);
        i++;
    }
    char *out = R->o.p ? R->o.p : strdup("");
    for (int k = 0; k < R->nstructs; k++) free(R->structs[k]);
    for (int k = 0; k < R->narr; k++) free(R->arrn[k]);
    for (int k = 0; k < R->nbmem; k++) { free(R->bmem[k]); free(R->bmap[k]); }
    free(R->T.v);
    free(R);
    return out;
}
