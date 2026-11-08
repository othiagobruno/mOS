/*
 * mOS — tradutor arm64: AdvSIMD inteiro (frente SIMD). Three same/different, two-reg misc, across
 * lanes, pairwise, shift por imediato, por elemento, copy, modified immediate, permutações, EXT,
 * TBL/TBX, three-register extension (dot product, I8MM, RDM). Lanes FP vão para interp_simd_fp.c.
 */
#include "interp_simd.h"

/* ---------------------------------------------------------------------------------------------
 * utilidades
 * ------------------------------------------------------------------------------------------- */

/* shift por registrador (SSHL/USHL/SRSHL/URSHL/SQSHL/UQSHL/SQRSHL/UQRSHL), sh = SInt(Vm<7:0>) */
static a64_u64 shl_reg(a64_cpu *c, a64_u64 x, int sh, int eb, int uns, int rnd, int sat) {
    i128 v = sint(x, eb, uns), r;
    if (sh >= 0) {
        if (v == 0) return 0;
        if (sh >= eb) {
            if (!sat) return 0;
            r = v < 0 ? -((i128)1 << 100) : ((i128)1 << 100);
        } else {
            r = (i128)((u128)v << sh);
        }
    } else {
        int k = -sh;
        if (k > 100) k = 100;
        if (rnd) v += (i128)1 << (k - 1);
        r = v >> k;
    }
    if (sat) return uns ? satu(c, r, eb) : sats(c, r, eb);
    return (a64_u64)r & sm(eb);
}

static a64_u64 pmul8(a64_u64 x, a64_u64 y) {   /* produto polinomial de 8 bits (8 bits baixos) */
    a64_u64 r = 0;
    for (int b = 0; b < 8; b++) if ((y >> b) & 1) r ^= (x & 0xFF) << b;
    return r & 0xFF;
}
static a64_u64 pmul8l(a64_u64 x, a64_u64 y) {  /* produto polinomial 8x8 → 16 */
    a64_u64 r = 0;
    for (int b = 0; b < 8; b++) if ((y >> b) & 1) r ^= (x & 0xFF) << b;
    return r & 0xFFFF;
}

static int clz_n(a64_u64 x, int eb) {
    x &= sm(eb);
    if (!x) return eb;
    return __builtin_clzll(x) - (64 - eb);
}

/* ---------------------------------------------------------------------------------------------
 * three same (C4.1.96.x "Advanced SIMD three same" / "scalar three same")
 * ------------------------------------------------------------------------------------------- */

static a64_u64 int3(a64_cpu *c, int u, int op, int eb, a64_u64 x, a64_u64 y, a64_u64 acc) {
    a64_u64 mk = sm(eb);
    i128 sx = sint(x, eb, u), sy = sint(y, eb, u), t;
    switch (op) {
    case 0x00: return (a64_u64)((sx + sy) >> 1) & mk;
    case 0x01: return u ? satu(c, sx + sy, eb) : sats(c, sx + sy, eb);
    case 0x02: return (a64_u64)((sx + sy + 1) >> 1) & mk;
    case 0x04: return (a64_u64)((sx - sy) >> 1) & mk;
    case 0x05: return u ? satu(c, sx - sy, eb) : sats(c, sx - sy, eb);
    case 0x06: return sx > sy ? mk : 0;
    case 0x07: return sx >= sy ? mk : 0;
    case 0x08: return shl_reg(c, x, (int8_t)y, eb, u, 0, 0);
    case 0x09: return shl_reg(c, x, (int8_t)y, eb, u, 0, 1);
    case 0x0A: return shl_reg(c, x, (int8_t)y, eb, u, 1, 0);
    case 0x0B: return shl_reg(c, x, (int8_t)y, eb, u, 1, 1);
    case 0x0C: case 0x14: return (a64_u64)(sx > sy ? sx : sy) & mk;
    case 0x0D: case 0x15: return (a64_u64)(sx < sy ? sx : sy) & mk;
    case 0x0E: t = sx - sy; return (a64_u64)(t < 0 ? -t : t) & mk;
    case 0x0F: t = sx - sy; return (acc + (a64_u64)(t < 0 ? -t : t)) & mk;
    case 0x10: return (u ? x - y : x + y) & mk;
    case 0x11: return u ? (((x ^ y) & mk) == 0 ? mk : 0) : ((x & y & mk) ? mk : 0);
    case 0x12: return (u ? acc - x * y : acc + x * y) & mk;
    case 0x13: return u ? pmul8(x, y) : (x * y) & mk;
    case 0x16: {
        i128 p = 2 * (i128)ssx(x, eb) * (i128)ssx(y, eb);
        if (u) p += (i128)1 << (eb - 1);
        return sats(c, p >> eb, eb);
    }
    case 0x17: return (x + y) & mk;
    }
    return 0;
}

