/* mOS — motor JavaScript "Brisa": definições internas compartilhadas entre os arquivos js_*.c
 *
 * Valores: NaN-boxing em 64 bits. Doubles normais (NaN canônico 0x7FF8...) e, no espaço de NaN
 * negativo, especiais (undefined/null/booleanos), strings e objetos (ponteiros de 48 bits).
 * Memória: coletor mark & sweep próprio; raízes = reinos vivos + páginas + varredura conservadora
 * da pilha C e dos registradores (o interpretador percorre a árvore sintática recursivamente,
 * então valores temporários vivem na pilha). Ponteiros internos a objetos também contam. */
#pragma once
#include "kernel.h"
#include "web.h"
#include "js.h"

typedef u64 jsv;

#define JS_TAG_SPEC  0xFFF9000000000000ull
#define JS_TAG_STR   0xFFFA000000000000ull
#define JS_TAG_OBJ   0xFFFB000000000000ull
#define JS_TAG_BIG   0xFFFC000000000000ull   /* BigInt (jsbig*) */
#define JS_TAG_MASK  0xFFFF000000000000ull
#define JS_PTRMASK   0x0000FFFFFFFFFFFFull
#define JS_UNDEF  (JS_TAG_SPEC | 0)
#define JS_NULL   (JS_TAG_SPEC | 1)
#define JS_FALSE  (JS_TAG_SPEC | 2)
#define JS_TRUE   (JS_TAG_SPEC | 3)
#define JS_HOLE   (JS_TAG_SPEC | 4)   /* buraco em vetor / binding não inicializado (TDZ) */
#define JS_EXC    (JS_TAG_SPEC | 5)   /* "uma exceção foi lançada" (valor em J.exc) */
#define JS_SHORT  (JS_TAG_SPEC | 6)   /* curto-circuito de ?. */
#define JS_NAN_BITS 0x7FF8000000000000ull

/* ============================================================ coletor */
enum { GT_FREE, GT_STR, GT_OBJ, GT_ENV, GT_SCRIPT, GT_BIG };
typedef struct gch { u8 gtype, mark, pad1, pad2; u32 bsize; } gch_t;   /* 8 bytes */

void *gc_alloc(int gtype, usize size);   /* zerado */
void  gc_collect(void);
void  gc_maybe(void);
void  gc_mark_val(jsv v);
void  gc_mark_ptr(void *p);
usize gc_heap_bytes(void);

/* ============================================================ strings */
typedef struct jsstr {
    gch_t h;
    u32 len;          /* bytes UTF-8 (WTF-8: surrogates soltos codificados em 3 bytes) */
    u32 hash;
    u32 ulen;         /* comprimento em unidades UTF-16 */
    u8 ascii, sym, intern, pad;
    char d[];         /* terminada em 0 */
} jsstr;

jsstr *js_strn(const char *s, int len);
jsstr *js_strz(const char *s);
jsstr *js_intern(const char *s, int len);     /* string permanente/compartilhada (nomes) */
jsstr *js_str_cat(jsstr *a, jsstr *b);
jsstr *js_str_fmt(const char *fmt, ...);
bool   js_str_eq(jsstr *a, jsstr *b);
int    js_str_cmp(jsstr *a, jsstr *b);
u32    js_str_unit(jsstr *s, int i);          /* unidade UTF-16 na posição i */
int    js_str_boff(jsstr *s, int ui);         /* deslocamento em bytes do índice UTF-16 ui */
int    js_str_uidx(jsstr *s, int boff);       /* índice UTF-16 do deslocamento em bytes */
jsstr *js_str_usub(jsstr *s, int ua, int ub); /* substring por índices UTF-16 */
jsstr *js_str_from_units(const u16 *u, int n);
jsstr *js_new_symbol(jsstr *desc);
bool   js_str_isindex(jsstr *s, u32 *idx);
int    js_utf8_put(char *o, u32 cp);          /* codifica (inclui surrogates soltos) */
u32    js_utf8_get(const char *s, int len, int *i); /* decodifica um code point */

