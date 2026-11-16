/* mOS — teste de host dos diálogos JS bloqueantes: a execução para em confirm/prompt/alert dentro de uma costack,
 * outra página roda JS e força o coletor enquanto ela espera, e a resposta do "app" retoma o script do ponto exato. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

void gc_collect(void);

typedef struct { int kind; char msg[128]; char def[64]; bool has_def; int reply_ok; char reply[64]; int asked; } dlg_t;
static dlg_t D;

static int hook(void *ctx, js_page_t *p, int kind, const char *msg, const char *def, sbuf_t *out) {
    (void)ctx; (void)p;
    D.kind = kind;
    snprintf(D.msg, sizeof D.msg, "%s", msg);
    D.has_def = def != NULL;
    snprintf(D.def, sizeof D.def, "%s", def ? def : "");
    D.asked++;
    if (!cs_current()) return 0;                 /* fora de corrotina: não bloqueia */
    void *e = js_suspend_exec();
    cs_yield();
    js_resume_exec(e);
    if (kind == JS_DLG_PROMPT && D.reply_ok) sb_puts(out, D.reply);
    return D.reply_ok;
}

typedef struct { js_page_t *p; const char *code; sbuf_t out; int type; } run_t;
static void runner(void *a) {
    run_t *r = a;
    r->type = js_page_eval_value(r->p, r->code, (int)strlen(r->code), &r->out);
}

static js_page_t *page(const char *html, doc_t **dout) {
    doc_t *d = doc_parse(html, strlen(html), "http://teste/d.html");
    *dout = d;
    return js_page_new2(d, "http://teste/d.html", 300, 200, NULL, NULL);
}

WEB_HOST_CASE(dialogo_bloqueante) {
    doc_t *da, *db;
    js_page_t *a = page("<html><body><script>"
                        "function go(){ var big=[]; for (var i=0;i<3000;i++) big.push({i:i, s:'x'+i});"
                        "  var log=['a']; var c=confirm('Continuar?'); log.push(c);"
                        "  var t=prompt('Nome', 'Zé'); log.push(t); var t2=prompt('Outro');"
                        "  log.push(t2); alert('fim'); log.push(big.length + ':' + big[2999].s); return log.join(','); }"
                        "</script></body></html>", &da);
    js_page_set_dialog(a, hook, NULL);
    memset(&D, 0, sizeof D);
    run_t r = { a, "go()", { 0 }, 0 };
    costack_t *c = cs_new(1536 * 1024, runner, &r);
    CHECK(c != NULL, "costack");
    if (!c) return;
    bool done = cs_resume(c);
    CHECK(!done && D.kind == JS_DLG_CONFIRM && !strcmp(D.msg, "Continuar?"), "parou no confirm: done=%d kind=%d msg=%s", done, D.kind, D.msg);
    /* outra página roda JS e o coletor passa enquanto a primeira espera */
    js_page_t *b = page("<html><body><script>var junk=[]; for (var i=0;i<20000;i++) junk.push({k:i}); junk=null;</script></body></html>", &db);
    gc_collect();
    sbuf_t o = { 0 };
    int t = js_page_eval_value(b, "[1,2,3].map(function(x){return x*x}).join('-')", 46, &o);
    CHECK(t == JS_WK_STRING && o.p && !strcmp(o.p, "1-4-9"), "outra página rodou durante o diálogo: %s", o.p ? o.p : "");
    sb_free(&o);
    gc_collect();
    /* responde OK ao confirm: para no prompt com o texto padrão */
    D.reply_ok = 1;
    done = cs_resume(c);
    CHECK(!done && D.kind == JS_DLG_PROMPT && D.has_def && !strcmp(D.def, "Zé"), "parou no prompt: %d %s", D.kind, D.def);
    D.reply_ok = 1; snprintf(D.reply, sizeof D.reply, "Ana");
    done = cs_resume(c);
    CHECK(!done && D.kind == JS_DLG_PROMPT && !D.has_def, "segundo prompt sem texto padrão");
    gc_collect();
    D.reply_ok = 0;                      /* cancelar → null */
    done = cs_resume(c);
    CHECK(!done && D.kind == JS_DLG_ALERT && !strcmp(D.msg, "fim"), "parou no alert: %s", D.msg);
    D.reply_ok = 1;
    done = cs_resume(c);
    CHECK(done, "terminou depois do alert");
    CHECK(r.type == JS_WK_STRING && r.out.p && !strcmp(r.out.p, "a,true,Ana,,3000:x2999"), "resultado: %d %s", r.type, r.out.p ? r.out.p : "");
    CHECK(D.asked == 4, "4 diálogos: %d", D.asked);
    sb_free(&r.out);
    cs_free(c);
    /* fora de corrotina o gancho não bloqueia: confirm devolve o que ele devolver (0 = false) */
    sbuf_t o2 = { 0 };
    t = js_page_eval_value(a, "String(confirm('x'))", 20, &o2);
    CHECK(t == JS_WK_STRING && o2.p && !strcmp(o2.p, "false"), "sem corrotina: %s", o2.p ? o2.p : "");
    sb_free(&o2);
    js_page_free(b); doc_free(db);
    js_page_free(a); doc_free(da);
}

