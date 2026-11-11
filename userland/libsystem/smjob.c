/*
 * ServiceManagement: SMJobSubmit, SMJobRemove, SMJobCopyDictionary, SMCopyAllJobDictionaries e
 * SMLoginItemSetEnabled sobre o launchd do mOS (userland/init.c, serviço LAUNCHD_SERVICE).
 *
 * SMJobSubmit grava o dicionário em ~/Library/Application Support/com.mos.smjob/<rótulo>.plist e pede LC_LOAD.
 * Esse diretório o launchd não varre: o job some no próximo boot, como no macOS. SMJobRemove pede LC_REMOVE e
 * apaga o arquivo. Os dois domínios (usuário e sistema) caem no mesmo launchd; o mOS tem uma sessão só.
 * SMLoginItemSetEnabled acha o helper em Contents/Library/LoginItems pelo CFBundleIdentifier e o põe na lista de
 * login items (loginitems.c), que o launchd lê a cada 5 s e no boot.
 * SMJobBless continua em chrome.c, falhando: instalar ferramenta privilegiada pede o fluxo do Authorization.
 * Leitura das respostas do launchd: smjob_core.inc (tools/smjobtest.c no host).
 */
#include "foundation.h"
#include "libc.h"
#include "../svcproto.h"
#include "smjob_core.inc"

extern const void *kSMErrorDomainLaunchd;
int cx_launchd_cmd(unsigned op, const char *arg, char *out, unsigned cap);
int cx_login_add(const char *path);
int cx_login_remove(const char *path);
int cx_sm_job_write(const char *label, const void *bytes, size_t n, char *path, size_t cap);
int cx_sm_job_remove(const char *label);
id CFPropertyListCreateData(void *alloc, id plist, long format, unsigned long opt, id *err);
id CFErrorCreate(void *alloc, id domain, long code, id info);
long CFDataGetLength(id d);
const unsigned char *CFDataGetBytePtr(id d);

#define SMJ_XML_FORMAT 100   /* kCFPropertyListXMLFormat_v1_0 */

static void smj_fail(id *err, int code) {
    if (err) *err = CFErrorCreate(0, (id)kSMErrorDomainLaunchd, smj_errno_of(code), 0);
}

static const char *smj_label_of(id job) {
    if (!job) return 0;
    id l = MSG(id, job, "objectForKey:", cx_str("Label"));
    if (!l || !cx_is_str(l)) return 0;
    return cx_cstr(l);
}

u8 SMJobSubmit(id domain, id job, void *auth, id *err) {
    (void)domain; (void)auth;
    if (err) *err = 0;
    const char *label = smj_label_of(job);
    if (!label || !label[0]) { smj_fail(err, 22); return 0; }
    id data = CFPropertyListCreateData(0, job, SMJ_XML_FORMAT, 0, 0);
    long n = data ? CFDataGetLength(data) : 0;
    if (n <= 0) { smj_fail(err, 22); return 0; }
    char path[1024], out[512];
    if (cx_sm_job_write(label, CFDataGetBytePtr(data), (size_t)n, path, sizeof path) != 0) { smj_fail(err, 22); return 0; }
    int code = cx_launchd_cmd(LC_LOAD, path, out, sizeof out);
    if (code == 0) return 1;
    /* "já carregado" vem com 17 quando o rótulo é de outro arquivo: o job antigo continua; o arquivo novo sai */
    if (code == SMJ_LAUNCHD_EXISTS || strstr(out, "carregado")) code = SMJ_LAUNCHD_EXISTS;
    else cx_sm_job_remove(label);
    smj_fail(err, code);
    return 0;
}

u8 SMJobRemove(id domain, id label, void *auth, u8 wait, id *err) {
    (void)domain; (void)auth;
    if (err) *err = 0;
    const char *l = label && cx_is_str(label) ? cx_cstr(label) : 0;
    if (!l || !l[0]) { smj_fail(err, 22); return 0; }
    int code = cx_launchd_cmd(LC_REMOVE, l, 0, 0);
    cx_sm_job_remove(l);
    if (code != 0) { smj_fail(err, code); return 0; }
    /* wait: o LC_REMOVE já mandou SIGTERM e esqueceu o job; espera o rótulo sumir da lista (até 2 s) */
    for (int i = 0; wait && i < 40; i++) {
        if (cx_launchd_cmd(LC_LIST, l, 0, 0) == SMJ_LAUNCHD_NOTFOUND) break;
        usleep(50000);
    }
    return 1;
}