/* ============================================================ objetos */
enum {
    CL_OBJECT, CL_ARRAY, CL_FUNCTION, CL_NATIVE, CL_BOUND, CL_ERROR, CL_DATE, CL_REGEXP, CL_BOOLEAN, CL_NUMBER,
    CL_STRING, CL_SYMBOL, CL_ARGUMENTS, CL_PROMISE, CL_MAP, CL_SET, CL_ITER, CL_GEN, CL_BIGINT, CL_MODULE, CL_NODE, CL_STYLE, CL_CLASSLIST,
    CL_EVENT, CL_STORAGE, CL_LOCATION, CL_DATASET, CL_HOST, CL_COUNT
};
enum { PF_ACCESSOR = 1, PF_NOENUM = 2, PF_RO = 4, PF_NOCONF = 8 };

typedef struct jsprop { jsstr *k; jsv v; jsv set; u32 flags; } jsprop;

struct jsfunc;
struct jsenv;
struct jsscript;
struct jsobj;
struct jsnode;
typedef jsv (*jsnative)(jsv thisv, int argc, jsv *argv, struct jsobj *self, int flags);
enum { NF_NEW = 1 };

typedef struct rxprog rxprog;
struct jscoro;
struct jsmodule;

/* BigInt: magnitude em limbs de 32 bits (little-endian, sem zeros à esquerda; zero = n 0) */
typedef struct jsbig { gch_t h; u32 n; u8 neg, pad[3]; u32 d[]; } jsbig;

typedef struct jsobj {
    gch_t h;
    u8 cls, noext, frozen, is_class;
    u8 is_derived, is_arrow, is_method, has_proto_prop;
    struct jsobj *proto;
    jsprop *props;
    u32 nprops, cprops;
    u32 icap;
    u32 *index;               /* tabela hash (índice+1) quando nprops > 8 */
    /* elementos densos (vetores, arguments) */
    jsv *arr;
    u32 alen, acap;
    union {
        struct { struct jsfunc *fi; struct jsenv *env; struct jsscript *script; struct jsobj *home; struct jsnode *cls; struct jsenv *cenv; } f;
        struct { jsnative fn; int magic; jsv data; jsv data2; } nat;
        struct { jsv target, bthis; struct jsobj *args; } bound;
        struct { rxprog *prog; jsstr *src; jsstr *flags; } re;
        struct { int state; jsv value; struct jsobj *reactions; bool handled; } prom;
        struct { struct jsobj *keys, *vals; u32 n, live; } map;   /* Map/Set: vetores paralelos */
        struct { jsv target; int kind; u32 pos; } it;
        struct { struct js_page *page; node_t *node; jsv extra; } host;
        struct { struct jscoro *co; } gen;      /* gerador / função async em execução */
        struct jsmodule *mod;                   /* registro de módulo ES */
        double date;
        jsv prim;
    } u;
} jsobj;

/* ============================================================ ambientes léxicos */
enum { VF_CONST = 1, VF_LEX = 2, VF_REF = 4 };   /* VF_REF: importação viva ([namespace, nome]) */
typedef struct jsenv {
    gch_t h;
    struct jsenv *parent;
    u32 n, cap;
    jsstr **names;
    jsv *vals;
    u8 *vflags;
    jsv thisv;
    u8 has_this, is_func, pad1, pad2;
    struct jsobj *fnobj;      /* função em execução (para super / home object) */
    jsv newtarget;
} jsenv;

/* ============================================================ árvore sintática */
typedef struct jsnode {
    u8 type, op;
    u16 flags;
    int line;
    struct jsnode *a, *b, *c, *d;
    struct jsnode **list;
    int n;
    jsstr *str;
    double num;
    void *x;                  /* jsfunc*, jsblock*, rxprog*, jsstr* ... */
} jsnode;

typedef struct jsblock {      /* declarações léxicas de um bloco */
    jsstr **names;
    u8 *isconst;
    int n, cap;
    struct jsnode **funcs;
    int nfuncs, cfuncs;
} jsblock;

