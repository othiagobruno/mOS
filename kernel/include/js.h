/* mOS — motor JavaScript do Horizonte ("Brisa"): interface pública para o navegador
 *
 * Cada documento com scripts ganha uma "página JS" (js_page_t): um reino (realm) próprio com
 * objeto global window, DOM ligado ao doc_t, temporizadores, fila de microtarefas e console.
 * Toda execução tem orçamento de tempo; erros nunca derrubam o navegador (viram mensagens no console). */
#pragma once
#include "web.h"

typedef struct js_page js_page_t;

extern bool js_enabled;                 /* Visualizar ▸ Ativar JavaScript (padrão: ligado) */

/* cria a página JS de um documento recém-carregado e executa os scripts inline
 * (os externos são baixados e executados em ordem pelo js_page_tick) */
js_page_t *js_page_new(doc_t *d, const char *url, int view_w, int view_h);
void       js_page_free(js_page_t *p);  /* antes de doc_free */

/* chamado periodicamente (tick do app): scripts externos, temporizadores, microtarefas.
 * Devolve true se o DOM mudou (precisa repintar). */
bool js_page_tick(js_page_t *p);
int  js_page_next_ms(js_page_t *p);     /* ms até o próximo temporizador (-1 = nenhum) */
void js_page_viewport(js_page_t *p, int w, int h, int scroll);

/* eventos vindos da interface; devolvem true se o script chamou preventDefault() */
enum { JSE_CLICK, JSE_INPUT, JSE_CHANGE, JSE_SUBMIT, JSE_KEYDOWN, JSE_KEYUP, JSE_FOCUS, JSE_BLUR };
bool js_page_event(js_page_t *p, node_t *target, int type, int key, u16 mods);
node_t *js_hit_node(doc_t *d, int x, int y);   /* elemento mais profundo sob o ponto (coord. do documento) */
void js_page_eval_url(js_page_t *p, const char *code); /* links javascript: */

/* navegação pedida por location.href = ... / reload(); devolve NULL se nenhuma */
const char *js_page_take_nav(js_page_t *p, bool *reload);
/* rolagem pedida por window.scrollTo (-1 = nenhuma) */
int  js_page_take_scroll(js_page_t *p);
/* formulário cujo envio foi pedido por form.submit() (NULL = nenhum) */
node_t *js_page_take_submit(js_page_t *p);
/* nome legível da tecla para KeyboardEvent.key */
void js_key_name(int key, char *out, int cap);

/* console da página (texto acumulado) */
const char *js_page_console(js_page_t *p);
int  js_page_errors(js_page_t *p);

/* uso do heap do motor (para diagnóstico) */
usize js_heap_bytes(void);