int a64s_three_same(a64_cpu *c, a64_u32 i, int sc) {
    int q = sc ? 1 : SBT(i, 30), u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 15, 11);
    int m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (op >= 0x18) {
        int a = size >> 1;
        if (!sc && !(size & 1) && ((!u && op == 0x1D) || (u && op == 0x19)))
            return a64s_fmlal_vec(c, i, a, u);            /* FMLAL/FMLSL (U=0) e FMLAL2/FMLSL2 (U=1) */
        return a64s_fp3(c, i, sc, u, a, op & 7, (size & 1) ? 8 : 4, 0);
    }
    vr a = c->v[n], b = c->v[m], dd = c->v[d], r;
    if (op == 0x03) {                                     /* lógicas */
        if (sc) return UDF;
        for (int k = 0; k < 2; k++) {
            a64_u64 x = a.d[k], y = b.d[k], z = dd.d[k], o;
            switch ((u << 2) | size) {
            case 0: o = x & y; break;
            case 1: o = x & ~y; break;
            case 2: o = x | y; break;
            case 3: o = x | ~y; break;
            case 4: o = x ^ y; break;
            case 5: o = y ^ ((y ^ x) & z); break;         /* BSL */
            case 6: o = z ^ ((z ^ x) & y); break;         /* BIT */
            default: o = z ^ ((z ^ x) & ~y); break;       /* BIF */
            }
            r.d[k] = o;
        }
        wq(c, d, &r, q);
        return NEXT;
    }
    /* validade */
    switch (op) {
    case 0x01: case 0x05: case 0x09: case 0x0B:           /* QADD QSUB QSHL QRSHL: qualquer tamanho */
        break;
    case 0x06: case 0x07: case 0x08: case 0x0A: case 0x10: case 0x11:
        if (sc && size != 3) return UDF;
        break;
    case 0x16:
        if (size == 0 || size == 3) return UDF;
        break;
    case 0x17:
        if (sc || u) return UDF;
        break;
    case 0x13:
        if (sc || (u && size != 0) || size == 3) return UDF;
        break;
    default:                                              /* HADD RHADD HSUB MAX MIN ABD ABA MLA MAXP MINP */
        if (sc || size == 3) return UDF;
        break;
    }
    if (!sc && size == 3 && !q) return UDF;
    int eb = 8 << size, ne = sc ? 1 : (q ? 128 : 64) / eb;
    r.d[0] = 0; r.d[1] = 0;
    if (op == 0x14 || op == 0x15 || op == 0x17) {         /* pareados: concat(Vm:Vn) */
        vr cat[2] = { a, b };
        if (!q) { cat[0].d[1] = b.d[0]; }
        const vr *src = cat;
        for (int e = 0; e < ne; e++) {
            int k0 = 2 * e, k1 = 2 * e + 1, per = 128 / eb;
            a64_u64 x = vel(&src[k0 / per], k0 % per, eb), y = vel(&src[k1 / per], k1 % per, eb);
            vset(&r, e, eb, int3(c, u, op, eb, x, y, 0));
        }
    } else {
        for (int e = 0; e < ne; e++)
            vset(&r, e, eb, int3(c, u, op, eb, vel(&a, e, eb), vel(&b, e, eb), vel(&dd, e, eb)));
    }
    if (sc) wsc(c, d, r.d[0], eb);
    else wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * three different
 * ------------------------------------------------------------------------------------------- */

int a64s_three_diff(a64_cpu *c, a64_u32 i, int sc) {
    int q = SBT(i, 30), u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 15, 12);
    int m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    int sqd = op == 9 || op == 11 || op == 13;
    if (sc) {
        if (!sqd) return UDF;
        q = 0;
    }
    if (op == 15) return UDF;
    if (op == 14) {
        if (u || size == 1 || size == 2) return UDF;
        if (size == 3) return a64s_pmull64(c, i);
    } else if (size == 3) return UDF;
    if (sqd && (u || size == 0)) return UDF;
    int eb = 8 << size, wb = 2 * eb, ne = sc ? 1 : 64 / eb, hi = q ? ne : 0;
    vr a = c->v[n], b = c->v[m], acc = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    if (op == 4 || op == 6) {                             /* (R)ADDHN/(R)SUBHN */
        for (int e = 0; e < ne; e++) {
            a64_u64 x = vel(&a, e, wb), y = vel(&b, e, wb);
            a64_u64 s = op == 4 ? x + y : x - y;
            if (u) s += 1ull << (eb - 1);
            vset(&r, e, eb, (s & sm(wb)) >> eb);
        }
        wpart(c, d, r.d[0], q);
        return NEXT;
    }
    for (int e = 0; e < ne; e++) {
        i128 x = (op == 1 || op == 3) ? sint(vel(&a, e, wb), wb, u) : sint(vel(&a, hi + e, eb), eb, u);
        i128 y = sint(vel(&b, hi + e, eb), eb, u);
        i128 ac = sint(vel(&acc, e, wb), wb, u), t, res;
        a64_u64 o;
        switch (op) {
        case 0: case 1: res = x + y; break;
        case 2: case 3: res = x - y; break;
        case 5: t = x - y; res = ac + (t < 0 ? -t : t); break;
        case 7: t = x - y; res = t < 0 ? -t : t; break;
        case 8: res = ac + x * y; break;
        case 10: res = ac - x * y; break;
        case 12: res = x * y; break;
        case 14: res = (i128)pmul8l((a64_u64)x, (a64_u64)y); break;
        case 9: case 11: {
            i128 p = (i128)ssx(sats(c, 2 * x * y, wb), wb);
            o = sats(c, op == 9 ? ac + p : ac - p, wb);
            vset(&r, e, wb, o);
            continue;
        }
        default: /* 13 */
            vset(&r, e, wb, sats(c, 2 * x * y, wb));
            continue;
        }
        vset(&r, e, wb, (a64_u64)res & sm(wb));
    }
    if (sc) wsc(c, d, r.d[0], wb);
    else wq(c, d, &r, 1);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * two-register misc
 * ------------------------------------------------------------------------------------------- */