typedef struct jsfunc {
    jsstr *name;
    jsnode **params;
    int nparams;
    jsnode *body;             /* N_BLOCK (ou expressão em arrow de expressão) */
    jsstr **vars;             /* var içadas */
    int nvars, cvars;
    jsnode **funcs;           /* declarações de função no topo do corpo */
    int nfuncs, cfuncs;
    u8 is_arrow, expr_body, uses_args, is_method, is_ctor, is_derived, simple_params, is_getter, is_static, uses_this;
    u8 is_gen, is_async;
    int line;
    int length;               /* nº de parâmetros antes do primeiro com padrão/resto */
} jsfunc;

/* metadados de módulo: imp NULL = namespace (import * as); em exports, local NULL = reexportação */
typedef struct { jsstr *spec, *imp, *local; } jsimport;
typedef struct { jsstr *exp, *local, *spec, *imp; } jsexport;   /* reexport: imp NULL = export *, imp "*" = export * as */

typedef struct jsscript {
    gch_t h;
    bool is_module;
    jsstr **mreq;             /* especificadores pedidos (sem repetição) */
    int nmreq, cmreq;
    jsimport *imports;
    int nimports, cimports;
    jsexport *exports;
    int nexports, cexports;
    jsstr **strs;             /* todas as strings referenciadas pela árvore (mantidas vivas) */
    int nstrs, cstrs;
    arena_t arena;            /* nós da árvore */
    rxprog **rx;
    int nrx, crx;
    jsstr *name;
    jsfunc *top;
    jsnode *body;
} jsscript;

/* tipos de nó */
enum {
    N_EMPTY, N_NUM, N_STR, N_TEMPLATE, N_TAGGED, N_REGEX, N_IDENT, N_THIS, N_SUPER, N_ARRAY, N_OBJECT, N_PROP,
    N_FUNC, N_CLASS, N_UNARY, N_UPDATE, N_BINARY, N_LOGICAL, N_ASSIGN, N_COND, N_CALL, N_NEW, N_MEMBER, N_OPTCHAIN,
    N_SEQ, N_SPREAD, N_ELISION, N_NEWTARGET,
    N_VAR, N_DECL, N_FUNCDECL, N_CLASSDECL, N_RETURN, N_IF, N_FOR, N_FORIN, N_FOROF, N_WHILE, N_DOWHILE, N_BREAK,
    N_CONTINUE, N_THROW, N_TRY, N_SWITCH, N_CASE, N_BLOCK, N_EXPR, N_LABELED, N_PROGRAM, N_CLASSMEMBER, N_DEBUGGER,
    N_YIELD, N_AWAIT, N_BIGINT, N_EXPORTDEF, N_IMPORTCALL, N_IMPORTMETA
};
/* operadores */
enum {
    OP_NONE, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_EXP, OP_SHL, OP_SHR, OP_USHR, OP_BAND, OP_BOR, OP_BXOR,
    OP_EQ, OP_NE, OP_SEQ, OP_SNE, OP_LT, OP_GT, OP_LE, OP_GE, OP_INSTANCEOF, OP_IN, OP_AND, OP_OR, OP_NULLISH,
    OP_NEG, OP_PLUS, OP_NOT, OP_BNOT, OP_TYPEOF, OP_VOID, OP_DELETE, OP_INC, OP_DEC, OP_ASSIGN
};
/* flags */
enum {
    NF_COMPUTED = 1, NF_OPTIONAL = 2, NF_PREFIX = 4, NF_HASLEX = 8, NF_PERITER = 16, NF_SHORTHAND = 32,
    NF_GETTER = 64, NF_SETTER = 128, NF_METHOD = 256, NF_STATIC = 512, NF_SPREADPROP = 1024, NF_FIELD = 2048,
    NF_AWAIT = 4096
};
enum { VK_VAR, VK_LET, VK_CONST };

