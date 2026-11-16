/*
 * mOS libSystem — OpenGL por software (compat): ARB_vertex_program e ARB_fragment_program ("!!ARBvp1.0",
 * "!!ARBfp1.0"). O assembly vira GLSL 1.20 de compatibilidade e roda no mesmo pipeline do GLSL (gl_glsl.c):
 *   ATTRIB/PARAM/TEMP/ADDRESS/OUTPUT/ALIAS, todas as instruções do vp (ABS ADD ARL DP3 DP4 DPH DST EX2 EXP FLR FRC
 *   LG2 LIT LOG MAD MAX MIN MOV MUL POW RCP RSQ SGE SLT SUB SWZ XPD) e do fp (as do vp sem ARL/EXP/LOG, mais CMP
 *   COS KIL LRP SCS SIN TEX TXB TXP e o sufixo _SAT), swizzles, negação, máscara de escrita, endereçamento
 *   relativo, os vínculos vertex.*, fragment.*, result.*, state.* (matrizes com .row/.inverse/.transpose/.invtrans,
 *   luz, material, fog, planos, ponto, texgen, texenv, depth range), program.env[] e program.local[], OPTION
 *   ARB_position_invariant e ARB_fog_linear/exp/exp2. Erro de sintaxe vai para GL_PROGRAM_ERROR_POSITION_ARB e
 *   GL_PROGRAM_ERROR_STRING_ARB.
 */
#include "gl_int.h"
#include "gl_proto.h"

int snprintf(char *b, size_t n, const char *fmt, ...);
gl_prog_t *gl_glsl_build(gl_ctx_t *C, const char *vs, const char *fs, const char *const *anames, const int *alocs, int na, char **log);
void gl_glsl_free(gl_prog_t *p);
void gl_glsl_set(gl_prog_t *p, const char *name, int idx, const float *v, int n);
void gl_glsl_sampler_unit(gl_prog_t *p, const char *name, int unit);

#define NENV 256
#define NLOC 256
typedef struct arbp {
    u32 name;
    GLenum target;
    char *src;
    int srclen;
    char *glsl;                /* tradução */
    float local[NLOC][4];
    u32 tex_used;              /* unidades usadas por TEX (fp) */
    int tex_tgt[16];           /* alvo por unidade: 1D 2D 3D CUBE RECT */
    int ninstr, ntemps, nparams, nattribs;
    int version;
} arbp_t;
typedef struct {
    gl_names_t names;
    arbp_t *bound[2];          /* 0 vértice, 1 fragmento */
    float env[2][NENV][4];
    int err_pos;
    char err[256];
    /* cache do par ligado */
    arbp_t *cv, *cf;
    int cver[2];
    gl_prog_t *combo;
    int gen;
} arbs_t;
static arbs_t *st(gl_ctx_t *C) {
    if (!C->arb_state) C->arb_state = calloc(1, sizeof(arbs_t));
    return C->arb_state;
}
static int tslot(GLenum t) { return t == GL_VERTEX_PROGRAM_ARB ? 0 : t == GL_FRAGMENT_PROGRAM_ARB ? 1 : -1; }

/* ================================================================ texto de saída */
typedef struct { char *p; u64 n, cap; } ob_t;
static void ob(ob_t *o, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    extern int vsnprintf(char *, size_t, const char *, va_list);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof tmp) n = sizeof tmp - 1;
    if (o->n + (u64)n + 1 > o->cap) { u64 c = o->cap ? o->cap * 2 : 8192; while (c < o->n + (u64)n + 1) c *= 2; char *x = realloc(o->p, c); if (!x) return; o->p = x; o->cap = c; }
    memcpy(o->p + o->n, tmp, (size_t)n);
    o->n += (u64)n;
    o->p[o->n] = 0;
}

/* ================================================================ léxico */
typedef struct { int k; const char *p; int n, pos; double v; } tk_t;   /* k: 'i' id, 'n' número, 'p' pontuação, 0 fim */
typedef struct {
    const char *s; int len, pos;
    tk_t t;
    int fp;
    char err[200]; int errpos;
} lx_t;
static void next(lx_t *L) {
    const char *s = L->s;
    int i = L->pos;
    for (;;) {
        while (i < L->len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
        if (i < L->len && s[i] == '#') { while (i < L->len && s[i] != '\n') i++; continue; }
        break;
    }
    tk_t t = {0, s + i, 0, i, 0};
    if (i >= L->len) { L->t = t; L->pos = i; return; }
    char c = s[i];
    int dim = c >= '1' && c <= '3' && i + 1 < L->len && s[i + 1] == 'D' && !(i + 2 < L->len && ((s[i + 2] >= 'a' && s[i + 2] <= 'z') || (s[i + 2] >= 'A' && s[i + 2] <= 'Z') || s[i + 2] == '_'));
    if (dim) { t.k = 'i'; t.n = 2; L->pos = i + 2; L->t = t; return; }   /* alvos 1D, 2D, 3D */
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$') {
        int j = i;
        while (j < L->len && ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= '0' && s[j] <= '9') || s[j] == '_' || s[j] == '$')) j++;
        t.k = 'i';
        t.n = j - i;
        L->pos = j;
    } else if ((c >= '0' && c <= '9') || (c == '.' && i + 1 < L->len && s[i + 1] >= '0' && s[i + 1] <= '9')) {
        char *e;
        t.v = strtod(s + i, &e);
        t.k = 'n';
        t.n = (int)(e - (s + i));
        L->pos = i + t.n;
    } else {
        t.k = 'p';
        t.n = 1;
        if (c == '.' && i + 1 < L->len && s[i + 1] == '.') t.n = 2;
        L->pos = i + t.n;
    }
    L->t = t;
}
static int is(lx_t *L, const char *w) { int n = (int)strlen(w); return L->t.n == n && !memcmp(L->t.p, w, (size_t)n) && L->t.k; }
static int fail(lx_t *L, const char *m) { if (!L->err[0]) { snprintf(L->err, sizeof L->err, "%s (posição %d)", m, L->t.pos); L->errpos = L->t.pos; } return 0; }
static int accept(lx_t *L, const char *w) { if (is(L, w)) { next(L); return 1; } return 0; }
static int expect(lx_t *L, const char *w) { if (accept(L, w)) return 1; char m[64]; snprintf(m, sizeof m, "esperava '%s'", w); return fail(L, m); }
static int ident(lx_t *L, char *out, int cap) {
    if (L->t.k != 'i') return fail(L, "esperava um nome");
    int n = L->t.n < cap - 1 ? L->t.n : cap - 1;
    memcpy(out, L->t.p, (size_t)n);
    out[n] = 0;
    next(L);
    return 1;
}
static int number(lx_t *L, double *v) {
    int neg = 0;
    if (accept(L, "-")) neg = 1;
    else accept(L, "+");
    if (L->t.k != 'n') return fail(L, "esperava um número");
    *v = neg ? -L->t.v : L->t.v;
    next(L);
    return 1;
}
static int integer(lx_t *L, int *v) { double d; if (!number(L, &d)) return 0; *v = (int)d; return 1; }

/* ================================================================ símbolos */
enum { SY_ATTRIB = 1, SY_PARAM, SY_PARAMARR, SY_TEMP, SY_ADDR, SY_OUTPUT };
typedef struct { char name[64]; int kind; char expr[192]; int arrn; } sym_t;
typedef struct {
    lx_t L;
    ob_t decl, pre, body;
    sym_t sy[512];
    int nsy;
    int fp;
    int posinv, fogmode;
    arbp_t *P;
    int ntemp, ninstr;
    int uses_attr[GLX_ATTR];
    int writes_depth;
} pr_t;
static sym_t *sym(pr_t *R, const char *n) { for (int i = R->nsy - 1; i >= 0; i--) if (!strcmp(R->sy[i].name, n)) return &R->sy[i]; return 0; }
static sym_t *sym_add(pr_t *R, const char *n, int kind) {
    if (R->nsy >= 512) { fail(&R->L, "nomes demais"); return 0; }
    if (sym(R, n)) { fail(&R->L, "nome repetido"); return 0; }
    sym_t *s = &R->sy[R->nsy++];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", n);
    s->kind = kind;
    return s;
}