int a64s_misc(a64_cpu *c, a64_u32 i, int sc) {
    int q = sc ? 0 : SBT(i, 30), u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 16, 12);
    int n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (op >= 0x16 || (op >= 0x0C && op <= 0x0F && (size & 2)))
        return a64s_fpmisc(c, i, sc, u, size >> 1, op, (size & 1) ? 8 : 4, 0);
    int eb = 8 << size, ne = sc ? 1 : (q ? 128 : 64) / eb;
    vr a = c->v[n], dv = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    a64_u64 mk = sm(eb);
    switch ((u << 5) | op) {
    case 0x00: case 0x20: case 0x01: {                    /* REV64 REV32 REV16 */
        int cb = op == 1 ? 16 : u ? 32 : 64;
        if (sc || eb >= cb) return UDF;
        int epc = cb / eb;
        for (int e = 0; e < ne; e++) vset(&r, e, eb, vel(&a, (e / epc) * epc + (epc - 1 - e % epc), eb));
        break;
    }
    case 0x02: case 0x22: case 0x06: case 0x26: {        /* SADDLP UADDLP SADALP UADALP */
        if (sc || size == 3) return UDF;
        int wb = 2 * eb, nw = (q ? 128 : 64) / wb;
        for (int e = 0; e < nw; e++) {
            i128 s = sint(vel(&a, 2 * e, eb), eb, u) + sint(vel(&a, 2 * e + 1, eb), eb, u);
            if (op == 6) s += (i128)vel(&dv, e, wb);
            vset(&r, e, wb, (a64_u64)s & sm(wb));
        }
        break;
    }
    case 0x03: case 0x23:                                 /* SUQADD USQADD */
        if (!sc && size == 3 && !q) return UDF;
        for (int e = 0; e < ne; e++) {
            a64_u64 x = vel(&a, e, eb), y = vel(&dv, e, eb);
            vset(&r, e, eb, u ? satu(c, sint(y, eb, 1) + sint(x, eb, 0), eb)
                              : sats(c, sint(y, eb, 0) + sint(x, eb, 1), eb));
        }
        break;
    case 0x04: case 0x24:                                 /* CLS CLZ */
        if (sc || size == 3) return UDF;
        for (int e = 0; e < ne; e++) {
            a64_u64 x = vel(&a, e, eb);
            int k = u ? clz_n(x, eb) : clz_n(((x >> 1) ^ x) & sm(eb - 1), eb - 1);
            vset(&r, e, eb, (a64_u64)k);
        }
        break;
    case 0x05:                                            /* CNT */
        if (sc || size != 0) return UDF;
        for (int e = 0; e < ne; e++) r.b[e] = (a64_u8)__builtin_popcount(a.b[e]);
        break;
    case 0x25:                                            /* NOT RBIT */
        if (sc || size > 1) return UDF;
        for (int e = 0; e < (q ? 16 : 8); e++) {
            a64_u8 x = a.b[e];
            if (size == 0) r.b[e] = (a64_u8)~x;
            else { a64_u8 y = 0; for (int k = 0; k < 8; k++) if (x & (1 << k)) y |= (a64_u8)(0x80 >> k); r.b[e] = y; }
        }
        break;
    case 0x07: case 0x27:                                 /* SQABS SQNEG */
        if (!sc && size == 3 && !q) return UDF;
        for (int e = 0; e < ne; e++) {
            i128 x = sint(vel(&a, e, eb), eb, 0);
            vset(&r, e, eb, sats(c, u ? -x : (x < 0 ? -x : x), eb));
        }
        break;
    case 0x08: case 0x28: case 0x09: case 0x29: case 0x0A: case 0x0B: case 0x2B:
        if (sc ? size != 3 : (size == 3 && !q)) return UDF;
        for (int e = 0; e < ne; e++) {
            a64_i64 x = ssx(vel(&a, e, eb), eb);
            a64_u64 o;
            switch ((u << 5) | op) {
            case 0x08: o = x > 0 ? mk : 0; break;
            case 0x28: o = x >= 0 ? mk : 0; break;
            case 0x09: o = x == 0 ? mk : 0; break;
            case 0x29: o = x <= 0 ? mk : 0; break;
            case 0x0A: o = x < 0 ? mk : 0; break;
            case 0x0B: o = (a64_u64)(x < 0 ? -(a64_u64)x : (a64_u64)x) & mk; break;
            default: o = (0 - (a64_u64)x) & mk; break;
            }
            vset(&r, e, eb, o);
        }
        break;
    case 0x12: case 0x14: case 0x34: case 0x32: {         /* XTN SQXTN UQXTN SQXTUN */
        if (size == 3 || (sc && op == 0x12 && !u)) return UDF;
        int wb = 2 * eb, nn = sc ? 1 : 64 / eb;
        for (int e = 0; e < nn; e++) {
            a64_u64 x = vel(&a, e, wb), o;
            switch ((u << 5) | op) {
            case 0x12: o = x & mk; break;
            case 0x14: o = sats(c, sint(x, wb, 0), eb); break;
            case 0x34: o = satu(c, sint(x, wb, 1), eb); break;
            default: o = satu(c, sint(x, wb, 0), eb); break;
            }
            vset(&r, e, eb, o);
        }
        if (sc) wsc(c, d, r.d[0], eb);
        else wpart(c, d, r.d[0], SBT(i, 30));
        return NEXT;
    }
    case 0x33: {                                          /* SHLL */
        if (sc || size == 3) return UDF;
        int wb = 2 * eb, nn = 64 / eb, hi = SBT(i, 30) ? nn : 0;
        for (int e = 0; e < nn; e++) vset(&r, e, wb, vel(&a, hi + e, eb) << eb);
        wq(c, d, &r, 1);
        return NEXT;
    }
    default:
        return UDF;
    }
    if (sc) wsc(c, d, r.d[0], eb);
    else wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * across lanes e scalar pairwise
 * ------------------------------------------------------------------------------------------- */

