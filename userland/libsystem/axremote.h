/* AX entre processos, a parte sem ObjC (testável no host: tools/axremote_host.c).
 *
 * - Transporte com reserva: o pedido vai pelo vtable vinculado (xpc-mach); se o fio falhar (60, 61, ...), tenta o
 *   de reserva (af-unix). O servidor escuta nos dois, então um cliente que só tem AF_UNIX ainda acha o app.
 * - Paginação: um ax_msg leva 12 filhos e 95 bytes de texto. A resposta diz o total em len e o início em loc; o
 *   cliente pede de novo com loc = o que já tem até completar. Servidor antigo (len 0) vale como página única.
 * - Elemento único (AXFocusedUIElement, AXFocusedWindow, AXMainWindow, AXParent, o "elemento na posição"): vai como
 *   AX_PAY_KIDS com um filho e loc = -1.
 * - Dono da janela num ponto (coordenadas do compositor, origem no canto superior esquerdo), para o
 *   AXUIElementCopyElementAtPosition do system-wide perguntar ao processo certo. */
#pragma once
#include "axproto.h"
#include "axtransport.h"

#define AX_SINGLE (-1)            /* loc da resposta: um elemento só, não uma lista */
#define AX_PAGE_KIDS_MAX 512      /* teto de filhos que o cliente junta */
#define AX_PAGE_STR_MAX 65536     /* teto de bytes de texto que o cliente junta */

/* reserva do transporte (o af-unix quando o vinculado é o xpc-mach) */
void cx_ax_transport_set_fallback(const cx_ax_transport *t);
const cx_ax_transport *cx_ax_transport_fallback(void);

/* pedido e resposta, com a reserva. 0 = ok (rep decodificado); -25204 = nenhum fio respondeu. */
int cx_ax_call(int pid, const ax_msg *req, ax_msg *rep);
/* lista inteira: junta as páginas. *n = filhos lidos. Devolve o status do servidor ou -25204. */
int cx_ax_call_kids(int pid, const ax_msg *req, int *kinds, int *ids, int cap, int *n, ax_msg *first);
/* texto inteiro: junta as páginas em out (sempre terminado em 0). */
int cx_ax_call_str(int pid, const ax_msg *req, char *out, int cap, ax_msg *first);

/* servidor: uma página da lista (a partir de offset) ou do texto */
void ax_page_kids(ax_msg *rep, const int *kinds, const int *ids, int n, int offset);
void ax_page_str(ax_msg *rep, const char *s, int offset);
void ax_page_single(ax_msg *rep, int kind, int id);

/* janela da frente que contém (x, y). at(i, out) é cx_cg_window_at (0 = acabou). 1 = achou. */
typedef struct { unsigned number; int pid, layer, onscreen, x, y, w, h; } ax_win_t;
int ax_window_owner_at(int (*at)(int index, ax_win_t *out), int x, int y, int *pid, unsigned *wid);
