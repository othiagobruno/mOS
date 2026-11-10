/* Login items em ~/Library/Preferences/com.mos.loginitems.
 * LSSharedFileList* em vscode_frameworks.c (frente Sistemas) só chama daqui.
 * A UI de Ajustes não mora aqui: o ponto de entrada é
 * -[SMAppService openSystemSettingsLoginItems] em slack_frameworks.c. */
#include "foundation.h"
#include "libc.h"

id NSHomeDirectory(void);

#include "../../kernel/include/loginitem.inc"

static const char *li_home(void) {
    const char *h = getenv("HOME");
    const char *n;
    if (h && h[0] == '/') return h;
    n = cx_cstr(NSHomeDirectory());
    if (n && n[0] == '/') return n;
    return "/Users/Visitante";
}

static int li_pref_path(char *out, int cap) {
    const char *home = li_home();
    const char *suf = "/Library/Preferences/com.mos.loginitems";
    int i = 0, j = 0;
    if (!out || cap < 2) return -1;
    while (home[i] && i + 1 < cap) { out[i] = home[i]; i++; }
    while (suf[j] && i + 1 < cap) out[i++] = suf[j++];
    out[i] = 0;
    if (suf[j] || !li_path_ok(out)) return -1;
    return 0;
}

int cx_login_add(const char *path) {
    char file[512];
    if (li_pref_path(file, (int)sizeof file)) return -1;
    return li_file_add(file, path);
}

int cx_login_remove(const char *path) {
    char file[512];
    if (!path || !li_path_ok(path)) return -1;
    if (li_pref_path(file, (int)sizeof file)) return -1;
    return li_file_remove(file, path);
}

id cx_login_snapshot(unsigned *seed) {
    char file[512], text[LI_TEXT];
    char paths[LI_MAX][LI_PATH];
    int n, i;
    id a = cx_arr_new(0);
    text[0] = 0;
    if (li_pref_path(file, (int)sizeof file) == 0) li_file_read(file, text, LI_TEXT);
    if (seed) *seed = li_seed(text);
    n = li_parse(text, paths, LI_MAX);
    for (i = 0; i < n; i++) {
        id u = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(paths[i]));
        if (u) cx_arr_add(a, u);
    }
    return a;
}
