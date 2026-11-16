/*
 * mOS libSystem — consultas de versão do dyld (mach-o/dyld_priv.h do macOS 10.15+, públicas desde o macOS 13):
 * dyld_program_sdk_at_least, dyld_program_minos_at_least, dyld_sdk_at_least, dyld_minos_at_least,
 * dyld_get_active_platform, dyld_get_base_platform, dyld_is_simulator_platform e os "version tokens".
 * O SwiftUI.framework (OpenSwiftUI) as usa para escolher o comportamento por SDK do app (isLinkedOnOrAfter).
 *
 * Lê LC_BUILD_VERSION (ou LC_VERSION_MIN_MACOSX) do cabeçalho Mach-O. Versão: xxxx.yy.zz em 0xXXXXYYZZ.
 * Platform 0xffffffff é um "version set" (dyld_fall_2020_os_versions...): ano e mês/dia em 0x0YYYMMDD; a tabela
 * abaixo traz o macOS de cada set (o iOS para o Mac Catalyst), como no dyld da Apple.
 */
#include "sys.h"

#define LC_VERSION_MIN_MACOSX 0x24
#define LC_VERSION_MIN_IPHONEOS 0x25
#define LC_BUILD_VERSION 0x32

#define PLATFORM_MACOS 1
#define PLATFORM_IOS 2
#define PLATFORM_TVOS 3
#define PLATFORM_WATCHOS 4
#define PLATFORM_MACCATALYST 6
#define PLATFORM_IOSSIMULATOR 7
#define PLATFORM_TVOSSIMULATOR 8
#define PLATFORM_WATCHOSSIMULATOR 9
#define PLATFORM_VISIONOS 11
#define PLATFORM_VISIONOSSIMULATOR 12
#define VERSION_SET 0xffffffffu

typedef u32 dyld_platform_t;
typedef struct { dyld_platform_t platform; u32 version; } dyld_build_version_t;

extern const void *_dyld_get_image_header(u32 i);

/* version set -> macOS e iOS do mesmo lançamento */
static const struct { u32 set, macos, ios; } sets[] = {
    { 0x07e30901, 0x000a0f00, 0x000d0000 },   /* fall 2019:   10.15 / 13.0 */
    { 0x07e40301, 0x000a0f04, 0x000d0400 },   /* spring 2020: 10.15.4 / 13.4 */
    { 0x07e40901, 0x000b0000, 0x000e0000 },   /* fall 2020:   11.0 / 14.0 */
    { 0x07e50301, 0x000b0300, 0x000e0500 },   /* spring 2021: 11.3 / 14.5 */
    { 0x07e50901, 0x000c0000, 0x000f0000 },   /* fall 2021:   12.0 / 15.0 */
    { 0x07e60301, 0x000c0300, 0x000f0400 },   /* spring 2022: 12.3 / 15.4 */
    { 0x07e60901, 0x000d0000, 0x00100000 },   /* fall 2022:   13.0 / 16.0 */
    { 0x07e70301, 0x000d0300, 0x00100400 },   /* spring 2023: 13.3 / 16.4 */
    { 0x07e70901, 0x000e0000, 0x00110000 },   /* fall 2023:   14.0 / 17.0 */
    { 0x07e80301, 0x000e0400, 0x00110400 },   /* spring 2024: 14.4 / 17.4 */
    { 0x07e80901, 0x000f0000, 0x00120000 },   /* fall 2024:   15.0 / 18.0 */
    { 0x07e90301, 0x000f0400, 0x00120400 },   /* spring 2025: 15.4 / 18.4 */
    { 0x07e90901, 0x001a0000, 0x001a0000 },   /* fall 2025:   26.0 / 26.0 */
    { 0x07ea0301, 0x001a0400, 0x001a0400 },   /* spring 2026: 26.4 / 26.4 */
    { 0x07ea0901, 0x001b0000, 0x001b0000 },   /* fall 2026:   27.0 / 27.0 */
};

struct ver { u32 platform, minos, sdk; };

/* plataforma, minos e sdk de uma imagem; 0 se o cabeçalho não tem os comandos */
static int read_version(const void *mh, struct ver *v) {
    v->platform = v->minos = v->sdk = 0;
    if (!mh) return 0;
    const u32 *h = (const u32 *)mh;
    if (h[0] != 0xfeedfacfu) return 0;                 /* MH_MAGIC_64 */
    u32 ncmds = h[4];
    const unsigned char *p = (const unsigned char *)mh + 32;
    for (u32 i = 0; i < ncmds; i++) {
        const u32 *lc = (const u32 *)p;
        if (lc[1] < 8) break;
        if (lc[0] == LC_BUILD_VERSION) {
            v->platform = lc[2]; v->minos = lc[3]; v->sdk = lc[4];
            return 1;
        }
        if (lc[0] == LC_VERSION_MIN_MACOSX || lc[0] == LC_VERSION_MIN_IPHONEOS) {
            v->platform = lc[0] == LC_VERSION_MIN_MACOSX ? PLATFORM_MACOS : PLATFORM_IOS;
            v->minos = lc[2]; v->sdk = lc[3];
            return 1;
        }
        p += lc[1];
    }
    return 0;
}

