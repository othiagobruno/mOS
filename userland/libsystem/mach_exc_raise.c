/*
 * Rotinas MIG cliente de exceção Mach (mach/exc.h): exception_raise, exception_raise_state e
 * exception_raise_state_identity mandam a exceção ao exception port seguinte. O mOS não encaminha
 * exceções Mach entre portas, então não há destino: devolvem MACH_SEND_INVALID_DEST, como o
 * mach_msg quando o port não existe. Antes eram stubs void (sys_zerados.c), e o chamador lia lixo
 * em rax como kern_return_t (o reporter de falhas do app podia entender "tratada").
 * Quem chama (PLCrashReporter, Sentry, o Postico): mandam a exceção adiante e seguem o próprio
 * fluxo se o envio falha.
 */
typedef int kern_return_t;
typedef unsigned int mach_port_t;
typedef int exception_type_t;
typedef int *exception_data_t;
typedef unsigned int mach_msg_type_number_t;
typedef unsigned int *thread_state_t;

#define MACH_SEND_INVALID_DEST 0x10000003

kern_return_t exception_raise(mach_port_t exception_port, mach_port_t thread, mach_port_t task,
                              exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_cnt) {
    (void)exception_port; (void)thread; (void)task; (void)exception; (void)code; (void)code_cnt;
    return MACH_SEND_INVALID_DEST;
}

kern_return_t exception_raise_state(mach_port_t exception_port, exception_type_t exception, exception_data_t code,
                                    mach_msg_type_number_t code_cnt, int *flavor, thread_state_t old_state,
                                    mach_msg_type_number_t old_cnt, thread_state_t new_state,
                                    mach_msg_type_number_t *new_cnt) {
    (void)exception_port; (void)exception; (void)code; (void)code_cnt; (void)flavor; (void)old_state; (void)old_cnt;
    (void)new_state;
    if (new_cnt) *new_cnt = 0;
    return MACH_SEND_INVALID_DEST;
}

kern_return_t exception_raise_state_identity(mach_port_t exception_port, mach_port_t thread, mach_port_t task,
                                             exception_type_t exception, exception_data_t code,
                                             mach_msg_type_number_t code_cnt, int *flavor, thread_state_t old_state,
                                             mach_msg_type_number_t old_cnt, thread_state_t new_state,
                                             mach_msg_type_number_t *new_cnt) {
    (void)exception_port; (void)thread; (void)task; (void)exception; (void)code; (void)code_cnt; (void)flavor;
    (void)old_state; (void)old_cnt; (void)new_state;
    if (new_cnt) *new_cnt = 0;
    return MACH_SEND_INVALID_DEST;
}
