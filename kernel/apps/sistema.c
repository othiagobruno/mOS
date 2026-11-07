/* mOS — Informações do Sistema (relatório real do hardware e do software, exportável) e Utilitário de Disco
 * (discos SATA/NVMe/USB, volumes montados, uso de espaço, imagens .dmg, Primeiros Socorros, ejetar). */
#include "kit.h"
#include "net.h"
#include "storage.h"
#include "power.h"
#include "usb.h"
#include "fs.h"

const char *bt_status(void);
extern app_t app_sysinfo, app_disco;

static void fmt_bytes(char *o, int n, u64 b) {
    if (b >= (1ull << 40)) snprintf(o, n, "%llu,%llu TB", (unsigned long long)(b >> 40), (unsigned long long)(((b >> 30) & 1023) * 10 / 1024));
    else if (b >= (1ull << 30)) snprintf(o, n, "%llu,%llu GB", (unsigned long long)(b >> 30), (unsigned long long)(((b >> 20) & 1023) * 10 / 1024));
    else if (b >= (1ull << 20)) snprintf(o, n, "%llu MB", (unsigned long long)(b >> 20));
    else if (b >= 1024) snprintf(o, n, "%llu KB", (unsigned long long)(b >> 10));
    else snprintf(o, n, "%llu bytes", (unsigned long long)b);
}

/* ================================================================== Informações do Sistema */
enum { S_OVERVIEW, S_CPU, S_MEM, S_PCI, S_USB, S_STORAGE, S_AUDIO, S_BT, S_DISPLAY, S_POWER, S_NET, S_SOFT, S_APPS, S_COUNT };
static const char *sec_names[S_COUNT] = {"Visão Geral", "Processador", "Memória", "PCI", "USB", "Armazenamento", "Áudio", "Bluetooth",
                                         "Gráficos/Monitores", "Energia", "Rede", "Software", "Aplicativos"};
static struct { window_t *w; int sec, scroll, sel_row; } I;

typedef struct { char k[64], v[160]; bool head; } kv_t;
static kv_t rows[512];
static int nrows;
static void kv(const char *k, const char *fmt, ...) {
    if (nrows >= 512) return;
    snprintf(rows[nrows].k, 64, "%s", k);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(rows[nrows].v, 160, fmt, ap);
    va_end(ap);
    rows[nrows].head = false;
    nrows++;
}
static void head(const char *t) { if (nrows >= 512) return; snprintf(rows[nrows].k, 64, "%s", t); rows[nrows].v[0] = 0; rows[nrows].head = true; nrows++; }

