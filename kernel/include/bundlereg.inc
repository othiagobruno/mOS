/* XPC services e plugins de autorização achados num .app.
 * Sem string.h. Quem inclui já tem memcpy. Não carrega o plugin e não dá privilégio.
 * BR_XPC: sob demanda (runatload 0). BR_AUTH: só o registro. BR_AUTH_NO: recusa com motivo. */
#ifndef BR_MAX
#define BR_MAX 24
#endif
#define BR_NAME 128
#define BR_PATH 320

enum { BR_XPC = 1, BR_AUTH = 2, BR_AUTH_NO = 3 };

typedef struct br_fs {
    int (*list)(void *ctx, const char *dir, int index, char *name, int cap);
    int (*read)(void *ctx, const char *path, char *buf, int cap);
    int (*isfile)(void *ctx, const char *path);
    void *ctx;
} br_fs_t;

typedef struct br_item {
    int kind;
    char label[BR_NAME];
    char exe[BR_PATH];
    char where[BR_PATH];
    char why[64];
} br_item_t;

static int br_copy(char *d, int cap, const char *s) {
    int i = 0;
    if (!d || cap < 1) return -1;
    if (!s) s = "";
    while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; }
    d[i] = 0;
    return s[i] ? -1 : 0;
}

static int br_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int br_ends(const char *s, const char *suf) {
    int a = 0, b = 0, i;
    if (!s || !suf) return 0;
    while (s[a]) a++;
    while (suf[b]) b++;
    if (a < b) return 0;
    for (i = 0; i < b; i++) if (s[a - b + i] != suf[i]) return 0;
    return 1;
}

static int br_has(const char *s, const char *n) {
    int i, j, ln = 0;
    if (!s || !n) return 0;
    while (n[ln]) ln++;
    if (!ln) return 0;
    for (i = 0; s[i]; i++) {
        for (j = 0; j < ln && s[i + j] == n[j]; j++) {}
        if (j == ln) return 1;
    }
    return 0;
}

/* Um componente de caminho. Recusa vazio, ".", "..", barra e quebra de linha. */
static int br_name_ok(const char *s) {
    int i;
    if (!s || !s[0] || s[0] == '.') return 0;
    for (i = 0; s[i]; i++) {
        if (s[i] == '/' || s[i] == '\\' || s[i] == '\n' || s[i] == '\r') return 0;
        if (i >= 180) return 0;
    }
    return 1;
}

static int br_id_ok(const char *s) {
    int i;
    if (!s || !s[0]) return 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
        if (!ok || i >= BR_NAME - 2) return 0;
    }
    return 1;
}

static int br_join(char *out, int cap, const char *a, const char *b) {
    int i = 0, j = 0;
    if (!out || cap < 2 || !a || !br_name_ok(b)) return -1;
    while (a[i] && i + 1 < cap) { out[i] = a[i]; i++; }
    if (a[i]) return -1;
    if (i && out[i - 1] != '/') {
        if (i + 1 >= cap) return -1;
        out[i++] = '/';
    }
    while (b[j] && i + 1 < cap) out[i++] = b[j++];
    if (b[j]) return -1;
    out[i] = 0;
    return 0;
}

static int br_put(char *out, int cap, int o, const char *s) {
    if (!out || cap < 1 || o < 0) return -1;
    while (s && *s) {
        if (o + 1 >= cap) { if (cap > 0) out[cap - 1] = 0; return -1; }
        out[o++] = *s++;
    }
    if (o >= cap) return -1;
    out[o] = 0;
    return o;
}

static int br_fmt(const br_item_t *it, char *out, int cap) {
    int o = 0;
    if (!it || !out) return -1;
    if (it->kind == BR_XPC) {
        o = br_put(out, cap, 0, "xpc ");
        o = br_put(out, cap, o, it->label);
        o = br_put(out, cap, o, " -> ");
        o = br_put(out, cap, o, it->exe);
        return o;
    }
    if (it->kind == BR_AUTH) {
        o = br_put(out, cap, 0, "authplugin ");
        o = br_put(out, cap, o, it->label);
        o = br_put(out, cap, o, " registrado em ");
        o = br_put(out, cap, o, it->where);
        o = br_put(out, cap, o, "; não carregado e sem privilégio");
        return o;
    }
    if (it->kind == BR_AUTH_NO) {
        o = br_put(out, cap, 0, "authplugin recusado ");
        o = br_put(out, cap, o, it->where);
        o = br_put(out, cap, o, " (");
        o = br_put(out, cap, o, it->why[0] ? it->why : "sem identificador");
        o = br_put(out, cap, o, "); nenhum privilégio concedido");
        return o;
    }
    return -1;
}

