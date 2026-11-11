/* CGEventPostToPid sem ObjC e sem syscall (testável no host: tools/cgpid_host.c).
 *
 * No macOS o evento dirigido a um pid vai só para aquele processo: não passa pelos taps de sessão nem de HID dos
 * outros processos, não muda o foco e não move o cursor. Aqui:
 *   - cx_cgpid_taps diz se os taps entram (só quando o alvo é o próprio processo);
 *   - cx_cgpid_packets monta os pacotes de DEV_INPUT_TO_PID (compat_gui.h), todos com o pid do alvo.
 * cgevent.c manda cada pacote com SYS4(GUI_DEVICE, DEV_INPUT_TO_PID, b, c, d). */
#pragma once

typedef struct {
    unsigned type;            /* CGEventType: 10/11 tecla, 1..7 e 25..27 mouse, 22 rolagem */
    unsigned mods;            /* modificadores no formato do kernel (mos_mods) */
    int key;                  /* tecla do mOS (mos_key), ou -1 */
    const unsigned short *uni;/* CGEventKeyboardSetUnicodeString */
    unsigned nuni;
    int wheel;                /* linhas, já com o sinal do kernel */
    int x, y;                 /* posição global */
} cgpid_ev;

typedef struct { unsigned long long b, c, d; } cgpid_pkt;

/* 1 = os taps (o local) recebem o evento antes; 0 = não passam por tap nenhum. Nunca há fan-out para outros
 * processos: o tap de outro app não vê o evento dirigido a este. */
int cx_cgpid_taps(int target_pid, int self_pid);
/* pacotes em out (até cap). Devolve quantos; 0 = nada a mandar (tecla sem mapa, tipo desconhecido, pid inválido). */
int cx_cgpid_packets(const cgpid_ev *e, int pid, cgpid_pkt *out, int cap);