static void collect(int sec) {
    nrows = 0;
    char a[64], b[64];
    switch (sec) {
    case S_OVERVIEW: {
        char vendor[16] = "", brand[64] = "";
        cpu_info(vendor, brand);
        kv("Sistema", "%s %s", MOS_NAME, MOS_VERSION);
        kv("Processador", "%s", brand[0] ? brand : vendor);
        kv("Núcleos", "%d", par_cpus());
        fmt_bytes(a, sizeof a, pmm_total_pages() * 4096);
        kv("Memória", "%s", a);
        kv("Discos", "%d", storage_disk_count());
        kv("Dispositivos PCI", "%d", pci_count());
        kv("Dispositivos USB", "%d", usb_device_count());
        u64 up = timer_ticks() / 1000;
        kv("Tempo desde o início", "%llu h %llu min %llu s", (unsigned long long)(up / 3600), (unsigned long long)(up / 60 % 60), (unsigned long long)(up % 60));
    } break;
    case S_CPU: {
        char vendor[16] = "", brand[64] = "";
        cpu_info(vendor, brand);
        kv("Nome do Processador", "%s", brand);
        kv("Fabricante", "%s", vendor);
        kv("Arquitetura", "x86_64 (long mode)");
        kv("Núcleos ativos", "%d (os extras trabalham em paralelo na composição gráfica)", par_cpus());
        u64 t0 = rdtsc(), m0 = timer_ticks();
        while (timer_ticks() - m0 < 20) __asm__ volatile("pause");
        u64 t1 = rdtsc();
        kv("Velocidade estimada", "%llu MHz", (unsigned long long)((t1 - t0) / 20000));
        u32 eax = 1, ebx, ecx, edx;
        __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
        kv("Família/Modelo/Revisão", "%u / %u / %u", ((eax >> 8) & 15) + ((eax >> 20) & 255), ((eax >> 4) & 15) | (((eax >> 16) & 15) << 4), eax & 15);
        char feat[160] = "";
        if (edx & (1 << 25)) strcat(feat, "SSE ");
        if (edx & (1 << 26)) strcat(feat, "SSE2 ");
        if (ecx & 1) strcat(feat, "SSE3 ");
        if (ecx & (1 << 9)) strcat(feat, "SSSE3 ");
        if (ecx & (1 << 19)) strcat(feat, "SSE4.1 ");
        if (ecx & (1 << 20)) strcat(feat, "SSE4.2 ");
        if (ecx & (1 << 25)) strcat(feat, "AES ");
        if (ecx & (1 << 28)) strcat(feat, "AVX ");
        if (ecx & (1 << 30)) strcat(feat, "RDRAND ");
        if (ecx & (1 << 31)) strcat(feat, "Hipervisor ");
        kv("Recursos", "%s", feat);
    } break;
    case S_MEM:
        fmt_bytes(a, sizeof a, pmm_total_pages() * 4096);
        kv("Memória total", "%s", a);
        fmt_bytes(a, sizeof a, (pmm_total_pages() - pmm_free_pages()) * 4096);
        kv("Em uso", "%s", a);
        fmt_bytes(a, sizeof a, pmm_free_pages() * 4096);
        kv("Livre", "%s", a);
        fmt_bytes(a, sizeof a, heap_used());
        kv("Heap do núcleo", "%s", a);
        fmt_bytes(a, sizeof a, vfs_total_bytes());
        kv("Arquivos em memória", "%s", a);
        kv("Tamanho da página", "4 KB (núcleo em páginas de 2 MB)");
        break;
    case S_PCI:
        for (int i = 0; i < pci_count(); i++) {
            pci_dev_t *d = pci_get(i);
            snprintf(a, sizeof a, "%02x:%02x.%x", d->bus, d->dev, d->fn);
            kv(a, "%s — %04x:%04x%s", pci_class_name(d->class_, d->subclass), d->vendor, d->device, d->irq && d->irq < 255 ? "" : "");
        }
        if (!pci_count()) kv("PCI", "Nenhum dispositivo");
        break;
    case S_USB:
        kv("Controladores xHCI", "%d (%d portas)", xhci_count(), xhci_total_ports());
        for (int i = 0; i < usb_device_count(); i++) { snprintf(a, sizeof a, "Dispositivo %d", i + 1); kv(a, "%s", usb_device_desc(i)); }
        if (!usb_device_count()) kv("Barramento USB", "Nenhum dispositivo conectado");
        kv("Estado", "%s", usb_status());
        break;
    case S_STORAGE:
        for (int i = 0; i < storage_disk_count(); i++) {
            head(storage_disk_name(i));
            kv("Modelo", "%s", storage_disk_desc(i));
            blkdev_t *d = storage_disk(i);
            fmt_bytes(a, sizeof a, d ? d->size : 0);
            kv("Capacidade", "%s", a);
            kv("Gravável", "%s", d && d->write ? "Sim" : "Não");
            kv("Disco do sistema", "%s", persist_disk_index() == i ? "Sim (persistência ativa)" : "Não");
        }
        if (!storage_disk_count()) kv("Discos", "Nenhum disco interno (o sistema roda da memória)");
        kv("SATA (AHCI)", "%s", ahci_status());
        break;
    case S_AUDIO:
        kv("Saída", "%s", audio_device_name());
        kv("Volume", "%d%%", audio_get_volume());
        kv("Entrada", "%s", audio_input_available() ? audio_input_name() : "Nenhuma");
        kv("Formato", "48 kHz, 16 bits, estéreo; mixer de 16 vozes");
        break;
    case S_BT:
        kv("Estado", "%s", bt_status());
        break;
    case S_DISPLAY:
        for (int i = 0; i < display_count(); i++) {
            rect_t r = display_rect(i);
            head(display_name(i));
            kv("Resolução", "%d × %d", r.w, r.h);
            kv("Posição", "%d, %d", r.x, r.y);
            kv("Profundidade", "32 bits (ARGB8888)");
        }
        kv("Aceleração", "Composição por software em %d núcleo(s), SSE2", par_cpus());
        break;
    case S_POWER: {
        battery_status_t st;
        battery_status(&st);
        if (!st.present) kv("Bateria", "Nenhuma (computador de mesa)");
        else {
            kv("Carga", "%d%%", st.percent);
            kv("Fonte", "%s", st.source ? st.source : "—");
            kv("Carregando", "%s", st.charging ? "Sim" : "Não");
            if (st.minutes_left >= 0) kv("Tempo restante", "%d h %02d min", st.minutes_left / 60, st.minutes_left % 60);
            kv("Driver", "%s", st.driver ? st.driver : "—");
        }
    } break;
    case S_NET: {
        net_info_t *n = net_info();
        kv("Interface", "%s", net_device_name());
        kv("Driver", "%s", n ? n->driver : "—");
        if (n) {
            kv("Endereço MAC", "%02x:%02x:%02x:%02x:%02x:%02x", n->mac[0], n->mac[1], n->mac[2], n->mac[3], n->mac[4], n->mac[5]);
            kv("Conectado", "%s", n->link ? "Sim" : "Não");
            ip_to_str(n->ip, a); kv("Endereço IPv4", "%s", a);
            ip_to_str(n->mask, a); kv("Máscara", "%s", a);
            ip_to_str(n->gw, a); kv("Roteador", "%s", a);
            ip_to_str(n->dns, a); kv("DNS", "%s", a);
            fmt_bytes(a, sizeof a, n->rx_bytes); fmt_bytes(b, sizeof b, n->tx_bytes);
            kv("Recebidos / Enviados", "%s / %s", a, b);
            kv("Pacotes", "%llu recebidos, %llu enviados", (unsigned long long)n->rx_packets, (unsigned long long)n->tx_packets);
        }
        char st[128];
        net_status_string(st, sizeof st);
        kv("Estado", "%s", st);
    } break;
    case S_SOFT: {
        kv("Versão do Sistema", "%s %s", MOS_NAME, MOS_VERSION);
        kv("Núcleo", "Nucleo (x86_64, UEFI)");
        kv("Modo de inicialização", "UEFI, sem carregador intermediário");
        kv("Volume de inicialização", "%s", persist_available() ? "Disco com persistência" : "Pendrive / memória");
        kv("Usuário", "%s", CUR_HOME);
        datetime_t d;
        rtc_now(&d);
        kv("Data e hora", "%02d/%02d/%04d %02d:%02d", d.day, d.month, d.year, d.hour, d.minute);
        kv("Compatibilidade", "Executáveis Mach-O x86_64 (macOS) e ELF64 do mOS");
    } break;
    case S_APPS:
        for (int i = 0; i < apps_count(); i++) {
            app_t *ap = apps_get(i);
            kv(ap->name, "%s%s", ap->exec && !strncmp(ap->exec, "mac:", 4) ? ap->exec + 4 : "Aplicativo do sistema", ap->running ? " — aberto" : "");
        }
        break;
    }
}

