/*
 * mOS — Launch Services (kernel/compat/launchsvc.c): o banco que diz que tipo cada arquivo tem (UTIs) e que app
 * abre cada tipo e cada esquema de URL, como o lsd do macOS.
 *
 *  - Tipos: tabela do sistema (public.*, com.apple.*...) + UTExportedTypeDeclarations/UTImportedTypeDeclarations
 *    dos apps instalados, com a árvore de conformidade (public.png → public.image → public.data → public.item).
 *  - Apps: os do sistema (Horizonte, Pré-Visualização, Editor de Texto, Utilitário de Compactação...) e cada .app em
 *    /Aplicativos, lido do Info.plist: CFBundleDocumentTypes (LSItemContentTypes, CFBundleTypeExtensions,
 *    CFBundleTypeRole, LSHandlerRank, CFBundleTypeIconFile) e CFBundleURLTypes. Um app instalado depois é
 *    registrado sozinho (a pasta é vigiada).
 *  - Padrões: escolha do usuário (Obter Informações > Abrir com > Alterar Tudo, Ajustes > Navegador padrão,
 *    LSSetDefault...) em ~/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist,
 *    no formato do macOS (LSHandlers). Sem escolha: maior LSHandlerRank (Owner > Default > Alternate), depois a
 *    declaração mais específica, depois o app do sistema, depois o registrado primeiro.
 */
#pragma once
#include "kernel.h"
#include "gfx.h"

struct app;
typedef struct ls_app ls_app_t;

/* papéis (LSRolesMask) */
enum { LS_ROLE_NONE = 1, LS_ROLE_VIEWER = 2, LS_ROLE_EDITOR = 4, LS_ROLE_SHELL = 8, LS_ROLE_ALL = 0xFFFFFFFFu };

void        ls_init(void);
void        ls_tick(void);                       /* loop principal: apps novos/removidos e entregas pendentes */
void        ls_rescan(void);                     /* relê /Aplicativos agora */

/* ---------------- tipos */
const char *ls_type_of_path(const char *path);   /* UTI do item (buffer rotativo) */
const char *ls_type_for_tag(const char *tag_class, const char *tag, const char *conforming);   /* "public.filename-extension" / "public.mime-type" */
const char *ls_tag_for_type(const char *uti, const char *tag_class);   /* extensão ou MIME preferido, ou NULL */
const char *ls_type_desc(const char *uti);       /* "Imagem PNG" */
bool        ls_conforms(const char *uti, const char *to);
int         ls_type_parents(const char *uti, const char **out, int max);
int         ls_type_tags(const char *uti, const char *tag_class, const char **out, int max);
const char *ls_kind_for_path(const char *path);  /* "Tipo" do Arquivos: nome do tipo no app que abre, ou a descrição */

/* ---------------- apps */
int         ls_app_count(void);
ls_app_t   *ls_app_at(int i);
ls_app_t   *ls_app_by_id(const char *bundle_id);
ls_app_t   *ls_app_by_path(const char *bundle_path);   /* registra o .app se ainda não estiver no banco */
ls_app_t   *ls_app_by_name(const char *name);           /* "Google Chrome" ou "Google Chrome.app" */
ls_app_t   *ls_app_for_builtin(struct app *a);
const char *ls_app_name(const ls_app_t *a);
const char *ls_app_id(const ls_app_t *a);
const char *ls_app_path(const ls_app_t *a);       /* "" para os apps do kernel */
struct app *ls_app_builtin(const ls_app_t *a);    /* app do kernel ou NULL */
bool        ls_app_is_browser(const ls_app_t *a);
void        ls_app_draw_icon(const ls_app_t *a, surface_t *s, int x, int y, int size);

/* ---------------- quem abre */
/* em ordem: o padrão primeiro. Só papéis em roles; os que declaram só public.data/"*" ficam de fora */
int         ls_handlers_for_path(const char *path, u32 roles, ls_app_t **out, int max);
ls_app_t   *ls_default_for_path(const char *path, u32 roles);
int         ls_handlers_for_type(const char *uti, u32 roles, ls_app_t **out, int max);
ls_app_t   *ls_default_for_type(const char *uti, u32 roles);
int         ls_handlers_for_scheme(const char *scheme, ls_app_t **out, int max);
ls_app_t   *ls_default_for_scheme(const char *scheme);
ls_app_t   *ls_default_browser(void);
int         ls_browsers(ls_app_t **out, int max);
surface_t  *ls_doc_icon(const char *path, int size);   /* ícone do documento no app que o abre (cache; não libere) */

/* ---------------- escolhas do usuário (gravadas) */
bool        ls_set_default_for_type(const char *uti, u32 roles, ls_app_t *a);
bool        ls_set_default_for_path_type(const char *path, ls_app_t *a);     /* "Alterar Tudo…" */
void        ls_set_handler_for_file(const char *path, ls_app_t *a);           /* só este arquivo (NULL = volta ao padrão) */
bool        ls_set_default_for_scheme(const char *scheme, ls_app_t *a);
void        ls_set_default_browser(ls_app_t *a);                               /* http, https, public.html... */
/* pedido de um app (LSSetDefaultHandlerForURLScheme, setDefaultApplicationAtURL:...): para http/https o sistema
 * pergunta antes ("Deseja alterar o navegador padrão..."), como no macOS */
void        ls_request_default_for_scheme(const char *scheme, ls_app_t *a);

/* ---------------- abrir */
/* target: caminho, file://..., ou URL de qualquer esquema. with = NULL: o padrão. false se ninguém abre */
bool        ls_open(const char *target, ls_app_t *with);

/* ---------------- o motor Chromium do Horizonte (docs/navegador-chromium.md)
 * O Horizonte é o navegador do sistema; o motor dele é o Chromium de código aberto neste caminho, fora de /Aplicativos
 * (um app só para o usuário). Sem ele, o Horizonte usa o motor próprio (kernel/apps/horizonte.c). */
#define HORIZONTE_MOTOR "/Sistema/Biblioteca/Horizonte/Chromium.app"
bool        horizonte_motor_disponivel(void);
bool        horizonte_motor_abrir(const char *url);      /* url NULL: abre ou traz o Horizonte; false: sem motor ou não iniciou */
bool        horizonte_motor_bundle(const char *bundle);  /* o .app é o motor? (Dock, barra de menus, alertas) */
void        horizonte_motor_encerrar(void);              /* "Encerrar" do Horizonte: pede ao motor que encerre */
void        horizonte_legado_abrir(const char *url);     /* kernel/apps/horizonte.c: a URL no motor próprio do Horizonte */
/* Ninguém abre o documento: alerta "Não há um aplicativo definido..." com "Escolher Aplicativo…" (Arquivos) */
extern void (*ls_choose_app_hook)(const char *path);

/* syscall SYS_LS (kernel/proc/process.c) */
struct process;
i64         ls_syscall(struct process *p, u64 op, u64 ureq);
