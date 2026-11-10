/*
 * mOS libSystem — o pedaço da libc++/libc++abi do macOS (libc++.1.dylib) que os módulos nativos (.node)
 * do processo principal do VS Code importam: policy-watcher, spdlog, sqlite3, keymapping, watchdog,
 * fs-copyfile, os-proxy-resolver (lista medida com tools/machocheck.c; docs/porte-vscode.md).
 * operator new/delete, guardas de estáticos e call_once ficam em cxxabi.c (outra sessão); aqui não se
 * repete nenhum deles.
 *
 * O código desses módulos foi compilado com os cabeçalhos da libc++ da Apple: quase tudo é inline, e as
 * funções daqui precisam respeitar o mesmo layout de objetos do x86_64 (sem o "alternate string layout"):
 *   std::string  {u64 w0; u64 size; char *data} longo (bit 0 de w0 = 1, capacidade alocada = w0 & ~1)
 *                ou curto: byte 0 = tamanho << 1 (bit 0 = 0), texto a partir do byte 1 (até 22 chars)
 *   std::mutex = pthread_mutex_t, std::condition_variable = pthread_cond_t, std::thread = pthread_t
 *   __shared_weak_count {vptr; long owners-1; long weak_owners-1}; vtable: D1 D0 on_zero_shared
 *                get_deleter on_zero_shared_weak
 *   logic_error/runtime_error {vptr; const char *msg}, system_error + error_code {int; const category*}
 * Exceções: __cxa_throw / __gxx_personality_v0 estão em cxx_unwind.c (x86_64 desenrola de verdade).
 * Nomes: símbolos Itanium sem o "_" extra do Mach-O (o carregador o tira).
 */
#include "foundation.h"
#include "libc.h"
#include "../../kernel/include/syscall.h"

typedef unsigned long usz;
void *cxx_new(usz n) __asm__("_Znwm");
void cxx_del(void *p) __asm__("_ZdlPv");
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int pthread_mutex_destroy(void *m);
int pthread_cond_wait(void *c, void *m);
int pthread_cond_signal(void *c);
int pthread_cond_destroy(void *c);
int pthread_join(unsigned long t, void **ret);
int pthread_key_create(unsigned long *k, void (*dtor)(void *));
int usleep(unsigned us);

static void cxx_fatal(const char *what, const char *detail) {
    cx_puts_fd(2, "libc++[mOS]: ");
    cx_puts_fd(2, what);
    if (detail) { cx_puts_fd(2, ": "); cx_puts_fd(2, detail); }
    cx_puts_fd(2, "\n");
    abort();
}

/* ================================================================ std::string (basic_string<char>) */
typedef struct { u64 w0, size; char *data; } cxx_str_t;
static int s_long(const cxx_str_t *s) { return ((const u8 *)s)[0] & 1; }
static usz s_size(const cxx_str_t *s) { return s_long(s) ? s->size : ((const u8 *)s)[0] >> 1; }
static char *s_data(cxx_str_t *s) { return s_long(s) ? s->data : (char *)s + 1; }
static usz s_cap(const cxx_str_t *s) { return s_long(s) ? (s->w0 & ~1ull) - 1 : 22; }
static void s_set_size(cxx_str_t *s, usz n) {
    if (s_long(s)) s->size = n; else ((u8 *)s)[0] = (u8)(n << 1);
    s_data(s)[n] = 0;
}
/* garante capacidade para n caracteres (mais o NUL), preservando o conteúdo */
static void s_reserve(cxx_str_t *s, usz n) {
    usz cap = s_cap(s);
    if (n <= cap) return;
    usz want = n > 2 * cap ? n : 2 * cap;
    usz alloc = (want + 1 + 15) & ~15ull;                   /* múltiplo de 16, como o __recommend da libc++ */
    char *nb = cxx_new(alloc);
    usz len = s_size(s);
    memcpy(nb, s_data(s), len + 1);
    if (s_long(s)) cxx_del(s->data);
    s->w0 = alloc | 1;
    s->size = len;
    s->data = nb;
}
static cxx_str_t *s_append(cxx_str_t *s, const char *p, usz n) {
    usz len = s_size(s);
    /* p pode apontar para dentro da própria string: copia antes de realocar */
    char tmp[64];
    char *src = (char *)p;
    int heap = 0;
    if (p >= s_data(s) && p < s_data(s) + len + 1 && len + n > s_cap(s)) {
        src = n <= sizeof tmp ? tmp : (heap = 1, cxx_new(n));
        memcpy(src, p, n);
    }
    s_reserve(s, len + n);
    memmove(s_data(s) + len, src, n);
    s_set_size(s, len + n);
    if (heap) cxx_del(src);
    return s;
}

