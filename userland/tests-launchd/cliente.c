/* mOS — teste dos serviços do sistema (launchd, XPC, notifyd, logd, configd), de ponta a ponta no QEMU.
 * Roda como LaunchdTeste.app, iniciado pelo launchd no boot (RunAtLoad). Saída no stderr (log serial com
 * stderr-serial), linhas "launchdteste: ...", e no fim "launchdteste: N/M PASS".
 *  1. XPC com um daemon sob demanda (com.mos.teste.xpc: o launchd só o inicia na primeira mensagem): sync e async
 *  2. o daemon termina; a conexão recebe INTERRUPTED, o próximo envio reconecta e o launchd o reinicia
 *  3. XPC service dentro do bundle (Contents/XPCServices/com.mos.teste.bundle.xpc)
 *  4. notify entre processos: post de "ping" acorda o servidor (register_dispatch), que posta "pong" (register_check e
 *     register_dispatch aqui)
 *  5. os_log chega ao store do logd e aparece no `log show`
 *  6. configd: State:/Network/Global/IPv4 e proxies
 *  7. launchctl list mostra os jobs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <spawn.h>
#include <sys/wait.h>
#include "svcteste.h"

extern char **environ;
static int pass, total;
static void check(int ok, const char *what) {
    total++;
    if (ok) pass++;
    fprintf(stderr, "launchdteste: %s %s\n", ok ? "ok  " : "FALHA", what);
}

static int64_t soma(xpc_connection_t c, int64_t a, int64_t b, int64_t *pid) {
    xpc_object_t m = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(m, "op", "soma");
    xpc_dictionary_set_int64(m, "a", a);
    xpc_dictionary_set_int64(m, "b", b);
    xpc_object_t r = xpc_connection_send_message_with_reply_sync(c, m);
    xpc_release(m);
    if (xpc_get_type(r) != XPC_TYPE_DICTIONARY) return -1;
    if (pid) *pid = xpc_dictionary_get_int64(r, "pid");
    return xpc_dictionary_get_int64(r, "r");
}

static volatile int interrupted, invalid;
static xpc_connection_t conectar(const char *name, int mach) {
    xpc_connection_t c = mach ? xpc_connection_create_mach_service(name, NULL, 0) : xpc_connection_create(name, NULL);
    xpc_connection_set_event_handler(c, ^(xpc_object_t ev) {
        if (ev == XPC_ERROR_CONNECTION_INTERRUPTED) interrupted = 1;
        else if (ev == XPC_ERROR_CONNECTION_INVALID) invalid = 1;
    });
    xpc_connection_resume(c);
    return c;
}

static int run(const char *const argv[]) {
    pid_t pid;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, 2, 1);           /* stdout da ferramenta -> nosso stderr (log serial) */
    int r = posix_spawn(&pid, argv[0], &fa, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    if (r != 0) return -1;
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static int store_has(const char *needle) {
    FILE *f = fopen("/private/var/db/diagnostics/mos-log.txt", "r");
    if (!f) return 0;
    static char line[16384];
    int found = 0;
    while (!found && fgets(line, sizeof line, f)) if (strstr(line, needle)) found = 1;
    fclose(f);
    return found;
}

int main(void) {
    sleep(2);
    fprintf(stderr, "launchdteste: início (pid %d)\n", getpid());
    os_log_t lg = os_log_create("com.mos.teste", "cliente");

    /* 1. daemon sob demanda */
    xpc_connection_t c = conectar("com.mos.teste.xpc", 1);
    int64_t pid1 = 0, pid2 = 0;
    int64_t r = soma(c, 2, 3, &pid1);
    check(r == 5, "XPC sync com daemon sob demanda (2+3=5)");
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block int64_t ar = -1;
    xpc_object_t m = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(m, "op", "eco");
    xpc_dictionary_set_string(m, "s", "olá, launchd");
    xpc_connection_send_message_with_reply(c, m, dispatch_get_global_queue(0, 0), ^(xpc_object_t rep) {
        const char *s = xpc_get_type(rep) == XPC_TYPE_DICTIONARY ? xpc_dictionary_get_string(rep, "s") : NULL;
        ar = s && !strcmp(s, "olá, launchd") ? 1 : 0;
        dispatch_semaphore_signal(sem);
    });
    xpc_release(m);
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
    check(ar == 1, "XPC assíncrono (eco)");

    /* 2. o daemon sai; INTERRUPTED; o próximo envio reconecta e o launchd o inicia de novo */
    m = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(m, "op", "sair");
    xpc_object_t bye = xpc_connection_send_message_with_reply_sync(c, m);
    xpc_release(m);
    (void)bye;
    for (int i = 0; i < 50 && !interrupted; i++) usleep(100000);
    check(interrupted, "INTERRUPTED quando o daemon termina");
    sleep(1);
    r = soma(c, 40, 2, &pid2);
    check(r == 42 && pid2 && pid2 != pid1, "reconexão: o launchd reinicia o daemon (pid novo)");
    fprintf(stderr, "launchdteste: pids do daemon %lld -> %lld\n", (long long)pid1, (long long)pid2);

    /* 3. XPC service do bundle */
    xpc_connection_t b = conectar("com.mos.teste.bundle", 0);
    check(soma(b, 7, 8, NULL) == 15, "XPC service dentro do bundle (Contents/XPCServices)");

    /* 4. notify entre processos */
    int tcheck = 0, tdisp = 0, changed = 0;
    __block volatile int got = 0;
    notify_register_check("com.mos.teste.pong", &tcheck);
    notify_check(tcheck, &changed);                       /* a 1ª chamada sempre diz "mudou" */
    notify_register_dispatch("com.mos.teste.pong", &tdisp, dispatch_get_global_queue(0, 0), ^(int t) { (void)t; got = 1; });
    notify_post("com.mos.teste.ping");
    changed = 0;
    for (int i = 0; i < 50 && !(changed && got); i++) { usleep(100000); if (!changed) notify_check(tcheck, &changed); }
    check(changed, "notify_post acorda outro processo (register_check)");
    check(got, "notify register_dispatch recebe o pong");
    uint64_t st = 0;
    notify_set_state(tcheck, 0x1234);
    notify_get_state(tcheck, &st);
    check(st == 0x1234, "notify_set_state/get_state no notifyd");

    /* 5. os_log -> logd -> log show */
    char tag[64];
    snprintf(tag, sizeof tag, "mensagem de teste %d", getpid());
    os_log(lg, "%{public}s ok", tag);
    os_log_error(lg, "erro de teste %d", 7);
    int seen = 0;
    for (int i = 0; i < 30 && !seen; i++) { usleep(100000); seen = store_has(tag); }
    check(seen, "os_log gravado no store do logd");
    const char *show[] = { "/usr/bin/log", "show", "--style", "compact", "--predicate", "subsystem == \"com.mos.teste\"", NULL };
    check(run(show) == 0, "log show --predicate subsystem");

    /* 6. configd */
    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("launchdteste"), NULL, NULL);
    CFDictionaryRef ip = (CFDictionaryRef)SCDynamicStoreCopyValue(ds, CFSTR("State:/Network/Global/IPv4"));
    CFStringRef pi = ip ? CFDictionaryGetValue(ip, CFSTR("PrimaryInterface")) : NULL;
    char ifn[32] = "";
    if (pi) CFStringGetCString(pi, ifn, sizeof ifn, kCFStringEncodingUTF8);
    fprintf(stderr, "launchdteste: interface primária: %s\n", ifn[0] ? ifn : "(nenhuma)");
    check(ip && !strcmp(ifn, "en0"), "configd: State:/Network/Global/IPv4");
    CFDictionaryRef px = SCDynamicStoreCopyProxies(ds);
    check(px && CFDictionaryGetValue(px, CFSTR("ExceptionsList")), "configd: proxies");
    CFStringRef cn = SCDynamicStoreCopyComputerName(ds, NULL);
    check(cn != NULL, "configd: nome do computador");

    /* 7. launchctl */
    const char *lc[] = { "/usr/bin/launchctl", "list", NULL };
    check(run(lc) == 0, "launchctl list");

    fprintf(stderr, "launchdteste: %d/%d %s\n", pass, total, pass == total ? "PASS" : "FALHOU");
    os_log(lg, "resultado: %d/%d", pass, total);
    return pass == total ? 0 : 1;
}