int a64s_across(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 16, 12);
    int n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (op == 0x0C || op == 0x0F) return a64s_fp_across(c, i);
    if (size == 3 || (size == 2 && !q)) return UDF;
    int eb = 8 << size, ne = (q ? 128 : 64) / eb;
    const vr *a = &c->v[n];
    i128 acc = sint(vel(a, 0, eb), eb, u);
    switch (op) {
    case 0x03:                                            /* SADDLV UADDLV */
        for (int e = 1; e < ne; e++) acc += sint(vel(a, e, eb), eb, u);
        wsc(c, d, (a64_u64)acc & sm(2 * eb), 2 * eb);
        return NEXT;
    case 0x0A: case 0x1A:                                 /* MAXV MINV */
        for (int e = 1; e < ne; e++) {
            i128 x = sint(vel(a, e, eb), eb, u);
            if (op == 0x0A ? x > acc : x < acc) acc = x;
        }
        break;
    case 0x1B:                                            /* ADDV */
        if (u) return UDF;
        for (int e = 1; e < ne; e++) acc += sint(vel(a, e, eb), eb, u);
        break;
    default:
        return UDF;
    }
    wsc(c, d, (a64_u64)acc & sm(eb), eb);
    return NEXT;
}

int a64s_pairwise(a64_cpu *c, a64_u32 i) {
    int u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 16, 12), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (!u && op == 0x1B) {                               /* ADDP (escalar) */
        if (size != 3) return UDF;
        wsc(c, d, c->v[n].d[0] + c->v[n].d[1], 64);
        return NEXT;
    }
    return a64s_fp_pairwise(c, i);
}

/* ---------------------------------------------------------------------------------------------
 * shift por imediato (vetor e escalar)
 * ------------------------------------------------------------------------------------------- */

static a64_u64 shr_imm(a64_u64 x, int sh, int eb, int uns, int rnd) {
    i128 v = sint(x, eb, uns);
    if (rnd) v += (i128)1 << (sh - 1);
    return (a64_u64)(v >> sh) & sm(eb);
}