/* ============================================================ reino (um por página) */
enum { E_ERROR, E_TYPE, E_RANGE, E_SYNTAX, E_REF, E_EVAL, E_URI, E_COUNT };

typedef struct jsrealm {
    jsobj *global;
    jsenv *glex;
    jsobj *p_object, *p_function, *p_array, *p_string, *p_number, *p_boolean, *p_symbol, *p_regexp, *p_date,
          *p_promise, *p_map, *p_set, *p_iter, *p_arguments, *p_error, *p_gen, *p_asyncgen, *p_bigint;
    jsobj *modules;           /* mapa URL -> registro de módulo (CL_MODULE) */
    jsobj *modjobs;           /* carregamentos de módulo pendentes */
    jsobj *p_err[E_COUNT], *c_err[E_COUNT];
    jsobj *c_object, *c_array, *c_function, *c_promise, *c_regexp, *c_symbol, *c_string, *c_number;
    jsobj *p_node, *p_elem, *p_doc, *p_text, *p_event, *p_style, *p_classlist, *p_storage, *p_location, *p_response, *p_xhr;
    jsobj *jobs;              /* fila de microtarefas: vetor [fn, a0, a1, kind, ...] */
    u32 jobpos;
    struct js_page *page;
    struct jsrealm *next;
    bool dead;
} jsrealm;

/* ============================================================ estado global do interpretador */
enum { CMP_NORMAL, CMP_RETURN, CMP_BREAK, CMP_CONTINUE };
#define JS_MAXROOTS 512

typedef struct {
    jsrealm *realm, *realms;
    bool interrupted;
    jsv exc;
    u64 deadline;
    u32 steps;
    uptr stack_base;
    u32 stack_limit;          /* bytes de pilha permitidos a partir de stack_base */
    struct jscoro *cur_coro;  /* corrotina (gerador/async) em execução, ou NULL */
    int entry;
    int calldepth;
    jsv roots[JS_MAXROOTS];
    int nroots;
    int line;
    jsstr *srcname;
    jsv retval;
    jsstr *label;             /* rótulo de break/continue */
    jsv completion;
    bool gc_running;
    u32 gc_count;
    jsv newtarget;            /* new.target para construtores nativos */
} jsstate;
extern jsstate J;

static inline void js_push_root(jsv v) { if (J.nroots < JS_MAXROOTS) J.roots[J.nroots++] = v; }
static inline void js_pop_roots(int n) { J.nroots -= n; if (J.nroots < 0) J.nroots = 0; }

/* ============================================================ átomos (strings permanentes) */
#define JS_ATOMS(X) X(length) X(prototype) X(constructor) X(name) X(message) X(toString) X(valueOf) X(callee) \
    X(lastIndex) X(index) X(input) X(get) X(set) X(value) X(writable) X(enumerable) X(configurable) X(next) \
    X(done) X(then) X(join) X(stack) X(undefined) X(handleEvent) X(toJSON) X(groups) X(arguments) X(caller) \
    X(cause) X(errors) X(status) X(reason) X(add) X(size) X(default) X(source) X(flags) X(global) X(key) \
    X(target) X(type) X(detail) X(once) X(capture) X(passive) X(object) X(function) X(string) X(number) \
    X(boolean) X(symbol) X(bigint) X(null) X(true) X(false) X(empty) X(NaN) X(Infinity) X(anonymous) X(raw) \
    X(description) X(data) X(this) X(super) X(window) X(document) X(event) X(concat) X(call) X(apply) X(of) \
    X(let) X(static) X(async) X(await) X(yield) X(lineNumber) X(fulfilled) X(rejected)
#define JS_ATOM_ENUM(n) A_##n,
enum { JS_ATOMS(JS_ATOM_ENUM) A_neg_infinity, A_sym_iterator, A_sym_hasInstance, A_sym_toPrimitive,
       A_sym_toStringTag, A_sym_asyncIterator, A_COUNT };
extern jsstr *A[A_COUNT];