/* ---- vínculos de estado: devolve a expressão GLSL de um vec4 (ou a linha i de uma matriz) */
static int idx_opt(lx_t *L, int *v, int dflt) {
    *v = dflt;
    if (!accept(L, "[")) return 1;
    if (!integer(L, v)) return 0;
    return expect(L, "]");
}
static void row_expr(char *out, int cap, const char *mat, int inv, int tr, int row) {
    /* linha i da matriz M (coluna maior no GLSL): vec4(M[0][i], M[1][i], M[2][i], M[3][i]); com transposta é a coluna */
    char m[96];
    if (inv && tr) snprintf(m, sizeof m, "%sInverseTranspose", mat);
    else if (inv) snprintf(m, sizeof m, "%sInverse", mat);
    else if (tr) snprintf(m, sizeof m, "%sTranspose", mat);
    else snprintf(m, sizeof m, "%s", mat);
    snprintf(out, (size_t)cap, "vec4(%s[0][%d], %s[1][%d], %s[2][%d], %s[3][%d])", m, row, m, row, m, row, m, row);
}
/* state.matrix.X[.inverse|.transpose|.invtrans][.row[a..b]]: preenche linhas; devolve quantas */
static int matrix_rows(pr_t *R, char rows[4][192]) {
    lx_t *L = &R->L;
    char mat[96];
    int unit = 0;
    if (accept(L, "mvp")) snprintf(mat, sizeof mat, "gl_ModelViewProjectionMatrix");
    else if (accept(L, "modelview")) { if (!idx_opt(L, &unit, 0)) return 0; snprintf(mat, sizeof mat, "gl_ModelViewMatrix"); }
    else if (accept(L, "projection")) snprintf(mat, sizeof mat, "gl_ProjectionMatrix");
    else if (accept(L, "texture")) { if (!idx_opt(L, &unit, 0)) return 0; snprintf(mat, sizeof mat, "gl_TextureMatrix[%d]", unit); }
    else if (accept(L, "palette") || accept(L, "program")) { int k; if (!idx_opt(L, &k, 0)) return 0; snprintf(mat, sizeof mat, "__ident"); }
    else return fail(L, "matriz desconhecida");
    int inv = 0, tr = 0, r0 = 0, r1 = 3;
    while (accept(L, ".")) {
        if (accept(L, "inverse")) inv = 1;
        else if (accept(L, "transpose")) tr = 1;
        else if (accept(L, "invtrans")) { inv = 1; tr = 1; }
        else if (accept(L, "row")) {
            if (!expect(L, "[") || !integer(L, &r0)) return 0;
            r1 = r0;
            if (accept(L, "..") && !integer(L, &r1)) return 0;
            if (!expect(L, "]")) return 0;
        } else return fail(L, "sufixo de matriz desconhecido");
    }
    if (!strncmp(mat, "gl_TextureMatrix[", 17)) {
        char base[96];
        snprintf(base, sizeof base, "gl_TextureMatrix%s[%d]", inv && tr ? "InverseTranspose" : inv ? "Inverse" : tr ? "Transpose" : "", unit);
        int n = 0;
        for (int r = r0; r <= r1 && r < 4; r++, n++) snprintf(rows[n], 192, "vec4(%s[0][%d], %s[1][%d], %s[2][%d], %s[3][%d])", base, r, base, r, base, r, base, r);
        return n;
    }
    if (!strcmp(mat, "__ident")) { int n = 0; for (int r = r0; r <= r1 && r < 4; r++, n++) snprintf(rows[n], 192, "vec4(%s)", r == 0 ? "1,0,0,0" : r == 1 ? "0,1,0,0" : r == 2 ? "0,0,1,0" : "0,0,0,1"); return n; }
    int n = 0;
    for (int r = r0; r <= r1 && r < 4; r++, n++) row_expr(rows[n], 192, mat, inv, tr, r);
    return n;
}
static int state_vec(pr_t *R, char *out, int cap) {
    lx_t *L = &R->L;
    if (!expect(L, ".")) return 0;
    int i = 0;
#define S(...) do { snprintf(out, (size_t)cap, __VA_ARGS__); return 1; } while (0)
    if (accept(L, "light")) {
        if (!idx_opt(L, &i, 0) || !expect(L, ".")) return 0;
        if (accept(L, "ambient")) S("gl_LightSource[%d].ambient", i);
        if (accept(L, "diffuse")) S("gl_LightSource[%d].diffuse", i);
        if (accept(L, "specular")) S("gl_LightSource[%d].specular", i);
        if (accept(L, "position")) S("gl_LightSource[%d].position", i);
        if (accept(L, "half")) S("gl_LightSource[%d].halfVector", i);
        if (accept(L, "attenuation")) S("vec4(gl_LightSource[%d].constantAttenuation, gl_LightSource[%d].linearAttenuation, gl_LightSource[%d].quadraticAttenuation, gl_LightSource[%d].spotExponent)", i, i, i, i);
        if (accept(L, "spot")) { if (!expect(L, ".") || !expect(L, "direction")) return 0; S("vec4(gl_LightSource[%d].spotDirection, gl_LightSource[%d].spotCosCutoff)", i, i); }
        return fail(L, "propriedade de luz desconhecida");
    }
    if (accept(L, "lightprod")) {
        if (!idx_opt(L, &i, 0) || !expect(L, ".")) return 0;
        const char *side = "Front";
        if (accept(L, "back")) { side = "Back"; if (!expect(L, ".")) return 0; }
        else if (accept(L, "front")) { if (!expect(L, ".")) return 0; }
        if (accept(L, "ambient")) S("gl_%sLightProduct[%d].ambient", side, i);
        if (accept(L, "diffuse")) S("gl_%sLightProduct[%d].diffuse", side, i);
        if (accept(L, "specular")) S("gl_%sLightProduct[%d].specular", side, i);
        return fail(L, "propriedade de lightprod desconhecida");
    }
    if (accept(L, "lightmodel")) {
        if (!expect(L, ".")) return 0;
        if (accept(L, "ambient")) S("gl_LightModel.ambient");
        const char *side = "Front";
        if (accept(L, "back")) { side = "Back"; if (!expect(L, ".")) return 0; }
        else if (accept(L, "front")) { if (!expect(L, ".")) return 0; }
        if (accept(L, "scenecolor")) S("gl_%sLightModelProduct.sceneColor", side);
        return fail(L, "propriedade de lightmodel desconhecida");
    }
    if (accept(L, "material")) {
        if (!expect(L, ".")) return 0;
        const char *side = "Front";
        if (accept(L, "back")) { side = "Back"; if (!expect(L, ".")) return 0; }
        else if (accept(L, "front")) { if (!expect(L, ".")) return 0; }
        if (accept(L, "ambient")) S("gl_%sMaterial.ambient", side);
        if (accept(L, "diffuse")) S("gl_%sMaterial.diffuse", side);
        if (accept(L, "specular")) S("gl_%sMaterial.specular", side);
        if (accept(L, "emission")) S("gl_%sMaterial.emission", side);
        if (accept(L, "shininess")) S("vec4(gl_%sMaterial.shininess, 0.0, 0.0, 1.0)", side);
        return fail(L, "propriedade de material desconhecida");
    }
    if (accept(L, "fog")) {
        if (!expect(L, ".")) return 0;
        if (accept(L, "color")) S("gl_Fog.color");
        if (accept(L, "params")) S("vec4(gl_Fog.density, gl_Fog.start, gl_Fog.end, gl_Fog.scale)");
        return fail(L, "propriedade de fog desconhecida");
    }
    if (accept(L, "clip")) { if (!idx_opt(L, &i, 0) || !expect(L, ".") || !expect(L, "plane")) return 0; S("gl_ClipPlane[%d]", i); }
    if (accept(L, "point")) {
        if (!expect(L, ".")) return 0;
        if (accept(L, "size")) S("vec4(gl_Point.size, gl_Point.sizeMin, gl_Point.sizeMax, gl_Point.fadeThresholdSize)");
        if (accept(L, "attenuation")) S("vec4(gl_Point.distanceConstantAttenuation, gl_Point.distanceLinearAttenuation, gl_Point.distanceQuadraticAttenuation, 1.0)");
        return fail(L, "propriedade de ponto desconhecida");
    }
    if (accept(L, "texgen")) {
        if (!idx_opt(L, &i, 0) || !expect(L, ".")) return 0;
        int eye = accept(L, "eye");
        if (!eye && !accept(L, "object")) return fail(L, "texgen sem eye/object");
        if (!expect(L, ".")) return 0;
        char c = L->t.k == 'i' && L->t.n == 1 ? L->t.p[0] : 0;
        next(L);
        const char *cc = c == 's' ? "S" : c == 't' ? "T" : c == 'r' ? "R" : c == 'q' ? "Q" : 0;
        if (!cc) return fail(L, "coordenada de texgen inválida");
        S("gl_%sPlane%s[%d]", eye ? "Eye" : "Object", cc, i);
    }
    if (accept(L, "texenv")) { if (!idx_opt(L, &i, 0) || !expect(L, ".") || !expect(L, "color")) return 0; S("gl_TextureEnvColor[%d]", i); }
    if (accept(L, "depth")) { if (!expect(L, ".") || !expect(L, "range")) return 0; S("vec4(gl_DepthRange.near, gl_DepthRange.far, gl_DepthRange.diff, 1.0)"); }
    if (accept(L, "matrix")) {
        if (!expect(L, ".")) return 0;
        char rows[4][192];
        int n = matrix_rows(R, rows);
        if (n != 1) return fail(L, "matriz inteira onde se esperava um vetor");
        S("%s", rows[0]);
    }
#undef S
    return fail(L, "estado desconhecido");
}
/* constante {x, y, z, w} ou escalar */
static int const_vec(pr_t *R, char *out, int cap) {
    lx_t *L = &R->L;
    double v[4] = {0, 0, 0, 1};
    if (accept(L, "{")) {
        int n = 0;
        do { if (n >= 4) return fail(L, "constante com mais de 4 componentes"); if (!number(L, &v[n++])) return 0; } while (accept(L, ","));
        if (!expect(L, "}")) return 0;
        if (n == 1) v[1] = v[2] = v[3] = v[0];
        else for (int i = n; i < 3; i++) v[i] = 0;
    } else {
        if (!number(L, &v[0])) return 0;
        v[1] = v[2] = v[3] = v[0];
    }
    snprintf(out, (size_t)cap, "vec4(%.9g, %.9g, %.9g, %.9g)", v[0], v[1], v[2], v[3]);
    return 1;
}
/* program.env[n] / program.local[n] (aceita faixa a..b em PARAM de array) */
static int prog_param(pr_t *R, char *out, int cap, int *a, int *b) {
    lx_t *L = &R->L;
    if (!expect(L, ".")) return 0;
    int env = accept(L, "env");
    if (!env && !expect(L, "local")) return 0;
    if (!expect(L, "[") || !integer(L, a)) return 0;
    *b = *a;
    if (accept(L, "..") && !integer(L, b)) return 0;
    if (!expect(L, "]")) return 0;
    if (*a < 0 || *b >= (env ? NENV : NLOC) || *b < *a) return fail(L, "índice de parâmetro fora do limite");
    snprintf(out, (size_t)cap, "%s", env ? "__env" : "__loc");
    return 1;
}
/* vínculo de entrada (vertex.* / fragment.*) */
static int input_binding(pr_t *R, char *out, int cap) {
    lx_t *L = &R->L;
    int i = 0;
    if (!R->fp) {
        if (!expect(L, "vertex") || !expect(L, ".")) return 0;
        if (accept(L, "position")) { snprintf(out, (size_t)cap, "gl_Vertex"); return 1; }
        if (accept(L, "normal")) { snprintf(out, (size_t)cap, "vec4(gl_Normal, 1.0)"); return 1; }
        if (accept(L, "color")) {
            if (accept(L, ".")) { if (accept(L, "secondary")) { snprintf(out, (size_t)cap, "gl_SecondaryColor"); return 1; } if (!expect(L, "primary")) return 0; }
            snprintf(out, (size_t)cap, "gl_Color");
            return 1;
        }
        if (accept(L, "fogcoord")) { snprintf(out, (size_t)cap, "vec4(gl_FogCoord, 0.0, 0.0, 1.0)"); return 1; }
        if (accept(L, "texcoord")) { if (!idx_opt(L, &i, 0)) return 0; if (i < 0 || i > 7) return fail(L, "unidade de textura inválida"); snprintf(out, (size_t)cap, "gl_MultiTexCoord%d", i); return 1; }
        if (accept(L, "attrib")) {
            if (!idx_opt(L, &i, 0)) return 0;
            if (i < 0 || i >= GLX_ATTR) return fail(L, "atributo inválido");
            if (i == 0) snprintf(out, (size_t)cap, "gl_Vertex");
            else { R->uses_attr[i] = 1; snprintf(out, (size_t)cap, "__attr%d", i); }
            return 1;
        }
        if (accept(L, "weight")) { if (!idx_opt(L, &i, 0)) return 0; snprintf(out, (size_t)cap, "vec4(1.0, 0.0, 0.0, 0.0)"); return 1; }
        return fail(L, "vínculo de vértice desconhecido");
    }
    if (!expect(L, "fragment") || !expect(L, ".")) return 0;
    if (accept(L, "color")) {
        if (accept(L, ".")) { if (accept(L, "secondary")) { snprintf(out, (size_t)cap, "gl_SecondaryColor"); return 1; } if (!expect(L, "primary")) return 0; }
        snprintf(out, (size_t)cap, "gl_Color");
        return 1;
    }
    if (accept(L, "texcoord")) { if (!idx_opt(L, &i, 0)) return 0; if (i < 0 || i > 7) return fail(L, "unidade de textura inválida"); snprintf(out, (size_t)cap, "gl_TexCoord[%d]", i); return 1; }
    if (accept(L, "fogcoord")) { snprintf(out, (size_t)cap, "vec4(gl_FogFragCoord, 0.0, 0.0, 1.0)"); return 1; }
    if (accept(L, "position")) { snprintf(out, (size_t)cap, "gl_FragCoord"); return 1; }
    return fail(L, "vínculo de fragmento desconhecido");
}
/* vínculo de saída (result.*): nome do lvalue GLSL */
static int output_binding(pr_t *R, char *out, int cap) {
    lx_t *L = &R->L;
    if (!expect(L, "result") || !expect(L, ".")) return 0;
    int i = 0;
    if (!R->fp) {
        if (accept(L, "position")) { snprintf(out, (size_t)cap, "gl_Position"); return 1; }
        if (accept(L, "fogcoord")) { snprintf(out, (size_t)cap, "__fog"); return 1; }
        if (accept(L, "pointsize")) { snprintf(out, (size_t)cap, "__psize"); return 1; }
        if (accept(L, "texcoord")) { if (!idx_opt(L, &i, 0)) return 0; if (i < 0 || i > 7) return fail(L, "unidade inválida"); snprintf(out, (size_t)cap, "gl_TexCoord[%d]", i); return 1; }
        if (accept(L, "color")) {
            int back = 0, sec = 0;
            while (accept(L, ".")) {
                if (accept(L, "back")) back = 1;
                else if (accept(L, "front")) back = 0;
                else if (accept(L, "secondary")) sec = 1;
                else if (accept(L, "primary")) sec = 0;
                else return fail(L, "cor de resultado inválida");
            }
            snprintf(out, (size_t)cap, "gl_%s%sColor", back ? "Back" : "Front", sec ? "Secondary" : "");
            return 1;
        }
        return fail(L, "resultado de vértice desconhecido");
    }
    if (accept(L, "color")) {
        if (accept(L, "[")) { if (!integer(L, &i) || !expect(L, "]")) return 0; snprintf(out, (size_t)cap, "gl_FragData[%d]", i); return 1; }
        snprintf(out, (size_t)cap, "gl_FragColor");
        return 1;
    }
    if (accept(L, "depth")) { R->writes_depth = 1; snprintf(out, (size_t)cap, "__depth"); return 1; }
    return fail(L, "resultado de fragmento desconhecido");
}

