/* mOS — caminho do carregador EFI a partir dos nós de arquivo do LoadedImage->FilePath (ver include/origin.h).
 * Separado de origin.c para o teste de host (tools/loaderpath_host.c) compilar só esta função. */
#ifdef LOADERPATH_HOST
#include "loaderpath_host.h"
#else
#include "origin.h"
#include "fs.h"
#endif

bool origin_parse_loader_path(const u8 *fp, usize len, char *out, usize cap) {
    usize n = 0, o = 0;
    bool ended = false;
    if (cap < 2) return false;
    out[0] = 0;
    for (int nodes = 0; fp && nodes < 64 && n + 4 <= len; nodes++) {
        u8 ty = fp[n], sub = fp[n + 1];
        u16 nl = le16(fp + n + 2);
        if (nl < 4 || n + nl > len) goto bad;
        if (ty == 0x7F && sub == 0xFF) { ended = true; break; }
        if (ty != 0x04 || sub != 0x04) goto bad;                       /* só nós de arquivo: GRUB e firmware mandam isso */
        if (o && out[o - 1] != '/' && nl >= 6 && le16(fp + n + 4) != '\\') {   /* nó seguinte sem a barra inicial */
            if (o + 1 >= cap) goto bad;
            out[o++] = '/';
        }
        for (usize i = n + 4; i + 1 < n + nl; i += 2) {
            u16 c = le16(fp + i);
            if (!c) break;
            if (c >= 0x80 || c < 0x20) goto bad;
            char ch = c == '\\' ? '/' : (char)c;
            if (ch == '/' && o && out[o - 1] == '/') continue;              /* "\EFI\" + "\mOS" de nós vizinhos */
            if (!o && ch != '/') { if (o + 1 >= cap) goto bad; out[o++] = '/'; }
            if (o + 1 >= cap) goto bad;
            out[o++] = ch;
        }
        n += nl;
    }
    out[o] = 0;
    if (!ended || o < 10) { out[0] = 0; return false; }
    /* /EFI/<pasta>/<nome>.EFI: exatamente três componentes */
    int slashes = 0;
    for (usize i = 0; i < o; i++) slashes += out[i] == '/';
    bool ok = slashes == 3 && !strncasecmp(out, "/EFI/", 5) && out[5] != '/' && !strstr(out, "..") &&
              o > 4 && !strcasecmp(out + o - 4, ".EFI") && out[o - 5] != '/';
    if (!ok) out[0] = 0;
    return ok;
bad:
    out[0] = 0;
    return false;
}
