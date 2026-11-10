/*
 * Registro das seções __swift5_* que o dyld já mapeou.
 *
 * A entrega ao runtime não é esta tabela. O libswiftCore (ImageInspectionMachO.cpp
 * da tag swift-6.4.2) registra, no inicializador, objc_addLoadImageFunc e, se esse
 * símbolo não existir, _dyld_register_func_for_add_image. Os dois estão no libSystem
 * (objc.c e dyldimg.c). O callback chama getsectiondata e consome os descritores
 * de __swift5_types, __swift5_protos, __swift5_proto, __swift5_replace,
 * __swift5_replac2 e __swift5_acfuncs. typeref, reflstr, fieldmd, assocty e capture
 * o runtime lê depois, também por getsectiondata, quando reflete.
 *
 * A tabela abaixo só espelha endereço e tamanho para o teste de host
 * (tools/swiftsect_test.c) e para quem consulta cx_swift_lookup.
 */
#include "foundation.h"
#include "libc.h"

extern u8 *getsectiondata(const void *header, const char *seg, const char *sect, u64 *size);

static const char *const swift_sect_names[] = {
    "__swift5_types", "__swift5_protos", "__swift5_proto", "__swift5_fieldmd",
    "__swift5_reflstr", "__swift5_assocty", "__swift5_capture", "__swift5_typeref",
    "__swift5_builtin", "__swift5_mpenum", "__swift5_replace", "__swift5_replac2",
    "__swift5_acfuncs", "__swift5_entry",
};

typedef struct {
    const void *mh;
    i64 slide;
    char name[20];
    u8 *addr;
    u64 size;
} swift_rec_t;

static swift_rec_t swift_tab[512];
static int swift_n;

static int same_name(const char *a, const char *b) {
    int i;
    for (i = 0; i < 16; i++) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

void cx_swift_reset(void) { swift_n = 0; }

void cx_swift_note(const void *mh, i64 slide) {
    int i, k;
    if (!mh) return;
    for (i = 0; i < swift_n; i++) if (swift_tab[i].mh == mh) return;
    for (k = 0; k < (int)(sizeof swift_sect_names / sizeof swift_sect_names[0]); k++) {
        u64 sz = 0;
        u8 *p = getsectiondata(mh, "__TEXT", swift_sect_names[k], &sz);
        if (!p || !sz || swift_n >= (int)(sizeof swift_tab / sizeof swift_tab[0])) continue;
        swift_tab[swift_n].mh = mh;
        swift_tab[swift_n].slide = slide;
        for (i = 0; i < 19; i++) {
            swift_tab[swift_n].name[i] = swift_sect_names[k][i];
            if (!swift_sect_names[k][i]) break;
        }
        swift_tab[swift_n].name[19] = 0;
        swift_tab[swift_n].addr = p;
        swift_tab[swift_n].size = sz;
        swift_n++;
    }
}

void cx_swift_forget(const void *mh) {
    int i, w = 0;
    if (!mh) return;
    for (i = 0; i < swift_n; i++) if (swift_tab[i].mh != mh) swift_tab[w++] = swift_tab[i];
    swift_n = w;
}

int cx_swift_count(void) { return swift_n; }

int cx_swift_lookup(const void *mh, const char *sect, void **addr, u64 *size) {
    int i;
    for (i = 0; i < swift_n; i++) {
        if (mh && swift_tab[i].mh != mh) continue;
        if (!same_name(swift_tab[i].name, sect)) continue;
        if (addr) *addr = swift_tab[i].addr;
        if (size) *size = swift_tab[i].size;
        return 1;
    }
    return 0;
}
