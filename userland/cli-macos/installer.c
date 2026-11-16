/*
 * mOS — /usr/sbin/installer (Mach-O): instala pacotes .pkg/.mpkg como o installer(8) do macOS.
 *   installer -pkg <pacote> -target <volume>       volume: "/", "LocalSystem", "CurrentUserHomeDirectory" ou /Volumes/X
 *   installer -pkginfo -pkg <pacote>               lista os pacotes de componente
 *   installer -volinfo -pkg <pacote>               volumes em que dá para instalar
 *   installer -showChoicesXML -pkg <pacote>        escolhas (plist)
 *   installer -applyChoiceChangesXML <plist> ...   liga/desliga escolhas antes de instalar
 *   -verbose, -verboseR, -dumplog, -allowUntrusted, -query RestartAction
 * O motor (xar, Distribution, PackageInfo, Scripts, Payload, Bom, recibos) está em userland/pkg/pkgengine.c.
 * Os scripts preinstall/postinstall rodam com os argumentos e o ambiente do Installer da Apple; um script que
 * falha cancela a instalação. Os recibos vão para <volume>/var/db/receipts (pkgutil os lê).
 */
#include "../pkg/pkgengine.c"

static int dumplog;
static void log_line(const char *m) { if (pk_verbose || dumplog) printf("installer: %s\n", m); }

/* banner do desktop do mOS (SYS_DESKTOP, MOS_DESK_BANNER) quando instalado pelo Finder/Arquivos */
static void banner(const char *title, const char *body) {
#if defined(__APPLE__) && defined(__x86_64__)
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(63L), "D"(1L), "S"(title), "d"(body), "r"(0L) : "rcx", "r11", "memory");
    (void)r;
#else
    (void)title; (void)body;
#endif
}

static void usage(void) {
    fprintf(stderr, "uso: installer -pkg <pacote> -target <volume> [-verbose] [-dumplog] [-allowUntrusted]\n"
                    "     installer -pkginfo | -volinfo | -showChoicesXML -pkg <pacote>\n"
                    "     installer -applyChoiceChangesXML <plist> -pkg <pacote> -target <volume>\n");
}

static const char *resolve_target(const char *t, char *buf, size_t cap) {
    if (!t || !strcmp(t, "/") || !strcmp(t, "LocalSystem")) return "/";
    if (!strcmp(t, "CurrentUserHomeDirectory")) { const char *h = getenv("HOME"); snprintf(buf, cap, "%s", h ? h : "/Users/Visitante"); return buf; }
    snprintf(buf, cap, "%s", t);
    return buf;
}

/* -applyChoiceChangesXML: array de dicts {choiceIdentifier, choiceAttribute = selected, attributeSetting = 0/1} */
static int apply_choices(pkg_t *p, const char *file) {
    u64 n;
    u8 *d = read_file(file, &n);
    if (!d) { fprintf(stderr, "installer: não foi possível ler %s\n", file); return -1; }
    xnode *x = xparse((char *)d, n), *arr = xfind(x, "array");
    xnode *gs = p->dist ? xfind(p->dist, "installer-gui-script") : 0;
    if (!gs && p->dist) gs = xfind(p->dist, "installer-script");
    for (xnode *dict = arr ? arr->child : 0; dict; dict = dict->next) {
        if (strcmp(dict->name, "dict")) continue;
        const char *cid = 0, *attr = 0;
        int val = -1;
        for (xnode *k = dict->child; k; k = k->next) {
            if (strcmp(k->name, "key") || !k->next) continue;
            if (!strcmp(xtext(k), "choiceIdentifier")) cid = xtext(k->next);
            else if (!strcmp(xtext(k), "choiceAttribute")) attr = xtext(k->next);
            else if (!strcmp(xtext(k), "attributeSetting")) {
                xnode *v = k->next;
                val = !strcmp(v->name, "true") ? 1 : !strcmp(v->name, "false") ? 0 : atoi(xtext(v));
            }
            k = k->next;
        }
        if (!cid || !attr || strcmp(attr, "selected") || val < 0) continue;
        for (xnode *ch = gs ? gs->child : 0; ch; ch = ch->next) {
            if (strcmp(ch->name, "choice") || !xattr_of(ch, "id") || strcmp(xattr_of(ch, "id"), cid)) continue;
            for (xnode *r = ch->child; r; r = r->next) {
                if (strcmp(r->name, "pkg-ref") || !xattr_of(r, "id")) continue;
                for (int i = 0; i < p->nc; i++) if (!strcmp(p->c[i].id, xattr_of(r, "id"))) p->c[i].selected = val;
            }
        }
        for (int i = 0; i < p->nc; i++) if (!strcmp(p->c[i].id, cid)) p->c[i].selected = val;   /* pacote de componente */
    }
    xfree(x);
    free(d);
    return 0;
}