int a64s_shift(a64_cpu *c, a64_u32 i, int sc) {
    int q = sc ? 1 : SBT(i, 30), u = SBT(i, 29), immh = SB(i, 22, 19), op = SB(i, 15, 11);
    int hb = (int)SB(i, 22, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    int eb = (immh & 8) ? 64 : (immh & 4) ? 32 : (immh & 2) ? 16 : 8;
    if (op == 0x1C || op == 0x1F) return a64s_fp_fixcvt(c, i, sc);
    int narrow = op >= 0x10 && op <= 0x14;
    if (narrow) {
        if (immh & 8) return UDF;
    } else if (!sc && (immh & 8) && !q) return UDF;
    int rsh = 2 * eb - hb, lsh = hb - eb;
    vr a = c->v[n], dv = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    a64_u64 mk = sm(eb);
    if (narrow) {
        int wb = 2 * eb, nn = sc ? 1 : 64 / eb, part = sc ? 0 : SBT(i, 30);
        if (op == 0x14) {                                 /* SSHLL USHLL */
            if (sc) return UDF;
            int hi = part ? nn : 0;
            for (int e = 0; e < nn; e++)
                vset(&r, e, wb, (a64_u64)shl128(sint(vel(&a, hi + e, eb), eb, u), lsh) & sm(wb));
            wq(c, d, &r, 1);
            return NEXT;
        }
        if (sc && !u && (op == 0x10 || op == 0x11)) return UDF;     /* SHRN/RSHRN não têm escalar */
        int rnd = op & 1;
        for (int e = 0; e < nn; e++) {
            a64_u64 x = vel(&a, e, wb), o;
            if (!u && op <= 0x11) {                       /* SHRN RSHRN */
                o = (a64_u64)((((u128)x) + (rnd ? ((u128)1 << (rsh - 1)) : 0)) >> rsh) & mk;
            } else {
                int suns = u && op >= 0x12;               /* UQ(R)SHRN: fonte sem sinal */
                i128 v = sint(x, wb, suns);
                if (rnd) v += (i128)1 << (rsh - 1);
                v >>= rsh;
                o = (op <= 0x11 || suns) ? satu(c, v, eb) : sats(c, v, eb);  /* SQ(R)SHRUN: sem sinal */
            }
            vset(&r, e, eb, o);
        }
        if (sc) wsc(c, d, r.d[0], eb);
        else wpart(c, d, r.d[0], part);
        return NEXT;
    }
    switch (op) {
    case 0x00: case 0x02: case 0x04: case 0x06: case 0x0A:
        if (sc && eb != 64) return UDF;
        break;
    case 0x08: case 0x0C:
        if (!u || (sc && op == 0x08 && eb != 64)) return UDF;
        break;
    case 0x0E:
        break;
    default:
        return UDF;
    }
    int ne = sc ? 1 : (q ? 128 : 64) / eb;
    for (int e = 0; e < ne; e++) {
        a64_u64 x = vel(&a, e, eb), z = vel(&dv, e, eb), o;
        switch (op) {
        case 0x00: o = shr_imm(x, rsh, eb, u, 0); break;
        case 0x02: o = (z + shr_imm(x, rsh, eb, u, 0)) & mk; break;
        case 0x04: o = shr_imm(x, rsh, eb, u, 1); break;
        case 0x06: o = (z + shr_imm(x, rsh, eb, u, 1)) & mk; break;
        case 0x08: {                                      /* SRI */
            a64_u64 msk = rsh >= 64 ? 0 : (mk >> rsh);
            a64_u64 sx = rsh >= 64 ? 0 : ((x & mk) >> rsh);
            o = (z & ~msk & mk) | sx;
            break;
        }
        case 0x0A:
            if (!u) o = (x << lsh) & mk;
            else { a64_u64 msk = (mk << lsh) & mk; o = (z & ~msk & mk) | ((x << lsh) & mk); }  /* SLI */
            break;
        case 0x0C: o = satu(c, shl128(sint(x, eb, 0), lsh), eb); break;              /* SQSHLU */
        default: o = u ? satu(c, shl128(sint(x, eb, 1), lsh), eb) : sats(c, shl128(sint(x, eb, 0), lsh), eb);
        }
        vset(&r, e, eb, o);
    }
    if (sc) wsc(c, d, r.d[0], eb);
    else wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * por elemento (vetor e escalar) — parte inteira; FP vai para a64s_fp_indexed
 * ------------------------------------------------------------------------------------------- */

static int dot_elem(a64_cpu *c, a64_u32 i, int nu, int mu) {     /* SDOT UDOT USDOT SUDOT (elemento) */
    int q = SBT(i, 30), L = SBT(i, 21), H = SBT(i, 11), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    int idx = (H << 1) | L;
    vr a = c->v[n], b = c->v[m], r = c->v[d];
    int ne = q ? 4 : 2;
    for (int e = 0; e < ne; e++) {
        a64_u32 s = r.s[e];
        for (int k = 0; k < 4; k++)
            s += (a64_u32)((a64_i64)sint(a.b[4 * e + k], 8, nu) * (a64_i64)sint(b.b[4 * idx + k], 8, mu));
        r.s[e] = s;
    }
    wq(c, d, &r, q);
    return NEXT;
}

int a64s_indexed(a64_cpu *c, a64_u32 i, int sc) {
    int q = sc ? 0 : SBT(i, 30), u = SBT(i, 29), size = SB(i, 23, 22), L = SBT(i, 21), M = SBT(i, 20);
    int rm = SB(i, 19, 16), op = SB(i, 15, 12), H = SBT(i, 11), n = SB(i, 9, 5), d = SB(i, 4, 0);
    /* ponto flutuante */
    if ((!u && (op == 1 || op == 5 || op == 9 || op == 0 || op == 4)) || (u && (op == 9 || op == 8 || op == 12)) ||
        (u && (op & 1) && op < 8))
        return a64s_fp_indexed(c, i, sc);
    if (op == 14) {                                       /* SDOT UDOT */
        if (sc || size != 2) return UDF;
        return dot_elem(c, i, u, u);
    }
    if (op == 15 && !u) {
        if (sc) return UDF;
        switch (size) {
        case 0: return dot_elem(c, i, 0, 1);              /* SUDOT */
        case 1: return a64s_bfdot(c, i, (H << 1) | L);
        case 2: return dot_elem(c, i, 1, 0);              /* USDOT */
        default: return a64s_bfmlal(c, i, (H << 2) | (L << 1) | M);
        }
    }
    if (size != 1 && size != 2) return UDF;
    int eb = 8 << size, idx, mreg;
    if (size == 1) { idx = (H << 2) | (L << 1) | M; mreg = rm; }
    else { idx = (H << 1) | L; mreg = (M << 4) | rm; }
    i128 ys = sint(vel(&c->v[mreg], idx, eb), eb, 0), yu = sint(vel(&c->v[mreg], idx, eb), eb, 1);
    vr a = c->v[n], acc = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    a64_u64 mk = sm(eb);
    int key = (u << 4) | op;
    switch (key) {
    case 0x08: case 0x10: case 0x14: {                    /* MUL MLA MLS */
        if (sc) return UDF;
        int ne = (q ? 128 : 64) / eb;
        for (int e = 0; e < ne; e++) {
            a64_u64 x = vel(&a, e, eb), z = vel(&acc, e, eb), p = x * (a64_u64)ys;
            vset(&r, e, eb, (key == 0x08 ? p : key == 0x10 ? z + p : z - p) & mk);
        }
        wq(c, d, &r, q);
        return NEXT;
    }
    case 0x02: case 0x12: case 0x06: case 0x16: case 0x0A: case 0x1A:   /* (S|U)MLAL (S|U)MLSL (S|U)MULL */
    case 0x03: case 0x07: case 0x0B: {                    /* SQDMLAL SQDMLSL SQDMULL */
        int sqd = key == 0x03 || key == 0x07 || key == 0x0B;
        if (sc && !sqd) return UDF;
        int wb = 2 * eb, nn = sc ? 1 : 64 / eb, hi = (!sc && SBT(i, 30)) ? nn : 0;
        i128 y = u ? yu : ys;
        for (int e = 0; e < nn; e++) {
            i128 x = sint(vel(&a, hi + e, eb), eb, u), ac = sint(vel(&acc, e, wb), wb, u);
            a64_u64 o;
            switch (op) {
            case 2: o = (a64_u64)(ac + x * y) & sm(wb); break;
            case 6: o = (a64_u64)(ac - x * y) & sm(wb); break;
            case 10: o = (a64_u64)(x * y) & sm(wb); break;
            case 3: case 7: {
                i128 p = (i128)ssx(sats(c, 2 * x * y, wb), wb);
                o = sats(c, op == 3 ? ac + p : ac - p, wb);
                break;
            }
            default: o = sats(c, 2 * x * y, wb); break;
            }
            vset(&r, e, wb, o);
        }
        if (sc) wsc(c, d, r.d[0], wb);
        else wq(c, d, &r, 1);
        return NEXT;
    }
    case 0x0C: case 0x0D: case 0x1D: case 0x1F: {         /* SQDMULH SQRDMULH SQRDMLAH SQRDMLSH */
        int ne = sc ? 1 : (q ? 128 : 64) / eb;
        for (int e = 0; e < ne; e++) {
            i128 x = sint(vel(&a, e, eb), eb, 0), p, rc = (i128)1 << (eb - 1);
            if (key == 0x0C) p = (2 * x * ys) >> eb;
            else if (key == 0x0D) p = (2 * x * ys + rc) >> eb;
            else {
                i128 z = sint(vel(&acc, e, eb), eb, 0) * ((i128)1 << eb);
                p = (key == 0x1D ? z + 2 * x * ys : z - 2 * x * ys) + rc;
                p >>= eb;
            }
            vset(&r, e, eb, sats(c, p, eb));
        }
        if (sc) wsc(c, d, r.d[0], eb);
        else wq(c, d, &r, q);
        return NEXT;
    }
    default:
        return UDF;
    }
}

/* ---------------------------------------------------------------------------------------------
 * copy (DUP/INS/SMOV/UMOV) e DUP escalar
 * ------------------------------------------------------------------------------------------- */

int a64s_copy(a64_cpu *c, a64_u32 i, int sc) {
    int q = SBT(i, 30), op = SBT(i, 29), imm5 = SB(i, 20, 16), imm4 = SB(i, 14, 11);
    int n = SB(i, 9, 5), d = SB(i, 4, 0);
    if ((imm5 & 0xF) == 0) return UDF;
    int size = __builtin_ctz(imm5), eb = 8 << size, idx = imm5 >> (size + 1);
    vr r;
    if (sc) {
        if (op || imm4 != 0) return UDF;
        wsc(c, d, vel(&c->v[n], idx, eb), eb);
        return NEXT;
    }
    if (op) {                                             /* INS (elemento) */
        if (!q) return UDF;
        int si = imm4 >> size;
        a64_u64 x = vel(&c->v[n], si, eb);
        vset(&c->v[d], idx, eb, x);
        return NEXT;
    }
    switch (imm4) {
    case 0x0: case 0x1: {                                 /* DUP (elemento / geral) */
        if (size == 3 && !q) return UDF;
        a64_u64 x = imm4 ? a64_xr(c, n) : vel(&c->v[n], idx, eb);
        for (int e = 0; e < 128 / eb; e++) vset(&r, e, eb, x);
        wq(c, d, &r, q);
        return NEXT;
    }
    case 0x3:                                             /* INS (geral) */
        if (!q) return UDF;
        vset(&c->v[d], idx, eb, a64_xr(c, n));
        return NEXT;
    case 0x5:                                             /* SMOV */
        if (size == 3 || (size == 2 && !q)) return UDF;
        if (q) a64_xw(c, d, (a64_u64)ssx(vel(&c->v[n], idx, eb), eb));
        else a64_ww(c, d, (a64_u32)ssx(vel(&c->v[n], idx, eb), eb));
        return NEXT;
    case 0x7:                                             /* UMOV */
        if (q ? size != 3 : size == 3) return UDF;
        a64_xw(c, d, vel(&c->v[n], idx, eb));
        return NEXT;
    default:
        return UDF;
    }
}

/* ---------------------------------------------------------------------------------------------
 * modified immediate (MOVI/MVNI/ORR/BIC/FMOV)
 * ------------------------------------------------------------------------------------------- */

int a64s_modimm(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), op = SBT(i, 29), cmode = SB(i, 15, 12), o2 = SBT(i, 11), d = SB(i, 4, 0);
    a64_u64 imm8 = (SB(i, 18, 16) << 5) | SB(i, 9, 5), imm = 0;
    int k = 0;                                            /* 0 MOVI, 1 MVNI, 2 ORR, 3 BIC */
    if (o2) {
        if (cmode != 0xF || op) return UDF;
        a64_u64 h = ((imm8 >> 7) << 15) | ((((imm8 >> 6) & 1) ^ 1) << 14) | (((imm8 >> 6) & 1) ? 0x3000 : 0) |
                    ((imm8 & 0x3F) << 6);
        imm = h * 0x0001000100010001ull;
    } else {
        switch (cmode >> 1) {
        case 0: case 1: case 2: case 3:
            imm = (imm8 << (8 * (cmode >> 1))) * 0x0000000100000001ull;
            k = (cmode & 1) ? (op ? 3 : 2) : op;
            break;
        case 4: case 5:
            imm = (imm8 << (8 * ((cmode >> 1) & 1))) * 0x0001000100010001ull;
            k = (cmode & 1) ? (op ? 3 : 2) : op;
            break;
        case 6:
            imm = ((cmode & 1) ? ((imm8 << 16) | 0xFFFF) : ((imm8 << 8) | 0xFF)) * 0x0000000100000001ull;
            k = op;
            break;
        default:
            if (!(cmode & 1)) {
                if (!op) imm = imm8 * 0x0101010101010101ull;
                else for (int b = 0; b < 8; b++) if ((imm8 >> b) & 1) imm |= 0xFFull << (8 * b);
            } else if (!op) {
                a64_u64 s = ((imm8 >> 7) << 31) | ((((imm8 >> 6) & 1) ^ 1) << 30) | (((imm8 >> 6) & 1) ? 0x3E000000ull : 0) |
                            ((imm8 & 0x3F) << 19);
                imm = s * 0x0000000100000001ull;
            } else {
                if (!q) return UDF;
                imm = ((imm8 >> 7) << 63) | ((((imm8 >> 6) & 1) ^ 1) << 62) | (((imm8 >> 6) & 1) ? 0x3FC0000000000000ull : 0) |
                      ((imm8 & 0x3F) << 48);
            }
            break;
        }
    }
    vr r = c->v[d];
    for (int e = 0; e < 2; e++) {
        switch (k) {
        case 0: r.d[e] = imm; break;
        case 1: r.d[e] = ~imm; break;
        case 2: r.d[e] |= imm; break;
        default: r.d[e] &= ~imm; break;
        }
    }
    wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * permutações, EXT, TBL/TBX
 * ------------------------------------------------------------------------------------------- */

int a64s_perm(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), size = SB(i, 23, 22), op = SB(i, 14, 12), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if ((op & 3) == 0 || (size == 3 && !q)) return UDF;
    int eb = 8 << size, ne = (q ? 128 : 64) / eb, part = op >> 2, half = ne / 2;
    vr a = c->v[n], b = c->v[m], r;
    r.d[0] = 0; r.d[1] = 0;
    for (int e = 0; e < ne; e++) {
        a64_u64 x;
        switch (op & 3) {
        case 1: {                                          /* UZP */
            int k = 2 * e + part;
            x = k < ne ? vel(&a, k, eb) : vel(&b, k - ne, eb);
            break;
        }
        case 2:                                            /* TRN */
            x = (e & 1) ? vel(&b, (e & ~1) + part, eb) : vel(&a, e + part, eb);
            break;
        default:                                           /* ZIP */
            x = (e & 1) ? vel(&b, part * half + e / 2, eb) : vel(&a, part * half + e / 2, eb);
            break;
        }
        vset(&r, e, eb, x);
    }
    wq(c, d, &r, q);
    return NEXT;
}

int a64s_ext(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), imm4 = SB(i, 14, 11), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (SB(i, 23, 22) != 0 || (!q && (imm4 & 8))) return UDF;
    int bytes = q ? 16 : 8;
    vr r;
    r.d[0] = 0; r.d[1] = 0;
    for (int e = 0; e < bytes; e++) {
        int k = e + imm4;
        r.b[e] = k < bytes ? c->v[n].b[k] : c->v[m].b[k - bytes];
    }
    wq(c, d, &r, q);
    return NEXT;
}

