/* mOS — estado do sistema em execução (fs/persist.c). O estado vem do arquivo /mOS/Sistema/Biblioteca/estado-sistema
 * do volume de origem CONFIÁVEL; sem volume confiável (RAM / origem ambígua) o estado é sempre SYS_EPHEMERAL. */
#pragma once
#include "kernel.h"

enum {
    SYS_EPHEMERAL,      /* live: nada é gravado em nenhum disco (padrão; também RAM-only e origem ambígua) */
    SYS_LIVE_PERSIST,   /* live em pendrive USB com persistência consentida pelo usuário */
    SYS_FIRSTBOOT,      /* sistema recém-instalado: falta criar a conta e a senha (no primeiro boot, sem o pendrive) */
    SYS_INSTALLED,      /* sistema instalado e configurado */
    SYS_INCOMPLETE      /* instalação interrompida: nunca é tratada como instalada */
};
int         sysstate(void);
const char *sysstate_name(void);
bool        sysstate_guest_allowed(void);   /* a conta Visitante (sem senha) só existe no live efêmero */
bool        persist_writable(void);         /* a gravação no volume do sistema está liberada */
bool        persist_consent_live(void);     /* live USB: usuário consentiu em gravar no pendrive */
bool        persist_set_state(int st);