static id smj_dict(const smj_info_t *in) {
    id d = MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init");
    if (!d) return 0;
    MSG(void, d, "setObject:forKey:", cx_str(in->label), cx_str("Label"));
    MSG(void, d, "setObject:forKey:", cx_num_i(in->last_status), cx_str("LastExitStatus"));
    if (in->pid > 0) MSG(void, d, "setObject:forKey:", cx_num_i(in->pid), cx_str("PID"));
    if (in->program[0]) MSG(void, d, "setObject:forKey:", cx_str(in->program), cx_str("Program"));
    id yes = MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (BOOL)1);
    if (yes) MSG(void, d, "setObject:forKey:", yes, cx_str("OnDemand"));
    return d;
}

/* regra Copy: o chamador solta (o alloc/init acima já devolve +1) */
id SMJobCopyDictionary(id domain, id label) {
    (void)domain;
    const char *l = label && cx_is_str(label) ? cx_cstr(label) : 0;
    if (!l || !l[0]) return 0;
    char *out = (char *)malloc(16384);
    if (!out) return 0;
    smj_info_t in;
    id d = 0;
    if (cx_launchd_cmd(LC_LIST, l, out, 16384) == 0 && smj_parse_one(out, &in) == 0) d = smj_dict(&in);
    free(out);
    return d;
}

id SMCopyAllJobDictionaries(id domain) {
    (void)domain;
    id a = MSG0(id, MSG0(id, (id)cx_class_named("NSMutableArray"), "alloc"), "init");
    char *out = (char *)malloc(16384);
    if (!a || !out) { free(out); return a; }
    if (cx_launchd_cmd(LC_LIST, 0, out, 16384) == 0) {
        const char *cur = out;
        smj_info_t in;
        while (smj_parse_row(&cur, &in)) {
            id d = smj_dict(&in);
            if (d) { MSG(void, a, "addObject:", d); MSG0(void, d, "release"); }
        }
    }
    free(out);
    return a;
}

/* acha <bundle principal>/Contents/Library/LoginItems/<X>.app com esse CFBundleIdentifier */
static int smj_find_login_helper(const char *ident, char *path, size_t cap) {
    id mb = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bp = mb ? MSG0(id, mb, "bundlePath") : 0;
    char dir[1024];
    if (!bp || smj_loginitems_dir(cx_cstr(bp), dir, (int)sizeof dir)) return -1;
    id fm = MSG0(id, (id)cx_class_named("NSFileManager"), "defaultManager");
    id names = fm ? MSG(id, fm, "contentsOfDirectoryAtPath:error:", cx_str(dir), (id *)0) : 0;
    long n = names ? MSG0(long, names, "count") : 0;
    for (long i = 0; i < n; i++) {
        const char *name = cx_cstr(MSG(id, names, "objectAtIndex:", (unsigned long)i));
        if (!name || !strstr(name, ".app")) continue;
        snprintf(path, cap, "%s/%s", dir, name);
        id b = MSG(id, (id)cx_class_named("NSBundle"), "bundleWithPath:", cx_str(path));
        id bid = b ? MSG0(id, b, "bundleIdentifier") : 0;
        if (bid && !strcmp(cx_cstr(bid), ident)) return 0;
    }
    return -1;
}

u8 SMLoginItemSetEnabled(id ident, u8 enabled) {
    const char *s = ident && cx_is_str(ident) ? cx_cstr(ident) : 0;
    char path[1200];
    if (!s || !s[0] || smj_find_login_helper(s, path, sizeof path)) return 0;
    if (enabled) return cx_login_add(path) == 0;
    cx_login_remove(path);           /* já fora da lista também vale como desligado */
    return 1;
}
