/*
 * SMAppService: o status sobrevive em Application Support e o plist habilitado
 * é copiado para ~/Library/LaunchAgents ou /Library/LaunchDaemons, que o launchd
 * já varre no boot. O load imediato continua em slack_frameworks.c (cx_launchd_cmd).
 */
#include "foundation.h"
#include "libc.h"

char *fgets(char *s, int n, void *f);
int fputs(const char *s, void *f);
int sscanf(const char *s, const char *fmt, ...);
int rename(const char *a, const char *b);

#include "smlogin_core.inc"

id NSHomeDirectory(void);

static const char *sm_home(void) {
    id h = NSHomeDirectory();
    const char *s = h ? cx_cstr(h) : 0;
    if (!s || !s[0] || !strcmp(s, "(null)")) return "/Users/Visitante";
    return s;
}

static void sm_status_path(char *out, size_t cap) {
    snprintf(out, cap, "%s/Library/Application Support/com.mos.smappservice/status.txt", sm_home());
}

int cx_sm_remember(const char *key, int status) {
    char path[512];
    sm_status_path(path, sizeof path);
    return sm_status_set(path, key, status);
}

int cx_sm_recall(const char *key) {
    char path[512];
    sm_status_path(path, sizeof path);
    return sm_status_get(path, key);
}

int cx_sm_publish_file(int daemon, const char *plist_path) {
    if (!plist_path) return -1;
    FILE_ *f = fopen(plist_path, "rb");
    if (!f) return -1;
    char *buf = (char *)malloc(65536);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, 65535, f);
    fclose(f);
    buf[n] = 0;
    char label[200];
    if (sm_label_xml(buf, n, label, sizeof label) != 0) { free(buf); return -1; }
    char dir[512];
    if (daemon) snprintf(dir, sizeof dir, "/Library/LaunchDaemons");
    else snprintf(dir, sizeof dir, "%s/Library/LaunchAgents", sm_home());
    int rc = sm_copy_plist(dir, label, buf, n);
    free(buf);
    return rc;
}

int cx_sm_unpublish_file(int daemon, const char *plist_path) {
    if (!plist_path) return -1;
    FILE_ *f = fopen(plist_path, "rb");
    if (!f) return -1;
    char *buf = (char *)malloc(65536);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, 65535, f);
    fclose(f);
    buf[n] = 0;
    char label[200];
    int rc = -1;
    if (sm_label_xml(buf, n, label, sizeof label) == 0) {
        char dir[512];
        if (daemon) snprintf(dir, sizeof dir, "/Library/LaunchDaemons");
        else snprintf(dir, sizeof dir, "%s/Library/LaunchAgents", sm_home());
        rc = sm_remove_plist(dir, label);
    }
    free(buf);
    return rc;
}
