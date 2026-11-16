/* mOS PDF — funções (ISO 32000-1 §7.10): tipo 0 (amostrada, interpolação multilinear), 2 (exponencial),
 * 3 (composta por intervalos) e 4 (calculadora PostScript). Incluído por cgpdf_draw.c. */

enum { PF_MAXIN = 8, PF_MAXOUT = 32 };
typedef struct pdf_fn pdf_fn;
struct pdf_fn {
    int type, m, n;                    /* entradas, saídas */
    double domain[2 * PF_MAXIN], range[2 * PF_MAXOUT];
    int has_range;
    /* 0 */
    int size[PF_MAXIN], bps;
    double encode[2 * PF_MAXIN], decode[2 * PF_MAXOUT];
    const u8 *samples;
    size_t nsamples;
    /* 2 */
    double c0[PF_MAXOUT], c1[PF_MAXOUT], N;
    /* 3 */
    pdf_fn **fns;
    int k;
    double bounds[64], enc3[128];
    /* 4 */
    int *code;
    double *consts;
    int ncode;
};

static double pf_clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static int pf_nums(pdf_doc *pd, pdf_obj *a, double *out, int max) {
    a = pdf_resolve(pd, a);
    if (!a || a->type != PO_ARRAY) return 0;
    int n = (int)a->n < max ? (int)a->n : max;
    for (int i = 0; i < n; i++) if (!pdf_num(pdf_array_get(pd, a, (u32)i), &out[i])) out[i] = 0;
    return n;
}

/* calculadora PostScript: código como lista de inteiros (operador, ou -1 = constante seguida do índice) */
enum { PS_CONST = -1, PS_IF = -2, PS_IFELSE = -3, PS_JMP = -4 };
static const char *const ps_ops[] = {"abs", "add", "atan", "ceiling", "cos", "cvi", "cvr", "div", "exp", "floor", "idiv", "ln", "log", "mod",
                                     "mul", "neg", "round", "sin", "sqrt", "sub", "truncate", "and", "bitshift", "eq", "false", "ge", "gt",
                                     "le", "lt", "ne", "not", "or", "true", "xor", "copy", "dup", "exch", "index", "pop", "roll", 0};
