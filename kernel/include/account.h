/* mOS — contas de usuário (lib/account.c). Contas ficam em /Sistema/Biblioteca/Usuarios/<nome>.conta
 * (persistidas no disco do sistema) com a senha guardada como SHA-256 iterado com sal aleatório.
 * "Visitante" é a conta padrão: sempre existe e não tem senha, a menos que alguém defina uma.
 * Todas as funções usam o VFS: chame do laço do desktop. */
#pragma once
#include "kernel.h"

#define ACCOUNT_DEFAULT "Visitante"

int         account_count(void);
bool        account_get(int i, char *name, usize ncap, char *fullname, usize fcap, bool *admin, bool *has_password);
bool        account_exists(const char *name);
bool        account_verify(const char *name, const char *password);       /* senha vazia vale para contas sem senha */
bool        account_create(const char *name, const char *fullname, const char *password, bool admin,
                           char *err, usize errlen);                       /* cria também /Users/<nome> */
bool        account_set_password(const char *name, const char *old_password, const char *new_password,
                                 char *err, usize errlen);                 /* new "" = remove a senha */
bool        account_delete(const char *name, bool remove_home, char *err, usize errlen);
bool        account_set_current(const char *name);                         /* muda o HOME efetivo */
const char *account_current(void);                                         /* nome da conta em uso */
const char *account_fullname(void);
const char *account_home(void);                                            /* "/Users/<nome>" */
bool        account_is_admin(const char *name);