static void export_report(void) {
    sbuf_t b = {0};
    sb_printf(&b, "Relatório do Sistema — %s %s\n\n", MOS_NAME, MOS_VERSION);
    for (int s = 0; s < S_COUNT; s++) {
        collect(s);
        sb_printf(&b, "== %s ==\n", sec_names[s]);
        for (int i = 0; i < nrows; i++) {
            if (rows[i].head) sb_printf(&b, "\n  %s\n", rows[i].k);
            else sb_printf(&b, "  %s: %s\n", rows[i].k, rows[i].v);
        }
        sb_putc(&b, '\n');
    }
    char p[200];
    snprintf(p, sizeof p, "%s/Mesa/Relatório do Sistema.txt", CUR_HOME);
    vfs_write_file(p, b.p, b.len);
    sb_free(&b);
    notify("Relatório salvo", p);
    collect(I.sec);
}

static void si_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Hardware", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < S_COUNT; i++) {
        if (i == S_NET) { y += 6; ui_label(u, 18, y + 2, "Rede", FONT_BOLD, 11.5f, T.text2); y += 22; }
        if (i == S_SOFT) { y += 6; ui_label(u, 18, y + 2, "Software", FONT_BOLD, 11.5f, T.text2); y += 22; }
        if (ui_sidebar_item(u, 100000 + i, rect_make(10, y, sb - 20, 26), sec_names[i], I.sec == i, NULL)) { I.sec = i; I.scroll = 0; }
        y += 27;
    }
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 150, TOOLBAR_Y(28), 150, 28), "Exportar Relatório…", true)) export_report();
    static u64 last;
    if (timer_ticks() - last > 2000 || nrows == 0) { collect(I.sec); last = timer_ticks(); }
    ui_label(u, a.x + 20, a.y + 14, sec_names[I.sec], FONT_BOLD, 20, T.text);
    rect_t view = rect_make(a.x, a.y + 50, a.w, a.h - 50);
    rect_t pc = surface_push_clip(s, view);
    int yy = view.y - I.scroll, kw = 210;
    for (int i = 0; i < nrows; i++) {
        if (rows[i].head) {
            yy += 6;
            text_draw(s, font_get(FONT_BOLD), 13.5f, a.x + 20, yy + 18, T.text, rows[i].k);
            yy += 26;
            continue;
        }
        if (i % 2) gfx_round_rect(s, a.x + 12, yy, a.w - 24, 24, 5, T.alt_row);
        int tw = text_width(font_get(FONT_REGULAR), 12.5f, rows[i].k);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, a.x + 20 + MAX(0, kw - tw - 10), yy + 17, kw - 10, T.text2, rows[i].k);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 12.5f, a.x + 20 + kw + 6, yy + 17, a.w - kw - 50, T.text, rows[i].v);
        yy += 24;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, yy + I.scroll - view.y + 20, &I.scroll);
    u->repaint = I.sec == S_OVERVIEW || I.sec == S_MEM || I.sec == S_NET;
}
static void si_close(window_t *w) { I.w = NULL; }
static void si_launch(app_t *a) {
    if (I.w) { wm_focus(I.w); return; }
    I.w = wm_create(&app_sysinfo, "Informações do Sistema", -1, -1, 860, 560, WIN_RESIZABLE);
    if (!I.w) return;
    I.w->paint = si_paint;
    I.w->on_close = si_close;
    I.w->toolbar_h = TOOLBAR_H;
    I.w->sidebar_w = 220;
    nrows = 0;
}
static void si_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(80, 84, 96), RGB(34, 36, 44));
    gfx_round_rect(s, x + 14 * u, y + 14 * u, 36 * u, 36 * u, 4 * u, RGB(60, 180, 120));
    gfx_round_rect(s, x + 20 * u, y + 20 * u, 24 * u, 24 * u, 2 * u, RGB(30, 30, 34));
    for (int i = 0; i < 4; i++) {
        float o = (17 + i * 8) * u;
        gfx_fill(s, (int)(x + o), (int)(y + 9 * u), MAX(1, (int)(2 * u)), (int)(5 * u), RGB(220, 200, 120));
        gfx_fill(s, (int)(x + o), (int)(y + 50 * u), MAX(1, (int)(2 * u)), (int)(5 * u), RGB(220, 200, 120));
        gfx_fill(s, (int)(x + 9 * u), (int)(y + o), (int)(5 * u), MAX(1, (int)(2 * u)), RGB(220, 200, 120));
        gfx_fill(s, (int)(x + 50 * u), (int)(y + o), (int)(5 * u), MAX(1, (int)(2 * u)), RGB(220, 200, 120));
    }
    text_draw_center(s, font_get(FONT_BOLD), 9 * u, rect_make((int)(x + 20 * u), (int)(y + 20 * u), (int)(24 * u), (int)(24 * u)), RGB(240, 240, 240), "i");
}
app_t app_sysinfo = {.name = "Informações do Sistema", .exec = "sysinfo", .draw_icon = si_icon, .launch = si_launch};

