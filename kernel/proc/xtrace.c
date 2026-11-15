/*
 * mOS — rastreio de syscalls por processo (a ideia do xtrace do Darling: cada chamada com nome, argumentos e
 * resultado). A parte Mach/MIG fica no libSystem (userland/libsystem/mach.c: cada mach_msg com o nome da rotina).
 *
 * /Sistema/Biblioteca/diag-xtrace, uma parte do nome do processo por linha; arquivo vazio = todos os processos.
 * Sem o arquivo, nada é rastreado. Saída no log do kernel (serial):
 *     xtrace <processo> <pid>:<tid> <syscall>(a, b, c) = <resultado>
 * Syscall que bloqueou e trocou de thread sai com "= (bloqueou)": o resultado chega quando a thread volta.
 * Nomes: kernel/proc/xtrace_nomes.inc (tools/gerar-xtrace-nomes.py, do enum de kernel/include/syscall.h).
 */
#include "proc.h"
#include "vfs.h"

static const char *const sys_names[128] = {
#include "xtrace_nomes.inc"
};

static int cfg_state = -1;        /* -1 = ainda não leu; 0 = sem arquivo; 1 = com arquivo */
static vnode_t *cfg;

static bool line_matches(const char *name) {
    if (!cfg->data || !cfg->size) return true;                    /* arquivo vazio: todos */
    const char *s = (const char *)cfg->data, *end = s + cfg->size;
    bool any = false;
    while (s < end) {
        const char *eol = s;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;
        usize n = (usize)(eol - s);
        if (n && n < 64) {
            char want[64];
            memcpy(want, s, n);
            want[n] = 0;
            any = true;
            if (strstr(name, want)) return true;
        }
        s = eol + 1;
    }
    return !any;                                                  /* só linhas em branco: todos */
}

bool xtrace_on(process_t *p) {
    if (cfg_state < 0) { cfg = vfs_lookup("/Sistema/Biblioteca/diag-xtrace"); cfg_state = cfg != NULL; }
    if (cfg_state != 1 || !p) return false;
    /* com o rastreio ligado, o vnode é buscado de novo a cada syscall: apagar ou recriar o arquivo com o sistema
     * rodando liberaria o vnode guardado, e line_matches leria memória já liberada. Desligado, o custo segue zero. */
    cfg = vfs_lookup("/Sistema/Biblioteca/diag-xtrace");
    if (!cfg) { cfg_state = 0; return false; }
    return line_matches(p->name);
}

void xtrace_log(process_t *p, int tid, u64 nr, u64 a, u64 b, u64 c, i64 res, bool blocked) {
    const char *n = nr < 128 && sys_names[nr] ? sys_names[nr] : NULL;
    char num[24];
    if (!n) { snprintf(num, sizeof num, "sys_%llu", (unsigned long long)nr); n = num; }
    if (blocked) kprintf("xtrace %s %d:%d %s(0x%llx, 0x%llx, 0x%llx) = (bloqueou)\n", p->name, p->pid, tid, n,
                         (unsigned long long)a, (unsigned long long)b, (unsigned long long)c);
    else kprintf("xtrace %s %d:%d %s(0x%llx, 0x%llx, 0x%llx) = %lld\n", p->name, p->pid, tid, n,
                 (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (long long)res);
}