int a64s_tbl(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), len = SB(i, 14, 13), tbx = SBT(i, 12), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (SB(i, 23, 22) != 0) return UDF;
    a64_u8 tab[64];
    int regs = len + 1;
    for (int k = 0; k < regs; k++) __builtin_memcpy(tab + 16 * k, c->v[(n + k) & 31].b, 16);
    vr idx = c->v[m], r = c->v[d];
    int ne = q ? 16 : 8;
    for (int e = 0; e < ne; e++) {
        unsigned x = idx.b[e];
        if (x < 16u * regs) r.b[e] = tab[x];
        else if (!tbx) r.b[e] = 0;
    }
    wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * three-register extension: SDOT/UDOT/USDOT, SMMLA/UMMLA/USMMLA, SQRDMLAH/SQRDMLSH,
 * FCMLA/FCADD, BFDOT/BFMMLA/BFMLALB/T
 * ------------------------------------------------------------------------------------------- */

static int rdm_vec(a64_cpu *c, a64_u32 i, int sc, int sub) {
    int q = sc ? 0 : SBT(i, 30), size = SB(i, 23, 22), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (size == 0 || size == 3) return UDF;
    int eb = 8 << size, ne = sc ? 1 : (q ? 128 : 64) / eb;
    vr a = c->v[n], b = c->v[m], acc = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    for (int e = 0; e < ne; e++) {
        i128 x = sint(vel(&a, e, eb), eb, 0), y = sint(vel(&b, e, eb), eb, 0);
        i128 z = sint(vel(&acc, e, eb), eb, 0) * ((i128)1 << eb);
        i128 p = (sub ? z - 2 * x * y : z + 2 * x * y) + ((i128)1 << (eb - 1));
        vset(&r, e, eb, sats(c, p >> eb, eb));
    }
    if (sc) wsc(c, d, r.d[0], eb);
    else wq(c, d, &r, q);
    return NEXT;
}