static void show_choices(pkg_t *p) {
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<array>\n");
    xnode *gs = p->dist ? xfind(p->dist, "installer-gui-script") : 0;
    if (!gs && p->dist) gs = xfind(p->dist, "installer-script");
    int any = 0;
    for (xnode *ch = gs ? gs->child : 0; ch; ch = ch->next) {
        if (strcmp(ch->name, "choice") || !xattr_of(ch, "id")) continue;
        int sel = 1;
        for (xnode *r = ch->child; r; r = r->next)
            if (!strcmp(r->name, "pkg-ref") && xattr_of(r, "id"))
                for (int i = 0; i < p->nc; i++) if (!strcmp(p->c[i].id, xattr_of(r, "id")) && !p->c[i].selected) sel = 0;
        printf("\t<dict>\n\t\t<key>choiceIdentifier</key>\n\t\t<string>%s</string>\n\t\t<key>choiceIsSelected</key>\n\t\t<integer>%d</integer>\n"
               "\t\t<key>choiceIsEnabled</key>\n\t\t<true/>\n\t\t<key>choiceIsVisible</key>\n\t\t<%s/>\n\t\t<key>title</key>\n\t\t<string>%s</string>\n\t</dict>\n",
               xattr_of(ch, "id"), sel, xattr_of(ch, "visible") && !strcmp(xattr_of(ch, "visible"), "false") ? "false" : "true",
               xattr_of(ch, "title") ? xattr_of(ch, "title") : xattr_of(ch, "id"));
        any = 1;
    }
    for (int i = 0; !any && i < p->nc; i++)
        printf("\t<dict>\n\t\t<key>choiceIdentifier</key>\n\t\t<string>%s</string>\n\t\t<key>choiceIsSelected</key>\n\t\t<integer>%d</integer>\n\t</dict>\n", p->c[i].id, p->c[i].selected);
    printf("</array>\n</plist>\n");
}

int main(int argc, char **argv) {
    const char *pkgp = 0, *target = 0, *choices = 0, *query = 0;
    int pkginfo = 0, volinfo = 0, showch = 0, from_gui = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((!strcmp(a, "-pkg") || !strcmp(a, "-package")) && i + 1 < argc) pkgp = argv[++i];
        else if (!strcmp(a, "-target") && i + 1 < argc) target = argv[++i];
        else if (!strcmp(a, "-applyChoiceChangesXML") && i + 1 < argc) choices = argv[++i];
        else if (!strcmp(a, "-query") && i + 1 < argc) query = argv[++i];
        else if (!strcmp(a, "-verbose") || !strcmp(a, "-verboseR") || !strcmp(a, "-vers")) pk_verbose = 1;
        else if (!strcmp(a, "-dumplog")) dumplog = 1;
        else if (!strcmp(a, "-pkginfo")) pkginfo = 1;
        else if (!strcmp(a, "-volinfo")) volinfo = 1;
        else if (!strcmp(a, "-showChoicesXML") || !strcmp(a, "-showChoiceChangesXML")) showch = 1;
        else if (!strcmp(a, "-allowUntrusted") || !strcmp(a, "-store") || !strcmp(a, "-plist")) {}
        else if (!strcmp(a, "-mosgui")) from_gui = 1;     /* aberto pelo Arquivos: avisa no fim com um banner */
        else if (!strcmp(a, "-help") || !strcmp(a, "-h")) { usage(); return 0; }
        else { fprintf(stderr, "installer: opção desconhecida: %s\n", a); usage(); return 1; }
    }
    if (!pkgp) { usage(); return 1; }
    pkg_t p;
    if (pkg_open(&p, pkgp) < 0) {
        fprintf(stderr, "installer: Error - %s\n", pk_err);
        if (from_gui) banner("Pacote recusado", pk_err);
        return 1;
    }
    pkg_apply_dist_choices(&p);
    if (choices && apply_choices(&p, choices) < 0) { pkg_close(&p); return 1; }
    if (pkginfo) { for (int i = 0; i < p.nc; i++) printf("%s\n", p.c[i].id); pkg_close(&p); return 0; }
    if (volinfo) { printf("/\n"); DIR *d = opendir("/Volumes"); struct dirent *e; while (d && (e = readdir(d))) if (e->d_name[0] != '.') printf("/Volumes/%s\n", e->d_name); if (d) closedir(d); pkg_close(&p); return 0; }
    if (showch) { show_choices(&p); pkg_close(&p); return 0; }
    if (query) {
        const char *rs = "None";
        xnode *ra = p.dist ? xfind(p.dist, "pkg-ref") : 0;
        for (; ra; ra = ra->next) if (xattr_of(ra, "onConclusion")) { rs = xattr_of(ra, "onConclusion"); break; }
        if (!strcmp(query, "RestartAction")) printf("%s\n", rs);
        else printf("\n");
        pkg_close(&p);
        return 0;
    }
    if (!target) { fprintf(stderr, "installer: falta -target\n"); usage(); pkg_close(&p); return 1; }
    char tb[1024];
    const char *vol = resolve_target(target, tb, sizeof tb);
    struct stat st;
    if (stat(vol, &st) < 0 || !S_ISDIR(st.st_mode)) { fprintf(stderr, "installer: Error - o volume %s não existe\n", vol); pkg_close(&p); return 1; }
    printf("installer: Package name is %s\n", p.title);
    printf("installer: Installing at base path %s\n", vol);
    fflush(stdout);
    inst_result_t res;
    int r = pkg_install(&p, vol, &res, log_line);
    if (r < 0) {
        printf("installer: The install failed. (%s)\n", pk_err);
        if (from_gui) banner("A instalação falhou", pk_err);
        pkg_close(&p);
        return 1;
    }
    if (pk_verbose) printf("installer: %d arquivo(s), %d pasta(s), %d link(s), %d script(s)\n", res.nfiles, res.ndirs, res.nlinks, res.scripts);
    printf("installer: The install was successful.\n");
    if (from_gui) { char b[600]; snprintf(b, sizeof b, "%s foi instalado.", p.title); banner("Instalação concluída", b); }
    pkg_close(&p);
    return 0;
}
