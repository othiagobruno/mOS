/* mOS — contas de usuário (lib/account.c). Contas ficam em /Sistema/Biblioteca/Usuarios/<nome>.conta
 * (persistidas no disco do sistema) com a senha guardada como PBKDF2-HMAC-SHA256 com sal do CSPRNG (formato 2; o formato 1,
 * SHA-256 iterado, só é aceito para verificar e é migrado ao entrar). "Visitante" só existe no live EFÊMERO (nada é gravado);
 * em sistema instalado ou live persistente ele não existe, para que ninguém entre sem senha.
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
void        account_ensure_dirs(const char *home);                         /* Mesa, Documentos e ~/Library/{Preferences,Application Support,...} */
bool        account_is_admin(const char *name);
u32         account_kdf_iterations(void);                                  /* PBKDF2: iterações calibradas neste hardware */
bool        account_password_ok(const char *pw, char *err, usize errlen);  /* regra do assistente: 8 a 60 caracteres */