dyld_platform_t dyld_get_base_platform(dyld_platform_t p) {
    switch (p) {
    case PLATFORM_IOSSIMULATOR: return PLATFORM_IOS;
    case PLATFORM_TVOSSIMULATOR: return PLATFORM_TVOS;
    case PLATFORM_WATCHOSSIMULATOR: return PLATFORM_WATCHOS;
    case PLATFORM_VISIONOSSIMULATOR: return PLATFORM_VISIONOS;
    default: return p;
    }
}

int dyld_is_simulator_platform(dyld_platform_t p) {
    return p == PLATFORM_IOSSIMULATOR || p == PLATFORM_TVOSSIMULATOR || p == PLATFORM_WATCHOSSIMULATOR ||
           p == PLATFORM_VISIONOSSIMULATOR;
}

/* a plataforma do processo: a do executável (Mac Catalyst para apps do Catalyst), macOS se ele não disser */
dyld_platform_t dyld_get_active_platform(void) {
    struct ver v;
    if (read_version(_dyld_get_image_header(0), &v) && v.platform) return v.platform;
    return PLATFORM_MACOS;
}

/* a versão pedida, traduzida para a plataforma da imagem; 0 = não se aplica */
static int wanted(u32 img_platform, dyld_build_version_t want, u32 *out) {
    u32 base = dyld_get_base_platform(img_platform);
    if (want.platform == VERSION_SET) {
        int k = -1;
        for (u32 i = 0; i < sizeof sets / sizeof sets[0]; i++)
            if (sets[i].set <= want.version) k = (int)i;
        if (k < 0) { *out = 0; return 1; }              /* set mais antigo que a tabela: qualquer versão serve */
        if (base == PLATFORM_MACOS) { *out = sets[k].macos; return 1; }
        if (base == PLATFORM_IOS || base == PLATFORM_MACCATALYST) { *out = sets[k].ios; return 1; }
        return 0;
    }
    if (want.platform == base) { *out = want.version; return 1; }
    /* o Mac Catalyst segue as versões do iOS */
    if (base == PLATFORM_MACCATALYST && want.platform == PLATFORM_IOS) { *out = want.version; return 1; }
    return 0;
}

static int at_least(const void *mh, dyld_build_version_t want, int sdk) {
    struct ver v;
    if (!read_version(mh, &v)) return 0;
    u32 need;
    if (!wanted(v.platform ? v.platform : PLATFORM_MACOS, want, &need)) return 0;
    return (sdk ? v.sdk : v.minos) >= need;
}

int dyld_sdk_at_least(const void *mh, dyld_build_version_t v) { return at_least(mh, v, 1); }
int dyld_minos_at_least(const void *mh, dyld_build_version_t v) { return at_least(mh, v, 0); }
int dyld_program_sdk_at_least(dyld_build_version_t v) { return at_least(_dyld_get_image_header(0), v, 1); }
int dyld_program_minos_at_least(dyld_build_version_t v) { return at_least(_dyld_get_image_header(0), v, 0); }

/* version token (macOS 14): plataforma nos 32 bits altos, versão nos baixos */
u64 dyld_get_program_sdk_version_token(void) {
    struct ver v;
    read_version(_dyld_get_image_header(0), &v);
    return ((u64)(v.platform ? v.platform : PLATFORM_MACOS) << 32) | v.sdk;
}
u64 dyld_get_program_minos_version_token(void) {
    struct ver v;
    read_version(_dyld_get_image_header(0), &v);
    return ((u64)(v.platform ? v.platform : PLATFORM_MACOS) << 32) | v.minos;
}
dyld_platform_t dyld_version_token_get_platform(u64 token) { return (dyld_platform_t)(token >> 32); }
int dyld_version_token_at_least(u64 token, dyld_build_version_t v) {
    u32 need;
    if (!wanted((u32)(token >> 32), v, &need)) return 0;
    return (u32)token >= need;
}

/* dyld_get_program_sdk_version / dyld_get_program_min_os_version (APIs antigas, mesmo cabeçalho) */
u32 dyld_get_program_sdk_version(void) {
    struct ver v;
    read_version(_dyld_get_image_header(0), &v);
    return v.sdk;
}
u32 dyld_get_program_min_os_version(void) {
    struct ver v;
    read_version(_dyld_get_image_header(0), &v);
    return v.minos;
}