typedef struct { int *c; int n, cap; double *k; int nk, kcap; } ps_buf;
static void psb_put(ps_buf *b, int v) {
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 64; b->c = realloc(b->c, (size_t)b->cap * sizeof(int)); }
    if (b->c) b->c[b->n++] = v;
}
static int psb_const(ps_buf *b, double v) {
    if (b->nk == b->kcap) { b->kcap = b->kcap ? b->kcap * 2 : 32; b->k = realloc(b->k, (size_t)b->kcap * sizeof(double)); }
    if (b->k) b->k[b->nk] = v;
    return b->nk++;
}
/* compila "{ ... }" em código linear; if/ifelse viram saltos */
static int ps_compile(const u8 *s, size_t n, size_t *p, ps_buf *b, int depth) {
    if (depth > 32) return -1;
    while (*p < n) {
        int c = s[*p];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == 0) { (*p)++; continue; }
        if (c == '{') {
            (*p)++;
            /* bloco: o próximo token decide (if/ifelse) */
            psb_put(b, PS_JMP);                         /* salto para depois do bloco, preenchido abaixo */
            int jat = b->n;
            psb_put(b, 0);
            int start = b->n;
            if (ps_compile(s, n, p, b, depth + 1)) return -1;
            b->c[jat] = b->n;
            /* guarda o início do bloco na pilha de compilação: como constante especial */
            psb_put(b, PS_CONST - 100);
            psb_put(b, start);
            continue;
        }
        if (c == '}') { (*p)++; return 0; }
        size_t q = *p;
        while (q < n && !(s[q] == ' ' || s[q] == '\n' || s[q] == '\r' || s[q] == '\t' || s[q] == '{' || s[q] == '}')) q++;
        char w[32];
        size_t L = q - *p < 31 ? q - *p : 31;
        memcpy(w, s + *p, L);
        w[L] = 0;
        *p = q;
        if ((w[0] >= '0' && w[0] <= '9') || w[0] == '-' || w[0] == '+' || w[0] == '.') {
            double v = 0, f = 0.1, sg = 1;
            int i = 0, dot = 0;
            if (w[0] == '-') { sg = -1; i = 1; } else if (w[0] == '+') i = 1;
            for (; w[i]; i++) {
                if (w[i] == '.') dot = 1;
                else if (w[i] >= '0' && w[i] <= '9') { if (dot) { v += (w[i] - '0') * f; f /= 10; } else v = v * 10 + (w[i] - '0'); }
            }
            psb_put(b, PS_CONST);
            psb_put(b, psb_const(b, sg * v));
            continue;
        }
        if (!strcmp(w, "if")) { psb_put(b, PS_IF); continue; }
        if (!strcmp(w, "ifelse")) { psb_put(b, PS_IFELSE); continue; }
        int op = -1;
        for (int i = 0; ps_ops[i]; i++) if (!strcmp(w, ps_ops[i])) { op = i; break; }
        if (op < 0) return -1;
        psb_put(b, op);
    }
    return 0;
}
static double rad(double d) { return d * 3.14159265358979323846 / 180; }
/* executa o código de 'from' até 'to'; pilha de valores com marcador de booleano/inteiro tratado como double */
static int ps_run(const pdf_fn *f, int from, int to, double *st, int *sp, int depth) {
    if (depth > 64) return -1;
    int blocks[4], nb = 0;
    for (int pc = from; pc < to;) {
        int c = f->code[pc++];
        if (c == PS_JMP) { int dst = f->code[pc]; pc = dst; continue; }
        if (c == PS_CONST - 100) { if (nb < 4) blocks[nb++] = f->code[pc]; pc++; continue; }
        if (c == PS_CONST) { if (*sp >= 100) return -1; st[(*sp)++] = f->consts[f->code[pc++]]; continue; }
        if (c == PS_IF || c == PS_IFELSE) {
            /* blocos referenciados: início guardado; o fim é o JMP que o antecede */
            int need = c == PS_IF ? 1 : 2;
            if (nb < need || *sp < 1) return -1;
            double cond = st[--(*sp)];
            int b = c == PS_IF ? blocks[nb - 1] : (cond != 0 ? blocks[nb - 2] : blocks[nb - 1]);
            nb -= need;
            if (c == PS_IF && cond == 0) continue;
            int end = f->code[b - 1];                   /* o JMP do bloco aponta para o fim */
            if (ps_run(f, b, end - 2, st, sp, depth + 1)) return -1;
            continue;
        }
        double a, bb;
        int s = *sp;
#define NEED(k) do { if (s < (k)) return -1; } while (0)
        switch (c) {
        case 0: NEED(1); st[s - 1] = fabs(st[s - 1]); break;
        case 1: NEED(2); st[s - 2] += st[s - 1]; s--; break;
        case 2: NEED(2); a = atan2(st[s - 2], st[s - 1]) * 180 / 3.14159265358979323846; if (a < 0) a += 360; st[s - 2] = a; s--; break;
        case 3: NEED(1); st[s - 1] = ceil(st[s - 1]); break;
        case 4: NEED(1); st[s - 1] = cos(rad(st[s - 1])); break;
        case 5: NEED(1); st[s - 1] = (double)(long)st[s - 1]; break;
        case 6: break;
        case 7: NEED(2); st[s - 2] = st[s - 1] != 0 ? st[s - 2] / st[s - 1] : 0; s--; break;
        case 8: NEED(2); st[s - 2] = pow(st[s - 2], st[s - 1]); s--; break;
        case 9: NEED(1); st[s - 1] = floor(st[s - 1]); break;
        case 10: NEED(2); st[s - 2] = (long)st[s - 1] ? (double)((long)st[s - 2] / (long)st[s - 1]) : 0; s--; break;
        case 11: NEED(1); st[s - 1] = st[s - 1] > 0 ? log(st[s - 1]) : 0; break;
        case 12: NEED(1); st[s - 1] = st[s - 1] > 0 ? log(st[s - 1]) / log(10.0) : 0; break;
        case 13: NEED(2); st[s - 2] = (long)st[s - 1] ? (double)((long)st[s - 2] % (long)st[s - 1]) : 0; s--; break;
        case 14: NEED(2); st[s - 2] *= st[s - 1]; s--; break;
        case 15: NEED(1); st[s - 1] = -st[s - 1]; break;
        case 16: NEED(1); st[s - 1] = floor(st[s - 1] + 0.5); break;
        case 17: NEED(1); st[s - 1] = sin(rad(st[s - 1])); break;
        case 18: NEED(1); st[s - 1] = st[s - 1] > 0 ? sqrt(st[s - 1]) : 0; break;
        case 19: NEED(2); st[s - 2] -= st[s - 1]; s--; break;
        case 20: NEED(1); st[s - 1] = (double)(long)st[s - 1]; break;
        case 21: NEED(2); st[s - 2] = (double)((long)st[s - 2] & (long)st[s - 1]); s--; break;
        case 22: NEED(2); { long v = (long)st[s - 2], k = (long)st[s - 1]; st[s - 2] = (double)(k >= 0 ? v << k : v >> -k); } s--; break;
        case 23: NEED(2); st[s - 2] = st[s - 2] == st[s - 1]; s--; break;
        case 24: if (s >= 100) return -1; st[s++] = 0; break;
        case 25: NEED(2); st[s - 2] = st[s - 2] >= st[s - 1]; s--; break;
        case 26: NEED(2); st[s - 2] = st[s - 2] > st[s - 1]; s--; break;
        case 27: NEED(2); st[s - 2] = st[s - 2] <= st[s - 1]; s--; break;
        case 28: NEED(2); st[s - 2] = st[s - 2] < st[s - 1]; s--; break;
        case 29: NEED(2); st[s - 2] = st[s - 2] != st[s - 1]; s--; break;
        case 30: NEED(1); st[s - 1] = st[s - 1] == 0 || st[s - 1] == 1 ? !st[s - 1] : (double)~(long)st[s - 1]; break;
        case 31: NEED(2); st[s - 2] = (double)((long)st[s - 2] | (long)st[s - 1]); s--; break;
        case 32: if (s >= 100) return -1; st[s++] = 1; break;
        case 33: NEED(2); st[s - 2] = (double)((long)st[s - 2] ^ (long)st[s - 1]); s--; break;
        case 34: { NEED(1); int k = (int)st[--s]; if (k < 0 || k > s || s + k > 100) return -1; for (int i = 0; i < k; i++) st[s + i] = st[s - k + i]; s += k; break; }
        case 35: NEED(1); if (s >= 100) return -1; st[s] = st[s - 1]; s++; break;
        case 36: NEED(2); a = st[s - 1]; st[s - 1] = st[s - 2]; st[s - 2] = a; break;
        case 37: { NEED(1); int k = (int)st[--s]; if (k < 0 || k >= s) return -1; st[s] = st[s - 1 - k]; s++; break; }
        case 38: NEED(1); s--; break;
        case 39: {
            NEED(2);
            int j = (int)st[--s], nn = (int)st[--s];
            if (nn < 0 || nn > s) return -1;
            if (nn) {
                j = ((j % nn) + nn) % nn;
                double tmp[100];
                for (int i = 0; i < nn; i++) tmp[(i + j) % nn] = st[s - nn + i];
                for (int i = 0; i < nn; i++) st[s - nn + i] = tmp[i];
            }
            break;
        }
        default: return -1;
        }
        (void)bb;
        *sp = s;
#undef NEED
    }
    return 0;
}