static int dot_vec(a64_cpu *c, a64_u32 i, int nu, int mu) {
    int q = SBT(i, 30), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (SB(i, 23, 22) != 2) return UDF;
    vr a = c->v[n], b = c->v[m], r = c->v[d];
    for (int e = 0; e < (q ? 4 : 2); e++) {
        a64_u32 s = r.s[e];
        for (int k = 0; k < 4; k++)
            s += (a64_u32)((a64_i64)sint(a.b[4 * e + k], 8, nu) * (a64_i64)sint(b.b[4 * e + k], 8, mu));
        r.s[e] = s;
    }
    wq(c, d, &r, q);
    return NEXT;
}

static int mmla(a64_cpu *c, a64_u32 i, int nu, int mu) {  /* SMMLA UMMLA USMMLA */
    int m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if (SB(i, 23, 22) != 2 || !SBT(i, 30)) return UDF;
    vr a = c->v[n], b = c->v[m], r = c->v[d];
    for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++) {
            a64_u32 s = r.s[2 * x + y];
            for (int k = 0; k < 8; k++)
                s += (a64_u32)((a64_i64)sint(a.b[8 * x + k], 8, nu) * (a64_i64)sint(b.b[8 * y + k], 8, mu));
            r.s[2 * x + y] = s;
        }
    c->v[d] = r;
    return NEXT;
}