static int br_plist_string(const char *xml, const char *key, char *out, int cap) {
    const char *p = xml ? xml : "";
    int kn = 0, i;
    while (key[kn]) kn++;
    while (*p) {
        if (p[0] == '<' && p[1] == 'k' && p[2] == 'e' && p[3] == 'y' && p[4] == '>') {
            int match = 1;
            p += 5;
            for (i = 0; i < kn; i++) if (p[i] != key[i]) { match = 0; break; }
            if (match && p[kn] == '<' && p[kn + 1] == '/') {
                p += kn;
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
                while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
                if (p[0] == '<' && p[1] == 's' && p[2] == 't' && p[3] == 'r' && p[4] == 'i' && p[5] == 'n' && p[6] == 'g' && p[7] == '>') {
                    int n = 0;
                    p += 8;
                    while (*p && !(*p == '<' && p[1] == '/')) {
                        if (n + 1 < cap) out[n++] = *p;
                        p++;
                    }
                    while (n && (out[n - 1] == ' ' || out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == '\t')) n--;
                    if (cap > 0) out[n < cap ? n : cap - 1] = 0;
                    return n > 0 && n < cap ? 0 : -1;
                }
            }
            continue;
        }
        p++;
    }
    return -1;
}

static void br_add(br_item_t *out, int *n, int max, int kind, const char *label, const char *exe, const char *where, const char *why) {
    br_item_t *it;
    int i;
    if (!out || !n || *n >= max || *n >= BR_MAX) return;
    if (kind == BR_XPC) {
        for (i = 0; i < *n; i++) if (out[i].kind == BR_XPC && br_eq(out[i].label, label)) return;
    }
    it = &out[*n];
    it->kind = kind;
    it->label[0] = it->exe[0] = it->where[0] = it->why[0] = 0;
    if (br_copy(it->label, BR_NAME, label) < 0 && kind == BR_XPC) return;
    br_copy(it->exe, BR_PATH, exe ? exe : "");
    br_copy(it->where, BR_PATH, where ? where : "");
    br_copy(it->why, (int)sizeof it->why, why ? why : "");
    (*n)++;
}

static void br_add_xpc(const br_fs_t *fs, const char *dir, const char *name, br_item_t *out, int *n, int max) {
    char base[BR_PATH], plist[BR_PATH], mac[BR_PATH], exe[BR_PATH], tmp[BR_PATH];
    char xml[2048], id[BR_NAME], bin[BR_NAME];
    int rn;
    if (!br_ends(name, ".xpc")) return;
    if (br_join(base, BR_PATH, dir, name)) return;
    if (br_join(tmp, BR_PATH, base, "Contents")) return;
    if (br_join(plist, BR_PATH, tmp, "Info.plist")) return;
    rn = fs->read(fs->ctx, plist, xml, (int)sizeof xml);
    id[0] = bin[0] = 0;
    if (rn >= 0) {
        br_plist_string(xml, "CFBundleIdentifier", id, BR_NAME);
        br_plist_string(xml, "CFBundleExecutable", bin, BR_NAME);
    }
    if (!br_id_ok(id)) {
        int i = 0, w = 0;
        while (name[i] && !(name[i] == '.' && br_eq(name + i, ".xpc"))) {
            if (w + 1 < BR_NAME) id[w++] = name[i];
            i++;
        }
        id[w] = 0;
        if (!br_id_ok(id)) return;
    }
    if (!br_name_ok(bin)) {
        int i = 0, w = 0;
        while (name[i] && !(name[i] == '.' && br_eq(name + i, ".xpc"))) {
            if (w + 1 < BR_NAME) bin[w++] = name[i];
            i++;
        }
        bin[w] = 0;
        if (!br_name_ok(bin)) return;
    }
    if (br_join(mac, BR_PATH, tmp, "MacOS")) return;
    if (br_join(exe, BR_PATH, mac, bin)) return;
    if (!fs->isfile(fs->ctx, exe)) return;
    br_add(out, n, max, BR_XPC, id, exe, base, "");
}

static void br_add_auth(const br_fs_t *fs, const char *dir, const char *name, br_item_t *out, int *n, int max) {
    char base[BR_PATH], tmp[BR_PATH], plist[BR_PATH], xml[2048], id[BR_NAME];
    int rn;
    if (br_join(base, BR_PATH, dir, name)) return;
    if (fs->isfile(fs->ctx, base)) return;
    if (br_join(tmp, BR_PATH, base, "Contents")) return;
    if (br_join(plist, BR_PATH, tmp, "Info.plist")) return;
    id[0] = 0;
    rn = fs->read(fs->ctx, plist, xml, (int)sizeof xml);
    if (rn < 0) {
        br_add(out, n, max, BR_AUTH_NO, "", "", base, "sem Info.plist");
        return;
    }
    if (br_plist_string(xml, "CFBundleIdentifier", id, BR_NAME) != 0 || !id[0]) {
        br_add(out, n, max, BR_AUTH_NO, "", "", base, "sem CFBundleIdentifier");
        return;
    }
    if (!br_id_ok(id)) {
        br_add(out, n, max, BR_AUTH_NO, "", "", base, "CFBundleIdentifier recusado");
        return;
    }
    br_add(out, n, max, BR_AUTH, id, "", base, "");
}

