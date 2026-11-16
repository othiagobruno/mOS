/* mOS — núcleo dos painéis de arquivo (NSOpenPanel/NSSavePanel), sem Objective-C e sem disco.
 * A cola (appkit_panel.c) lê a pasta e entrega as entradas; o teste de host (userland/tests-appkit/panel_host.c)
 * usa as mesmas funções. */
#pragma once

#define FP_PATH_MAX 1024
#define FP_NAME_MAX 256

enum { FP_SORT_NAME, FP_SORT_DATE, FP_SORT_SIZE, FP_SORT_KIND };
enum { FP_MOD_SHIFT = 1, FP_MOD_CMD = 2 };
enum { FP_KEY_UP, FP_KEY_DOWN, FP_KEY_HOME, FP_KEY_END, FP_KEY_PAGE_UP, FP_KEY_PAGE_DOWN };
/* resultado de fp_activate */
enum { FP_ACT_NONE, FP_ACT_ENTER_DIR, FP_ACT_CONFIRM };
/* fp_validate_name */
enum { FP_NAME_OK, FP_NAME_EMPTY, FP_NAME_SLASH, FP_NAME_COLON, FP_NAME_DOT, FP_NAME_LONG, FP_NAME_RESERVED };

typedef struct {
    char *name;
    int is_dir;        /* pasta no disco (pacotes incluídos) */
    int is_pkg;        /* pasta com extensão de pacote: aparece como arquivo */
    int hidden;
    int enabled;       /* pode ser escolhida como resultado (abrir) ou navegada (pasta) */
    int vetoed;        /* o delegate recusou (panel:shouldEnableURL:) */
    long long mtime;   /* segundos desde 1970 */
    long long size;
} fp_entry_t;

typedef struct fp_model {
    char dir[FP_PATH_MAX];
    char home[FP_PATH_MAX];
    fp_entry_t *e;
    int n, cap;
    int *row;          /* linha visível -> índice em e */
    int nrows;
    unsigned char *sel;/* por linha visível */
    int anchor, cursor;
    /* opções (nomes do Cocoa) */
    int open_panel;
    int can_files, can_dirs, multi, show_hidden, pkgs_as_dirs, allow_other_types;
    char **exts; int nexts;   /* extensões aceitas, minúsculas, sem ponto; nexts == 0 = todas */
    int types_set;            /* o app passou uma lista (mesmo que só de pastas) */
    int folders_ok;           /* a lista tinha public.folder/public.directory */
    int pkg_ok;               /* a lista tinha com.apple.application-bundle/com.apple.package */
    int all_files;            /* a lista tinha public.item/public.data/public.content */
    int sort_key, sort_desc;
    /* digitar para achar */
    char typed[64]; int ntyped; double typed_at;
} fp_model_t;

void fp_init(fp_model_t *m, int open_panel, const char *home);
void fp_free(fp_model_t *m);

/* caminhos: out tem FP_PATH_MAX bytes. ~ vira home. Devolve 0 se não coube. */
int fp_normalize(const char *path, const char *base, const char *home, char *out);
int fp_parent(const char *path, char *out);
int fp_join(const char *dir, const char *name, char *out);
const char *fp_basename(const char *path);
/* extensão (sem ponto) em minúsculas; "" se não há */
void fp_ext_of(const char *name, char *out, int cap);
int fp_is_pkg_ext(const char *ext);

/* Lista de tipos: cada item é extensão ("png", ".png") ou UTI ("public.image"). Substitui a anterior.
 * n == 0 limpa (todos os tipos). */
void fp_set_types(fp_model_t *m, const char *const *types, int n);
int fp_type_ok(const fp_model_t *m, const char *name);

/* troca de pasta: begin limpa, add acrescenta, end classifica, filtra, ordena e zera a seleção */
void fp_begin_dir(fp_model_t *m, const char *dir);
void fp_add_entry(fp_model_t *m, const char *name, int is_dir, long long mtime, long long size);
void fp_end_dir(fp_model_t *m);
/* refaz filtro e ordem mantendo a seleção pelos nomes (troca de ordenação, mostrar ocultos, veto do delegate) */
void fp_refresh(fp_model_t *m);
void fp_set_sort(fp_model_t *m, int key, int desc);

/* ordem Finder: sem caixa, números pelo valor, acentos latinos (UTF-8 C3xx) dobrados */
int fp_compare_names(const char *a, const char *b);

fp_entry_t *fp_row_entry(const fp_model_t *m, int row);
int fp_row_selectable(const fp_model_t *m, int row);

/* seleção */
void fp_select_none(fp_model_t *m);
void fp_click(fp_model_t *m, int row, int mods);
void fp_key(fp_model_t *m, int key, int shift, int page_rows);
/* digitar para achar: devolve a linha escolhida ou -1. now em segundos. */
int fp_type_ahead(fp_model_t *m, const char *utf8, double now);
int fp_select_name(fp_model_t *m, const char *name);
int fp_selected_count(const fp_model_t *m);
int fp_selected_row(const fp_model_t *m, int k); /* k-ésima linha selecionada, -1 se não há */

/* duplo clique / Enter numa linha */
int fp_activate(const fp_model_t *m, int row);
/* o botão principal fica ativo? save_name só no salvar */
int fp_can_confirm(const fp_model_t *m, const char *save_name);

/* salvar */
int fp_validate_name(const char *name);
/* acrescenta a primeira extensão aceita quando o nome não tem uma aceita e o painel não deixa outros tipos */
int fp_save_name(const fp_model_t *m, const char *name, char *out, int cap);
/* índice da entrada com esse nome, sem caixa (APFS padrão), ou -1 */
int fp_find(const fp_model_t *m, const char *name);
/* "pasta sem título", "pasta sem título 2", ... que não existe na pasta */
void fp_new_folder_name(const fp_model_t *m, const char *base, char *out, int cap);