int a64s_three_ext(a64_cpu *c, a64_u32 i, int sc) {
    int u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 14, 11);
    if (sc) {
        if (!u || op > 1) return UDF;
        return rdm_vec(c, i, 1, op);
    }
    if (!u) {
        switch (op) {
        case 2: return dot_vec(c, i, 0, 0);               /* SDOT */
        case 3: return dot_vec(c, i, 1, 0);               /* USDOT */
        case 4: return mmla(c, i, 0, 0);                  /* SMMLA */
        case 5: return mmla(c, i, 1, 0);                  /* USMMLA */
        default: return UDF;
        }
    }
    switch (op) {
    case 0: case 1: return rdm_vec(c, i, 0, op);          /* SQRDMLAH SQRDMLSH */
    case 2: return dot_vec(c, i, 1, 1);                   /* UDOT */
    case 4: return mmla(c, i, 1, 1);                      /* UMMLA */
    case 8: case 9: case 10: case 11: return a64s_fcmla_vec(c, i);
    case 12: case 14: return a64s_fcadd_vec(c, i);
    case 13: return (size == 1 && SBT(i, 30)) ? a64s_bfmmla(c, i) : UDF;
    case 15:
        if (size == 1) return a64s_bfdot(c, i, -1);
        if (size == 3) return a64s_bfmlal(c, i, -1);
        return UDF;
    default: return UDF;
    }
}