/* ============================================================ valores */
static inline bool jv_isnum(jsv v) { return v < JS_TAG_SPEC; }
static inline bool jv_isstr(jsv v) { return (v & JS_TAG_MASK) == JS_TAG_STR; }
static inline bool jv_isobj(jsv v) { return (v & JS_TAG_MASK) == JS_TAG_OBJ; }
static inline bool jv_isbig(jsv v) { return (v & JS_TAG_MASK) == JS_TAG_BIG; }
static inline jsbig *jv_b(jsv v) { return (jsbig *)(uptr)(v & JS_PTRMASK); }
static inline jsv jv_big(jsbig *b) { return b ? (JS_TAG_BIG | (u64)(uptr)b) : JS_EXC; }
static inline bool jv_isnullish(jsv v) { return v == JS_UNDEF || v == JS_NULL; }
static inline bool jv_isbool(jsv v) { return v == JS_TRUE || v == JS_FALSE; }
static inline double jv_n(jsv v) { union { u64 u; double d; } x; x.u = v; return x.d; }
static inline jsv jv_num(double d) { union { u64 u; double d; } x; x.d = d; if (d != d) return JS_NAN_BITS; return x.u; }
static inline jsstr *jv_s(jsv v) { return (jsstr *)(uptr)(v & JS_PTRMASK); }
static inline jsobj *jv_o(jsv v) { return (jsobj *)(uptr)(v & JS_PTRMASK); }
static inline jsv jv_str(jsstr *s) { return s ? (JS_TAG_STR | (u64)(uptr)s) : JS_EXC; }
static inline jsv jv_obj(jsobj *o) { return o ? (JS_TAG_OBJ | (u64)(uptr)o) : JS_NULL; }
static inline jsv jv_bool(bool b) { return b ? JS_TRUE : JS_FALSE; }
static inline jsv jv_int(int i) { return jv_num((double)i); }
static inline bool jv_iscls(jsv v, int cls) { return jv_isobj(v) && jv_o(v)->cls == cls; }
static inline bool jv_issym(jsv v) { return jv_isstr(v) && jv_s(v)->sym; }
#define ARG(i) ((i) < argc ? argv[i] : JS_UNDEF)

/* ============================================================ núcleo (js_core.c) */
void   js_core_init(void);
jsobj *js_newobj(jsobj *proto);
jsobj *js_newobj_cls(int cls, jsobj *proto);
jsobj *js_newarr(u32 n);
jsobj *js_arr_from(jsv *v, int n);
void   js_arr_push(jsobj *a, jsv v);
jsv    js_arr_get(jsobj *a, u32 i);
void   js_arr_set(jsobj *a, u32 i, jsv v);
void   js_arr_setlen(jsobj *a, u32 n);
void   js_arr_reserve(jsobj *a, u32 n);
bool   js_isarray(jsv v);

jsprop *js_ownprop(jsobj *o, jsstr *k);
jsprop *js_findprop(jsobj *o, jsstr *k, jsobj **owner);
jsprop *js_addprop(jsobj *o, jsstr *k);
void   js_defprop(jsobj *o, jsstr *k, jsv v, u32 flags);
void   js_defn(jsobj *o, const char *name, jsv v, u32 flags);
void   js_defaccessor(jsobj *o, jsstr *k, jsv getter, jsv setter, u32 flags);
bool   js_delprop(jsobj *o, jsstr *k);
jsobj *js_defnative(jsobj *o, const char *name, jsnative fn, int nargs, int magic);
jsobj *js_defgetter(jsobj *o, const char *name, jsnative fn, int magic);
jsobj *js_newnative(jsnative fn, const char *name, int nargs, int magic);
jsobj *js_newctor(jsnative fn, const char *name, int nargs, jsobj *proto);

