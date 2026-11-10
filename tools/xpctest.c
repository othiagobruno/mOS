/* gcc -std=c11 -Wall -Wextra -O1 -D_GNU_SOURCE -Ikernel/include tools/xpctest.c -o /tmp/xpctest && /tmp/xpctest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "bundlereg.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int hs_list(void *ctx, const char *dir, int index, char *name, int cap) {
    DIR *d;
    struct dirent *e;
    int i = 0;
    (void)ctx;
    if (!name || cap < 2) return 0;
    d = opendir(dir);
    if (!d) return 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (i == index) {
            snprintf(name, (size_t)cap, "%s", e->d_name);
            closedir(d);
            return name[0] ? 1 : 0;
        }
        i++;
    }
    closedir(d);
    return 0;
}

static int hs_read(void *ctx, const char *path, char *buf, int cap) {
    FILE *f;
    int n;
    (void)ctx;
    if (!buf || cap < 2) return -1;
    memset(buf, 0, (size_t)cap);
    f = fopen(path, "rb");
    if (!f) return -1;
    n = (int)fread(buf, 1, (size_t)cap - 1, f);
    fclose(f);
    if (n < 0) n = 0;
    buf[n] = 0;
    return n;
}

static int hs_isfile(void *ctx, const char *path) {
    struct stat st;
    (void)ctx;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void mk(const char *path) {
    char tmp[512], *s;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (s = tmp + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(tmp, 0755); *s = '/'; }
    mkdir(tmp, 0755);
}

static void wr(const char *path, const char *text) {
    FILE *f;
    char tmp[512], *s;
    snprintf(tmp, sizeof tmp, "%s", path);
    s = strrchr(tmp, '/');
    if (s) { *s = 0; mk(tmp); }
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    if (text) fputs(text, f);
    fclose(f);
}

static void xpc(const char *base, const char *id, const char *bin) {
    char p[512], plist[700];
    snprintf(p, sizeof p, "%s/Contents/Info.plist", base);
    snprintf(plist, sizeof plist,
             "<?xml version=\"1.0\"?><plist><dict>"
             "<key>CFBundleIdentifier</key><string>%s</string>"
             "<key>CFBundleExecutable</key><string>%s</string>"
             "</dict></plist>", id, bin);
    wr(p, plist);
    snprintf(p, sizeof p, "%s/Contents/MacOS/%s", base, bin);
    wr(p, "exe");
}

int main(void) {
    char dir[] = "/tmp/mos-xpc-XXXXXX";
    char root[256], base[512], line[480], all[4096];
    br_fs_t fs;
    br_item_t items[BR_MAX];
    int n, i, xpc_n = 0, auth = 0, refused = 0;
    const char *plist =
        "<?xml version=\"1.0\"?><plist><dict>"
        "<key>CFBundleIdentifier</key><string>com.openai.CodexComputerUseAuthorizationPlugin</string>"
        "</dict></plist>";
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(root, sizeof root, "%s/Demo.app", dir);
    snprintf(base, sizeof base, "%s/Contents/XPCServices/Local.xpc", root);
    xpc(base, "com.mos.local", "Local");
    snprintf(base, sizeof base, "%s/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Downloader.xpc", root);
    xpc(base, "org.sparkle-project.DownloaderService", "Downloader");
    snprintf(base, sizeof base, "%s/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Installer.xpc", root);
    xpc(base, "org.sparkle-project.InstallerService", "Installer");
    snprintf(base, sizeof base, "%s/Contents/Frameworks/Sparkle.framework/Versions/B/Updater.app/Contents/XPCServices/UpdaterXPC.xpc", root);
    xpc(base, "org.sparkle-project.UpdaterXPC", "UpdaterXPC");
    snprintf(base, sizeof base, "%s/Contents/XPCServices/Bad.xpc/Contents/Info.plist", root);
    wr(base, "<?xml version=\"1.0\"?><plist><dict><key>CFBundleIdentifier</key><string>org.evil.Escape</string>"
            "<key>CFBundleExecutable</key><string>../sh</string></dict></plist>");
    snprintf(base, sizeof base, "%s/fora", dir);
    wr(base, "SENTINELA");
    snprintf(base, sizeof base, "%s/Contents/Library/SecurityAgentPlugins/CodexComputerUseAuthorizationPlugin.bundle/Contents/Info.plist", root);
    wr(base, plist);
    snprintf(base, sizeof base, "%s/Contents/Library/SecurityAgentPlugins/SemInfo.bundle/Contents", root);
    mk(base);

    fs.list = hs_list;
    fs.read = hs_read;
    fs.isfile = hs_isfile;
    fs.ctx = 0;
    memset(items, 0, sizeof items);
    n = br_scan(&fs, root, items, BR_MAX);
    all[0] = 0;
    printf("registro:\n");
    for (i = 0; i < n; i++) {
        CHECK(br_fmt(&items[i], line, (int)sizeof line) > 0);
        printf("  %s\n", line);
        if (strlen(all) + strlen(line) + 2 < sizeof all) {
            strcat(all, line);
            strcat(all, "\n");
        }
        if (items[i].kind == BR_XPC) {
            xpc_n++;
            CHECK(items[i].exe[0] == '/');
            CHECK(!strstr(items[i].exe, ".."));
            CHECK(strncmp(items[i].exe, root, strlen(root)) == 0);
            printf("  job %s runatload=0 nsvc=1 mach=%s privilegio=0\n", items[i].label, items[i].label);
        } else {
            CHECK(items[i].exe[0] == 0);
            if (items[i].kind == BR_AUTH) auth++;
            if (items[i].kind == BR_AUTH_NO) refused++;
        }
    }
    CHECK(xpc_n == 4);
    CHECK(auth == 1);
    CHECK(refused == 1);
    CHECK(strstr(all, "xpc org.sparkle-project.DownloaderService -> ") != 0);
    CHECK(strstr(all, "xpc org.sparkle-project.InstallerService -> ") != 0);
    CHECK(strstr(all, "xpc com.mos.local -> ") != 0);
    CHECK(strstr(all, "xpc org.sparkle-project.UpdaterXPC -> ") != 0);
    CHECK(strstr(all, "authplugin com.openai.CodexComputerUseAuthorizationPlugin registrado em ") != 0);
    CHECK(strstr(all, "não carregado e sem privilégio") != 0);
    CHECK(strstr(all, "authplugin recusado ") != 0);
    CHECK(strstr(all, "nenhum privilégio concedido") != 0);
    CHECK(strstr(all, "org.evil.Escape") == 0);
    CHECK(strstr(all, "SENTINELA") == 0);
    CHECK(strstr(all, "/fora") == 0);
    printf("xpctest: %d xpc, %d auth registrado, %d recusado, privilegio=0\n", xpc_n, auth, refused);
    printf("xpctest: %d/%d PASS\n", checks - fails, checks);
    snprintf(line, sizeof line, "rm -rf '%s'", dir);
    if (system(line)) return 1;
    return fails ? 1 : 0;
}