/* ================================================================== Utilitário de Disco */
typedef struct { char name[64]; char kind[48]; u64 size, used; vnode_t *vol; int disk; bool removable; } dsk_item_t;
static struct { window_t *w; int sel; bool checking; u64 check_start; char check_log[2048]; int check_items; } D;
static dsk_item_t items[32];
static int nitems;

static u64 tree_bytes(vnode_t *n, int depth) {
    if (!n || depth > 24) return 0;
    if (!n->dir) return n->size;
    u64 t = 0;
    for (vnode_t *c = n->child; c; c = c->next) t += tree_bytes(c, depth + 1);
    return t;
}
static void scan(void) {
    nitems = 0;
    /* volume do sistema (árvore em memória, persistida no disco se houver) */
    dsk_item_t *it = &items[nitems++];
    snprintf(it->name, 64, "mOS HD");
    snprintf(it->kind, 48, persist_available() ? "Volume do sistema (FAT32, persistente)" : "Volume do sistema (memória)");
    it->vol = vfs_root();
    it->used = vfs_total_bytes();
    it->disk = persist_disk_index();
    it->size = it->disk >= 0 && storage_disk(it->disk) ? storage_disk(it->disk)->size : pmm_total_pages() * 4096 / 2;
    for (int i = 0; i < storage_disk_count() && nitems < 32; i++) {
        it = &items[nitems++];
        memset(it, 0, sizeof *it);
        snprintf(it->name, 64, "%s", storage_disk_desc(i));
        const char *nm = storage_disk_name(i);
        snprintf(it->kind, 48, "%s", !strncmp(nm, "nvme", 4) ? "Disco NVMe" : !strncmp(nm, "usb", 3) ? "Disco USB" : "Disco SATA");
        it->removable = !strncmp(nm, "usb", 3);
        blkdev_t *b = storage_disk(i);
        it->size = b ? b->size : 0;
        it->disk = i;
    }
    vnode_t *vols = vfs_lookup("/Volumes");
    for (vnode_t *c = vols && vols->dir ? vols->child : NULL; c && nitems < 32; c = c->next) {
        if (!vfs_is_volume(c)) continue;
        it = &items[nitems++];
        memset(it, 0, sizeof *it);
        snprintf(it->name, 64, "%s", c->name);
        snprintf(it->kind, 48, "Imagem de disco (somente leitura)");
        it->vol = c;
        it->disk = -1;
        it->removable = true;
        it->used = it->size = tree_bytes(c, 0);
    }
}

