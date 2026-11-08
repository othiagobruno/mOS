/* mOS — idioma do assistente/login e preferências do sistema (lib/i18n.c). Por enquanto só pt-BR e inglês, e só nas telas
 * do assistente, de login e do instalador; o restante do sistema continua em português. */
#pragma once
#include "kernel.h"

enum { LANG_PT, LANG_EN };
extern int g_lang;
extern int g_kbd_layout;       /* 0 = ABNT2, 1 = US (dev/input.c) */

enum {
#define X(id, pt, en) id,
#include "i18n_strings.h"
#undef X
    S__COUNT
};
const char *tr(int id);                         /* texto no idioma atual */
const char *tr_lang(int id, int lang);
const char *i18n_weekday(int wd);               /* 0 = domingo */
const char *i18n_month(int m);                  /* 1..12 */

/* preferências do sistema: /Sistema/Biblioteca/Preferencias/com.mos.sistema.cfg (idioma=pt|en, teclado=abnt2|us) */
void sysprefs_load(void);
void sysprefs_save(void);
bool sysprefs_present(void);                    /* o arquivo existe (ex.: gravado pelo instalador com o idioma escolhido) */
int  sysprefs_format(char *out, usize cap);     /* texto do arquivo (usado também pelo instalador) */
