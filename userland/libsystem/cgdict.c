/*
 * mOS libSystem — CGPoint/CGSize/CGRect em CFDictionary, como no CoreGraphics da Apple.
 * Chaves CFString "X", "Y", "Width" e "Height"; valores CFNumber em kCFNumberCGFloatType (double).
 * Make devolve false sem escrever lixo na saída quando o dicionário é NULL, falta chave
 * ou o valor não é CFNumber. CFNumber inteiro ou de ponto flutuante entra via CFNumberGetValue.
 */
#include <stdbool.h>
#include <stdint.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef void *cgid;

extern cgid cx_str(const char *s);
extern cgid CFNumberCreate(void *alloc, long type, const void *p);
extern cgid CFDictionaryCreate(void *alloc, const cgid *keys, const cgid *vals, long n, const void *kcb, const void *vcb);
extern cgid CFDictionaryGetValue(cgid d, cgid key);
extern uint64_t CFGetTypeID(cgid o);
extern uint64_t CFNumberGetTypeID(void);
extern uint8_t CFNumberGetValue(cgid n, long type, void *out);
extern void CFRelease(cgid o);
extern const char kCFTypeDictionaryKeyCallBacks;
extern const char kCFTypeDictionaryValueCallBacks;

enum { kCFNumberCGFloatType = 16 };

static cgid key_once(const char *s, cgid *slot) {
    if (!*slot) *slot = cx_str(s);
    return *slot;
}
static cgid key_x(void) { static cgid k; return key_once("X", &k); }
static cgid key_y(void) { static cgid k; return key_once("Y", &k); }
static cgid key_w(void) { static cgid k; return key_once("Width", &k); }
static cgid key_h(void) { static cgid k; return key_once("Height", &k); }

static bool read_cgfloat(cgid n, CGFloat *out) {
    if (!n || CFGetTypeID(n) != CFNumberGetTypeID()) return false;
    return CFNumberGetValue(n, kCFNumberCGFloatType, out) != 0;
}

static cgid dict_xywh(cgid kx, CGFloat vx, cgid ky, CGFloat vy) {
    cgid nx = CFNumberCreate(0, kCFNumberCGFloatType, &vx);
    cgid ny = CFNumberCreate(0, kCFNumberCGFloatType, &vy);
    cgid d = 0;
    if (nx && ny) {
        cgid keys[2] = { kx, ky };
        cgid vals[2] = { nx, ny };
        d = CFDictionaryCreate(0, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    if (nx) CFRelease(nx);
    if (ny) CFRelease(ny);
    return d;
}

cgid CGPointCreateDictionaryRepresentation(CGPoint point) {
    return dict_xywh(key_x(), point.x, key_y(), point.y);
}
cgid CGSizeCreateDictionaryRepresentation(CGSize size) {
    return dict_xywh(key_w(), size.width, key_h(), size.height);
}
cgid CGRectCreateDictionaryRepresentation(CGRect rect) {
    CGFloat x = rect.origin.x, y = rect.origin.y, w = rect.size.width, h = rect.size.height;
    cgid nx = CFNumberCreate(0, kCFNumberCGFloatType, &x);
    cgid ny = CFNumberCreate(0, kCFNumberCGFloatType, &y);
    cgid nw = CFNumberCreate(0, kCFNumberCGFloatType, &w);
    cgid nh = CFNumberCreate(0, kCFNumberCGFloatType, &h);
    cgid d = 0;
    if (nx && ny && nw && nh) {
        cgid keys[4] = { key_x(), key_y(), key_w(), key_h() };
        cgid vals[4] = { nx, ny, nw, nh };
        d = CFDictionaryCreate(0, keys, vals, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    if (nx) CFRelease(nx);
    if (ny) CFRelease(ny);
    if (nw) CFRelease(nw);
    if (nh) CFRelease(nh);
    return d;
}

static bool make2(cgid dict, cgid ka, CGFloat *a, cgid kb, CGFloat *b) {
    if (!dict) return false;
    if (!read_cgfloat(CFDictionaryGetValue(dict, ka), a)) return false;
    if (!read_cgfloat(CFDictionaryGetValue(dict, kb), b)) return false;
    return true;
}

bool CGPointMakeWithDictionaryRepresentation(cgid dict, CGPoint *point) {
    CGPoint z = {0, 0};
    if (point) *point = z;
    if (!make2(dict, key_x(), &z.x, key_y(), &z.y)) return false;
    if (point) *point = z;
    return true;
}
bool CGSizeMakeWithDictionaryRepresentation(cgid dict, CGSize *size) {
    CGSize z = {0, 0};
    if (size) *size = z;
    if (!make2(dict, key_w(), &z.width, key_h(), &z.height)) return false;
    if (size) *size = z;
    return true;
}
bool CGRectMakeWithDictionaryRepresentation(cgid dict, CGRect *rect) {
    CGRect z = {{0, 0}, {0, 0}};
    if (rect) *rect = z;
    if (!dict) return false;
    if (!read_cgfloat(CFDictionaryGetValue(dict, key_x()), &z.origin.x)) return false;
    if (!read_cgfloat(CFDictionaryGetValue(dict, key_y()), &z.origin.y)) return false;
    if (!read_cgfloat(CFDictionaryGetValue(dict, key_w()), &z.size.width)) return false;
    if (!read_cgfloat(CFDictionaryGetValue(dict, key_h()), &z.size.height)) return false;
    if (rect) *rect = z;
    return true;
}
