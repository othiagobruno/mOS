/* Transporte de bytes entre processos, por bootstrap Mach e o fio XPC desta lib
 * (userland/libsystem/xpc.c). Não abre AF_UNIX e não mexe em sockets.c.
 *
 * A Interface pluga isto no vtable de axtransport.h. Este cabeçalho não inclui
 * arquivos de AX e não chama cx_ax_transport_bind:
 *
 *   cx_ax_transport t;
 *   t.name = "xpc-mach";
 *   t.ready = cx_xpc_peer_ready;
 *   t.call = cx_xpc_peer_call;
 *   t.send = cx_xpc_peer_send;
 *   t.listen = cx_xpc_peer_listen;
 *   cx_ax_transport_bind(&t);
 *
 * O serviço de cada processo é "com.mos.xpc.peer.<pid>". call é um pedido com
 * resposta: o send feito dentro do listen, de volta ao pid de origem, é essa
 * resposta. send fora do listen é aviso sem espera.
 * 0 = ok. 22 argumento, 61 ninguém escutando, 60 prazo (10 s), 12 sem memória.
 */
#pragma once

int cx_xpc_peer_ready(void);
/* cx_xpc_peer_ready(); */

int cx_xpc_peer_call(int pid, const void *req, int n, void *reply, int cap, int *out_n);
/* int rn = 0; cx_xpc_peer_call(pid, req, nreq, reply, cap, &rn); */

int cx_xpc_peer_send(int pid, const void *bytes, int n);
/* cx_xpc_peer_send(from_pid, reply, rn); */

int cx_xpc_peer_listen(void (*fn)(int from_pid, const void *bytes, int n, void *ctx), void *ctx);
/* cx_xpc_peer_listen(on_bytes, 0); */