jsv    js_get(jsv o, jsv key);
jsv    js_getk(jsv o, jsstr *k);
jsv    js_getn(jsv o, const char *name);
jsv    js_getidx(jsv o, u32 i);
jsv    js_get_with_this(jsv o, jsstr *k, jsv thisv);
int    js_set(jsv o, jsv key, jsv v);       /* 0 ok, -1 exceção */
int    js_setk(jsv o, jsstr *k, jsv v);
int    js_setn(jsv o, const char *name, jsv v);
int    js_setidx(jsv o, u32 i, jsv v);
int    js_has(jsv o, jsv key);              /* 1/0, -1 exceção */
int    js_delete(jsv o, jsv key);
jsstr *js_tokey(jsv key);                   /* NULL = exceção */
jsobj *js_keylist(jsobj *o, bool only_enum, bool include_proto); /* vetor de strings */
jsobj *js_getproto(jsv v);
u32    js_lengthof(jsv o);                  /* ToLength(o.length) (0 em exceção, J.exc definido) */

/* conversões (JS_EXC / NULL em erro) */
jsv    js_toprim(jsv v, int hint);           /* 0 padrão, 1 número, 2 string */
double js_tonum(jsv v);                      /* NaN + exceção pendente: verifique js_pending() */
jsv    js_tonumv(jsv v);
jsstr *js_tostr(jsv v);
jsv    js_tostrv(jsv v);
bool   js_truthy(jsv v);
jsv    js_toobj(jsv v);
double js_tointeger(jsv v);
i32    js_toint32(jsv v);
u32    js_touint32(jsv v);
jsstr *js_typeof(jsv v);
bool   js_strict_eq(jsv a, jsv b);
int    js_loose_eq(jsv a, jsv b);           /* 1/0, -1 exceção */
bool   js_samevalue(jsv a, jsv b, bool zero);
const char *js_cstr(jsv v, char *buf, int cap); /* para mensagens; nunca lança */
jsstr *js_num_tostr(double d);
extern bool js_exc_pending;                  /* definido por js_throw; limpo por quem captura */
static inline bool js_pending(void) { return js_exc_pending; }

/* exceções */
jsv    js_throw(jsv v);
jsv    js_throw_err(int type, const char *fmt, ...);
jsobj *js_make_err(int type, jsstr *msg);
jsstr *js_exc_string(jsv e);                 /* "TypeError: ..." */
jsv    js_catch(void);                       /* pega a exceção pendente e limpa */

/* chamadas */
bool   js_iscallable(jsv v);
bool   js_isctor(jsv v);
jsv    js_call(jsv f, jsv thisv, int argc, jsv *argv);
jsv    js_construct(jsv f, int argc, jsv *argv, jsv newtarget);
jsv    js_invoke(jsv obj, jsstr *name, int argc, jsv *argv);
int    js_instanceof(jsv v, jsv ctor);      /* 1/0, -1 exceção */

/* entrada/saída do motor (pilha, orçamento) */
void   js_enter(void *frame);
void   js_leave(void);
bool   js_check_stack(void);                /* false = estourou (exceção lançada) */
void   js_set_budget(u32 ms);
bool   js_tick_budget(void);                /* false = tempo esgotado (exceção ininterrompível lançada) */

/* reinos */
jsrealm *js_realm_new(void);
void     js_realm_free(jsrealm *r);
void     js_mark_realm(jsrealm *r);

/* ============================================================ números (js_num.c) */
void   js_numfmt(double d, char *out, int cap);              /* Number::toString */
int    js_num_fixed(double d, int frac, char *out, int cap); /* toFixed */
int    js_num_prec(double d, int prec, char *out, int cap);  /* toPrecision */
int    js_num_expo(double d, int frac, char *out, int cap);  /* toExponential (frac<0 = mínimo) */
void   js_num_radix(double d, int radix, char *out, int cap);
double js_strtod(const char *s, int len, int *used);   /* literal decimal; used = bytes consumidos */
double js_str_to_number(jsstr *s);                    /* ToNumber(string) */
double m_sqrt(double x);
double m_floor(double x);
double m_ceil(double x);
double m_trunc(double x);
double m_round(double x);
double m_fmod(double a, double b);
double m_exp(double x);
double m_log(double x);
double m_pow(double b, double e);
double m_sin(double x);
double m_cos(double x);
double m_tan(double x);
double m_atan2(double y, double x);
double m_atan(double x);
double m_asin(double x);
double m_acos(double x);
double m_log2(double x);
double m_log10(double x);
double m_abs(double x);
bool   m_isfinite(double x);
bool   m_isnan(double x);
#define JS_INF (__builtin_inf())

