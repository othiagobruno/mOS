/*
 * mOS libSystem — a fonte monoespaçada do sistema.
 *
 * A Inter do kernel é proporcional, e o Menlo/Monaco/Courier do mOS era a própria Inter marcada como "mono" (só os
 * traços diziam monoespaçada; glifos e avanços continuavam proporcionais). O terminal do VS Code (xterm.js) mede a
 * célula pelo "W" e as letras finas ficavam soltas na grade ("b a s h - 5 . 3 $").
 *
 * A JetBrains Mono (SIL OFL 1.1, /Sistema/Biblioteca/Fontes/JetBrainsMono-Regular.ttf + JetBrainsMono-OFL.txt) faz o
 * papel do Menlo: é registrada no kernel na primeira vez que um NSFont monoespaçado é criado (appkit.c ft_make), por
 * CTFontManagerCreateFontDescriptorFromData — o mesmo caminho das fontes web, que também lê os nomes da tabela 'name'; o
 * mesmo arquivo dá o mesmo id em todo processo. O desenho pelo CoreText (Skia/Chromium) usa a face registrada; o
 * desenho de texto do AppKit nativo cai na Inter regular para ids de fontes registradas (kernel/compat/guisys.c). Sem
 * o arquivo, -1: o NSFont segue na Inter, como antes.
 */
#include "foundation.h"
#include "sys.h"

extern u8 *cx_read_file(const char *path, u64 *len);
id CTFontManagerCreateFontDescriptorFromData(id data);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
id objc_retain(id o);
void objc_release(id o);
void free(void *p);

#define MONO_FILE "/Sistema/Biblioteca/Fontes/JetBrainsMono-Regular.ttf"

static volatile int mono_id;          /* 0 = ainda não tentou; -1 = sem fonte; senão o id da face no kernel */
static volatile int mono_lock;

int cx_mono_face(void) {
    int r = mono_id;
    if (r) return r;
    while (__sync_lock_test_and_set(&mono_lock, 1)) SYS0(SYS_YIELD);
    if (!mono_id) {
        int fid = -1;
        u64 n = 0;
        u8 *b = cx_read_file(MONO_FILE, &n);
        if (b && n >= 12 && n <= (8u << 20)) {
            id data = CFDataCreate(0, b, (long)n);
            id d = data ? CTFontManagerCreateFontDescriptorFromData(data) : 0;
            id v = d ? MSG(id, d, "objectForKey:", cx_str("mOSFontId")) : 0;
            if (v) fid = (int)MSG0(i64, v, "longLongValue");
            if (d) objc_release(d);
            if (data) objc_release(data);
        }
        free(b);
        mono_id = fid > 0 ? fid : -1;
    }
    r = mono_id;
    __sync_lock_release(&mono_lock);
    return r;
}