/* ---- declarações */
static int decl_param(pr_t *R) {
    lx_t *L = &R->L;
    char name[64];
    if (!ident(L, name, sizeof name)) return 0;
    int arr = 0, arrn = -1;
    if (accept(L, "[")) { arr = 1; if (!is(L, "]") && !integer(L, &arrn)) return 0; if (!expect(L, "]")) return 0; }
    if (!expect(L, "=")) return 0;
    if (!arr) {
        sym_t *s = sym_add(R, name, SY_PARAM);
        if (!s) return 0;
        char e[192];
        int a, b;
        if (is(L, "state")) { next(L); if (!state_vec(R, e, sizeof e)) return 0; }
        else if (is(L, "program")) { next(L); char base[16]; if (!prog_param(R, base, sizeof base, &a, &b)) return 0; snprintf(e, sizeof e, "%s[%d]", base, a); }
        else if (!const_vec(R, e, sizeof e)) return 0;
        snprintf(s->expr, sizeof s->expr, "%s", e);
        return 1;
    }
    /* array: { item, item, ... } com itens que podem ser faixas */
    sym_t *s = sym_add(R, name, SY_PARAMARR);
    if (!s) return 0;
    int n = 0;
    char items[256][192];
    int single = !is(L, "{");
    if (!single) next(L);
    do {
        if (n >= 256) return fail(L, "array grande demais");
        if (is(L, "state")) {
            next(L);
            if (!expect(L, ".")) return 0;
            if (accept(L, "matrix")) {
                if (!expect(L, ".")) return 0;
                char rows[4][192];
                int k = matrix_rows(R, rows);
                if (!k) return 0;
                for (int j = 0; j < k && n < 256; j++) snprintf(items[n++], 192, "%s", rows[j]);
                continue;
            }
            /* outro estado: volta o ponto */
            L->pos = L->t.pos - 1;
            next(L);
            char e[192];
            if (!state_vec(R, e, sizeof e)) return 0;
            snprintf(items[n++], 192, "%s", e);
        } else if (is(L, "program")) {
            next(L);
            char base[16];
            int a, b;
            if (!prog_param(R, base, sizeof base, &a, &b)) return 0;
            for (int j = a; j <= b && n < 256; j++) snprintf(items[n++], 192, "%s[%d]", base, j);
        } else {
            char e[192];
            if (!const_vec(R, e, sizeof e)) return 0;
            snprintf(items[n++], 192, "%s", e);
        }
    } while (!single && accept(L, ","));
    if (!single && !expect(L, "}")) return 0;
    if (arrn >= 0 && arrn != n) return fail(L, "tamanho do array não confere");
    s->arrn = n;
    ob(&R->pre, "    vec4 %s[%d];\n", name, n);
    for (int j = 0; j < n; j++) ob(&R->pre, "    %s[%d] = %s;\n", name, j, items[j]);
    R->P->nparams += n;
    return 1;
}

