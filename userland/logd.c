/* mOS — logd: recebe os_log, asl e syslog(3) dos processos e grava o store legível.
 *
 * Serviço Mach LOGD_SERVICE, sob demanda pelo launchd (/Sistema/Biblioteca/LaunchDaemons/com.apple.logd.plist).
 * Cada mensagem (LG_WRITE, só envio) vira uma linha em LOG_STORE, com campos separados por tabulação:
 *     milissegundos-unix  pid  nível  processo  subsistema  categoria  mensagem
 * (quebras de linha e tabs da mensagem viram "\n" e "\t"). O `log show` (/usr/bin/log) lê e formata como o macOS.
 * As que vêm do syslog/asl (c = 1) também vão para /private/var/log/system.log no formato BSD.
 * O store gira em LOG_STORE_MAX (o anterior fica em LOG_STORE_OLD). */
#include "mos.h"
#include "svcnative.h"

static int fd = -1, sysfd = -1;
static i64 used;

static void open_store(void) {
    mkdir("/private"); mkdir("/private/var"); mkdir("/private/var/db"); mkdir(LOG_STORE_DIR); mkdir("/private/var/log");
    if (file_size(LOG_STORE) < 0) write_file(LOG_STORE, "", 0);
    fd = open(LOG_STORE, MOS_O_WRONLY | MOS_O_CREAT | MOS_O_APPEND);
    used = file_size(LOG_STORE);
    if (used < 0) used = 0;
    if (file_size("/private/var/log/system.log") < 0) write_file("/private/var/log/system.log", "", 0);
    sysfd = open("/private/var/log/system.log", MOS_O_WRONLY | MOS_O_CREAT | MOS_O_APPEND);
}

static char line[SVC_MAX_MSG + 512];
static usize ln;
static void put(const char *s, usize n) { for (usize i = 0; i < n && s[i] && ln + 4 < sizeof line; i++) {
        char c = s[i];
        if (c == '\n') { line[ln++] = '\\'; line[ln++] = 'n'; }
        else if (c == '\t') { line[ln++] = '\\'; line[ln++] = 't'; }
        else if (c == '\\') { line[ln++] = '\\'; line[ln++] = '\\'; }
        else if (c != '\r') line[ln++] = c;
    } }
static void putnum(u64 v) { char b[24]; int n = 0; do b[n++] = (char)('0' + v % 10); while (v /= 10); while (n && ln + 1 < sizeof line) line[ln++] = b[--n]; }
static void tab(void) { if (ln + 1 < sizeof line) line[ln++] = '\t'; }

/* data BSD "Nov  7 10:00:00" a partir dos segundos unix (UTC) */
static void bsd_date(i64 t, char *out) {
    static const char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    i64 days = t / 86400, sec = t % 86400;
    i64 z = days + 719468, era = z / 146097, doe = z - era * 146097;
    i64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    i64 mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    const char *mn = mon[m - 1];
    out[0] = mn[0]; out[1] = mn[1]; out[2] = mn[2]; out[3] = ' ';
    out[4] = d >= 10 ? (char)('0' + d / 10) : ' '; out[5] = (char)('0' + d % 10); out[6] = ' ';
    int hh = (int)(sec / 3600), mm = (int)(sec / 60 % 60), ss = (int)(sec % 60);
    out[7] = (char)('0' + hh / 10); out[8] = (char)('0' + hh % 10); out[9] = ':';
    out[10] = (char)('0' + mm / 10); out[11] = (char)('0' + mm % 10); out[12] = ':';
    out[13] = (char)('0' + ss / 10); out[14] = (char)('0' + ss % 10); out[15] = 0;
}

void _start(const char *arg) {
    (void)arg;
    unsigned port = 0;
    if (bs_op(MACH_BS_CHECK_IN, LOGD_SERVICE, &port) != 0 || !port) { print("logd: check_in falhou\n"); exit(1); }
    open_store();
    print("logd: gravando em " LOG_STORE "\n");
    static unsigned char msg[SVC_MAX_MSG + 128];
    u64 boot_ms = time_ms();
    i64 boot_unix = unixtime();
    for (;;) {
        memset(msg, 0, sizeof(mhdr_t));
        i64 r = mach_msg_n(msg, M_RCV | M_TRAILER, 0, sizeof msg, port, 0);
        if (r) { if (r != 0x10004004) sleep_ms(10); continue; }
        mhdr_t *h = (mhdr_t *)msg;
        svc_body_t *b = svc_body(msg, sizeof msg, 0);
        unsigned reply = (h->bits >> 8 & 0x1f) == D_MOVE_SO ? h->local : 0;
        if (!b || h->id != SVC_MSG_ID || b->op != LG_WRITE) { if (reply) svc_reply(reply, 22, 0, 0, 0, 0); continue; }
        int pid = msg_sender_pid(msg);
        if (!pid) pid = (int)b->b;
        /* s1 = processo\0subsistema\0categoria\0 */
        const char *meta = svc_s1(b), *end = meta + b->n1;
        const char *proc = meta, *subsys = "", *cat = "";
        const char *p = proc;
        while (p < end && *p) p++;
        if (p + 1 < end) { subsys = p + 1; p = subsys; while (p < end && *p) p++; if (p + 1 < end) cat = p + 1; }
        u64 now_ms = (u64)boot_unix * 1000 + (time_ms() - boot_ms);
        if (b->q) now_ms = b->q;
        ln = 0;
        putnum(now_ms); tab(); putnum((u64)pid); tab(); putnum(b->a); tab();
        put(proc, (usize)(end - proc)); tab(); put(subsys, (usize)(end - subsys)); tab(); put(cat, (usize)(end - cat)); tab();
        put(svc_s2(b), b->n2);
        line[ln++] = '\n';
        if (used + (i64)ln > (i64)LOG_STORE_MAX) {
            close(fd);
            unlink(LOG_STORE_OLD);
            rename(LOG_STORE, LOG_STORE_OLD);
            write_file(LOG_STORE, "", 0);
            fd = open(LOG_STORE, MOS_O_WRONLY | MOS_O_CREAT | MOS_O_APPEND);
            used = 0;
        }
        if (fd >= 0 && write(fd, line, ln) > 0) used += (i64)ln;
        if ((b->c & 0xff) == 1 && sysfd >= 0) {
            /* syslog/asl (c = 1 | prioridade << 8): "Nov  7 10:00:00 mos processo[pid] <Notice>: mensagem" */
            static const char *lv[] = { "Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug" };
            char d[16];
            bsd_date((i64)(now_ms / 1000), d);
            const char *l = lv[(b->c >> 8) & 7];
            ln = 0;
            put(d, 15); put(" mos ", 5); put(proc, (usize)(end - proc)); put("[", 1); putnum((u64)pid); put("] <", 3);
            put(l, strlen(l)); put(">: ", 3);
            const char *m = svc_s2(b);
            for (usize i = 0; i < b->n2 && m[i] && ln + 2 < sizeof line; i++) line[ln++] = m[i] == '\n' ? ' ' : m[i];
            line[ln++] = '\n';
            write(sysfd, line, ln);
        }
        if (reply) svc_reply(reply, 0, 0, 0, 0, 0);
    }
}