static pdf_fn *pf_load(pdf_doc *pd, pdf_obj *o, int depth);
static void pf_free(pdf_fn *f) {
    if (!f) return;
    for (int i = 0; i < f->k; i++) pf_free(f->fns[i]);
    free(f->fns);
    free(f->code);
    free(f->consts);
    free(f);
}
static pdf_fn *pf_load(pdf_doc *pd, pdf_obj *o, int depth) {
    o = pdf_resolve(pd, o);
    if (!o || depth > 8) return 0;
    if (o->type == PO_NAME && !strcmp(o->v.name, "Identity")) return 0;
    double t = -1;
    if (!pdf_num(pdf_dict_get(pd, o, "FunctionType"), &t)) return 0;
    pdf_fn *f = calloc(1, sizeof *f);
    if (!f) return 0;
    f->type = (int)t;
    f->m = pf_nums(pd, pdf_dict_get(pd, o, "Domain"), f->domain, 2 * PF_MAXIN) / 2;
    int nr = pf_nums(pd, pdf_dict_get(pd, o, "Range"), f->range, 2 * PF_MAXOUT);
    f->has_range = nr > 0;
    f->n = nr / 2;
    if (f->m < 1) f->m = 1, f->domain[0] = 0, f->domain[1] = 1;
    switch (f->type) {
    case 0: {
        f->bps = 8;
        double v;
        if (pdf_num(pdf_dict_get(pd, o, "BitsPerSample"), &v)) f->bps = (int)v;
        double sz[PF_MAXIN];
        int ns = pf_nums(pd, pdf_dict_get(pd, o, "Size"), sz, PF_MAXIN);
        for (int i = 0; i < f->m; i++) { f->size[i] = i < ns ? (int)sz[i] : 1; if (f->size[i] < 1) f->size[i] = 1; }
        if (pf_nums(pd, pdf_dict_get(pd, o, "Encode"), f->encode, 2 * PF_MAXIN) < 2 * f->m)
            for (int i = 0; i < f->m; i++) { f->encode[2 * i] = 0; f->encode[2 * i + 1] = f->size[i] - 1; }
        if (pf_nums(pd, pdf_dict_get(pd, o, "Decode"), f->decode, 2 * PF_MAXOUT) < 2 * f->n) memcpy(f->decode, f->range, sizeof f->decode);
        f->samples = pdf_stream_data(pd, o, &f->nsamples, 0);
        if (!f->samples || !f->n) { pf_free(f); return 0; }
        break;
    }
    case 2: {
        int a = pf_nums(pd, pdf_dict_get(pd, o, "C0"), f->c0, PF_MAXOUT), b = pf_nums(pd, pdf_dict_get(pd, o, "C1"), f->c1, PF_MAXOUT);
        if (!a) { f->c0[0] = 0; a = 1; }
        if (!b) { f->c1[0] = 1; b = 1; }
        f->n = a > b ? a : b;
        f->N = 1;
        pdf_num(pdf_dict_get(pd, o, "N"), &f->N);
        break;
    }
    case 3: {
        pdf_obj *fa = pdf_dict_get(pd, o, "Functions");
        if (!fa || fa->type != PO_ARRAY || !fa->n || fa->n > 64) { pf_free(f); return 0; }
        f->k = (int)fa->n;
        f->fns = calloc((size_t)f->k, sizeof *f->fns);
        for (int i = 0; i < f->k; i++) f->fns[i] = pf_load(pd, fa->v.arr[i], depth + 1);
        pf_nums(pd, pdf_dict_get(pd, o, "Bounds"), f->bounds, 64);
        pf_nums(pd, pdf_dict_get(pd, o, "Encode"), f->enc3, 128);
        if (!f->n) for (int i = 0; i < f->k; i++) if (f->fns[i] && f->fns[i]->n > f->n) f->n = f->fns[i]->n;
        break;
    }
    case 4: {
        size_t n, p = 0;
        const u8 *s = pdf_stream_data(pd, o, &n, 0);
        ps_buf b = {0};
        while (s && p < n && s[p] != '{') p++;
        if (!s || p >= n) { pf_free(f); return 0; }
        p++;
        if (ps_compile(s, n, &p, &b, 0)) { free(b.c); free(b.k); pf_free(f); return 0; }
        f->code = b.c; f->ncode = b.n; f->consts = b.k;
        break;
    }
    default: pf_free(f); return 0;
    }
    return f;
}
static u32 pf_sample(const pdf_fn *f, size_t idx) {
    size_t bit = idx * (size_t)f->bps;
    if (f->bps == 8) return bit / 8 < f->nsamples ? f->samples[bit / 8] : 0;
    if (f->bps == 16) return bit / 8 + 1 < f->nsamples ? (u32)(f->samples[bit / 8] << 8 | f->samples[bit / 8 + 1]) : 0;
    if (f->bps == 32) return bit / 8 + 3 < f->nsamples ? (u32)f->samples[bit / 8] << 24 | (u32)f->samples[bit / 8 + 1] << 16 | (u32)f->samples[bit / 8 + 2] << 8 | f->samples[bit / 8 + 3] : 0;
    if (f->bps == 24) return bit / 8 + 2 < f->nsamples ? (u32)f->samples[bit / 8] << 16 | (u32)f->samples[bit / 8 + 1] << 8 | f->samples[bit / 8 + 2] : 0;
    u32 v = 0;
    for (int b = 0; b < f->bps; b++, bit++) v = v << 1 | (bit / 8 < f->nsamples ? (u32)(f->samples[bit / 8] >> (7 - bit % 8) & 1) : 0);
    return v;
}
static int pf_eval(const pdf_fn *f, const double *in, double *out) {
    if (!f) return 0;
    double x[PF_MAXIN];
    for (int i = 0; i < f->m; i++) x[i] = pf_clamp(in[i], f->domain[2 * i], f->domain[2 * i + 1]);
    switch (f->type) {
    case 0: {
        double e[PF_MAXIN];
        int i0[PF_MAXIN];
        double fr[PF_MAXIN];
        for (int i = 0; i < f->m; i++) {
            double d0 = f->domain[2 * i], d1 = f->domain[2 * i + 1];
            e[i] = d1 != d0 ? f->encode[2 * i] + (x[i] - d0) * (f->encode[2 * i + 1] - f->encode[2 * i]) / (d1 - d0) : f->encode[2 * i];
            e[i] = pf_clamp(e[i], 0, f->size[i] - 1);
            i0[i] = (int)e[i];
            if (i0[i] >= f->size[i] - 1) i0[i] = f->size[i] > 1 ? f->size[i] - 2 : 0;
            fr[i] = f->size[i] > 1 ? e[i] - i0[i] : 0;
        }
        double maxv = (double)((f->bps >= 32 ? 4294967295.0 : (double)((1ull << f->bps) - 1)));
        for (int o = 0; o < f->n; o++) {
            double acc = 0;
            for (int cm = 0; cm < (1 << f->m); cm++) {
                double w = 1;
                size_t idx = 0, stride = 1;
                for (int i = 0; i < f->m; i++) {
                    int bit = cm >> i & 1;
                    w *= bit ? fr[i] : 1 - fr[i];
                    idx += (size_t)(i0[i] + (f->size[i] > 1 ? bit : 0)) * stride;
                    stride *= (size_t)f->size[i];
                }
                if (w > 0) acc += w * pf_sample(f, idx * (size_t)f->n + (size_t)o);
            }
            out[o] = f->decode[2 * o] + acc / maxv * (f->decode[2 * o + 1] - f->decode[2 * o]);
        }
        break;
    }
    case 2:
        for (int o = 0; o < f->n; o++) out[o] = f->c0[o] + pow(x[0], f->N) * (f->c1[o] - f->c0[o]);
        break;
    case 3: {
        int i = 0;
        while (i < f->k - 1 && x[0] >= f->bounds[i]) i++;
        double lo = i ? f->bounds[i - 1] : f->domain[0], hi = i < f->k - 1 ? f->bounds[i] : f->domain[1];
        double e0 = f->enc3[2 * i], e1 = f->enc3[2 * i + 1];
        double t = hi != lo ? e0 + (x[0] - lo) * (e1 - e0) / (hi - lo) : e0;
        if (!f->fns[i]) { for (int o = 0; o < f->n; o++) out[o] = t; break; }
        pf_eval(f->fns[i], &t, out);
        break;
    }
    case 4: {
        double st[104];
        int sp = 0;
        for (int i = 0; i < f->m; i++) st[sp++] = x[i];
        if (ps_run(f, 0, f->ncode, st, &sp, 0)) { for (int o = 0; o < f->n; o++) out[o] = 0; break; }
        for (int o = 0; o < f->n; o++) out[o] = sp - f->n + o >= 0 ? st[sp - f->n + o] : 0;
        break;
    }
    }
    if (f->has_range) for (int o = 0; o < f->n; o++) out[o] = pf_clamp(out[o], f->range[2 * o], f->range[2 * o + 1]);
    return f->n;
}
/* array de funções (uma por saída) ou uma só */
typedef struct { pdf_fn *f[PF_MAXOUT]; int nf; } pf_set;
static void pfs_load(pdf_doc *pd, pdf_obj *o, pf_set *s) {
    o = pdf_resolve(pd, o);
    s->nf = 0;
    if (!o) return;
    if (o->type == PO_ARRAY) { for (u32 i = 0; i < o->n && s->nf < PF_MAXOUT; i++) s->f[s->nf++] = pf_load(pd, o->v.arr[i], 0); }
    else s->f[s->nf++] = pf_load(pd, o, 0);
}
static int pfs_eval(const pf_set *s, const double *in, double *out) {
    if (s->nf == 1) return pf_eval(s->f[0], in, out);
    for (int i = 0; i < s->nf; i++) { double v[PF_MAXOUT]; pf_eval(s->f[i], in, v); out[i] = v[0]; }
    return s->nf;
}
static void pfs_free(pf_set *s) { for (int i = 0; i < s->nf; i++) pf_free(s->f[i]); s->nf = 0; }