/* ============================================================ BigInt (js_bigint.c) */
jsbig *js_big_from_double(double d);              /* d inteiro */
jsbig *js_big_parse(const char *s, int len, int radix, bool *ok);   /* radix 0 = aceita prefixos 0x/0o/0b */
double js_big_to_double(jsbig *b);
jsstr *js_big_tostr(jsbig *b, int radix);
int    js_big_cmp(jsbig *a, jsbig *b);
int    js_big_cmp_num(jsbig *a, double d);         /* -1/0/1, 2 = NaN */
jsv    js_big_binop(int op, jsbig *a, jsbig *b);   /* JS_EXC em erro */
jsv    js_big_neg(jsbig *a);
jsv    js_big_add_small(jsbig *a, int k);
jsv    js_big_from_str_value(jsstr *s);            /* StringToBigInt; JS_UNDEF se inválida */
void   js_bigint_init(jsrealm *r);

/* ============================================================ módulos ES (js_module.c) */
jsv    js_dynamic_import(jsv spec);                 /* import(): Promise do namespace */
jsv    js_module_start_script(struct js_page *p, const char *url, const char *inline_src, int len);  /* devolve job */
int    js_module_job_state(jsv job);                /* 0 pendente, 1 ok, -1 erro */
void   js_module_fetched(jsobj *mod, const char *data, int len, int status, const char *err);
void   js_module_mark(jsobj *o);
void   js_module_free(jsobj *o);
jsv    js_import_meta(void);
void   js_net_fetch_module(struct js_page *p, const char *url, jsobj *mod);   /* em js_dom.c */
const char *js_page_base(struct js_page *p);

/* ============================================================ parser (js_parse.c) */
jsscript *js_parse(const char *src, int len, const char *name, int line0, jsstr **err);
jsscript *js_parse_ex(const char *src, int len, const char *name, int line0, bool module, jsstr **err);

/* ============================================================ interpretador (js_interp.c) */
jsv    js_run_script(jsscript *s);                /* executa no reino atual; devolve completion ou JS_EXC */
jsv    js_call_function(jsobj *f, jsv thisv, int argc, jsv *argv, jsv newtarget);
jsobj *js_make_closure(jsfunc *fi, jsenv *env, jsscript *sc);
jsenv *js_newenv(jsenv *parent, int cap);
void   js_env_decl(jsenv *e, jsstr *name, jsv v, u8 flags);
jsv    js_eval_string(jsstr *code, const char *name); /* eval indireto (escopo global) */
jsv    js_new_function(int argc, jsv *argv);      /* new Function(...) */
jsv    js_run_module(jsscript *s, jsenv *menv);   /* corpo de um módulo já ligado */
jsv    js_env_get(jsenv *e, jsstr *name);         /* binding de um ambiente (TDZ lança) */
jsscript *js_cur_script(void);

/* contexto do interpretador salvo ao trocar de corrotina */
typedef struct { struct jsscript *script; jsstr *plabel, *label, *srcname; int calldepth, line; } jsictx;
void   js_ictx_save(jsictx *c);
void   js_ictx_restore(jsictx *c);
jsv    js_call_body(jsobj *f, jsv thisv, int argc, jsv *argv, jsv newtarget);