/* ---- operandos */
static int swz_comp(char c) { return c == 'x' || c == 'r' ? 0 : c == 'y' || c == 'g' ? 1 : c == 'z' || c == 'b' ? 2 : c == 'w' || c == 'a' ? 3 : -1; }
/* referência a um vetor de origem (sem negação nem swizzle) */
static int src_ref(pr_t *R, char *out, int cap) {
    lx_t *L = &R->L;
    char e[256];
    if (is(L, "vertex") || is(L, "fragment")) { if (!input_binding(R, e, sizeof e)) return 0; snprintf(out, (size_t)cap, "(%s)", e); return 1; }
    if (is(L, "state")) { next(L); if (!state_vec(R, e, sizeof e)) return 0; snprintf(out, (size_t)cap, "(%s)", e); return 1; }
    if (is(L, "program")) { next(L); char base[16]; int a, b; if (!prog_param(R, base, sizeof base, &a, &b)) return 0; snprintf(out, (size_t)cap, "%s[%d]", base, a); return 1; }
    if (is(L, "{") || L->t.k == 'n') { if (!const_vec(R, e, sizeof e)) return 0; snprintf(out, (size_t)cap, "%s", e); return 1; }
    char name[64];
    if (!ident(L, name, sizeof name)) return 0;
    sym_t *s = sym(R, name);
    if (!s) return fail(L, "nome não declarado");
    switch (s->kind) {
    case SY_TEMP: case SY_PARAM: case SY_ATTRIB:
        snprintf(out, (size_t)cap, "%s", s->kind == SY_TEMP ? s->name : s->expr[0] ? s->expr : s->name);
        if (s->kind != SY_TEMP) { char t[256]; snprintf(t, sizeof t, "(%s)", out); snprintf(out, (size_t)cap, "%s", t); }
        return 1;
    case SY_PARAMARR: {
        if (!expect(L, "[")) return 0;
        if (L->t.k == 'n') {
            int i;
            if (!integer(L, &i)) return 0;
            if (i < 0 || i >= s->arrn) return fail(L, "índice fora do array");
            snprintf(out, (size_t)cap, "%s[%d]", s->name, i);
        } else {
            /* endereçamento relativo: a0.x [+ - n] */
            char an[64];
            if (!ident(L, an, sizeof an)) return 0;
            sym_t *a = sym(R, an);
            if (!a || a->kind != SY_ADDR) return fail(L, "esperava um registrador de endereço");
            if (!expect(L, ".") || !expect(L, "x")) return 0;
            int off = 0, neg = 0;
            if (accept(L, "+") || (neg = accept(L, "-"))) { if (!integer(L, &off)) return 0; if (neg) off = -off; }
            snprintf(out, (size_t)cap, "%s[clamp(int(%s.x) + %d, 0, %d)]", s->name, an, off, s->arrn - 1);
        }
        return expect(L, "]");
    }
    case SY_OUTPUT: return fail(L, "OUTPUT não é lido");
    case SY_ADDR: return fail(L, "endereço não é operando de origem");
    }
    return fail(L, "operando inválido");
}
/* operando completo: [-] ref [.swizzle] -> expressão vec4 */
static int src_op(pr_t *R, char *out, int cap, int scalar) {
    lx_t *L = &R->L;
    int neg = 0;
    if (accept(L, "-")) neg = 1;
    else accept(L, "+");
    char ref[300];
    if (!src_ref(R, ref, sizeof ref)) return 0;
    char swz[8] = "xyzw";
    if (is(L, ".")) {
        next(L);
        if (L->t.k != 'i' || (L->t.n != 1 && L->t.n != 4)) return fail(L, "swizzle inválido");
        for (int i = 0; i < L->t.n; i++) if (swz_comp(L->t.p[i]) < 0) return fail(L, "swizzle inválido");
        if (L->t.n == 1) for (int i = 0; i < 4; i++) swz[i] = "xyzw"[swz_comp(L->t.p[0])];
        else for (int i = 0; i < 4; i++) swz[i] = "xyzw"[swz_comp(L->t.p[i])];
        next(L);
    }
    if (scalar) snprintf(out, (size_t)cap, "%s(%s).%c", neg ? "-" : "", ref, swz[0]);
    else snprintf(out, (size_t)cap, "%s(%s).%s", neg ? "-" : "", ref, swz);
    return 1;
}
/* destino: nome [.máscara] */
static int dst_op(pr_t *R, char *lv, int cap, char *mask, int *isaddr) {
    lx_t *L = &R->L;
    *isaddr = 0;
    char e[128];
    if (is(L, "result")) { if (!output_binding(R, e, sizeof e)) return 0; snprintf(lv, (size_t)cap, "%s", e); }
    else {
        char name[64];
        if (!ident(L, name, sizeof name)) return 0;
        sym_t *s = sym(R, name);
        if (!s || (s->kind != SY_TEMP && s->kind != SY_OUTPUT && s->kind != SY_ADDR)) return fail(L, "destino inválido");
        *isaddr = s->kind == SY_ADDR;
        snprintf(lv, (size_t)cap, "%s", s->kind == SY_OUTPUT ? s->expr : s->name);
    }
    snprintf(mask, 5, "xyzw");
    if (is(L, ".")) {
        next(L);
        if (L->t.k != 'i' || L->t.n > 4) return fail(L, "máscara inválida");
        int last = -1, k = 0;
        for (int i = 0; i < L->t.n; i++) {
            int c = swz_comp(L->t.p[i]);
            if (c <= last) return fail(L, "máscara fora de ordem");
            last = c;
            mask[k++] = "xyzw"[c];
        }
        mask[k] = 0;
        next(L);
    }
    return 1;
}