static void first_aid_start(void) {
    D.checking = true;
    D.check_start = timer_ticks();
    D.check_log[0] = 0;
    D.check_items = 0;
}
static int fa_count(vnode_t *n, int depth, int *dirs, int *bad) {
    if (!n || depth > 24) return 0;
    int k = 1;
    if (n->dir) { (*dirs)++; for (vnode_t *c = n->child; c; c = c->next) { if (c->parent != n) (*bad)++; k += fa_count(c, depth + 1, dirs, bad); } }
    else if (n->size > n->cap && n->cap) (*bad)++;
    return k;
}

static void dk_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    static u64 last;
    if (timer_ticks() - last > 3000 || !nitems) { scan(); last = timer_ticks(); }
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Interno", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    bool ext_head = false;
    for (int i = 0; i < nitems; i++) {
        if (items[i].removable && !ext_head) { ext_head = true; y += 6; ui_label(u, 18, y + 2, "Externo", FONT_BOLD, 11.5f, T.text2); y += 22; }
        if (ui_sidebar_item(u, 101000 + i, rect_make(10, y, sb - 20, 28), items[i].name, D.sel == i, glyph_sb_disk)) D.sel = i;
        y += 30;
    }
    D.sel = CLAMP(D.sel, 0, nitems - 1);
    dsk_item_t *it = &items[D.sel];
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    /* barra de ferramentas */
    int bx = u->cw - 12;
    bx -= 96;
    bool can_eject = it->vol && it->removable;
    if (ui_toolbar_button(u, UI_ID, rect_make(bx, TOOLBAR_Y(28), 96, 28), "Ejetar", can_eject)) {
        char nm[64];
        snprintf(nm, sizeof nm, "%s", it->name);
        if (vfs_eject(it->vol)) { notify("Volume ejetado", nm); scan(); D.sel = 0; return; }
    }
    bx -= 8 + 150;
    if (ui_toolbar_button(u, UI_ID, rect_make(bx, TOOLBAR_Y(28), 150, 28), "Primeiros Socorros", !D.checking)) first_aid_start();
    /* cabeçalho */
    icon_disk(s, a.x + 22, a.y + 16, 56);
    ui_label(u, a.x + 92, a.y + 20, it->name, FONT_BOLD, 20, T.text);
    char sz[32], tl[96];
    fmt_bytes(sz, sizeof sz, it->size);
    snprintf(tl, sizeof tl, "%s · %s", it->kind, sz);
    ui_label(u, a.x + 92, a.y + 48, tl, FONT_REGULAR, 13, T.text2);
    /* barra de uso */
    rect_t bar = rect_make(a.x + 24, a.y + 96, a.w - 48, 22);
    float frac = it->size ? (float)it->used / it->size : 0;
    gfx_round_rect(s, bar.x, bar.y, bar.w, bar.h, 6, T.dark ? RGBA(255, 255, 255, 24) : RGBA(0, 0, 0, 14));
    if (it->vol && frac > 0) gfx_round_rect(s, bar.x, bar.y, MAX(12, (int)(bar.w * MIN(1.0f, frac))), bar.h, 6, T.accent);
    int ly = bar.y + 40;
    char ub[32], fb[32];
    fmt_bytes(ub, sizeof ub, it->used);
    fmt_bytes(fb, sizeof fb, it->size > it->used ? it->size - it->used : 0);
    if (it->vol) {
        gfx_circle(s, bar.x + 6, ly - 4, 5, T.accent);
        ui_label(u, bar.x + 16, ly - 13, "Usado", FONT_SEMIBOLD, 12, T.text);
        ui_label(u, bar.x + 16, ly + 3, ub, FONT_REGULAR, 12, T.text2);
        gfx_circle_stroke(s, bar.x + 156, ly - 4, 5, 1, T.text3);
        ui_label(u, bar.x + 166, ly - 13, "Livre", FONT_SEMIBOLD, 12, T.text);
        ui_label(u, bar.x + 166, ly + 3, fb, FONT_REGULAR, 12, T.text2);
    }
    /* tabela de propriedades */
    rect_t g = rect_make(a.x + 24, ly + 40, a.w - 48, 4 * 30);
    ui_group_box(u, g);
    rect_t c;
    c = ui_form_row(u, g, 0, 30, 4, "Tipo");
    ui_detail(u, UI_ID, c, it->kind, DETAIL_NONE);
    c = ui_form_row(u, g, 1, 30, 4, "Capacidade");
    ui_detail(u, UI_ID, c, sz, DETAIL_NONE);
    c = ui_form_row(u, g, 2, 30, 4, "Dispositivo");
    char dv[64];
    if (it->disk >= 0) snprintf(dv, sizeof dv, "%s", storage_disk_name(it->disk));
    else snprintf(dv, sizeof dv, "%s", it->vol == vfs_root() ? "memória (sem disco)" : "imagem montada");
    ui_detail(u, UI_ID, c, dv, DETAIL_NONE);
    c = ui_form_row(u, g, 3, 30, 4, "Gravável");
    bool wr = it->disk >= 0 ? storage_disk(it->disk) && storage_disk(it->disk)->write : it->vol == vfs_root();
    ui_detail(u, UI_ID, c, wr ? "Sim" : "Não", DETAIL_NONE);
    /* Primeiros Socorros: verificação da árvore do volume */
    int fy = g.y + g.h + 20;
    if (D.checking || D.check_log[0]) {
        rect_t lg = rect_make(a.x + 24, fy, a.w - 48, a.y + a.h - fy - 16);
        gfx_round_rect(s, lg.x, lg.y, lg.w, lg.h, 8, T.dark ? RGB(24, 24, 26) : RGB(246, 246, 248));
        if (D.checking) {
            u64 el = timer_ticks() - D.check_start;
            static const char *steps[] = {"Verificando a estrutura do volume…", "Verificando as pastas…", "Verificando os arquivos…", "Verificando o espaço livre…"};
            int st = (int)MIN(3, el / 700);
            ui_progress_bar(u, rect_make(lg.x + 16, lg.y + 16, lg.w - 32, 8), MIN(1.0f, el / 3000.0f), true);
            ui_label(u, lg.x + 16, lg.y + 32, steps[st], FONT_REGULAR, 12.5f, T.text);
            if (el >= 3000) {
                int dirs = 0, bad = 0;
                int n = fa_count(it->vol ? it->vol : vfs_root(), 0, &dirs, &bad);
                snprintf(D.check_log, sizeof D.check_log,
                         "Primeiros Socorros em \"%s\"\n\n%d itens verificados (%d pastas).\n%s\nEspaço em uso: %s.\n\n%s",
                         it->name, n, dirs, bad ? "Foram encontradas inconsistências." : "Nenhuma inconsistência encontrada na árvore de arquivos.",
                         ub, bad ? "Reinicie o computador e tente de novo." : "A operação foi concluída com êxito.");
                D.checking = false;
                if (!bad) sound_alert();
            }
            u->repaint = true;
        } else {
            text_wrap(s, font_get(FONT_REGULAR), 12.5f, rect_make(lg.x + 16, lg.y + 12, lg.w - 32, lg.h - 24), 19, T.text, D.check_log, true);
        }
    }
}
static void dk_close(window_t *w) { D.w = NULL; }
static void dk_launch(app_t *a) {
    if (D.w) { wm_focus(D.w); return; }
    D.w = wm_create(&app_disco, "Utilitário de Disco", -1, -1, 820, 560, WIN_RESIZABLE);
    if (!D.w) return;
    D.w->paint = dk_paint;
    D.w->on_close = dk_close;
    D.w->toolbar_h = TOOLBAR_H;
    D.w->sidebar_w = 230;
    D.check_log[0] = 0;
    nitems = 0;
}
static void dk_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(230, 230, 236), RGB(180, 182, 190));
    gfx_round_rect_grad(s, x + 12 * u, y + 22 * u, 40 * u, 24 * u, 4 * u, RGB(210, 212, 218), RGB(150, 152, 160));
    gfx_fill(s, (int)(x + 16 * u), (int)(y + 38 * u), (int)(32 * u), MAX(1, (int)u), RGB(120, 120, 128));
    gfx_circle(s, x + 45 * u, y + 30 * u, 2 * u, RGB(80, 200, 120));
    /* estetoscópio */
    gfx_ring_arc(s, x + 30 * u, y + 20 * u, 9 * u, 2 * u, 0, PI_F, RGB(60, 60, 66));
    gfx_line(s, x + 39 * u, y + 20 * u, x + 39 * u, y + 12 * u, 2 * u, RGB(60, 60, 66));
    gfx_circle(s, x + 39 * u, y + 11 * u, 3 * u, RGB(150, 150, 160));
}
app_t app_disco = {.name = "Utilitário de Disco", .exec = "disco", .draw_icon = dk_icon, .launch = dk_launch};
