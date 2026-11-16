/*
 * mOS libSystem — OpenGL por software: o prelúdio do GLSL. É código no dialeto do compilador de MSL do
 * mOS (mtlshader.c) que vem antes do shader traduzido: os tipos do GLSL (vec*, ivec*, mat*...), as
 * funções embutidas que o MSL não tem com o mesmo nome ou a mesma semântica (mod, atan(y, x), lessThan,
 * matrixCompMult, inverse, findMSB, pack/unpack...), os sampler* como par textura + sampler com todas as
 * funções de textura do GLSL 1.10 a 4.10, e as variáveis embutidas gl_* de cada estágio (as do perfil de
 * compatibilidade só quando o shader é compat).
 */
#include "gl_int.h"
int snprintf(char *b, size_t n, const char *fmt, ...);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);

typedef struct { char *p; u64 n, cap; } sb_t;
static void sb_put(sb_t *b, const char *s, u64 n) {
    if (b->n + n + 1 > b->cap) {
        u64 nc = b->cap ? b->cap * 2 : 65536;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void sb_s(sb_t *b, const char *s) { sb_put(b, s, strlen(s)); }
static void sb_f(sb_t *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) sb_put(b, tmp, (u64)(n < (int)sizeof tmp ? n : (int)sizeof tmp - 1));
}

static const char *FT[4] = {"float", "float2", "float3", "float4"};
static const char *IT[4] = {"int", "int2", "int3", "int4"};
static const char *UT[4] = {"uint", "uint2", "uint3", "uint4"};
static const char *BT[4] = {"bool", "bool2", "bool3", "bool4"};
static const char *CMP[4] = {"x", "y", "z", "w"};

static void types(sb_t *b) {
    for (int n = 2; n <= 4; n++) {
        sb_f(b, "typedef float%d vec%d; typedef int%d ivec%d; typedef uint%d uvec%d; typedef bool%d bvec%d; typedef float%d dvec%d;\n", n, n, n, n, n, n, n, n, n, n);
        sb_f(b, "typedef float%dx%d mat%d; typedef float%dx%d dmat%d;\n", n, n, n, n, n, n);
        for (int r = 2; r <= 4; r++) sb_f(b, "typedef float%dx%d mat%dx%d; typedef float%dx%d dmat%dx%d;\n", n, r, n, r, n, r, n, r);
    }
    sb_s(b, "#define inversesqrt rsqrt\n#define dFdx dfdx\n#define dFdy dfdy\n#define dFdxFine dfdx\n#define dFdyFine dfdy\n"
            "#define dFdxCoarse dfdx\n#define dFdyCoarse dfdy\n#define fwidthFine fwidth\n#define fwidthCoarse fwidth\n"
            "#define bitCount popcount\n#define bitfieldReverse reverse_bits\n#define bitfieldExtract extract_bits\n"
            "#define bitfieldInsert insert_bits\n");
}
static void math(sb_t *b) {
    for (int i = 0; i < 4; i++) {
        const char *T = FT[i];
        sb_f(b, "%s mod(%s x, %s y) { return x - y * floor(x / y); }\n", T, T, T);
        if (i) sb_f(b, "%s mod(%s x, float y) { return x - y * floor(x / y); }\n", T, T);
        sb_f(b, "%s atan(%s y, %s x) { return atan2(y, x); }\n", T, T, T);
        sb_f(b, "%s radians(%s d) { return d * 0.017453292519943295; }\n", T, T);
        sb_f(b, "%s degrees(%s r) { return r * 57.29577951308232; }\n", T, T);
        sb_f(b, "%s roundEven(%s x) { return rint(x); }\n", T, T);
        sb_f(b, "%s modf(%s x, thread %s &ip) { ip = trunc(x); return x - ip; }\n", T, T, T);
        sb_f(b, "%s ldexp(%s x, %s e) { return x * exp2(%s(e)); }\n", T, T, IT[i], T);
        sb_f(b, "%s frexp(%s x, thread %s &e) { %s r = x; ", T, T, IT[i], T);
        for (int k = 0; k <= i; k++) {
            const char *c = i ? CMP[k] : "";
            const char *dot = i ? "." : "";
            sb_f(b, "{ float v = x%s%s; int ex = v == 0.0 ? 0 : int(floor(log2(abs(v)))) + 1; e%s%s = ex; r%s%s = v == 0.0 ? 0.0 : v / exp2(float(ex)); } ", dot, c, dot, c, dot, c);
        }
        sb_s(b, "return r; }\n");
        sb_f(b, "%s mix(%s x, %s y, %s a) { return select(x, y, a); }\n", T, T, T, BT[i]);
        sb_f(b, "%s mix(%s x, %s y, %s a) { return select(x, y, a); }\n", IT[i], IT[i], IT[i], BT[i]);
        sb_f(b, "%s mix(%s x, %s y, %s a) { return select(x, y, a); }\n", UT[i], UT[i], UT[i], BT[i]);
        sb_f(b, "%s mix(%s x, %s y, %s a) { return select(x, y, a); }\n", BT[i], BT[i], BT[i], BT[i]);
        sb_f(b, "%s floatBitsToInt(%s v) { return as_type<%s>(v); }\n", IT[i], T, IT[i]);
        sb_f(b, "%s floatBitsToUint(%s v) { return as_type<%s>(v); }\n", UT[i], T, UT[i]);
        sb_f(b, "%s intBitsToFloat(%s v) { return as_type<%s>(v); }\n", T, IT[i], T);
        sb_f(b, "%s uintBitsToFloat(%s v) { return as_type<%s>(v); }\n", T, UT[i], T);
        sb_f(b, "%s interpolateAtCentroid(%s x) { return x; }\n", T, T);
        sb_f(b, "%s interpolateAtSample(%s x, int s) { return x; }\n", T, T);
        sb_f(b, "%s interpolateAtOffset(%s x, float2 o) { return x; }\n", T, T);
        sb_f(b, "float noise1(%s x) { return 0.0; }\nfloat2 noise2(%s x) { return float2(0.0); }\n", T, T);
        sb_f(b, "float3 noise3(%s x) { return float3(0.0); }\nfloat4 noise4(%s x) { return float4(0.0); }\n", T, T);
        sb_f(b, "%s findLSB(%s v) { %s r; ", IT[i], UT[i], IT[i]);
        for (int k = 0; k <= i; k++) { const char *c = i ? CMP[k] : ""; const char *dot = i ? "." : ""; sb_f(b, "r%s%s = v%s%s == 0u ? -1 : int(ctz(v%s%s)); ", dot, c, dot, c, dot, c); }
        sb_s(b, "return r; }\n");
        sb_f(b, "%s findLSB(%s v) { return findLSB(%s(v)); }\n", IT[i], IT[i], UT[i]);
        sb_f(b, "%s findMSB(%s v) { %s r; ", IT[i], UT[i], IT[i]);
        for (int k = 0; k <= i; k++) { const char *c = i ? CMP[k] : ""; const char *dot = i ? "." : ""; sb_f(b, "r%s%s = v%s%s == 0u ? -1 : 31 - int(clz(v%s%s)); ", dot, c, dot, c, dot, c); }
        sb_s(b, "return r; }\n");
        sb_f(b, "%s findMSB(%s v) { %s r; ", IT[i], IT[i], IT[i]);
        for (int k = 0; k <= i; k++) { const char *c = i ? CMP[k] : ""; const char *dot = i ? "." : ""; sb_f(b, "{ uint u = v%s%s < 0 ? uint(~v%s%s) : uint(v%s%s); r%s%s = u == 0u ? -1 : 31 - int(clz(u)); } ", dot, c, dot, c, dot, c, dot, c); }
        sb_s(b, "return r; }\n");
        sb_f(b, "%s uaddCarry(%s x, %s y, thread %s &c) { %s s = x + y; c = select(%s(0u), %s(1u), s < x); return s; }\n", UT[i], UT[i], UT[i], UT[i], UT[i], UT[i], UT[i]);
        sb_f(b, "%s usubBorrow(%s x, %s y, thread %s &c) { c = select(%s(0u), %s(1u), x < y); return x - y; }\n", UT[i], UT[i], UT[i], UT[i], UT[i], UT[i]);
        sb_f(b, "void umulExtended(%s x, %s y, thread %s &hi, thread %s &lo) { hi = mulhi(x, y); lo = x * y; }\n", UT[i], UT[i], UT[i], UT[i]);
        sb_f(b, "void imulExtended(%s x, %s y, thread %s &hi, thread %s &lo) { hi = mulhi(x, y); lo = x * y; }\n", IT[i], IT[i], IT[i], IT[i]);
        if (!i) continue;
        /* comparações componente a componente (o == do GLSL compara o vetor inteiro) */
        const char *tys[3][1] = {{FT[i]}, {IT[i]}, {UT[i]}};
        for (int t = 0; t < 3; t++) {
            const char *V = tys[t][0];
            static const char *nm[6] = {"lessThan", "lessThanEqual", "greaterThan", "greaterThanEqual", "equal", "notEqual"};
            static const char *op[6] = {"<", "<=", ">", ">=", "==", "!="};
            for (int o = 0; o < 6; o++) {
                sb_f(b, "%s %s(%s a, %s b) { return %s(", BT[i], nm[o], V, V, BT[i]);
                for (int k = 0; k <= i; k++) sb_f(b, "%sa.%s %s b.%s", k ? ", " : "", CMP[k], op[o], CMP[k]);
                sb_s(b, "); }\n");
            }
        }
        sb_f(b, "%s equal(%s a, %s b) { return %s(", BT[i], BT[i], BT[i], BT[i]);
        for (int k = 0; k <= i; k++) sb_f(b, "%sa.%s == b.%s", k ? ", " : "", CMP[k], CMP[k]);
        sb_s(b, "); }\n");
        sb_f(b, "%s notEqual(%s a, %s b) { return %s(", BT[i], BT[i], BT[i], BT[i]);
        for (int k = 0; k <= i; k++) sb_f(b, "%sa.%s != b.%s", k ? ", " : "", CMP[k], CMP[k]);
        sb_s(b, "); }\n");
        sb_f(b, "%s not(%s a) { return !a; }\n", BT[i], BT[i]);
    }
    /* matrizes */
    for (int c = 2; c <= 4; c++)
        for (int r = 2; r <= 4; r++) {
            sb_f(b, "float%dx%d matrixCompMult(float%dx%d a, float%dx%d b) { return float%dx%d(", c, r, c, r, c, r, c, r);
            for (int k = 0; k < c; k++) sb_f(b, "%sa[%d] * b[%d]", k ? ", " : "", k, k);
            sb_s(b, "); }\n");
            sb_f(b, "float%dx%d outerProduct(float%d cv, float%d rv) { return float%dx%d(", c, r, r, c, c, r);
            for (int k = 0; k < c; k++) sb_f(b, "%scv * rv.%s", k ? ", " : "", CMP[k]);
            sb_s(b, "); }\n");
        }
    sb_s(b, "float2x2 inverse(float2x2 m) { float d = m[0][0] * m[1][1] - m[1][0] * m[0][1]; return float2x2(float2(m[1][1], -m[0][1]) / d, float2(-m[1][0], m[0][0]) / d); }\n"
            "float3x3 inverse(float3x3 m) { float3 a = m[0], b = m[1], c = m[2]; float3 r0 = cross(b, c), r1 = cross(c, a), r2 = cross(a, b);"
            " float d = dot(a, r0); return transpose(float3x3(r0 / d, r1 / d, r2 / d)); }\n"
            "float4x4 inverse(float4x4 m) {"
            " float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3], a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];"
            " float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3], a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];"
            " float b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10, b03 = a01 * a12 - a02 * a11;"
            " float b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12, b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30;"
            " float b08 = a20 * a33 - a23 * a30, b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;"
            " float d = 1.0 / (b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06);"
            " return float4x4(float4(a11 * b11 - a12 * b10 + a13 * b09, a02 * b10 - a01 * b11 - a03 * b09, a31 * b05 - a32 * b04 + a33 * b03, a22 * b04 - a21 * b05 - a23 * b03) * d,"
            " float4(a12 * b08 - a10 * b11 - a13 * b07, a00 * b11 - a02 * b08 + a03 * b07, a32 * b02 - a30 * b05 - a33 * b01, a20 * b05 - a22 * b02 + a23 * b01) * d,"
            " float4(a10 * b10 - a11 * b08 + a13 * b06, a01 * b08 - a00 * b10 - a03 * b06, a30 * b04 - a31 * b02 + a33 * b00, a21 * b02 - a20 * b04 - a23 * b00) * d,"
            " float4(a11 * b07 - a10 * b09 - a12 * b06, a00 * b09 - a01 * b07 + a02 * b06, a31 * b01 - a30 * b03 - a32 * b00, a20 * b03 - a21 * b01 + a22 * b00) * d); }\n");
    /* empacotamento */
    sb_s(b, "uint packUnorm2x16(float2 v) { return pack_float_to_unorm2x16(v); }\n"
            "uint packSnorm2x16(float2 v) { return pack_float_to_snorm2x16(v); }\n"
            "uint packUnorm4x8(float4 v) { return pack_float_to_unorm4x8(v); }\n"
            "uint packSnorm4x8(float4 v) { return pack_float_to_snorm4x8(v); }\n"
            "float2 unpackUnorm2x16(uint p) { return unpack_unorm2x16_to_float(p); }\n"
            "float2 unpackSnorm2x16(uint p) { return unpack_snorm2x16_to_float(p); }\n"
            "float4 unpackUnorm4x8(uint p) { return unpack_unorm4x8_to_float(p); }\n"
            "float4 unpackSnorm4x8(uint p) { return unpack_snorm4x8_to_float(p); }\n"
            "uint packHalf2x16(float2 v) { return as_type<uint>(half2(v)); }\n"
            "float2 unpackHalf2x16(uint p) { return float2(as_type<half2>(p)); }\n");
}

/* ---- samplers: nome GLSL, textura MSL, dimensão da coordenada, dimensão do tamanho, escalar de retorno, sombra */
typedef struct { const char *g, *tex; int nc, ns, arr, shadow, ms, buf, cube; } samp_t;
static const samp_t SAMPS[] = {
    {"1D", "texture1d", 1, 1, 0, 0, 0, 0, 0}, {"2D", "texture2d", 2, 2, 0, 0, 0, 0, 0}, {"3D", "texture3d", 3, 3, 0, 0, 0, 0, 0},
    {"Cube", "texturecube", 3, 2, 0, 0, 0, 0, 1}, {"2DRect", "texture2d", 2, 2, 0, 0, 0, 0, 0},
    {"1DArray", "texture1d_array", 1, 2, 1, 0, 0, 0, 0}, {"2DArray", "texture2d_array", 2, 3, 1, 0, 0, 0, 0},
    {"CubeArray", "texturecube_array", 3, 3, 1, 0, 0, 0, 1}, {"Buffer", "texture_buffer", 1, 1, 0, 0, 0, 1, 0},
    {"2DMS", "texture2d_ms", 2, 2, 0, 0, 1, 0, 0}, {"2DMSArray", "texture2d_ms_array", 2, 3, 1, 0, 1, 0, 0},
    {"1DShadow", "texture1d", 1, 1, 0, 1, 0, 0, 0}, {"2DShadow", "texture2d", 2, 2, 0, 1, 0, 0, 0},
    {"CubeShadow", "texturecube", 3, 2, 0, 1, 0, 0, 1}, {"2DRectShadow", "texture2d", 2, 2, 0, 1, 0, 0, 0},
    {"1DArrayShadow", "texture1d_array", 1, 2, 1, 1, 0, 0, 0}, {"2DArrayShadow", "texture2d_array", 2, 3, 1, 1, 0, 0, 0},
    {"CubeArrayShadow", "texturecube_array", 3, 3, 1, 1, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
const char *gl_glsl_sampler_name(int i) { return SAMPS[i].g; }
/* chamada de amostragem: coordenada c (já sem a camada), camada l, opção opt ("", "level(x)", "bias(x)") */
static void samp_funcs(sb_t *b, const samp_t *s, const char *pfx, const char *scal) {
    char ty[64], r4[16], cv[16], sv[16], iv[16];
    snprintf(ty, sizeof ty, "%ssampler%s", pfx, s->g);
    snprintf(r4, sizeof r4, "%s4", scal);
    int nfull = s->nc + s->arr;                     /* coordenada do GLSL sem a referência de sombra */
    snprintf(cv, sizeof cv, "%s", FT[nfull - 1]);
    snprintf(sv, sizeof sv, "%s", IT[s->ns - 1]);
    snprintf(iv, sizeof iv, "%s", IT[(s->buf ? 1 : s->nc + s->arr) - 1]);
    sb_f(b, "struct %s { %s<%s> t; sampler s; };\n", ty, s->tex, scal);
    /* tamanho */
    if (s->buf) sb_f(b, "int textureSize(%s x) { return int(x.t.get_width()); }\n", ty);
    else {
        sb_f(b, "%s textureSize(%s x, int l) { return %s(int(x.t.get_width(l))", sv, ty, sv);
        if (s->ns >= 2) sb_s(b, s->nc >= 2 || s->cube ? ", int(x.t.get_height(l))" : ", int(x.t.get_array_size())");
        if (s->ns >= 3) sb_s(b, s->arr ? ", int(x.t.get_array_size())" : ", int(x.t.get_depth(l))");
        sb_s(b, "); }\n");
        if (s->ms) sb_f(b, "%s textureSize(%s x) { return textureSize(x, 0); }\n", sv, ty);
        if (!strcmp(s->g, "2DRect") || !strcmp(s->g, "2DRectShadow")) sb_f(b, "%s textureSize(%s x) { return textureSize(x, 0); }\n", sv, ty);
    }
    if (s->ms || s->buf) {
        if (s->buf) sb_f(b, "%s texelFetch(%s x, int p) { return x.t.read(uint(p)); }\n", r4, ty);
        else if (s->arr) sb_f(b, "%s texelFetch(%s x, int3 p, int smp) { return x.t.read(uint2(p.xy), uint(p.z), uint(smp)); }\n", r4, ty);
        else sb_f(b, "%s texelFetch(%s x, int2 p, int smp) { return x.t.read(uint2(p), uint(smp)); }\n", r4, ty);
        return;
    }
    /* coordenada e camada a partir do vetor do GLSL */
    const char *cx = s->nc == 1 ? "c.x" : s->nc == 2 ? "c.xy" : "c.xyz";
    const char *lx = s->nc == 1 ? "c.y" : s->nc == 2 ? "c.z" : "c.w";
    char call[160], callo[200];
    if (s->nc == 1 && !s->arr) snprintf(call, sizeof call, "x.t.sample(x.s, c");
    else if (!s->arr) snprintf(call, sizeof call, "x.t.sample(x.s, %s", cx);
    else snprintf(call, sizeof call, "x.t.sample(x.s, %s, uint(rint(%s))", cx, lx);
    (void)callo;
    if (s->shadow) {
        /* sombra: a referência vem depois da coordenada (ou no argumento extra do cube array) */
        int refi = nfull;   /* índice do componente de referência */
        if (s->nc == 1 && !s->arr) refi = 2;   /* sampler1DShadow: vec3(s, -, ref) */
        char rv[16];
        snprintf(rv, sizeof rv, "%s", FT[(refi < 4 ? refi + 1 : 4) - 1]);
        const char *ref = refi == 1 ? "c.y" : refi == 2 ? "c.z" : refi == 3 ? "c.w" : "r";
        char cmp[160];
        if (!s->arr) snprintf(cmp, sizeof cmp, "x.t.sample_compare(x.s, %s, %s", s->nc == 1 ? "c.x" : cx, ref);
        else snprintf(cmp, sizeof cmp, "x.t.sample_compare(x.s, %s, uint(rint(%s)), %s", cx, lx, ref);
        if (refi < 4) {
            sb_f(b, "float texture(%s x, %s c) { return %s); }\n", ty, rv, cmp);
            sb_f(b, "float texture(%s x, %s c, float bs) { return %s, bias(bs)); }\n", ty, rv, cmp);
            sb_f(b, "float textureLod(%s x, %s c, float lv) { return %s, level(lv)); }\n", ty, rv, cmp);
            sb_f(b, "float textureGrad(%s x, %s c, %s dx, %s dy) { return %s); }\n", ty, rv, FT[s->nc - 1], FT[s->nc - 1], cmp);
            if (!s->arr && !s->cube) {
                char pv[16];
                snprintf(pv, sizeof pv, "float4");
                sb_f(b, "float textureProj(%s x, float4 q) { %s c = %s(q.%s / q.w, q.z / q.w); return %s); }\n", ty, rv, rv, s->nc == 1 ? "x, 0.0" : "xy", cmp);
                sb_f(b, "float textureProjLod(%s x, float4 q, float lv) { %s c = %s(q.%s / q.w, q.z / q.w); return %s, level(lv)); }\n", ty, rv, rv, s->nc == 1 ? "x, 0.0" : "xy", cmp);
                sb_f(b, "float textureOffset(%s x, %s c, %s o) { return texture(x, c); }\n", ty, rv, IT[s->nc - 1]);
                (void)pv;
            }
        } else {
            sb_f(b, "float texture(%s x, float4 c, float r) { return %s); }\n", ty, cmp);
        }
        /* nomes antigos: shadow1D/shadow2D devolvem vec4 */
        if (!strcmp(s->g, "2DShadow") || !strcmp(s->g, "1DShadow") || !strcmp(s->g, "2DRectShadow")) {
            const char *base = !strcmp(s->g, "2DShadow") ? "shadow2D" : !strcmp(s->g, "1DShadow") ? "shadow1D" : "shadow2DRect";
            sb_f(b, "float4 %s(%s x, float3 c) { float v = texture(x, %s(c%s)); return float4(v, v, v, 1.0); }\n", base, ty, rv, "");
            sb_f(b, "float4 %s(%s x, float3 c, float bs) { float v = texture(x, %s(c%s), bs); return float4(v, v, v, 1.0); }\n", base, ty, rv, "");
            sb_f(b, "float4 %sProj(%s x, float4 q) { float v = textureProj(x, q); return float4(v, v, v, 1.0); }\n", base, ty);
            if (strcmp(base, "shadow2DRect")) sb_f(b, "float4 %sLod(%s x, float3 c, float l) { float v = textureLod(x, %s(c%s), l); return float4(v, v, v, 1.0); }\n", base, ty, rv, "");
        }
        return;
    }
    /* texture / textureLod / textureGrad / offsets */
    sb_f(b, "%s texture(%s x, %s c) { return %s); }\n", r4, ty, cv, call);
    sb_f(b, "%s texture(%s x, %s c, float bs) { return %s, bias(bs)); }\n", r4, ty, cv, call);
    sb_f(b, "%s textureLod(%s x, %s c, float lv) { return %s, level(lv)); }\n", r4, ty, cv, call);
    {
        const char *D = FT[s->nc - 1];
        sb_f(b, "%s textureGrad(%s x, %s c, %s dx, %s dy) { float2 sz = float2(float(x.t.get_width(0)), %s); ", r4, ty, cv, D, D,
             s->nc >= 2 ? "float(x.t.get_height(0))" : "1.0");
        if (s->nc == 1) sb_s(b, "float a = dx * sz.x, b2 = dy * sz.x; float lv = 0.5 * log2(max(a * a, b2 * b2));");
        else sb_s(b, "float2 a = dx.xy * sz, b2 = dy.xy * sz; float lv = 0.5 * log2(max(dot(a, a), dot(b2, b2)));");
        sb_f(b, " return textureLod(x, c, lv); }\n");
    }
    if (!s->cube) {
        const char *O = IT[s->nc - 1];
        int rect = !strcmp(s->g, "2DRect");
        char off[128];
        if (rect) snprintf(off, sizeof off, "%s(o)", FT[s->nc - 1]);
        else if (s->nc == 1) snprintf(off, sizeof off, "float(o) / float(x.t.get_width(0))");
        else if (s->nc == 2) snprintf(off, sizeof off, "float2(o) / float2(float(x.t.get_width(0)), float(x.t.get_height(0)))");
        else snprintf(off, sizeof off, "float3(o) / float3(float(x.t.get_width(0)), float(x.t.get_height(0)), float(x.t.get_depth(0)))");
        const char *sel = s->nc == 1 ? (s->arr ? ".x" : "") : s->nc == 2 ? ".xy" : ".xyz";
        sb_f(b, "%s textureOffset(%s x, %s c, %s o) { %s d = c; d%s += %s; return texture(x, d); }\n", r4, ty, cv, O, cv, sel, off);
        sb_f(b, "%s textureOffset(%s x, %s c, %s o, float bs) { %s d = c; d%s += %s; return texture(x, d, bs); }\n", r4, ty, cv, O, cv, sel, off);
        sb_f(b, "%s textureLodOffset(%s x, %s c, float lv, %s o) { %s d = c; d%s += %s; return textureLod(x, d, lv); }\n", r4, ty, cv, O, cv, sel, off);
        sb_f(b, "%s textureGradOffset(%s x, %s c, %s dx, %s dy, %s o) { %s d = c; d%s += %s; return textureGrad(x, d, dx, dy); }\n", r4, ty, cv, FT[s->nc - 1], FT[s->nc - 1], O, cv, sel, off);
        /* texelFetch */
        if (rect) sb_f(b, "%s texelFetch(%s x, int2 p) { return x.t.read(uint2(p)); }\n", r4, ty);
        else if (s->arr && s->nc == 1) sb_f(b, "%s texelFetch(%s x, int2 p, int l) { return x.t.read(uint(p.x), uint(p.y), uint(l)); }\n", r4, ty);
        else if (s->arr) sb_f(b, "%s texelFetch(%s x, int3 p, int l) { return x.t.read(uint2(p.xy), uint(p.z), uint(l)); }\n", r4, ty);
        else if (s->nc == 1) sb_f(b, "%s texelFetch(%s x, int p, int l) { return x.t.read(uint(p), uint(l)); }\n", r4, ty);
        else sb_f(b, "%s texelFetch(%s x, %s p, int l) { return x.t.read(%s(p), uint(l)); }\n", r4, ty, iv, s->nc == 2 ? "uint2" : "uint3");
        if (!rect) sb_f(b, "%s texelFetchOffset(%s x, %s p, int l, %s o) { return texelFetch(x, p + %s(o%s), l); }\n", r4, ty, iv, O, iv, s->arr ? ", 0" : "");
        else sb_f(b, "%s texelFetchOffset(%s x, int2 p, int2 o) { return texelFetch(x, p + o); }\n", r4, ty);
        if (!s->arr) {
            /* projeções: divide pelo último componente */
            const char *P1 = FT[s->nc], *pc = s->nc == 1 ? "q.x" : s->nc == 2 ? "q.xy" : "q.xyz";
            const char *pw1 = s->nc == 1 ? "q.y" : s->nc == 2 ? "q.z" : "q.w";
            sb_f(b, "%s textureProj(%s x, %s q) { return texture(x, %s / %s); }\n", r4, ty, P1, pc, pw1);
            sb_f(b, "%s textureProj(%s x, %s q, float bs) { return texture(x, %s / %s, bs); }\n", r4, ty, P1, pc, pw1);
            sb_f(b, "%s textureProjLod(%s x, %s q, float lv) { return textureLod(x, %s / %s, lv); }\n", r4, ty, P1, pc, pw1);
            sb_f(b, "%s textureProjGrad(%s x, %s q, %s dx, %s dy) { return textureGrad(x, %s / %s, dx, dy); }\n", r4, ty, P1, FT[s->nc - 1], FT[s->nc - 1], pc, pw1);
            sb_f(b, "%s textureProjOffset(%s x, %s q, %s o) { return textureOffset(x, %s / %s, o); }\n", r4, ty, P1, O, pc, pw1);
            if (s->nc < 3) {
                sb_f(b, "%s textureProj(%s x, float4 q) { return texture(x, q.%s / q.w); }\n", r4, ty, s->nc == 1 ? "x" : "xy");
                sb_f(b, "%s textureProj(%s x, float4 q, float bs) { return texture(x, q.%s / q.w, bs); }\n", r4, ty, s->nc == 1 ? "x" : "xy");
                sb_f(b, "%s textureProjLod(%s x, float4 q, float lv) { return textureLod(x, q.%s / q.w, lv); }\n", r4, ty, s->nc == 1 ? "x" : "xy");
            }
        }
    }
    if (s->nc >= 2 && !s->buf) {
        /* textureGather (GLSL 4.00): rgba = (i0,j1) (i1,j1) (i1,j0) (i0,j0) */
        if (s->nc == 2 && !s->arr && !s->cube) {
            sb_f(b, "%s textureGather(%s x, %s c) { return x.t.gather(x.s, c); }\n", r4, ty, cv);
            sb_f(b, "%s textureGather(%s x, %s c, int comp) { return x.t.gather(x.s, c, comp); }\n", r4, ty, cv);
            sb_f(b, "%s textureGatherOffset(%s x, %s c, int2 o) { return textureGather(x, c + float2(o) / float2(float(x.t.get_width(0)), float(x.t.get_height(0)))); }\n", r4, ty, cv);
        }
        sb_f(b, "float2 textureQueryLod(%s x, %s c) { return float2(0.0, 0.0); }\n", ty, FT[s->nc - 1]);
    }
}
static void samplers(sb_t *b) {
    static const char *pf[3] = {"", "i", "u"}, *sc[3] = {"float", "int", "uint"};
    for (int k = 0; SAMPS[k].g; k++)
        for (int p = 0; p < 3; p++) {
            if (SAMPS[k].shadow && p) continue;
            samp_funcs(b, &SAMPS[k], pf[p], sc[p]);
        }
    /* nomes do GLSL 1.10/1.20 e de ARB_shader_texture_lod / EXT_texture_array */
    static const struct { const char *name, *ty, *cv, *fn; } old[] = {
        {"texture1D", "sampler1D", "float", "texture"}, {"texture2D", "sampler2D", "float2", "texture"},
        {"texture3D", "sampler3D", "float3", "texture"}, {"textureCube", "samplerCube", "float3", "texture"},
        {"texture2DRect", "sampler2DRect", "float2", "texture"}, {"texture1DArray", "sampler1DArray", "float2", "texture"},
        {"texture2DArray", "sampler2DArray", "float3", "texture"}, {0, 0, 0, 0}};
    for (int i = 0; old[i].name; i++) {
        sb_f(b, "float4 %s(%s x, %s c) { return texture(x, c); }\n", old[i].name, old[i].ty, old[i].cv);
        if (strcmp(old[i].ty, "sampler2DRect")) sb_f(b, "float4 %s(%s x, %s c, float bs) { return texture(x, c, bs); }\n", old[i].name, old[i].ty, old[i].cv);
        if (strcmp(old[i].ty, "sampler2DRect")) {
            sb_f(b, "float4 %sLod(%s x, %s c, float l) { return textureLod(x, c, l); }\n", old[i].name, old[i].ty, old[i].cv);
            sb_f(b, "float4 %sLodARB(%s x, %s c, float l) { return textureLod(x, c, l); }\n", old[i].name, old[i].ty, old[i].cv);
            sb_f(b, "float4 %sLodEXT(%s x, %s c, float l) { return textureLod(x, c, l); }\n", old[i].name, old[i].ty, old[i].cv);
        }
    }
    static const struct { const char *name, *ty, *gv; } grad[] = {
        {"texture1DGradARB", "sampler1D", "float"}, {"texture2DGradARB", "sampler2D", "float2"}, {"texture3DGradARB", "sampler3D", "float3"},
        {"textureCubeGradARB", "samplerCube", "float3"}, {"texture2DRectGradARB", "sampler2DRect", "float2"}, {"texture2DGradEXT", "sampler2D", "float2"},
        {"textureCubeGradEXT", "samplerCube", "float3"}, {0, 0, 0}};
    for (int i = 0; grad[i].name; i++) {
        const char *cv = !strcmp(grad[i].ty, "sampler1D") ? "float" : !strcmp(grad[i].ty, "sampler3D") || !strcmp(grad[i].ty, "samplerCube") ? "float3" : "float2";
        sb_f(b, "float4 %s(%s x, %s c, %s dx, %s dy) { return textureGrad(x, c, dx, dy); }\n", grad[i].name, grad[i].ty, cv, grad[i].gv, grad[i].gv);
    }
    static const struct { const char *name, *ty, *pv; } proj[] = {
        {"texture1DProj", "sampler1D", "float2"}, {"texture1DProj", "sampler1D", "float4"}, {"texture2DProj", "sampler2D", "float3"},
        {"texture2DProj", "sampler2D", "float4"}, {"texture3DProj", "sampler3D", "float4"}, {"texture2DRectProj", "sampler2DRect", "float3"},
        {"texture2DRectProj", "sampler2DRect", "float4"}, {0, 0, 0}};
    for (int i = 0; proj[i].name; i++) {
        sb_f(b, "float4 %s(%s x, %s q) { return textureProj(x, q); }\n", proj[i].name, proj[i].ty, proj[i].pv);
        if (strcmp(proj[i].ty, "sampler2DRect")) {
            sb_f(b, "float4 %s(%s x, %s q, float bs) { return textureProj(x, q, bs); }\n", proj[i].name, proj[i].ty, proj[i].pv);
            sb_f(b, "float4 %sLod(%s x, %s q, float l) { return textureProjLod(x, q, l); }\n", proj[i].name, proj[i].ty, proj[i].pv);
        }
    }
}
/* variáveis embutidas */
static void builtins(sb_t *b, int stage, int compat) {
    sb_s(b, "struct gl_DepthRangeParameters { float near; float far; float diff; };\ngl_DepthRangeParameters gl_DepthRange;\n");
    if (stage == 0) {
        sb_s(b, "float4 gl_Position; float gl_PointSize; float gl_ClipDistance[8]; int gl_VertexID; int gl_InstanceID;\n");
        if (compat) sb_s(b, "float4 gl_Vertex; float3 gl_Normal; float4 gl_Color; float4 gl_SecondaryColor; float gl_FogCoord;\n"
                            "float4 gl_MultiTexCoord0; float4 gl_MultiTexCoord1; float4 gl_MultiTexCoord2; float4 gl_MultiTexCoord3;\n"
                            "float4 gl_MultiTexCoord4; float4 gl_MultiTexCoord5; float4 gl_MultiTexCoord6; float4 gl_MultiTexCoord7;\n"
                            "float4 gl_FrontColor; float4 gl_BackColor; float4 gl_FrontSecondaryColor; float4 gl_BackSecondaryColor;\n"
                            "float4 gl_TexCoord[8]; float gl_FogFragCoord; float4 gl_ClipVertex;\n");
    } else {
        sb_s(b, "float4 gl_FragCoord; bool gl_FrontFacing; float2 gl_PointCoord; float gl_FragDepth; int gl_PrimitiveID;\n"
                "float4 gl_FragColor; float4 gl_FragData[8]; int gl_SampleID; float2 gl_SamplePosition; float gl_ClipDistance[8];\n");
        if (compat) sb_s(b, "float4 gl_Color; float4 gl_SecondaryColor; float4 gl_TexCoord[8]; float gl_FogFragCoord;\n");
    }
    if (!compat) return;
    sb_s(b, "float4x4 gl_ModelViewMatrix; float4x4 gl_ProjectionMatrix; float4x4 gl_ModelViewProjectionMatrix; float4x4 gl_TextureMatrix[8];\n"
            "float3x3 gl_NormalMatrix; float4x4 gl_ModelViewMatrixInverse; float4x4 gl_ProjectionMatrixInverse;\n"
            "float4x4 gl_ModelViewProjectionMatrixInverse; float4x4 gl_TextureMatrixInverse[8]; float4x4 gl_ModelViewMatrixTranspose;\n"
            "float4x4 gl_ProjectionMatrixTranspose; float4x4 gl_ModelViewProjectionMatrixTranspose; float4x4 gl_TextureMatrixTranspose[8];\n"
            "float4x4 gl_ModelViewMatrixInverseTranspose; float4x4 gl_ProjectionMatrixInverseTranspose;\n"
            "float4x4 gl_ModelViewProjectionMatrixInverseTranspose; float4x4 gl_TextureMatrixInverseTranspose[8]; float gl_NormalScale;\n"
            "float4 gl_ClipPlane[8];\n"
            "struct gl_PointParameters { float size; float sizeMin; float sizeMax; float fadeThresholdSize; float distanceConstantAttenuation;"
            " float distanceLinearAttenuation; float distanceQuadraticAttenuation; };\ngl_PointParameters gl_Point;\n"
            "struct gl_MaterialParameters { float4 emission; float4 ambient; float4 diffuse; float4 specular; float shininess; };\n"
            "gl_MaterialParameters gl_FrontMaterial; gl_MaterialParameters gl_BackMaterial;\n"
            "struct gl_LightSourceParameters { float4 ambient; float4 diffuse; float4 specular; float4 position; float4 halfVector;"
            " float3 spotDirection; float spotExponent; float spotCutoff; float spotCosCutoff; float constantAttenuation;"
            " float linearAttenuation; float quadraticAttenuation; };\ngl_LightSourceParameters gl_LightSource[8];\n"
            "struct gl_LightModelParameters { float4 ambient; };\ngl_LightModelParameters gl_LightModel;\n"
            "struct gl_LightModelProducts { float4 sceneColor; };\ngl_LightModelProducts gl_FrontLightModelProduct; gl_LightModelProducts gl_BackLightModelProduct;\n"
            "struct gl_LightProducts { float4 ambient; float4 diffuse; float4 specular; };\n"
            "gl_LightProducts gl_FrontLightProduct[8]; gl_LightProducts gl_BackLightProduct[8];\n"
            "float4 gl_TextureEnvColor[8]; float4 gl_EyePlaneS[8]; float4 gl_EyePlaneT[8]; float4 gl_EyePlaneR[8]; float4 gl_EyePlaneQ[8];\n"
            "float4 gl_ObjectPlaneS[8]; float4 gl_ObjectPlaneT[8]; float4 gl_ObjectPlaneR[8]; float4 gl_ObjectPlaneQ[8];\n"
            "struct gl_FogParameters { float4 color; float density; float start; float end; float scale; };\ngl_FogParameters gl_Fog;\n");
    if (stage == 0) sb_s(b, "float4 ftransform() { return gl_ModelViewProjectionMatrix * gl_Vertex; }\n");
}
/* prelúdio do estágio (0 vértice, 1 fragmento); o texto vive até o fim do processo */
const char *gl_glsl_prelude(int stage, int compat, u64 *len) {
    static sb_t cache[2][2];
    sb_t *b = &cache[stage][compat];
    if (!b->p) {
        types(b);
        math(b);
        samplers(b);
        builtins(b, stage, compat);
    }
    *len = b->n;
    return b->p;
}