/* ---- instruções */
typedef struct { const char *op; int nsrc, scalar0, scalar1, fp_only, vp_only; } opi_t;
static const opi_t OPS[] = {
    {"ABS", 1, 0, 0, 0, 0}, {"ADD", 2, 0, 0, 0, 0}, {"ARL", 1, 1, 0, 0, 1}, {"DP3", 2, 0, 0, 0, 0}, {"DP4", 2, 0, 0, 0, 0},
    {"DPH", 2, 0, 0, 0, 0}, {"DST", 2, 0, 0, 0, 0}, {"EX2", 1, 1, 0, 0, 0}, {"EXP", 1, 1, 0, 0, 1}, {"FLR", 1, 0, 0, 0, 0},
    {"FRC", 1, 0, 0, 0, 0}, {"LG2", 1, 1, 0, 0, 0}, {"LIT", 1, 0, 0, 0, 0}, {"LOG", 1, 1, 0, 0, 1}, {"MAD", 3, 0, 0, 0, 0},
    {"MAX", 2, 0, 0, 0, 0}, {"MIN", 2, 0, 0, 0, 0}, {"MOV", 1, 0, 0, 0, 0}, {"MUL", 2, 0, 0, 0, 0}, {"POW", 2, 1, 1, 0, 0},
    {"RCP", 1, 1, 0, 0, 0}, {"RSQ", 1, 1, 0, 0, 0}, {"SGE", 2, 0, 0, 0, 0}, {"SLT", 2, 0, 0, 0, 0}, {"SUB", 2, 0, 0, 0, 0},
    {"SWZ", 1, 0, 0, 0, 0}, {"XPD", 2, 0, 0, 0, 0}, {"CMP", 3, 0, 0, 1, 0}, {"COS", 1, 1, 0, 1, 0}, {"SIN", 1, 1, 0, 1, 0},
    {"SCS", 1, 1, 0, 1, 0}, {"LRP", 3, 0, 0, 1, 0}, {"KIL", 1, 0, 0, 1, 0}, {"TEX", 1, 0, 0, 1, 0}, {"TXB", 1, 0, 0, 1, 0},
    {"TXP", 1, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0}};