static void br_scan_xpc_dir(const br_fs_t *fs, const char *dir, br_item_t *out, int *n, int max) {
    char name[200];
    int i;
    for (i = 0; fs->list(fs->ctx, dir, i, name, (int)sizeof name); i++) {
        if (!br_name_ok(name)) continue;
        br_add_xpc(fs, dir, name, out, n, max);
    }
}

static void br_scan_auth_dir(const br_fs_t *fs, const char *dir, int force, br_item_t *out, int *n, int max) {
    char name[200];
    int i;
    for (i = 0; fs->list(fs->ctx, dir, i, name, (int)sizeof name); i++) {
        if (!br_name_ok(name)) continue;
        if (!force && !br_has(name, "AuthorizationPlugin")) continue;
        br_add_auth(fs, dir, name, out, n, max);
    }
}

static void br_scan_updater(const br_fs_t *fs, const char *app, br_item_t *out, int *n, int max) {
    char c[BR_PATH], x[BR_PATH];
    if (br_join(c, BR_PATH, app, "Contents")) return;
    if (br_join(x, BR_PATH, c, "XPCServices")) return;
    br_scan_xpc_dir(fs, x, out, n, max);
}

static void br_scan_framework(const br_fs_t *fs, const char *fw, br_item_t *out, int *n, int max) {
    char sub[BR_PATH], ver[BR_PATH], x[BR_PATH], name[200];
    int i;
    if (br_join(sub, BR_PATH, fw, "XPCServices") == 0) br_scan_xpc_dir(fs, sub, out, n, max);
    if (br_join(sub, BR_PATH, fw, "Updater.app") == 0) br_scan_updater(fs, sub, out, n, max);
    if (br_join(sub, BR_PATH, fw, "Versions") == 0) {
        for (i = 0; fs->list(fs->ctx, sub, i, name, (int)sizeof name); i++) {
            if (!br_name_ok(name)) continue;
            if (br_join(ver, BR_PATH, sub, name)) continue;
            if (br_join(x, BR_PATH, ver, "XPCServices") == 0) br_scan_xpc_dir(fs, x, out, n, max);
            if (br_join(x, BR_PATH, ver, "Updater.app") == 0) br_scan_updater(fs, x, out, n, max);
        }
    }
}

static void br_scan_auth_bundle(const br_fs_t *fs, const char *app, br_item_t *out, int *n, int max) {
    char c[BR_PATH], lib[BR_PATH], d[BR_PATH];
    if (br_join(c, BR_PATH, app, "Contents")) return;
    if (br_join(lib, BR_PATH, c, "Library") == 0) {
        if (br_join(d, BR_PATH, lib, "SecurityAgentPlugins") == 0) br_scan_auth_dir(fs, d, 1, out, n, max);
        if (br_join(d, BR_PATH, lib, "AuthorizationPlugins") == 0) br_scan_auth_dir(fs, d, 1, out, n, max);
    }
    if (br_join(d, BR_PATH, c, "PlugIns") == 0) br_scan_auth_dir(fs, d, 0, out, n, max);
    br_scan_auth_dir(fs, c, 0, out, n, max);
}

static int br_scan(const br_fs_t *fs, const char *root, br_item_t *out, int max) {
    char contents[BR_PATH], fwdir[BR_PATH], one[BR_PATH], name[200];
    int n = 0, i;
    if (!fs || !fs->list || !fs->read || !fs->isfile || !root || !out || max < 1) return 0;
    if (br_join(contents, BR_PATH, root, "Contents")) return 0;
    if (br_join(one, BR_PATH, contents, "XPCServices") == 0) br_scan_xpc_dir(fs, one, out, &n, max);
    if (br_join(fwdir, BR_PATH, contents, "Frameworks") == 0) {
        for (i = 0; fs->list(fs->ctx, fwdir, i, name, (int)sizeof name); i++) {
            if (!br_name_ok(name)) continue;
            if (br_join(one, BR_PATH, fwdir, name)) continue;
            if (br_ends(name, ".framework")) br_scan_framework(fs, one, out, &n, max);
            else if (br_ends(name, ".app")) {
                br_scan_updater(fs, one, out, &n, max);
                br_scan_auth_bundle(fs, one, out, &n, max);
            }
        }
    }
    br_scan_auth_bundle(fs, root, out, &n, max);
    return n;
}
