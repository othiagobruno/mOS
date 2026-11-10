/* Cópia de plist de agente/daemon para o diretório que o launchd já varre,
 * e um arquivo de status (login item sem plist). Sem launchd neste núcleo.
 * Incluído por smlogin.c e por tools/smlogintest.c.
 */
#ifdef SM_HOST
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
typedef FILE sm_file;
#else
typedef FILE_ sm_file;
#endif

static int sm_label_ok(const char *s) {
    if (!s || !s[0] || strlen(s) > 180) return 0;
    if (!strcmp(s, ".") || !strcmp(s, "..") || strstr(s, "..")) return 0;
    for (const char *p = s; *p; p++) {
        char c = *p;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '+' || c == ':';
        if (!ok) return 0;
    }
    return 1;
}

static int sm_label_xml(const char *xml, size_t n, char *out, size_t cap) {
    if (!xml || !out || cap < 2) return -1;
    const char *key = "<key>Label</key>";
    const char *p = 0;
    size_t kn = strlen(key);
    for (size_t i = 0; i + kn <= n; i++) if (!memcmp(xml + i, key, kn)) { p = xml + i + kn; break; }
    if (!p) return -1;
    const char *a = strstr(p, "<string>");
    const char *b = a ? strstr(a, "</string>") : 0;
    if (!a || !b || (size_t)(b - (a + 8)) >= cap) return -1;
    size_t ln = (size_t)(b - (a + 8));
    memcpy(out, a + 8, ln);
    out[ln] = 0;
    return sm_label_ok(out) ? 0 : -1;
}

static void sm_plist_name(const char *label, char *out, size_t cap) {
    snprintf(out, cap, "%s.plist", label);
}

static int sm_mkdir_parent(const char *path) {
    char b[1024];
    snprintf(b, sizeof b, "%s", path);
    char *slash = strrchr(b, '/');
    if (!slash || slash == b) return 0;
    *slash = 0;
    for (char *p = b + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; }
    mkdir(b, 0755);
    return 0;
}

static int sm_copy_plist(const char *dir, const char *label, const char *bytes, size_t n) {
    if (!dir || !sm_label_ok(label) || !bytes) return -1;
    char path[1024], tmp[1100], name[200];
    sm_plist_name(label, name, sizeof name);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    sm_mkdir_parent(path);
    sm_file *f = fopen(tmp, "wb");
    if (!f) return -1;
    size_t w = fwrite(bytes, 1, n, f);
    if (fclose(f) != 0 || w != n) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static int sm_remove_plist(const char *dir, const char *label) {
    if (!dir || !sm_label_ok(label)) return -1;
    char path[1024], name[200];
    sm_plist_name(label, name, sizeof name);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (unlink(path) != 0) return -1;
    return 0;
}

static int sm_plist_present(const char *dir, const char *label) {
    if (!dir || !sm_label_ok(label)) return 0;
    char path[1024], name[200];
    sm_plist_name(label, name, sizeof name);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    sm_file *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int sm_status_set(const char *path, const char *key, int status) {
    if (!path || !sm_label_ok(key) || status < 0 || status > 9) return -1;
    char lines[64][220];
    int n = 0, found = 0;
    sm_file *in = fopen(path, "rb");
    if (in) {
        char line[256];
        while (n < 64 && fgets(line, sizeof line, in)) {
            char k[184];
            int st = 0;
            if (sscanf(line, "%183s %d", k, &st) != 2) continue;
            if (!strcmp(k, key)) { snprintf(lines[n], sizeof lines[n], "%s %d\n", key, status); found = 1; }
            else snprintf(lines[n], sizeof lines[n], "%s %d\n", k, st);
            n++;
        }
        fclose(in);
    }
    if (!found) {
        if (n >= 64) return -1;
        snprintf(lines[n], sizeof lines[n], "%s %d\n", key, status);
        n++;
    }
    sm_mkdir_parent(path);
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    sm_file *f = fopen(tmp, "wb");
    if (!f) return -1;
    for (int i = 0; i < n; i++) fputs(lines[i], f);
    if (fclose(f) != 0) return -1;
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static int sm_status_get(const char *path, const char *key) {
    if (!path || !sm_label_ok(key)) return -1;
    sm_file *f = fopen(path, "rb");
    if (!f) return -1;
    char line[256];
    int found = -1;
    while (fgets(line, sizeof line, f)) {
        char k[184];
        int st = 0;
        if (sscanf(line, "%183s %d", k, &st) == 2 && !strcmp(k, key)) found = st;
    }
    fclose(f);
    return found;
}
