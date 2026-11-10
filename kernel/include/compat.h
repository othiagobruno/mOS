/*
 * mOS — compatibilidade com apps do macOS (no espírito do ravynOS).
 *
 *   kernel/compat/macho.c     "dyld": carrega Mach-O x86_64 (universal ou thin) num processo de
 *                             usuário, aplica rebases/binds (dyld_info e chained fixups) contra o
 *                             libSystem.mos e monta argv/envp/apple na pilha
 *   kernel/compat/bundle.c    bundles .app: Info.plist (XML e binário) e relatório de compatibilidade
 *   userland/libsystem/       o libSystem do mOS (ring 3, System V): stdio, malloc, string, ctype
 *                             do macOS, arquivos, tempo... sobre as syscalls de kernel/include/syscall.h
 *
 * Fluxo: proc_spawn(caminho) vê o magic Mach-O e chama macho_load (gancho em proc/process.c).
 * Apps de terminal em C que só usam o libSystem rodam de verdade. Apps de Cocoa/Swift são
 * reconhecidos e o relatório explica o que falta.
 */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_proc.h"
#else
#include "proc.h"
#endif

bool macho_load(process_t *p, const u8 *img, usize len, const char *arg, u64 *entry,
                u64 *user_rsp, u64 *rdi, char *err, usize errlen);

/* relatório legível de um executável (arquiteturas, bibliotecas, símbolos, veredito);
 * retorna true se ele roda no mOS */
bool macho_report_image(const u8 *img, usize len, const char *name, char *out, usize cap);

/* o mesmo para um caminho do VFS: aceita um executável ou um bundle .app */
bool macho_report(const char *path, char *out, usize cap);

/* bundle .app -> caminho do executável (Contents/MacOS/<CFBundleExecutable>) */
bool macho_bundle_exec(const char *app, char *out, usize cap);

/* informações do Info.plist (campos vazios se ausentes) */
typedef struct {
    char name[128], identifier[128], version[64], executable[128], min_os[32], icon[128];
} bundle_info_t;
bool macho_bundle_info(const char *app, bundle_info_t *bi);
#ifndef MOS_HOST_TEST
#include "gfx.h"
/* ícone do bundle (CFBundleIconFile em Contents/Resources, entradas PNG do .icns) em size x size;
 * fica num cache: não libere. NULL se o app não tiver um ícone que o mOS leia. */
surface_t *macho_bundle_icon(const char *app, int size);
/* guisys.c: .app de um atalho de /Aplicativos ou das janelas de um processo (NULL se não houver) */
struct app;
const char *compat_app_bundle(struct app *a);
#endif

bool libsystem_dylib_ok(const char *install_name);

/* Porta de entrada para a interface (Terminal, Arquivos): executa um Mach-O ou um .app como
 * processo de usuário. args: linha de argumentos (aspas e espaços como num shell);
 * cwd: diretório atual do programa (PWD). Retorna o pid, ou -1 com o motivo em err. */
int  macho_spawn(const char *path, const char *args, const char *cwd, char *err, usize errlen);
void macho_set_cwd(const char *cwd);
bool macho_is_macho(const u8 *data, usize len);   /* magic Mach-O de 64 bits ou universal */

/* guisys.c: app_t da proxy de janelas de um processo (Dock/Launchpad), NULL se não tiver janelas */
struct app;
struct app *compat_gui_app_for_pid(int pid);
int compat_gui_pid_of(struct app *a);   /* pid do processo dono de um app proxy (0 se não for) */
/* Selo do Dock no app proxy e, se houver, no atalho de /Aplicativos. 1 = aplicado. */
int compat_gui_set_badge_label(struct process *p, const char *label);
/* Encerrar: GEV_QUIT, 3 s, SIGTERM, 2 s, SIGKILL. force != 0 vai ao SIGKILL.
 * O segundo pedido no mesmo pid sobe um estágio. proc_signal/proc_kill já existem (proc.h); process.c não muda. */
void compat_gui_request_quit(int pid, int force);
void compat_gui_quit_tick(void);        /* prazos; chame do loop do desktop */
int  compat_gui_quit_pending(int pid);  /* 1 enquanto o pid está na escalação (watch trata -15 como esperado) */