static int instruction(pr_t *R, const char *opname) {
    lx_t *L = &R->L;
    char base[16];
    snprintf(base, sizeof base, "%s", opname);
    int sat = 0;
    u64 bl = strlen(base);
    if (bl > 4 && !strcmp(base + bl - 4, "_SAT")) { sat = 1; base[bl - 4] = 0; }
    const opi_t *o = 0;
    for (int i = 0; OPS[i].op; i++) if (!strcmp(OPS[i].op, base)) o = &OPS[i];
    if (!o) return fail(L, "instrução desconhecida");
    if ((o->fp_only && !R->fp) || (o->vp_only && R->fp) || (sat && !R->fp)) return fail(L, "instrução não existe neste tipo de programa");
    R->ninstr++;
    char e[1400];
    if (!strcmp(base, "KIL")) {
        char a[400];
        if (!src_op(R, a, sizeof a, 0)) return 0;
        ob(&R->body, "    if (any(lessThan(%s, vec4(0.0)))) discard;\n", a);
        return expect(L, ";");
    }
    char lv[128], mask[5];
    int isaddr;
    if (!dst_op(R, lv, sizeof lv, mask, &isaddr)) return 0;
    char a[400] = "", b[400] = "", c[400] = "";
    if (!expect(L, ",")) return 0;
    if (!strcmp(base, "SWZ")) {
        /* SWZ d, s, c0, c1, c2, c3 com cada ci em {[-]x y z w 0 1} */
        int neg0 = accept(L, "-");
        char ref[300];
        if (!src_ref(R, ref, sizeof ref)) return 0;
        char comp[4][64];
        if (!expect(L, ",")) return 0;
        (void)neg0;
        for (int i = 0; i < 4; i++) {
            int ng = accept(L, "-");
            accept(L, "+");
            if (L->t.k == 'n') { snprintf(comp[i], 64, "%s%g", ng ? "-" : "", L->t.v); next(L); }
            else {
                if (L->t.k != 'i' || L->t.n != 1 || swz_comp(L->t.p[0]) < 0) return fail(L, "componente de SWZ inválido");
                snprintf(comp[i], 64, "%s(%s).%c", ng ? "-" : "", ref, "xyzw"[swz_comp(L->t.p[0])]);
                next(L);
            }
            if (i < 3 && !expect(L, ",")) return 0;
        }
        snprintf(e, sizeof e, "vec4(%s, %s, %s, %s)", comp[0], comp[1], comp[2], comp[3]);
        if (neg0) { char t[1400]; snprintf(t, sizeof t, "-(%s)", e); snprintf(e, sizeof e, "%s", t); }
    } else if (!strcmp(base, "TEX") || !strcmp(base, "TXB") || !strcmp(base, "TXP")) {
        if (!src_op(R, a, sizeof a, 0) || !expect(L, ",") || !expect(L, "texture")) return 0;
        int unit = 0;
        if (!idx_opt(L, &unit, 0) || !expect(L, ",")) return 0;
        if (unit < 0 || unit > 15) return fail(L, "unidade de textura inválida");
        int tgt;
        if (accept(L, "1D")) tgt = 1;
        else if (accept(L, "2D")) tgt = 2;
        else if (accept(L, "3D")) tgt = 3;
        else if (accept(L, "CUBE")) tgt = 4;
        else if (accept(L, "RECT")) tgt = 5;
        else if (accept(L, "SHADOW1D")) tgt = 6;
        else if (accept(L, "SHADOW2D")) tgt = 7;
        else if (accept(L, "SHADOWRECT")) tgt = 8;
        else return fail(L, "alvo de textura desconhecido");
        if (R->P->tex_used >> unit & 1 && R->P->tex_tgt[unit] != tgt) return fail(L, "a mesma unidade com dois alvos");
        R->P->tex_used |= 1u << unit;
        R->P->tex_tgt[unit] = tgt;
        static const char *sty[9] = {"", "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler2DRect", "sampler1DShadow", "sampler2DShadow", "sampler2DRectShadow"};
        char sn[32];
        snprintf(sn, sizeof sn, "__tex%d", unit);
        (void)sty;
        const char *cs = tgt == 1 ? ".x" : tgt == 3 || tgt == 4 || tgt >= 6 ? ".xyz" : ".xy";
        if (!strcmp(base, "TXP")) {
            if (tgt == 4) snprintf(e, sizeof e, "textureCube(%s, (%s).xyz)", sn, a);
            else if (tgt >= 6) snprintf(e, sizeof e, "vec4(texture(%s, (%s).xyz / (%s).w))", sn, a, a);
            else snprintf(e, sizeof e, "texture(%s, (%s)%s / (%s).w)", sn, a, cs, a);
        } else if (!strcmp(base, "TXB")) {
            if (tgt >= 5) snprintf(e, sizeof e, "vec4(texture(%s, (%s)%s))", sn, a, cs);
            else snprintf(e, sizeof e, "texture(%s, (%s)%s, (%s).w)", sn, a, cs, a);
        } else {
            if (tgt >= 6) snprintf(e, sizeof e, "vec4(texture(%s, (%s).xyz))", sn, a);
            else snprintf(e, sizeof e, "texture(%s, (%s)%s)", sn, a, cs);
        }
    } else {
        if (!src_op(R, a, sizeof a, o->scalar0)) return 0;
        if (o->nsrc >= 2) { if (!expect(L, ",") || !src_op(R, b, sizeof b, o->scalar1)) return 0; }
        if (o->nsrc >= 3) { if (!expect(L, ",") || !src_op(R, c, sizeof c, 0)) return 0; }
#define E(...) snprintf(e, sizeof e, __VA_ARGS__)
        if (!strcmp(base, "ABS")) E("abs(%s)", a);
        else if (!strcmp(base, "ADD")) E("(%s + %s)", a, b);
        else if (!strcmp(base, "ARL")) E("vec4(floor(%s))", a);
        else if (!strcmp(base, "DP3")) E("vec4(dot((%s).xyz, (%s).xyz))", a, b);
        else if (!strcmp(base, "DP4")) E("vec4(dot(%s, %s))", a, b);
        else if (!strcmp(base, "DPH")) E("vec4(dot((%s).xyz, (%s).xyz) + (%s).w)", a, b, b);
        else if (!strcmp(base, "DST")) E("vec4(1.0, (%s).y * (%s).y, (%s).z, (%s).w)", a, b, a, b);
        else if (!strcmp(base, "EX2")) E("vec4(exp2(%s))", a);
        else if (!strcmp(base, "EXP")) E("vec4(exp2(floor(%s)), fract(%s), exp2(%s), 1.0)", a, a, a);
        else if (!strcmp(base, "FLR")) E("floor(%s)", a);
        else if (!strcmp(base, "FRC")) E("fract(%s)", a);
        else if (!strcmp(base, "LG2")) E("vec4(log2(%s))", a);
        else if (!strcmp(base, "LIT"))
            E("__lit(%s)", a);
        else if (!strcmp(base, "LOG")) E("vec4(floor(log2(abs(%s))), abs(%s) / exp2(floor(log2(abs(%s)))), log2(abs(%s)), 1.0)", a, a, a, a);
        else if (!strcmp(base, "MAD")) E("(%s * %s + %s)", a, b, c);
        else if (!strcmp(base, "MAX")) E("max(%s, %s)", a, b);
        else if (!strcmp(base, "MIN")) E("min(%s, %s)", a, b);
        else if (!strcmp(base, "MOV")) E("(%s)", a);
        else if (!strcmp(base, "MUL")) E("(%s * %s)", a, b);
        else if (!strcmp(base, "POW")) E("vec4(pow(%s, %s))", a, b);
        else if (!strcmp(base, "RCP")) E("vec4(1.0 / (%s))", a);
        else if (!strcmp(base, "RSQ")) E("vec4(inversesqrt(abs(%s)))", a);
        else if (!strcmp(base, "SGE")) E("vec4(greaterThanEqual(%s, %s))", a, b);
        else if (!strcmp(base, "SLT")) E("vec4(lessThan(%s, %s))", a, b);
        else if (!strcmp(base, "SUB")) E("(%s - %s)", a, b);
        else if (!strcmp(base, "XPD")) E("vec4(cross((%s).xyz, (%s).xyz), 0.0)", a, b);
        else if (!strcmp(base, "CMP")) E("mix(%s, %s, vec4(lessThan(%s, vec4(0.0))))", c, b, a);
        else if (!strcmp(base, "COS")) E("vec4(cos(%s))", a);
        else if (!strcmp(base, "SIN")) E("vec4(sin(%s))", a);
        else if (!strcmp(base, "SCS")) E("vec4(cos(%s), sin(%s), 0.0, 0.0)", a, a);
        else if (!strcmp(base, "LRP")) E("(%s * %s + (vec4(1.0) - %s) * %s)", a, b, a, c);
#undef E
    }
    if (sat) { char t[1500]; snprintf(t, sizeof t, "clamp(%s, 0.0, 1.0)", e); snprintf(e, sizeof e, "%s", t); }
    if (isaddr) ob(&R->body, "    %s.x = (%s).x;\n", lv, e);
    else if (!strcmp(lv, "__fog") || !strcmp(lv, "__psize") || !strcmp(lv, "__depth")) ob(&R->body, "    %s.%s = (%s).%s;\n", lv, mask, e, mask);
    else ob(&R->body, "    %s.%s = (%s).%s;\n", lv, mask, e, mask);
    return expect(L, ";");
}
static int statement(pr_t *R) {
    lx_t *L = &R->L;
    char w[64];
    if (!ident(L, w, sizeof w)) return 0;
    if (!strcmp(w, "OPTION")) {
        char o[64];
        if (!ident(L, o, sizeof o)) return 0;
        if (!strcmp(o, "ARB_position_invariant")) R->posinv = 1;
        else if (!strcmp(o, "ARB_fog_linear")) R->fogmode = 1;
        else if (!strcmp(o, "ARB_fog_exp")) R->fogmode = 2;
        else if (!strcmp(o, "ARB_fog_exp2")) R->fogmode = 3;
        /* ARB_precision_hint_*: o renderizador usa float sempre */
        return expect(L, ";");
    }
    if (!strcmp(w, "TEMP") || !strcmp(w, "ADDRESS")) {
        do {
            char n[64];
            if (!ident(L, n, sizeof n)) return 0;
            sym_t *s = sym_add(R, n, w[0] == 'T' ? SY_TEMP : SY_ADDR);
            if (!s) return 0;
            ob(&R->decl, "vec4 %s = vec4(0.0);\n", n);
            if (w[0] == 'T') R->ntemp++;
        } while (accept(L, ","));
        return expect(L, ";");
    }
    if (!strcmp(w, "ATTRIB")) {
        char n[64], e[192];
        if (!ident(L, n, sizeof n) || !expect(L, "=") || !input_binding(R, e, sizeof e)) return 0;
        sym_t *s = sym_add(R, n, SY_ATTRIB);
        if (!s) return 0;
        snprintf(s->expr, sizeof s->expr, "%s", e);
        R->P->nattribs++;
        return expect(L, ";");
    }
    if (!strcmp(w, "PARAM")) { if (!decl_param(R)) return 0; return expect(L, ";"); }
    if (!strcmp(w, "OUTPUT")) {
        char n[64], e[128];
        if (!ident(L, n, sizeof n) || !expect(L, "=") || !output_binding(R, e, sizeof e)) return 0;
        sym_t *s = sym_add(R, n, SY_OUTPUT);
        if (!s) return 0;
        snprintf(s->expr, sizeof s->expr, "%s", e);
        return expect(L, ";");
    }
    if (!strcmp(w, "ALIAS")) {
        char n[64], t[64];
        if (!ident(L, n, sizeof n) || !expect(L, "=") || !ident(L, t, sizeof t)) return 0;
        sym_t *o = sym(R, t);
        if (!o) return fail(L, "ALIAS de nome não declarado");
        sym_t *s = sym_add(R, n, o->kind);
        if (!s) return 0;
        snprintf(s->expr, sizeof s->expr, "%s", o->kind == SY_TEMP || o->kind == SY_ADDR || o->kind == SY_PARAMARR ? o->name : o->expr);
        if (o->kind == SY_TEMP || o->kind == SY_ADDR || o->kind == SY_PARAMARR) { snprintf(s->name, sizeof s->name, "%s", n); ob(&R->decl, "#define %s %s\n", n, o->name); }
        s->arrn = o->arrn;
        return expect(L, ";");
    }
    return instruction(R, w);
}
/* traduz o texto; devolve GLSL ou 0 com o erro */
static char *translate(const char *src, int len, int fp, arbp_t *P, char *err, int errn, int *errpos) {
    pr_t *R = calloc(1, sizeof *R);
    if (!R) return 0;
    R->fp = fp;
    R->P = P;
    lx_t *L = &R->L;
    L->s = src;
    L->len = len;
    const char *hdr = fp ? "!!ARBfp1.0" : "!!ARBvp1.0";
    char *out = 0;
    if (len < 10 || memcmp(src, hdr, 10)) { snprintf(err, (size_t)errn, "cabeçalho %s ausente", hdr); *errpos = 0; free(R); return 0; }
    L->pos = 10;
    next(L);
    P->tex_used = 0;
    P->nparams = P->nattribs = 0;
    while (L->t.k && !is(L, "END")) if (!statement(R)) break;
    if (!L->err[0] && !is(L, "END")) fail(L, "END ausente");
    if (L->err[0]) { snprintf(err, (size_t)errn, "%s", L->err); *errpos = L->errpos; goto done; }
    ob_t g = {0};
    ob(&g, "#version 120\nuniform vec4 __env[%d];\nuniform vec4 __loc[%d];\n", NENV, NLOC);
    if (!fp) for (int i = 1; i < GLX_ATTR; i++) if (R->uses_attr[i]) ob(&g, "attribute vec4 __attr%d;\n", i);
    static const char *sty[9] = {"", "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler2DRect", "sampler1DShadow", "sampler2DShadow", "sampler2DRectShadow"};
    for (int u = 0; u < 16; u++) if (P->tex_used >> u & 1) ob(&g, "uniform %s __tex%d;\n", sty[P->tex_tgt[u]], u);
    ob(&g, "vec4 __lit(vec4 s) { float x = max(s.x, 0.0), y = max(s.y, 0.0), p = clamp(s.w, -128.0, 128.0);"
           " return vec4(1.0, x, x > 0.0 ? pow(y, p) : 0.0, 1.0); }\n");
    ob(&g, "%s", R->decl.p ? R->decl.p : "");
    ob(&g, "void main() {\n");
    if (!fp) ob(&g, "    vec4 __fog = vec4(0.0), __psize = vec4(1.0);\n");
    else ob(&g, "    vec4 __depth = vec4(gl_FragCoord.z);\n");
    ob(&g, "%s", R->pre.p ? R->pre.p : "");
    ob(&g, "%s", R->body.p ? R->body.p : "");
    if (!fp) {
        if (R->posinv) ob(&g, "    gl_Position = ftransform();\n");
        ob(&g, "    gl_FogFragCoord = __fog.x;\n    gl_PointSize = __psize.x;\n");
    } else {
        if (R->writes_depth) ob(&g, "    gl_FragDepth = __depth.z;\n");
        if (R->fogmode) {
            /* fog do programa: fator pela coordenada interpolada */
            ob(&g, "    float __z = gl_FogFragCoord, __f = %s;\n",
               R->fogmode == 1 ? "(gl_Fog.end - __z) * gl_Fog.scale" : R->fogmode == 2 ? "exp(-gl_Fog.density * __z)" : "exp(-(gl_Fog.density * __z) * (gl_Fog.density * __z))");
            ob(&g, "    gl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, clamp(__f, 0.0, 1.0));\n");
        }
    }
    ob(&g, "}\n");
    out = g.p;
    P->ninstr = R->ninstr;
    P->ntemps = R->ntemp;
done:
    free(R->decl.p);
    free(R->pre.p);
    free(R->body.p);
    free(R);
    return out;
}

