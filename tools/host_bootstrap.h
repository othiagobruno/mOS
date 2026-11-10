#ifndef HOST_BOOTSTRAP_H
#define HOST_BOOTSTRAP_H
/* SYS_MACH_BOOTSTRAP como kernel/proc/machipc.c bootstrap_op.
 * check_in de um nome novo cria o serviço e devolve o direito de receber.
 * look_up acha esse nome e devolve o mesmo porto (o direito de enviar). */
long host_bootstrap(unsigned op, const char *name, unsigned *port, unsigned (*new_recv)(void));
#endif
