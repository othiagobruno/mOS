/* mOS — textos em dois idiomas e preferências de idioma/teclado do sistema */
#include "i18n.h"
#include "vfs.h"

#define PREFS_DIR  "/Sistema/Biblioteca/Preferencias"
#define PREFS_FILE PREFS_DIR "/com.mos.sistema.cfg"

int g_lang = LANG_PT;

static const struct { const char *pt, *en; } T_[] = {
#define X(id, pt, en) {pt, en},
#include "i18n_strings.h"
#undef X
};

const char *tr_lang(int id, int lang) {
    if (id < 0 || id >= S__COUNT) return "";
    return lang == LANG_EN ? T_[id].en : T_[id].pt;
}
const char *tr(int id) { return tr_lang(id, g_lang); }

const char *i18n_weekday(int wd) {
    static const char *pt[7] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"};
    static const char *en[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    return (g_lang == LANG_EN ? en : pt)[wd % 7];
}
const char *i18n_month(int m) {
    static const char *pt[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
    static const char *en[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    return (g_lang == LANG_EN ? en : pt)[(m + 11) % 12];
}

int sysprefs_format(char *out, usize cap) {
    return snprintf(out, cap, "idioma=%s\nteclado=%s\n", g_lang == LANG_EN ? "en" : "pt", g_kbd_layout == 1 ? "us" : "abnt2");
}

/* grava no VFS; vai para o disco só se a persistência estiver ligada (fs/persist.c) */
void sysprefs_save(void) {
    char b[64];
    int n = sysprefs_format(b, sizeof b);
    vfs_mkdir(PREFS_DIR);
    vfs_write_file(PREFS_FILE, b, (usize)n);
}

void sysprefs_load(void) {
    vnode_t *v = vfs_lookup(PREFS_FILE);
    if (!v || v->dir || !v->data || v->size > 256) return;
    char t[260];
    memcpy(t, v->data, v->size);
    t[v->size] = 0;
    for (char *l = t; l && *l;) {
        char *nl = strchr(l, '\n');
        if (nl) *nl = 0;
        if (!strncmp(l, "idioma=", 7)) g_lang = !strncmp(l + 7, "en", 2) ? LANG_EN : LANG_PT;
        else if (!strncmp(l, "teclado=", 8)) g_kbd_layout = !strncmp(l + 8, "us", 2) ? 1 : 0;
        l = nl ? nl + 1 : NULL;
    }
}