/* ================================================================ API */
void glGenProgramsARB(GLsizei n, GLuint *ids) { GLC(); arbs_t *S = st(C); if (!S) GLERR(GL_OUT_OF_MEMORY, ); if (n < 0) GLERR(GL_INVALID_VALUE, ); for (int i = 0; i < n; i++) ids[i] = gl_names_gen(&S->names); }
static void prog_free(arbp_t *p) { if (p) { free(p->src); free(p->glsl); free(p); } }
void glDeleteProgramsARB(GLsizei n, const GLuint *ids) {
    GLC();
    arbs_t *S = st(C);
    if (!S) return;
    for (int i = 0; i < n; i++) {
        arbp_t *p = gl_names_get(&S->names, ids[i]);
        for (int k = 0; k < 2; k++) if (S->bound[k] == p) S->bound[k] = 0;
        if (S->cv == p || S->cf == p) { S->cv = S->cf = 0; gl_glsl_free(S->combo); S->combo = 0; }
        gl_names_del(&S->names, ids[i]);
        prog_free(p);
    }
}
GLboolean glIsProgramARB(GLuint id) { GLC(0); arbs_t *S = st(C); return S && gl_names_get(&S->names, id) != 0; }
void glBindProgramARB(GLenum target, GLuint id) {
    GLC();
    int k = tslot(target);
    if (k < 0) GLERR(GL_INVALID_ENUM, );
    arbs_t *S = st(C);
    if (!S) GLERR(GL_OUT_OF_MEMORY, );
    if (!id) { S->bound[k] = 0; return; }
    arbp_t *p = gl_names_get(&S->names, id);
    if (!p) {
        p = calloc(1, sizeof *p);
        if (!p) GLERR(GL_OUT_OF_MEMORY, );
        p->name = id;
        p->target = target;
        gl_names_set(&S->names, id, p);
    }
    if (p->target != target) GLERR(GL_INVALID_OPERATION, );
    S->bound[k] = p;
}
void glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *str) {
    GLC();
    int k = tslot(target);
    if (k < 0) GLERR(GL_INVALID_ENUM, );
    if (format != GL_PROGRAM_FORMAT_ASCII_ARB) GLERR(GL_INVALID_ENUM, );
    arbs_t *S = st(C);
    arbp_t *p = S ? S->bound[k] : 0;
    if (!p) GLERR(GL_INVALID_OPERATION, );
    char err[256] = "";
    int pos = -1;
    char *g = translate(str, len, k, p, err, sizeof err, &pos);
    if (g) {
        /* confere que o GLSL gerado liga com o estágio padrão do outro lado */
        char *log = 0;
        gl_prog_t *t = gl_glsl_build(C, k ? 0 : g, k ? g : 0, 0, 0, 0, &log);
        int ok = t && t->linked;
        if (!ok) { snprintf(err, sizeof err, "%s", log && log[0] ? log : "programa inválido"); pos = 0; free(g); g = 0; }
        free(log);
        gl_glsl_free(t);
    }
    S->err_pos = g ? -1 : pos;
    snprintf(S->err, sizeof S->err, "%s", g ? "" : err);
    if (!g) GLERR(GL_INVALID_OPERATION, );
    free(p->src);
    free(p->glsl);
    p->src = malloc((size_t)len + 1);
    if (p->src) { memcpy(p->src, str, (size_t)len); p->src[len] = 0; }
    p->srclen = len;
    p->glsl = g;
    p->version++;
    p->target = target;
}
static void param4(GLenum target, GLuint idx, int local, const float *v) {
    GLC();
    int k = tslot(target);
    if (k < 0) GLERR(GL_INVALID_ENUM, );
    arbs_t *S = st(C);
    if (!S) GLERR(GL_OUT_OF_MEMORY, );
    if (idx >= (local ? NLOC : NENV)) GLERR(GL_INVALID_VALUE, );
    if (local) { if (!S->bound[k]) GLERR(GL_INVALID_OPERATION, ); memcpy(S->bound[k]->local[idx], v, 16); }
    else memcpy(S->env[k][idx], v, 16);
}
void glProgramEnvParameter4fARB(GLenum t, GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { float v[4] = {x, y, z, w}; param4(t, i, 0, v); }
void glProgramEnvParameter4dARB(GLenum t, GLuint i, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { float v[4] = {(float)x, (float)y, (float)z, (float)w}; param4(t, i, 0, v); }
void glProgramEnvParameter4fvARB(GLenum t, GLuint i, const GLfloat *p) { param4(t, i, 0, p); }
void glProgramEnvParameter4dvARB(GLenum t, GLuint i, const GLdouble *p) { float v[4] = {(float)p[0], (float)p[1], (float)p[2], (float)p[3]}; param4(t, i, 0, v); }
void glProgramLocalParameter4fARB(GLenum t, GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { float v[4] = {x, y, z, w}; param4(t, i, 1, v); }
void glProgramLocalParameter4dARB(GLenum t, GLuint i, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { float v[4] = {(float)x, (float)y, (float)z, (float)w}; param4(t, i, 1, v); }
void glProgramLocalParameter4fvARB(GLenum t, GLuint i, const GLfloat *p) { param4(t, i, 1, p); }
void glProgramLocalParameter4dvARB(GLenum t, GLuint i, const GLdouble *p) { float v[4] = {(float)p[0], (float)p[1], (float)p[2], (float)p[3]}; param4(t, i, 1, v); }
void glProgramEnvParameters4fvEXT(GLenum t, GLuint i, GLsizei n, const GLfloat *p) { for (int k = 0; k < n; k++) param4(t, i + (GLuint)k, 0, p + 4 * k); }
void glProgramLocalParameters4fvEXT(GLenum t, GLuint i, GLsizei n, const GLfloat *p) { for (int k = 0; k < n; k++) param4(t, i + (GLuint)k, 1, p + 4 * k); }
static void getparam(GLenum target, GLuint idx, int local, float *out) {
    GLC();
    int k = tslot(target);
    if (k < 0) GLERR(GL_INVALID_ENUM, );
    arbs_t *S = st(C);
    if (!S || idx >= (local ? NLOC : NENV)) GLERR(GL_INVALID_VALUE, );
    if (local) { if (!S->bound[k]) GLERR(GL_INVALID_OPERATION, ); memcpy(out, S->bound[k]->local[idx], 16); }
    else memcpy(out, S->env[k][idx], 16);
}
void glGetProgramEnvParameterfvARB(GLenum t, GLuint i, GLfloat *p) { getparam(t, i, 0, p); }
void glGetProgramLocalParameterfvARB(GLenum t, GLuint i, GLfloat *p) { getparam(t, i, 1, p); }
void glGetProgramEnvParameterdvARB(GLenum t, GLuint i, GLdouble *p) { float v[4] = {0}; getparam(t, i, 0, v); for (int k = 0; k < 4; k++) p[k] = v[k]; }
void glGetProgramLocalParameterdvARB(GLenum t, GLuint i, GLdouble *p) { float v[4] = {0}; getparam(t, i, 1, v); for (int k = 0; k < 4; k++) p[k] = v[k]; }
void glGetProgramivARB(GLenum target, GLenum q, GLint *v) {
    GLC();
    int k = tslot(target);
    if (k < 0) GLERR(GL_INVALID_ENUM, );
    arbs_t *S = st(C);
    arbp_t *p = S ? S->bound[k] : 0;
    switch (q) {
    case GL_PROGRAM_LENGTH_ARB: *v = p ? p->srclen : 0; return;
    case GL_PROGRAM_FORMAT_ARB: *v = GL_PROGRAM_FORMAT_ASCII_ARB; return;
    case GL_PROGRAM_BINDING_ARB: *v = p ? (GLint)p->name : 0; return;
    case GL_PROGRAM_INSTRUCTIONS_ARB: case GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB: case GL_PROGRAM_ALU_INSTRUCTIONS_ARB: case GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB: *v = p ? p->ninstr : 0; return;
    case GL_PROGRAM_TEMPORARIES_ARB: case GL_PROGRAM_NATIVE_TEMPORARIES_ARB: *v = p ? p->ntemps : 0; return;
    case GL_PROGRAM_PARAMETERS_ARB: case GL_PROGRAM_NATIVE_PARAMETERS_ARB: *v = p ? p->nparams : 0; return;
    case GL_PROGRAM_ATTRIBS_ARB: case GL_PROGRAM_NATIVE_ATTRIBS_ARB: *v = p ? p->nattribs : 0; return;
    case GL_PROGRAM_TEX_INSTRUCTIONS_ARB: case GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB: case GL_PROGRAM_TEX_INDIRECTIONS_ARB: case GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB: *v = 0; return;
    case GL_PROGRAM_ADDRESS_REGISTERS_ARB: case GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB: *v = 0; return;
    case GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB: *v = 1; return;
    case GL_MAX_PROGRAM_INSTRUCTIONS_ARB: case GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB: case GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB:
    case GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB: case GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB: case GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB:
    case GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB: case GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB: *v = 65536; return;
    case GL_MAX_PROGRAM_TEMPORARIES_ARB: case GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB: *v = 256; return;
    case GL_MAX_PROGRAM_PARAMETERS_ARB: case GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB: *v = 256; return;
    case GL_MAX_PROGRAM_ATTRIBS_ARB: case GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB: *v = GLX_ATTR; return;
    case GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB: case GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB: *v = 1; return;
    case GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB: *v = NLOC; return;
    case GL_MAX_PROGRAM_ENV_PARAMETERS_ARB: *v = NENV; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glGetProgramStringARB(GLenum target, GLenum q, void *str) {
    GLC();
    int k = tslot(target);
    if (k < 0 || q != GL_PROGRAM_STRING_ARB) GLERR(GL_INVALID_ENUM, );
    arbs_t *S = st(C);
    arbp_t *p = S ? S->bound[k] : 0;
    if (p && p->src && str) memcpy(str, p->src, (size_t)p->srclen);
}
int gl_arb_error_pos(gl_ctx_t *C) { arbs_t *S = C->arb_state; return S ? S->err_pos : -1; }
const char *gl_arb_error_string(gl_ctx_t *C) { arbs_t *S = C->arb_state; return S ? S->err : ""; }

/* ---- draw: monta (com cache) o programa do par ligado e escreve env/local e as unidades */
int gl_arb_stage(gl_ctx_t *C) {
    C->arb_prog = 0;
    arbs_t *S = C->arb_state;
    if (!S) return 0;
    arbp_t *v = gl_is_on(C, GL_VERTEX_PROGRAM_ARB) && S->bound[0] && S->bound[0]->glsl ? S->bound[0] : 0;
    arbp_t *f = gl_is_on(C, GL_FRAGMENT_PROGRAM_ARB) && S->bound[1] && S->bound[1]->glsl ? S->bound[1] : 0;
    if (!v && !f) return 0;
    if (!S->combo || S->cv != v || S->cf != f || S->cver[0] != (v ? v->version : 0) || S->cver[1] != (f ? f->version : 0)) {
        gl_glsl_free(S->combo);
        S->combo = 0;
        const char *names[GLX_ATTR];
        int locs[GLX_ATTR], na = 0;
        char nb[GLX_ATTR][16];
        for (int i = 1; i < GLX_ATTR; i++) { snprintf(nb[i], 16, "__attr%d", i); names[na] = nb[i]; locs[na++] = i; }
        S->combo = gl_glsl_build(C, v ? v->glsl : 0, f ? f->glsl : 0, names, locs, na, 0);
        S->cv = v;
        S->cf = f;
        S->cver[0] = v ? v->version : 0;
        S->cver[1] = f ? f->version : 0;
    }
    gl_prog_t *p = S->combo;
    if (!p || !p->linked) { gl_err(C, GL_INVALID_OPERATION); return -1; }
    /* cada estágio recebe os parâmetros do seu alvo */
    extern void gl_glsl_set_stage(gl_prog_t *p, int stage, const char *name, const float *v, int n);
    if (v) { gl_glsl_set_stage(p, 0, "__env", &S->env[0][0][0], 4 * NENV); gl_glsl_set_stage(p, 0, "__loc", &v->local[0][0], 4 * NLOC); }
    if (f) {
        gl_glsl_set_stage(p, 1, "__env", &S->env[1][0][0], 4 * NENV);
        gl_glsl_set_stage(p, 1, "__loc", &f->local[0][0], 4 * NLOC);
        for (int u = 0; u < 16; u++) if (f->tex_used >> u & 1) { char sn[16]; snprintf(sn, sizeof sn, "__tex%d", u); gl_glsl_sampler_unit(p, sn, u); }
    }
    C->arb_prog = p;
    return 1;
}
void gl_arb_free(gl_ctx_t *C) {
    arbs_t *S = C->arb_state;
    if (!S) return;
    for (u32 i = 1; i < S->names.n; i++) prog_free(S->names.v ? S->names.v[i] : 0);
    free(S->names.v);
    free(S->names.used);
    gl_glsl_free(S->combo);
    free(S);
    C->arb_state = 0;
}
