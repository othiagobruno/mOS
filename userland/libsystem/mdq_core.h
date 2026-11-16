/* mOS libSystem — predicado de consulta do Spotlight (MDQuery), sem CF e sem ObjC (tools/agentes_host.c).
 *
 * Subconjunto da sintaxe de consulta do macOS ("File Metadata Query Expression Syntax"):
 *   atributo == "valor"[cdw]   atributo != "valor"[cdw]   (aspas duplas ou simples, \ escapa)
 *   atributo < > <= >= número
 *   "texto"[cdw] sozinho: compara com kMDItemDisplayName e kMDItemFSName
 *   && || ! e parênteses; true / false
 * O valor aceita * (qualquer sequência). Modificadores: c (sem diferença de caixa), d (sem acento, Latin-1),
 * w (por palavra: o padrão casa no começo de qualquer palavra). Atributo com vários valores
 * (kMDItemContentTypeTree): == casa se algum valor casar; != só se nenhum casar.
 *   InRange(atributo, mínimo, máximo); datas são números (CFAbsoluteTime) e aceitam
 *   $time.now[(s)] .today[(d)] .yesterday .this_week[(w)] .this_month[(m)] .this_year[(a)] .iso(AAAA-MM-DDThh:mm:ssZ)
 * Consulta inválida: mdq_parse devolve erro, e MDQueryCreate devolve NULL, como o macOS. */
#pragma once

typedef struct mdq_node mdq_node;
/* valor número i (0, 1, ...) do atributo no item; NULL quando acabou ou não existe */
typedef const char *(*mdq_get_fn)(void *ctx, const char *attr, int i);

mdq_node *mdq_parse(const char *query, char *err, int errcap);
int mdq_eval(const mdq_node *n, mdq_get_fn get, void *ctx);
/* relógio do $time: agora (CFAbsoluteTime) e o fuso em segundos a leste do UTC */
void mdq_set_clock(double now_abs, long tz_offset);
void mdq_free(mdq_node *n);
/* casamento de um valor com um padrão (os mesmos modificadores); exposto para o teste */
int mdq_match(const char *value, const char *pattern, int nocase, int nodiac, int word);