WEB_HOST_CASE(dialogo_pagina_fechada) {
    /* a página do diálogo é fechada enquanto ele está aberto: a retomada encerra o script sem tocar no reino morto */
    doc_t *da;
    js_page_t *a = page("<html><body></body></html>", &da);
    js_page_set_dialog(a, hook, NULL);
    memset(&D, 0, sizeof D);
    run_t r = { a, "alert('a'); 1+1", { 0 }, 0 };
    costack_t *c = cs_new(1536 * 1024, runner, &r);
    bool done = cs_resume(c);
    CHECK(!done && D.kind == JS_DLG_ALERT, "parou no alert");
    D.reply_ok = 1;
    done = cs_resume(c);
    CHECK(done, "terminou");
    sb_free(&r.out);
    cs_free(c);
    js_page_free(a); doc_free(da);
}

WEB_HOST_CASE(dialogo_em_async) {
    /* confirm dentro de uma função async (corrotina do motor) e o coletor rodando enquanto espera */
    doc_t *da;
    js_page_t *a = page("<html><body><script>window.r='';"
                        "async function f(){ var o=[]; for(var i=0;i<2000;i++) o.push('v'+i); await null; var c=confirm('x');"
                        " window.r = c + ':' + o[1999]; }</script></body></html>", &da);
    js_page_set_dialog(a, hook, NULL);
    memset(&D, 0, sizeof D);
    run_t r = { a, "f(); 0", { 0 }, 0 };
    costack_t *c = cs_new(1536 * 1024, runner, &r);
    bool done = cs_resume(c);
    /* o await adia para a fila de microtarefas: roda dentro do mesmo eval (js_run_jobs) e para no confirm */
    CHECK(!done && D.kind == JS_DLG_CONFIRM, "parou no confirm dentro do async (done=%d asked=%d)", done, D.asked);
    gc_collect();
    D.reply_ok = 1;
    if (!done) done = cs_resume(c);
    CHECK(done, "terminou");
    sb_free(&r.out);
    sbuf_t o = { 0 };
    js_page_eval_value(a, "window.r", 8, &o);
    CHECK(o.p && !strcmp(o.p, "true:v1999"), "resultado do async: %s", o.p ? o.p : "");
    sb_free(&o);
    cs_free(c);
    js_page_free(a); doc_free(da);
}

WEB_HOST_CASE(geradores_e_async) {
    /* as corrotinas do motor (js_gen.c) agora rodam também no host: geradores, yield*, async/await */
    doc_t *d;
    js_page_t *a = page("<html><body><script>"
                        "function* g(){ var x = yield 1; yield x*10; yield* [7,8]; return 9; }"
                        "var it=g(), out=[it.next().value, it.next(4).value, it.next().value, it.next().value, it.next().value, it.next().done];"
                        "window.gen=out.join(',');"
                        "async function h(){ var s=0; for (var i=0;i<5;i++) s += await Promise.resolve(i); return s; }"
                        "h().then(function(v){ window.as=v; });"
                        "</script></body></html>", &d);
    for (int i = 0; i < 5; i++) js_page_tick(a);
    sbuf_t o = { 0 };
    js_page_eval_value(a, "window.gen + '|' + window.as", 28, &o);
    CHECK(o.p && !strcmp(o.p, "1,40,7,8,9,true|10"), "geradores/async: %s", o.p ? o.p : "");
    sb_free(&o);
    js_page_free(a); doc_free(d);
}