cxx_str_t *str_append_pn(cxx_str_t *s, const char *p, usz n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm");
cxx_str_t *str_append_pn(cxx_str_t *s, const char *p, usz n) { return s_append(s, p, n); }

cxx_str_t *str_append_nc(cxx_str_t *s, usz n, char c) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEmc");
cxx_str_t *str_append_nc(cxx_str_t *s, usz n, char c) {
    usz len = s_size(s);
    s_reserve(s, len + n);
    memset(s_data(s) + len, c, n);
    s_set_size(s, len + n);
    return s;
}

void str_push_back(cxx_str_t *s, char c) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc");
void str_push_back(cxx_str_t *s, char c) { s_append(s, &c, 1); }

cxx_str_t *str_insert(cxx_str_t *s, usz pos, const char *p, usz n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKcm");
cxx_str_t *str_insert(cxx_str_t *s, usz pos, const char *p, usz n) {
    usz len = s_size(s);
    if (pos > len) cxx_fatal("std::out_of_range", "basic_string::insert");
    char *copy = cxx_new(n ? n : 1);                       /* p pode estar dentro de s */
    memcpy(copy, p, n);
    s_reserve(s, len + n);
    char *d = s_data(s);
    memmove(d + pos + n, d + pos, len - pos);
    memcpy(d + pos, copy, n);
    s_set_size(s, len + n);
    cxx_del(copy);
    return s;
}

/* std::operator+(const char*, const std::string&): retorno por sret (rdi) */
void str_plus_cs(cxx_str_t *ret, const char *l, const cxx_str_t *r) __asm__("_ZNSt3__1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9_");
void str_plus_cs(cxx_str_t *ret, const char *l, const cxx_str_t *r) {
    ret->w0 = ret->size = 0; ret->data = 0;
    usz ln = strlen(l), rn = s_size(r);
    s_reserve(ret, ln + rn);
    memcpy(s_data(ret), l, ln);
    memcpy(s_data(ret) + ln, s_data((cxx_str_t *)r), rn);
    s_set_size(ret, ln + rn);
}
static void s_init(cxx_str_t *s, const char *p) {
    s->w0 = s->size = 0; s->data = 0;
    s_append(s, p, strlen(p));
}

/* ================================================================ mutex, recursive_mutex, condition_variable */
void mutex_lock(void *m) __asm__("_ZNSt3__15mutex4lockEv");
void mutex_lock(void *m) { pthread_mutex_lock(m); }
void mutex_unlock(void *m) __asm__("_ZNSt3__15mutex6unlockEv");
void mutex_unlock(void *m) { pthread_mutex_unlock(m); }
void mutex_dtor(void *m) __asm__("_ZNSt3__15mutexD1Ev");
void mutex_dtor(void *m) { pthread_mutex_destroy(m); }

/* recursive_mutex: os 64 bytes do pthread_mutex_t; o mutex do mOS não é recursivo, então o dono e a
 * contagem ficam nos bytes 16.. (o pthread_mutex_lock usa só os bytes 8..11) */
typedef struct { u64 sig; volatile int lock, pad; volatile int owner, count; } rmutex_t;
void rmutex_ctor(rmutex_t *m) __asm__("_ZNSt3__115recursive_mutexC1Ev");
void rmutex_ctor(rmutex_t *m) { memset(m, 0, 64); m->sig = 0x32AAABA7; }
void rmutex_dtor(rmutex_t *m) __asm__("_ZNSt3__115recursive_mutexD1Ev");
void rmutex_dtor(rmutex_t *m) { (void)m; }
void rmutex_lock(rmutex_t *m) __asm__("_ZNSt3__115recursive_mutex4lockEv");
void rmutex_lock(rmutex_t *m) {
    int me = (int)SYS0(SYS_GETTID);
    if (m->owner == me) { m->count++; return; }
    while (__sync_lock_test_and_set(&m->lock, 1)) SYS0(SYS_YIELD);
    m->owner = me;
    m->count = 1;
}
void rmutex_unlock(rmutex_t *m) __asm__("_ZNSt3__115recursive_mutex6unlockEv");
void rmutex_unlock(rmutex_t *m) {
    if (--m->count > 0) return;
    m->owner = 0;
    __sync_lock_release(&m->lock);
}

typedef struct { void *m; u8 owns; } unique_lock_t;
void cv_wait(void *cv, unique_lock_t *lk) __asm__("_ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE");
void cv_wait(void *cv, unique_lock_t *lk) {
    if (!lk->owns) cxx_fatal("std::system_error", "condition_variable::wait: mutex não travado");
    pthread_cond_wait(cv, lk->m);
}
void cv_notify_one(void *cv) __asm__("_ZNSt3__118condition_variable10notify_oneEv");
void cv_notify_one(void *cv) { pthread_cond_signal(cv); }
void cv_dtor(void *cv) __asm__("_ZNSt3__118condition_variableD1Ev");
void cv_dtor(void *cv) { pthread_cond_destroy(cv); }

/* ================================================================ std::thread */
void thread_join(unsigned long *t) __asm__("_ZNSt3__16thread4joinEv");
void thread_join(unsigned long *t) {
    if (!*t) cxx_fatal("std::system_error", "thread::join de thread não juntável");
    pthread_join(*t, 0);
    *t = 0;
}
void thread_dtor(unsigned long *t) __asm__("_ZNSt3__16threadD1Ev");
void thread_dtor(unsigned long *t) { if (*t) cxx_fatal("std::terminate", "std::thread destruída ainda juntável"); }

/* __thread_struct (só o ponteiro para a implementação; aqui nada a guardar) e __thread_local_data(),
 * cujo set_pointer inline faz pthread_setspecific(chave, p) */
void thread_struct_ctor(void **ts) __asm__("_ZNSt3__115__thread_structC1Ev");
void thread_struct_ctor(void **ts) { *ts = 0; }
void thread_struct_dtor(void **ts) __asm__("_ZNSt3__115__thread_structD1Ev");
void thread_struct_dtor(void **ts) { (void)ts; }
static void thread_struct_free(void *p) { if (p) cxx_del(p); }
unsigned long *thread_local_data(void) __asm__("_ZNSt3__119__thread_local_dataEv");
unsigned long *thread_local_data(void) {
    static unsigned long key;
    static long once;
    if (__sync_bool_compare_and_swap(&once, 0, 1)) { pthread_key_create(&key, thread_struct_free); __sync_synchronize(); once = 2; }
    while (once != 2) SYS0(SYS_YIELD);
    return &key;
}

/* this_thread::sleep_for(nanoseconds) */
void this_thread_sleep(const i64 *ns) __asm__("_ZNSt3__111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE");
void this_thread_sleep(const i64 *ns) { if (*ns > 0) usleep((unsigned)((*ns + 999) / 1000)); }

/* chrono::system_clock: time_point em microssegundos */
i64 sysclock_now(void) __asm__("_ZNSt3__16chrono12system_clock3nowEv");
i64 sysclock_now(void) { mac_tv_t tv = {0, 0}; gettimeofday(&tv, 0); return (i64)tv.sec * 1000000 + tv.usec; }
long sysclock_to_time_t(const i64 *tp) __asm__("_ZNSt3__16chrono12system_clock9to_time_tERKNS0_10time_pointIS1_NS0_8durationIxNS_5ratioILl1ELl1000000EEEEEEE");
long sysclock_to_time_t(const i64 *tp) { return (long)(*tp / 1000000); }

/* __next_prime(n): o menor primo >= n (tabelas de hash do unordered_map) */
usz next_prime(usz n) __asm__("_ZNSt3__112__next_primeEm");
usz next_prime(usz n) {
    if (n <= 2) return 2;
    for (usz c = n | 1;; c += 2) {
        int prime = 1;
        for (usz d = 3; d * d <= c; d += 2) if (c % d == 0) { prime = 0; break; }
        if (prime) return c;
    }
}

/* ================================================================ shared_ptr / weak_ptr */
typedef struct { void **vt; volatile long owners, weak; } swc_t;
void swc_dtor(swc_t *s) __asm__("_ZNSt3__119__shared_weak_countD2Ev");
void swc_dtor(swc_t *s) { (void)s; }
void swc_release_weak(swc_t *s) __asm__("_ZNSt3__119__shared_weak_count14__release_weakEv");
void swc_release_weak(swc_t *s) {
    if (__atomic_load_n(&s->weak, __ATOMIC_ACQUIRE) == 0 || __atomic_sub_fetch(&s->weak, 1, __ATOMIC_ACQ_REL) == -1)
        ((void (*)(swc_t *))s->vt[4])(s);                 /* __on_zero_shared_weak */
}
swc_t *swc_lock(swc_t *s) __asm__("_ZNSt3__119__shared_weak_count4lockEv");
swc_t *swc_lock(swc_t *s) {
    long o = __atomic_load_n(&s->owners, __ATOMIC_RELAXED);
    while (o != -1) {
        if (__atomic_compare_exchange_n(&s->owners, &o, o + 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) return s;
    }
    return 0;
}
const void *swc_get_deleter(const swc_t *s, const void *ti) __asm__("_ZNKSt3__119__shared_weak_count13__get_deleterERKSt9type_info");
const void *swc_get_deleter(const swc_t *s, const void *ti) { (void)s; (void)ti; return 0; }

/* ================================================================ exceções (sem desenrolamento: abortam) */
typedef struct { void **vt; const char *msg; } cxx_err_t;
static const char *dup(const char *s) { usz n = strlen(s) + 1; char *d = cxx_new(n); memcpy(d, s, n); return d; }

const char *exc_what(const void *e) __asm__("_ZNKSt9exception4whatEv");
const char *exc_what(const void *e) { (void)e; return "std::exception"; }
void exc_dtor(void *e) __asm__("_ZNSt9exceptionD2Ev");
void exc_dtor(void *e) { (void)e; }
const char *rt_what(const cxx_err_t *e) __asm__("_ZNKSt13runtime_error4whatEv");
const char *rt_what(const cxx_err_t *e) { return e->msg ? e->msg : "std::runtime_error"; }
static void err_dtor(void *e) { (void)e; }
static void err_dtor_del(void *e) { cxx_del(e); }

/* vtables {offset_to_top, typeinfo, D1, D0, what}; o vptr do objeto aponta para a 3ª entrada */
const void *vt_length_error[5] __asm__("_ZTVSt12length_error") = {0, 0, (void *)err_dtor, (void *)err_dtor_del, (void *)rt_what};
const void *vt_out_of_range[5] __asm__("_ZTVSt12out_of_range") = {0, 0, (void *)err_dtor, (void *)err_dtor_del, (void *)rt_what};
const void *vt_bad_weak_ptr[5] __asm__("_ZTVNSt3__112bad_weak_ptrE") = {0, 0, (void *)err_dtor, (void *)err_dtor_del, (void *)exc_what};
static const void *vt_logic_error[5] = {0, 0, (void *)err_dtor, (void *)err_dtor_del, (void *)rt_what};
static const void *vt_runtime_error[5] = {0, 0, (void *)err_dtor, (void *)err_dtor_del, (void *)rt_what};
static const void *vt_bad_array_new[5] = {0, 0, (void *)err_dtor, (void *)err_dtor_del, (void *)exc_what};

void logic_error_ctor(cxx_err_t *e, const char *m) __asm__("_ZNSt11logic_errorC2EPKc");
void logic_error_ctor(cxx_err_t *e, const char *m) { e->vt = (void **)&vt_logic_error[2]; e->msg = dup(m ? m : ""); }
void runtime_error_ctor(cxx_err_t *e, const char *m) __asm__("_ZNSt13runtime_errorC2EPKc");
void runtime_error_ctor(cxx_err_t *e, const char *m) { e->vt = (void **)&vt_runtime_error[2]; e->msg = dup(m ? m : ""); }
void runtime_error_dtor(cxx_err_t *e) __asm__("_ZNSt13runtime_errorD2Ev");
void runtime_error_dtor(cxx_err_t *e) { (void)e; }
void length_error_dtor(cxx_err_t *e) __asm__("_ZNSt12length_errorD1Ev");
void length_error_dtor(cxx_err_t *e) { (void)e; }
void out_of_range_dtor(cxx_err_t *e) __asm__("_ZNSt12out_of_rangeD1Ev");
void out_of_range_dtor(cxx_err_t *e) { (void)e; }
void bad_weak_ptr_dtor(void *e) __asm__("_ZNSt3__112bad_weak_ptrD1Ev");
void bad_weak_ptr_dtor(void *e) { (void)e; }
void bad_array_new_ctor(void **e) __asm__("_ZNSt20bad_array_new_lengthC1Ev");
void bad_array_new_ctor(void **e) { *e = (void *)&vt_bad_array_new[2]; }
void bad_array_new_dtor(void *e) __asm__("_ZNSt20bad_array_new_lengthD1Ev");
void bad_array_new_dtor(void *e) { (void)e; }

/* error_category genérico: vtable {D1, D0, name, default_error_condition, equivalent x2, message} */
typedef struct { void **vt; } cxx_cat_t;
static const char *cat_name(const cxx_cat_t *c) { (void)c; return "generic"; }
static void cat_message(cxx_str_t *ret, const cxx_cat_t *c, int ev) { (void)c; s_init(ret, strerror(ev)); }
static u64 cat_default_cond(const cxx_cat_t *c, int ev) { (void)c; return (u64)(unsigned)ev; }   /* não usado */
static u8 cat_equiv(const cxx_cat_t *c, int a, const void *b) { (void)c; (void)a; (void)b; return 0; }
static const void *vt_generic_cat[9] = {0, 0, (void *)err_dtor, (void *)err_dtor, (void *)cat_name, (void *)cat_default_cond,
                                        (void *)cat_equiv, (void *)cat_equiv, (void *)cat_message};
static cxx_cat_t generic_cat = {(void **)&vt_generic_cat[2]};
const cxx_cat_t *generic_category(void) __asm__("_ZNSt3__116generic_categoryEv");
const cxx_cat_t *generic_category(void) { return &generic_cat; }

typedef struct { void **vt; const char *msg; int code; const cxx_cat_t *cat; } cxx_syserr_t;
void system_error_ctor(cxx_syserr_t *e, int code, const cxx_cat_t *cat, const char *what) __asm__("_ZNSt3__112system_errorC1ENS_10error_codeEPKc");
void system_error_ctor(cxx_syserr_t *e, int code, const cxx_cat_t *cat, const char *what) {
    e->vt = (void **)&vt_runtime_error[2]; e->msg = dup(what ? what : ""); e->code = code; e->cat = cat;
}
void system_error_dtor(void *e) __asm__("_ZNSt3__112system_errorD1Ev");
void system_error_dtor(void *e) { (void)e; }
void throw_system_error(int ev, const char *what) __asm__("_ZNSt3__120__throw_system_errorEiPKc");
void throw_system_error(int ev, const char *what) { cxx_fatal("std::system_error", what ? what : strerror(ev)); }

void cxx_terminate(void) __asm__("_ZSt9terminatev");
void cxx_terminate(void) { cxx_fatal("std::terminate", 0); }

/* libc++abi: o objeto da exceção vive num bloco com espaço reservado para um cabeçalho (como o real) */
#define EXC_HDR 128
void *__cxa_allocate_exception(usz n) { u8 *p = malloc(EXC_HDR + n); if (!p) cxx_fatal("bad_alloc", "__cxa_allocate_exception"); memset(p, 0, EXC_HDR); return p + EXC_HDR; }
void __cxa_free_exception(void *p) { if (p) free((u8 *)p - EXC_HDR); }
/* __cxa_throw, __cxa_begin_catch, __cxa_end_catch, __cxa_rethrow e __gxx_personality_v0: cxx_unwind.c */

/* vtables de type_info do ABI (__class_type_info etc.): os typeinfo dos módulos apontam para elas.
 * Sem RTTI de verdade (dynamic_cast/catch); entradas nulas. */
const void *vt_class_ti[16] __asm__("_ZTVN10__cxxabiv117__class_type_infoE");
const void *vt_si_class_ti[16] __asm__("_ZTVN10__cxxabiv120__si_class_type_infoE");
const void *vt_vmi_class_ti[16] __asm__("_ZTVN10__cxxabiv121__vmi_class_type_infoE");

/* ================================================================ std::locale e numpunct<char> (o fmt do spdlog) */
/* locale::id {once_flag; int id}; numpunct<char> com a vtable {D1, D0, on_zero_shared, do_decimal_point,
 * do_thousands_sep, do_grouping(sret), do_truename(sret), do_falsename(sret)} */
u64 numpunct_char_id[2] __asm__("_ZNSt3__18numpunctIcE2idE");
static char np_decimal(const void *f) { (void)f; return '.'; }
static char np_thousands(const void *f) { (void)f; return ','; }
static void np_grouping(cxx_str_t *ret, const void *f) { (void)f; s_init(ret, ""); }
static void np_true(cxx_str_t *ret, const void *f) { (void)f; s_init(ret, "true"); }
static void np_false(cxx_str_t *ret, const void *f) { (void)f; s_init(ret, "false"); }
static void np_noop(void *f) { (void)f; }
static const void *vt_numpunct[10] = {0, 0, (void *)np_noop, (void *)np_noop, (void *)np_noop, (void *)np_decimal,
                                      (void *)np_thousands, (void *)np_grouping, (void *)np_true, (void *)np_false};
/* facet {vptr; long owners (shared_count)}: nunca é liberado */
static struct { void **vt; long owners; char dp, ts; } numpunct_c = {(void **)&vt_numpunct[2], 0x3fffffff, '.', ','};
struct { long refs; } cx_locale_imp = {1};

void *cx_locale_classic_imp(void) { return &cx_locale_imp; }
void locale_ctor(void **l) __asm__("_ZNSt3__16localeC1Ev");
void locale_ctor(void **l) { *l = &cx_locale_imp; }
void locale_copy(void **l, void *const *o) __asm__("_ZNSt3__16localeC1ERKS0_");
void locale_copy(void **l, void *const *o) { *l = *o; }
void locale_dtor(void **l) __asm__("_ZNSt3__16localeD1Ev");
void locale_dtor(void **l) { (void)l; }
extern const void *cx_locale_facet(void *id);
const void *locale_use_facet(void *const *l, void *id) __asm__("_ZNKSt3__16locale9use_facetERNS0_2idE");
const void *locale_use_facet(void *const *l, void *id) {
    const void *f;
    (void)l;
    if (id == (void *)numpunct_char_id) return &numpunct_c;
    f = cx_locale_facet(id);
    if (f) return f;
    cxx_fatal("std::bad_cast", "locale::use_facet");
    return 0;
}
