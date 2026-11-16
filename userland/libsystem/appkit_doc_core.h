/* mOS — núcleo do NSDocument/NSDocumentController (sem Objective-C): contagem de mudanças, nome dos documentos
 * sem título, recentes, tipos de documento do Info.plist e título da janela.
 * Teste de host: userland/tests-appkit/document_host.c (-DCX_DC_HOST). */
#pragma once
/* NSDocumentChangeType */
enum { DC_DONE = 0, DC_UNDONE = 1, DC_CLEARED = 2, DC_READ_OTHER = 3, DC_AUTOSAVED = 4, DC_REDONE = 5, DC_DISCARDABLE = 256 };
typedef struct { long change, saved_at_autosave; int read_other, discardable; } dc_count_t;
void dc_update(dc_count_t *c, int type);
int dc_edited(const dc_count_t *c);
int dc_has_unautosaved(const dc_count_t *c);

/* "Sem título", "Sem título 2", ... que nenhum dos nomes em uso tem */
void dc_untitled_name(const char *base, const char *const *used, int nused, char *out, int cap);

/* recentes: o mais novo primeiro, sem repetição, até max */
#define DC_MRU_CAP 100
typedef struct { char *item[DC_MRU_CAP]; int n, max; } dc_mru_t;
void dc_mru_init(dc_mru_t *m, int max);
void dc_mru_add(dc_mru_t *m, const char *s);
void dc_mru_set_max(dc_mru_t *m, int max);
void dc_mru_clear(dc_mru_t *m);

/* CFBundleDocumentTypes */
enum { DC_ROLE_NONE, DC_ROLE_VIEWER, DC_ROLE_EDITOR, DC_ROLE_SHELL };
typedef struct {
    char name[128], cls[128];
    char ext[16][24]; int next;
    char uti[8][128]; int nuti;
    int role;
} dc_type_t;
void dc_type_init(dc_type_t *t, const char *name, const char *role, const char *cls);
void dc_type_add_ext(dc_type_t *t, const char *ext);
void dc_type_add_uti(dc_type_t *t, const char *uti);
/* o tipo que lê a extensão (sem caixa; "*" aceita tudo), preferindo Editor; -1 se nenhum */
int dc_type_for_ext(const dc_type_t *t, int n, const char *ext);
/* o tipo pelo nome ou por uma UTI dele; -1 se nenhum */
int dc_type_named(const dc_type_t *t, int n, const char *name);
/* tipo padrão (newDocument:): o 1º Editor com classe; senão o 1º com classe */
int dc_default_type(const dc_type_t *t, int n);
/* o tipo pode ser gravado (Editor)? */
int dc_type_writable(const dc_type_t *t);

/* título da janela: "nome", e "nome — Editado" com mudanças */
void dc_window_title(const char *display, int edited, char *out, int cap);
/* nome mostrado de uma URL: o último componente; sem a extensão quando hide_ext */
void dc_display_name(const char *path, int hide_ext, char *out, int cap);