/* ============================================================ geradores e async (js_gen.c) */
void   js_gen_init(jsrealm *r);
jsv    js_gen_create(jsobj *f, jsv thisv, int argc, jsv *argv);
jsv    js_async_call(jsobj *f, jsv thisv, int argc, jsv *argv);
jsv    js_asyncgen_create(jsobj *f, jsv thisv, int argc, jsv *argv);
jsv    js_yield(jsv v);
jsv    js_yield_star(jsv iterable);
jsv    js_await(jsv v);
int    js_gen_step(jsobj *g, jsv *out);          /* 1 valor, 0 fim, -1 exceção */
bool   js_is_genreturn(jsv e);                   /* exceção interna de gen.return() */
void   js_coro_free(struct jscoro *co);
void   js_coro_mark_owner(jsobj *o);
bool   js_coro_scan(void (*scan)(uptr lo, uptr hi));   /* varre pilhas suspensas alcançáveis */
void   js_coro_scan_chain(void (*scan)(uptr lo, uptr hi));
void   js_coro_gc_begin(void);
jsobj *js_promise_from(jsv v);
jsobj *js_promise_then(jsobj *p, jsv onf, jsv onr);

/* ============================================================ biblioteca (js_lib.c) */
void   js_lib_init(jsrealm *r);
jsv    js_iter_open(jsv v);                       /* objeto iterador (JS_EXC em erro) */
int    js_iter_step(jsv it, jsv *out);            /* 1 valor, 0 fim, -1 exceção */
jsobj *js_promise_new(void);
void   js_promise_resolve(jsobj *p, jsv v);
void   js_promise_reject(jsobj *p, jsv v);
void   js_enqueue_job(jsv fn, jsv arg0, jsv arg1, int kind);
bool   js_run_jobs(void);                         /* microtarefas; true se executou alguma */
jsv    js_json_stringify(jsv v, jsv replacer, jsv space);
jsv    js_json_parse(jsstr *s, jsv reviver);
jsv    js_regexp_new(jsstr *src, jsstr *flags);
double js_date_now(void);
jsstr *js_array_join(jsobj *a, jsstr *sep);
jsv    js_regexp_from_prog(rxprog *p, jsstr *src, jsstr *flags);
jsv    js_regexp_split(jsobj *re, jsstr *s, u32 limit);
jsv    js_regexp_replace(jsobj *re, jsstr *s, jsv rep);
jsv    js_regexp_match(jsobj *re, jsstr *s);
jsv    js_regexp_matchall(jsobj *re, jsstr *s);
jsv    js_regexp_search(jsobj *re, jsstr *s);
jsstr *js_expand_replacement(jsstr *rep, jsstr *s, int mstart, int mend, int *caps, int ncaps, jsobj *groups);
void   js_inspect(jsv v, sbuf_t *sb, int depth, bool top);
bool   js_array_iter_pristine(jsv v);
void   js_report_unhandled(void);

/* ============================================================ expressões regulares (js_regex.c) */
enum { RXF_G = 1, RXF_I = 2, RXF_M = 4, RXF_S = 8, RXF_Y = 16, RXF_U = 32 };
rxprog *rx_compile(const char *src, int len, int flags, char *err, int errcap);
void    rx_ref(rxprog *p);
void    rx_unref(rxprog *p);
int     rx_ngroups(rxprog *p);                /* inclui o grupo 0 */
const char *rx_group_name(rxprog *p, int i);  /* NULL se sem nome */
int     rx_flags(rxprog *p);
/* procura a partir do byte start; caps[2*i], caps[2*i+1] = offsets em bytes (-1 = não casou).
 * 1 casou, 0 não, -1 limite de passos */
int     rx_exec(rxprog *p, const char *s, int len, int start, int *caps, bool sticky);

/* ============================================================ DOM (js_dom.c) */
void   js_dom_init(jsrealm *r);
void   js_mark_page(struct js_page *p);
jsv    js_dom_get(jsobj *o, jsstr *k, bool *found);
int    js_dom_set(jsobj *o, jsstr *k, jsv v);      /* 1 tratado, 0 não, -1 exceção */
void   js_console_log(int level, const char *msg);
void   js_report_exception(void);                 /* imprime e limpa a exceção pendente */
void   js_mark_pages(void);
