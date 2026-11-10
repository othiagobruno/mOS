#pragma once

/* Lógica de credenciais e de modo, compartilhada pelo kernel e pelo teste de host.
 * handlers[s] == 1 é ignorar. extra: 1 SETSIGDEF, 2 SETSIGMASK, 4 RESETIDS.
 * O bit 0x10000 no modo marca "foi gravado" (chmod 0000 continua 0000). */
void cred_spawn_apply(unsigned long long handlers[32], unsigned *mask,
                      int *uid, int *euid, int *suid, int *gid, int *egid, int *sgid,
                      unsigned sigdef, unsigned sigmask, unsigned extra);
unsigned mode_for_creat(unsigned req);
void mode_apply(unsigned *slot, unsigned mode);
unsigned mode_stat(unsigned slot, int is_dir);
int cred_seteid(int real, int saved, int *eff, int neu);
