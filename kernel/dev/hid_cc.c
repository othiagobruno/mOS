/* mOS — parser de Consumer Control / System Control (ver hid_cc.h) */
#include "hid_cc.h"
#ifdef MOS_HOST_TEST
#include <string.h>
#else
#include "kernel.h"
#endif

int hidcc_parse(hidcc_t *c, const u8 *rd, int len) {
    memset(c, 0, sizeof *c);
    u32 upage = 0, rsize = 0, rcount = 0;
    i32 lmin = 0;
    u8 rid = 0;
    u16 off[256];
    memset(off, 0, sizeof off);
    u32 usages[16]; int nus = 0;
    u32 umin = 0; bool range = false;
    int depth = 0;
    u32 app = 0;                               /* uso da coleção de aplicação atual */
    for (int i = 0; i < len;) {
        u8 b = rd[i++];
        if (b == 0xFE) { if (i + 1 >= len) break; i += 2 + rd[i]; continue; }
        int sz = b & 3; if (sz == 3) sz = 4;
        if (i + sz > len) break;
        u32 uv = 0;
        for (int k = 0; k < sz; k++) uv |= (u32)rd[i + k] << (8 * k);
        i32 sv = sz == 1 ? (i8)uv : sz == 2 ? (i16)uv : (i32)uv;
        i += sz;
        int type = (b >> 2) & 3, tag = b >> 4;
        if (type == 0) {
            if (tag == 10) {                   /* Collection */
                if (depth == 0 && (uv & 0xFF) == 1) app = nus ? usages[0] : (upage << 16);
                depth++;
            } else if (tag == 12) { if (depth) depth--; }
            else if (tag == 8) {               /* Input */
                bool cnst = uv & 1, var = uv & 2;
                bool cc = app == 0x000C0001 || app == 0x00010080;
                if (!cnst && cc && c->n < HIDCC_FIELDS && (upage == 0x0C || upage == 0x01)) {
                    hidcc_field_t *f = &c->f[c->n++];
                    f->rid = rid; f->page = (u16)upage; f->off = off[rid]; f->size = (u16)rsize; f->count = (u16)rcount;
                    f->array = !var; f->lmin = lmin;
                    f->umin = (u16)(range ? umin : nus ? usages[0] : 0);
                    if (var && !range) for (int k = 0; k < nus && k < 16; k++) f->usages[f->nusages++] = (u16)usages[k];
                }
                off[rid] = (u16)(off[rid] + rsize * rcount);
            } else if (tag == 9 || tag == 11) { /* Output / Feature: não contam no relatório de entrada */ }
            nus = 0; range = false;
        } else if (type == 1) {
            switch (tag) {
            case 0: upage = uv; break;
            case 1: lmin = sv; break;
            case 7: rsize = uv; break;
            case 8: rid = (u8)uv; c->ids = true; break;
            case 9: rcount = uv; break;
            }
        } else if (type == 2) {
            u32 u = sz == 4 ? uv & 0xFFFF : uv;
            if (tag == 0 && nus < 16) usages[nus++] = depth == 0 ? ((sz == 4 ? uv : (upage << 16) | u)) : u;
            else if (tag == 1) { umin = u; range = true; }
        }
    }
    return c->n;
}

static u32 bits(const u8 *r, u32 len, u32 off, u32 size) {
    u32 v = 0;
    for (u32 i = 0; i < size && i < 32; i++) {
        u32 b = off + i;
        if (b / 8 >= len) break;
        if ((r[b / 8] >> (b % 8)) & 1) v |= 1u << i;
    }
    return v;
}

int hidcc_active(const hidcc_t *c, const u8 *rep, u32 len, u32 *out, int max) {
    int n = 0;
    u8 rid = 0;
    if (c->ids) { if (!len) return 0; rid = rep[0]; rep++; len--; }
    for (int k = 0; k < c->n; k++) {
        const hidcc_field_t *f = &c->f[k];
        if (f->rid != rid) continue;
        for (u32 i = 0; i < f->count && n < max; i++) {
            u32 v = bits(rep, len, f->off + i * f->size, f->size);
            if (f->array) {
                if ((i32)v < f->lmin || !v) continue;
                out[n++] = (u32)f->page << 16 | (u16)(f->umin + (v - (u32)f->lmin));
            } else if (v) {
                u16 u = i < f->nusages ? f->usages[i] : (u16)(f->umin + i);
                out[n++] = (u32)f->page << 16 | u;
            }
        }
    }
    return n;
}
