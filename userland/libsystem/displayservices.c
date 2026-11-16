/*
 * mOS libSystem — dados do monitor e brilho do monitor externo, das partes privadas CoreDisplay e
 * DisplayServices que apps como o IINA e o MonitorControl usam, e os números do EDID do CGDisplay.
 * Tudo vem do kernel pela ABI de GPU revisão 3 (MOS_GPU_DISPLAY_INFO / _BRIGHTNESS): o EDID é lido ao vivo
 * pela linha DDC e o brilho vai por DDC/CI (VCP 0x10) ao monitor. O display 1 do CoreGraphics é o primeiro
 * monitor que responde no DDC. Sem GPU AMD (ou sem monitor com EDID), as consultas devolvem erro e o
 * dicionário sai NULL, como no macOS sem o monitor.
 */
#include "gpucall.h"
#include "libc.h"

typedef void *id;
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id d, const void *k, const void *v);
id CFNumberCreate(void *alloc, long type, const void *p);
id CFDataCreate(void *alloc, const unsigned char *bytes, long n);
id CFStringCreateWithCString(void *alloc, const char *s, unsigned enc);
void CFRelease(id o);
extern const char kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks;

#define CF_UTF8      0x08000100u
#define CF_SINT32    3
#define CG_ERR_FAIL  1000           /* kCGErrorFailure */
#define CG_ERR_ARG   1001           /* kCGErrorIllegalArgument */

static int display(unsigned cg_id, mos_gpu_display_t *d) {
    if (!cg_id) return -MOS_EINVAL;
    memset(d, 0, sizeof *d);
    d->index = cg_id - 1;
    int e = mos_gpu_op(MOS_GPU_DISPLAY_INFO, d, sizeof *d);
    if (!e && d->edid_len < 128) e = -MOS_ENOENT;
    return e;
}

/* campos do bloco 0 do EDID */
static unsigned edid_vendor(const unsigned char *e) { return (unsigned)e[8] << 8 | e[9]; }          /* PNP, como o IOKit */
static unsigned edid_product(const unsigned char *e) { return (unsigned)e[10] | (unsigned)e[11] << 8; }
static unsigned edid_serial(const unsigned char *e) { return (unsigned)e[12] | (unsigned)e[13] << 8 | (unsigned)e[14] << 16 | (unsigned)e[15] << 24; }
/* nome do monitor: descritor 0xFC (até 13 caracteres, termina em 0x0A) */
static void edid_name(const unsigned char *e, char *out, unsigned n) {
    out[0] = 0;
    for (int i = 0; i < 4; i++) {
        const unsigned char *x = e + 54 + 18 * i;
        if (x[0] || x[1] || x[3] != 0xFC) continue;
        unsigned k = 0;
        for (int j = 5; j < 18 && k + 1 < n; j++) {
            if (x[j] == 0x0A) break;
            out[k++] = (char)x[j];
        }
        while (k && out[k - 1] == ' ') k--;
        out[k] = 0;
        return;
    }
}

unsigned mos_display_vendor(unsigned cg_id) { mos_gpu_display_t d; return display(cg_id, &d) ? 0 : edid_vendor(d.edid); }
unsigned mos_display_model(unsigned cg_id) { mos_gpu_display_t d; return display(cg_id, &d) ? 0 : edid_product(d.edid); }
unsigned mos_display_serial(unsigned cg_id) { mos_gpu_display_t d; return display(cg_id, &d) ? 0 : edid_serial(d.edid); }

static void put_num(id dict, const char *key, int v) {
    id k = CFStringCreateWithCString(0, key, CF_UTF8), n = CFNumberCreate(0, CF_SINT32, &v);
    CFDictionarySetValue(dict, k, n);
    CFRelease(k); CFRelease(n);
}

/* As chaves de IODisplayCreateInfoDictionary que os apps leem. */
id CoreDisplay_DisplayCreateInfoDictionary(unsigned cg_id) {
    static mos_gpu_display_t d;
    if (display(cg_id, &d)) return 0;
    id dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict) return 0;
    put_num(dict, "DisplayVendorID", (int)edid_vendor(d.edid));
    put_num(dict, "DisplayProductID", (int)edid_product(d.edid));
    put_num(dict, "DisplaySerialNumber", (int)edid_serial(d.edid));
    put_num(dict, "DisplayWeekManufacture", d.edid[16]);
    put_num(dict, "DisplayYearManufacture", 1990 + d.edid[17]);
    id ek = CFStringCreateWithCString(0, "IODisplayEDID", CF_UTF8), ev = CFDataCreate(0, d.edid, d.edid_len);
    CFDictionarySetValue(dict, ek, ev);
    CFRelease(ek); CFRelease(ev);
    id ok = CFStringCreateWithCString(0, "IODisplayEDIDOriginal", CF_UTF8), ov = CFDataCreate(0, d.edid, d.edid_len);
    CFDictionarySetValue(dict, ok, ov);
    CFRelease(ok); CFRelease(ov);
    char name[16];
    edid_name(d.edid, name, sizeof name);
    if (name[0]) {
        id names = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        id lk = CFStringCreateWithCString(0, "en_US", CF_UTF8), lv = CFStringCreateWithCString(0, name, CF_UTF8);
        CFDictionarySetValue(names, lk, lv);
        CFRelease(lk); CFRelease(lv);
        id nk = CFStringCreateWithCString(0, "DisplayProductName", CF_UTF8);
        CFDictionarySetValue(dict, nk, names);
        CFRelease(nk); CFRelease(names);
    }
    return dict;
}

/* brilho de 0 a 1, pelo DDC/CI do monitor */
static int brightness(unsigned cg_id, int set, float *v) {
    if (!cg_id || !v) return CG_ERR_ARG;
    mos_gpu_display_t d;
    memset(&d, 0, sizeof d);
    d.index = cg_id - 1;
    if (set) {
        /* primeiro o máximo do monitor (MCCS: em geral 100), depois o valor na escala dele */
        if (mos_gpu_op(MOS_GPU_DISPLAY_BRIGHTNESS, &d, sizeof d) || !d.brightness_max) return CG_ERR_FAIL;
        float f = *v < 0 ? 0 : *v > 1 ? 1 : *v;
        d.brightness = (unsigned)(f * (float)d.brightness_max + 0.5f);
        d.flags = MOS_GPU_DISPLAY_SET;
    }
    if (mos_gpu_op(MOS_GPU_DISPLAY_BRIGHTNESS, &d, sizeof d) || !d.brightness_max) return CG_ERR_FAIL;
    if (!set) *v = (float)d.brightness / (float)d.brightness_max;
    return 0;
}

int DisplayServicesGetBrightness(unsigned cg_id, float *v) { return brightness(cg_id, 0, v); }
int DisplayServicesSetBrightness(unsigned cg_id, float v) { return brightness(cg_id, 1, &v); }
_Bool DisplayServicesCanChangeBrightness(unsigned cg_id) { float v; return brightness(cg_id, 0, &v) == 0; }
