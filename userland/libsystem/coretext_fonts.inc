/* mOS Core Text — fontes: nomes pela tabela 'name', idiomas pela OS/2, carets de ligadura pelo GDEF, descritores
 * derivados, coleções (atributos, cópia, ordenação), o gerenciador (URLs e nomes disponíveis, registro em lote,
 * escopo, suporte, ativação automática, pedidos) e o que faltava de CTFont e CTRun. Incluído no fim do coretext.c. */

CTCONST(kCTFontSubFamilyNameKey, "CTFontSubFamilyName")
CTCONST(kCTFontVersionNameKey, "CTFontVersionName")
CTCONST(kCTFontTrademarkNameKey, "CTFontTrademarkName")
CTCONST(kCTFontManufacturerNameKey, "CTFontManufacturerName")
CTCONST(kCTFontDesignerNameKey, "CTFontDesignerName")
CTCONST(kCTFontDescriptionNameKey, "CTFontDescriptionName")
CTCONST(kCTFontVendorURLNameKey, "CTFontVendorURLName")
CTCONST(kCTFontDesignerURLNameKey, "CTFontDesignerURLName")
CTCONST(kCTFontLicenseNameKey, "CTFontLicenseName")
CTCONST(kCTFontLicenseURLNameKey, "CTFontLicenseURLName")
CTCONST(kCTFontSampleTextNameKey, "CTFontSampleTextName")
CTCONST(kCTFontPostScriptCIDNameKey, "CTFontPostScriptCIDName")
CTCONST(kCTFontDescriptorMatchingSourceDescriptor, "CTFontDescriptorMatchingSourceDescriptor")
CTCONST(kCTFontDescriptorMatchingDescriptors, "CTFontDescriptorMatchingDescriptors")
CTCONST(kCTFontDescriptorMatchingResult, "CTFontDescriptorMatchingResult")
CTCONST(kCTFontDescriptorMatchingPercentage, "CTFontDescriptorMatchingPercentage")
CTCONST(kCTFontDescriptorMatchingError, "CTFontDescriptorMatchingError")
CTCONST(kCTFontFeatureTypeIdentifierKeyAlias, "CTFeatureTypeIdentifier")

id CFStringCreateWithCharacters(void *alloc, const u16 *chars, long n);
int memcmp(const void *a, const void *b, size_t n);
/* ---- nomes: a entrada da tabela 'name' em Unicode (Windows 3/1 ou Unicode 0/x, senão Mac Roman) */
static id ct_name_string(id f, u32 want) {
    long n;
    u8 *own;
    const u8 *t = ct_font_table(f, 0x6E616D65u, &n, &own);
    id out = 0;
    if (t && n >= 6) {
        u32 count = rd16(t + 2), so = rd16(t + 4);
        int best = -1;
        u32 boff = 0, blen = 0, butf16 = 0;
        for (u32 i = 0; i < count && 6 + (long)i * 12 + 12 <= n; i++) {
            const u8 *r = t + 6 + i * 12;
            u32 plat = rd16(r), enc = rd16(r + 2), lang = rd16(r + 4), id_ = rd16(r + 6), len = rd16(r + 8), off = rd16(r + 10);
            if (id_ != want || so + off + len > (u32)n) continue;
            int score = plat == 3 && (enc == 1 || enc == 10) ? (lang == 0x409 ? 4 : 3) : plat == 0 ? 2 : plat == 1 && enc == 0 ? 1 : -1;
            if (score > best) { best = score; boff = so + off; blen = len; butf16 = plat != 1; }
        }
        if (best >= 0) {
            u16 buf[512];
            long k = 0;
            if (butf16) for (u32 j = 0; j + 1 < blen && k < 512; j += 2) buf[k++] = (u16)rd16(t + boff + j);
            else for (u32 j = 0; j < blen && k < 512; j++) { u8 c = t[boff + j]; buf[k++] = c < 0x80 ? c : 0xFFFD; }
            out = (id)CFStringCreateWithCharacters(0, buf, k);
        }
    }
    free(own);
    return out;
}
static const struct { const void **key; u32 id; } ct_name_keys[] = {
    {&kCTFontCopyrightNameKey, 0}, {&kCTFontFamilyNameKey, 1}, {&kCTFontSubFamilyNameKey, 2}, {&kCTFontStyleNameKey, 2},
    {&kCTFontUniqueNameKey, 3}, {&kCTFontFullNameKey, 4}, {&kCTFontVersionNameKey, 5}, {&kCTFontPostScriptNameKey, 6},
    {&kCTFontTrademarkNameKey, 7}, {&kCTFontManufacturerNameKey, 8}, {&kCTFontDesignerNameKey, 9}, {&kCTFontDescriptionNameKey, 10},
    {&kCTFontVendorURLNameKey, 11}, {&kCTFontDesignerURLNameKey, 12}, {&kCTFontLicenseNameKey, 13}, {&kCTFontLicenseURLNameKey, 14},
    {&kCTFontSampleTextNameKey, 19}, {&kCTFontPostScriptCIDNameKey, 20}};
/* CTFontCopyName pela chave (substitui a versão que só devolvia o nome PostScript) */
static id ct_copy_name(id f, id key) {
    if (!f || !key) return 0;
    const char *k = cx_cstr(key);
    int hit = -1;
    for (u32 i = 0; i < sizeof ct_name_keys / sizeof ct_name_keys[0] && hit < 0; i++) if ((const void *)key == *ct_name_keys[i].key) hit = (int)i;
    for (u32 i = 0; i < sizeof ct_name_keys / sizeof ct_name_keys[0] && hit < 0; i++) if (k[0] && !strcmp(k, cx_cstr((id)*ct_name_keys[i].key))) hit = (int)i;
    for (u32 i = (u32)(hit < 0 ? 0 : hit); hit >= 0 && i == (u32)hit; i++)
        {
            id s = ct_name_string(f, ct_name_keys[i].id);
            if (s) return s;
            if (ct_name_keys[i].id == 6 || ct_name_keys[i].id == 4) return objc_retain(cx_str(fname(f)));
            if (ct_name_keys[i].id == 1) return objc_retain(family_of(f));
            return 0;
        }
    return 0;
}
id CTFontCopyDisplayName(id f) { id s = ct_name_string(f, 4); return s ? s : objc_retain(cx_str(fname(f))); }

/* ---- idiomas: bits de página de código da OS/2 (ulCodePageRange1) e a cmap como confirmação */
id CTFontCopySupportedLanguages(id f) {
    static const struct { int bit; const char *langs; } cp[] = {
        {0, "en,pt,es,fr,de,it,nl,sv,da,no,fi,is,ca,eu,gl,ga,af,sq,id,ms,sw"}, {1, "cs,pl,hu,sk,sl,hr,ro,bs"}, {2, "ru,uk,be,bg,sr,mk,kk"},
        {3, "el"}, {4, "tr,az"}, {5, "he,yi"}, {6, "ar,fa,ur"}, {7, "et,lv,lt"}, {8, "vi"}, {16, "th"}, {17, "ja"}, {18, "zh-Hans"},
        {19, "ko"}, {20, "zh-Hant"}};
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    long n;
    u8 *own;
    const u8 *os2 = ct_font_table(f, 0x4F532F32u, &n, &own);
    u32 r1 = os2 && n >= 82 ? (u32)rd16(os2 + 78) << 16 | rd16(os2 + 80) : 1;
    free(own);
    for (u32 i = 0; i < sizeof cp / sizeof cp[0]; i++) {
        if (!(r1 >> cp[i].bit & 1)) continue;
        char buf[96];
        const char *s = cp[i].langs;
        while (*s) {
            int k = 0;
            while (*s && *s != ',' && k < 95) buf[k++] = *s++;
            buf[k] = 0;
            if (*s == ',') s++;
            MSG(void, arr, "addObject:", cx_str(buf));
        }
    }
    return objc_retain(arr);
}
id CTFontCopyFeatureSettings(id f) {
    (void)f;
    return objc_retain(MSG0(id, cls("NSMutableArray"), "array"));   /* as fontes do mOS saem com os recursos padrão */
}
id CTFontCreateCopyWithFamily(id f, double size, const void *matrix, id family) {
    if (!family) return 0;
    id n = CTFontCreateWithName(family, size > 0 ? size : (f ? fsize(f) : 12), matrix);
    if (!n || !f) return n;
    u32 t = ftraits(f) & (TRAIT_BOLD | TRAIT_ITALIC);
    if (!t) return n;
    id c = CTFontCreateCopyWithSymbolicTraits(n, 0, matrix, t, t);
    if (c) { objc_release(n); return c; }
    return n;
}
id CTFontCreateWithFontDescriptorAndOptions(id d, double size, const void *matrix, unsigned long options) { (void)options; return CTFontCreateWithFontDescriptor(d, size, matrix); }
id CTFontCreateWithNameAndOptions(id name, double size, const void *matrix, unsigned long options) { (void)options; return CTFontCreateWithName(name, size, matrix); }
/* o ATSFontRef do mOS é a face do kernel (0..3 Inter, 32+ registradas) */
id CTFontCreateWithPlatformFont(u32 face, double size, const void *matrix, id attrs) {
    (void)attrs;
    if (!((face <= 3) || is_user((int)face))) return 0;
    return objc_retain(ct_with_matrix(font_with_face(size > 0 ? size : 12, (int)face, 0), matrix, 0));
}
u32 CTFontGetPlatformFont(id f, id *desc) { if (desc) *desc = 0; return f ? (u32)fweight(f) : 0; }
u32 CTFontGetStringEncoding(id f) { (void)f; return 0x0100; }                  /* kCFStringEncodingUnicode */
u8 CTFontHasTable(id f, u32 tag) {
    long n;
    u8 *own;
    const u8 *t = ct_font_table(f, tag, &n, &own);
    free(own);
    return t && n > 0;
}
/* carets de ligadura: GDEF LigCaretList (CaretValue formatos 1 e 2 em unidades; 3 = 1 + device) */
long CTFontGetLigatureCaretPositions(id f, u16 glyph, double *positions, long max) {
    long n;
    u8 *own;
    const u8 *g = ct_font_table(f, 0x47444546u, &n, &own);
    long cnt = 0;
    if (g && n >= 12) {
        u32 lcl = rd16(g + 8);
        if (lcl && lcl + 4 <= (u32)n) {
            const u8 *l = g + lcl;
            u32 cov = rd16(l), nlig = rd16(l + 2);
            const u8 *c = l + cov;
            long idx = -1;
            if (c + 4 <= g + n) {
                u32 fmt = rd16(c), k = rd16(c + 2);
                if (fmt == 1) { for (u32 i = 0; i < k && c + 4 + i * 2 + 2 <= g + n; i++) if (rd16(c + 4 + i * 2) == glyph) { idx = i; break; } }
                else if (fmt == 2)
                    for (u32 i = 0; i < k && c + 4 + i * 6 + 6 <= g + n; i++) {
                        const u8 *r = c + 4 + i * 6;
                        if (glyph >= rd16(r) && glyph <= rd16(r + 2)) { idx = rd16(r + 4) + (glyph - rd16(r)); break; }
                    }
            }
            if (idx >= 0 && (u32)idx < nlig && l + 4 + idx * 2 + 2 <= g + n) {
                const u8 *lg = l + rd16(l + 4 + idx * 2);
                u32 nc = lg + 2 <= g + n ? rd16(lg) : 0;
                double sc = fsize(f) / em_metric(f, 2);
                for (u32 i = 0; i < nc && lg + 2 + i * 2 + 2 <= g + n; i++) {
                    const u8 *cv = lg + rd16(lg + 2 + i * 2);
                    if (cv + 4 > g + n) break;
                    if (positions && cnt < max) positions[cnt] = (double)(int16_t)rd16(cv + 2) * sc;
                    cnt++;
                }
            }
        }
    }
    free(own);
    return cnt;
}
/* limites ópticos: sem tabela 'opbd', a caixa de avanço (largura do avanço, ascent a descent) */
ct_rect_t CTFontGetOpticalBoundsForGlyphs(id f, const u16 *glyphs, ct_rect_t *rects, long n, unsigned long options) {
    (void)options;
    ct_rect_t u = {0, 0, 0, 0};
    double asc = CTFontGetAscent(f), desc = CTFontGetDescent(f);
    for (long i = 0; i < n; i++) {
        ct_sz_t adv;
        CTFontGetAdvancesForGlyphs(f, 0, glyphs ? &glyphs[i] : 0, &adv, 1);
        ct_rect_t r = {0, -desc, adv.w, asc + desc};
        if (rects) rects[i] = r;
        if (r.w > u.w) u.w = r.w;
        u.y = -desc; u.h = asc + desc;
    }
    return u;
}

/* ---- descritores derivados */
id CTFontDescriptorCreateCopyWithFamily(id d, id family) {
    if (!family) return 0;
    id n = CTFontDescriptorCreateWithAttributes(d);
    MSG(void, n, "setObject:forKey:", family, (id)kCTFontFamilyNameAttribute);
    MSG(void, n, "removeObjectForKey:", (id)kCTFontNameAttribute);
    return n;
}
id CTFontDescriptorCreateCopyWithFeature(id d, id type, id selector) {
    id n = CTFontDescriptorCreateWithAttributes(d);
    id old = MSG(id, n, "objectForKey:", (id)kCTFontFeatureSettingsAttribute);
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    if (old) MSG(void, arr, "addObjectsFromArray:", old);
    id one = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (type) MSG(void, one, "setObject:forKey:", type, (id)kCTFontFeatureTypeIdentifierKey);
    if (selector) MSG(void, one, "setObject:forKey:", selector, (id)kCTFontFeatureSelectorIdentifierKey);
    MSG(void, arr, "addObject:", one);
    MSG(void, n, "setObject:forKey:", arr, (id)kCTFontFeatureSettingsAttribute);
    return n;
}
id CTFontDescriptorCreateCopyWithSymbolicTraits(id d, u32 value, u32 mask) {
    id n = CTFontDescriptorCreateWithAttributes(d);
    id old = MSG(id, n, "objectForKey:", (id)kCTFontTraitsAttribute);
    id tr = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (old) MSG(void, tr, "addEntriesFromDictionary:", old);
    id cur = MSG(id, tr, "objectForKey:", (id)kCTFontSymbolicTrait);
    u32 t = cur ? (u32)MSG0(long long, cur, "longLongValue") : 0;
    t = (t & ~mask) | (value & mask);
    MSG(void, tr, "setObject:forKey:", cx_num_i((i64)t), (id)kCTFontSymbolicTrait);
    MSG(void, n, "setObject:forKey:", tr, (id)kCTFontTraitsAttribute);
    return n;
}
id CTFontDescriptorCreateCopyWithVariation(id d, id axis, double value) {
    id n = CTFontDescriptorCreateWithAttributes(d);
    id old = MSG(id, n, "objectForKey:", (id)kCTFontVariationAttribute);
    id v = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (old) MSG(void, v, "addEntriesFromDictionary:", old);
    if (axis) MSG(void, v, "setObject:forKey:", cx_num_d(value), axis);
    MSG(void, n, "setObject:forKey:", v, (id)kCTFontVariationAttribute);
    return n;
}
/* casamento com progresso: começa, devolve o resultado de cada um e termina (estados do macOS) */
u8 CTFontDescriptorMatchFontDescriptorsWithProgressHandler(id descs, id mandatory, void *block) {
    ct_block_t *b = block;
    if (!b || !b->invoke) return 0;
    u8 (*inv)(void *, int, id) = (u8 (*)(void *, int, id))b->invoke;
    if (!inv(block, 0 /* DidBegin */, 0)) return 1;
    long n = descs ? MSG0(long, descs, "count") : 0;
    id all = MSG0(id, cls("NSMutableArray"), "array");
    for (long i = 0; i < n; i++) {
        id d = MSG(id, descs, "objectAtIndex:", (u64)i);
        id m = CTFontDescriptorCreateMatchingFontDescriptors(d, mandatory);
        id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        MSG(void, info, "setObject:forKey:", d, (id)kCTFontDescriptorMatchingSourceDescriptor);
        if (m) { MSG(void, info, "setObject:forKey:", m, (id)kCTFontDescriptorMatchingResult); MSG(void, all, "addObjectsFromArray:", m); objc_release(m); }
        if (!inv(block, 7 /* DidMatch */, info)) return 1;
    }
    id fin = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, fin, "setObject:forKey:", all, (id)kCTFontDescriptorMatchingResult);
    inv(block, 1 /* DidFinish */, fin);
    return 1;
}

/* ---- coleções (array de descritores) */
unsigned CTFontCollectionGetTypeID(void) { return 0x43544643u; }
id CTFontCollectionCreateMutableCopy(id coll) {
    id a = MSG0(id, cls("NSMutableArray"), "array");
    id m = coll ? CTFontCollectionCreateMatchingFontDescriptors(coll) : 0;
    if (m) { MSG(void, a, "addObjectsFromArray:", m); objc_release(m); }
    return objc_retain(a);
}
id CTFontCollectionCreateCopyWithFontDescriptors(id orig, id descs, id options) {
    (void)options;
    id a = CTFontCollectionCreateMutableCopy(orig);
    if (descs) MSG(void, a, "addObjectsFromArray:", descs);
    return a;
}
id CTFontCollectionCopyFontAttribute(id coll, id name, u32 options) {
    (void)options;
    id m = CTFontCollectionCreateMatchingFontDescriptors(coll);
    id out = MSG0(id, cls("NSMutableArray"), "array");
    for (long i = 0; m && i < MSG0(long, m, "count"); i++) {
        id v = MSG(id, MSG(id, m, "objectAtIndex:", (u64)i), "objectForKey:", name);
        if (v) MSG(void, out, "addObject:", v);
    }
    objc_release(m);
    return objc_retain(out);
}
id CTFontCollectionCopyFontAttributes(id coll, id names, u32 options) {
    (void)options;
    id m = CTFontCollectionCreateMatchingFontDescriptors(coll);
    id out = MSG0(id, cls("NSMutableArray"), "array");
    id keys = names ? MSG0(id, names, "allObjects") : 0;
    for (long i = 0; m && i < MSG0(long, m, "count"); i++) {
        id d = MSG(id, m, "objectAtIndex:", (u64)i), one = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        for (long k = 0; keys && k < MSG0(long, keys, "count"); k++) {
            id key = MSG(id, keys, "objectAtIndex:", (u64)k), v = MSG(id, d, "objectForKey:", key);
            if (v) MSG(void, one, "setObject:forKey:", v, key);
        }
        MSG(void, out, "addObject:", one);
    }
    objc_release(m);
    return objc_retain(out);
}
typedef long (*ct_sort_fn)(id a, id b, void *ref);
id CTFontCollectionCreateMatchingFontDescriptorsSortedWithCallback(id coll, ct_sort_fn cmp, void *ref) {
    id m = CTFontCollectionCreateMatchingFontDescriptors(coll);
    long n = m ? MSG0(long, m, "count") : 0;
    if (!n) { objc_release(m); return 0; }
    id *v = malloc((size_t)n * sizeof *v);
    if (!v) return m;
    for (long i = 0; i < n; i++) v[i] = MSG(id, m, "objectAtIndex:", (u64)i);
    for (long i = 1; cmp && i < n; i++) { id x = v[i]; long j = i - 1; while (j >= 0 && cmp(v[j], x, ref) > 0) { v[j + 1] = v[j]; j--; } v[j + 1] = x; }
    id out = MSG0(id, cls("NSMutableArray"), "array");
    for (long i = 0; i < n; i++) MSG(void, out, "addObject:", v[i]);
    free(v);
    objc_release(m);
    return objc_retain(out);
}

/* ---- gerenciador */
static u32 g32be(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
int closedir(void *d);
static int ct_auto_activation = 0;                   /* kCTFontManagerAutoActivationDefault */
static int ct_is_sfnt_file(const char *path) {
    u64 n = 0;
    u8 *b = cx_read_file(path, &n);
    int ok = b && n >= 12 && (g32be(b) == 0x00010000u || g32be(b) == 0x4F54544Fu || g32be(b) == 0x74727565u || g32be(b) == 0x74746366u);
    free(b);
    return ok;
}
id CTFontManagerCopyAvailableFontURLs(void) {
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    void *d = opendir(FB_DIR);
    if (d) {
        void *e;
        while ((e = readdir(d))) {
            const char *nm = (const char *)e + 21;
            size_t l = strlen(nm);
            if (l < 5 || (strcmp(nm + l - 4, ".ttf") && strcmp(nm + l - 4, ".otf") && strcmp(nm + l - 4, ".ttc"))) continue;
            char p[512];
            snprintf(p, sizeof p, FB_DIR "%s", nm);
            id u = MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(p));
            if (u) MSG(void, arr, "addObject:", u);
        }
        closedir(d);
    }
    return objc_retain(arr);
}
id CTFontManagerCopyAvailablePostScriptNames(void) {
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    static const char *const sys[] = {"Inter-Regular", "Inter-Medium", "Inter-SemiBold", "Inter-Bold"};
    for (u32 i = 0; i < 4; i++) MSG(void, arr, "addObject:", cx_str(sys[i]));
    for (int i = 0; i < USER_MAX; i++) if (user_bytes[i] && unames[i].ps[0]) MSG(void, arr, "addObject:", cx_str(unames[i].ps));
    return objc_retain(arr);
}
u8 CTFontManagerIsSupportedFont(id url) { return url ? (u8)ct_is_sfnt_file(ct_url_path(url)) : 0; }
u32 CTFontManagerGetScopeForURL(id url) {
    if (!url) return 0;
    id data = ct_data_from_path(ct_url_path(url));
    if (!data) return 0;
    const u8 *b = CFDataGetBytePtr(data);
    long n = CFDataGetLength(data);
    for (int i = 0; i < USER_MAX; i++)
        if (user_bytes[i] && user_nbytes[i] == n && !memcmp(user_bytes[i], b, (size_t)n)) { objc_release(data); return 1; }   /* processo */
    objc_release(data);
    return 0;
}
u8 CTFontManagerRegisterFontsForURLs(id urls, u32 scope, id *errors) {
    int all = 1;
    id errs = MSG0(id, cls("NSMutableArray"), "array");
    for (long i = 0; urls && i < MSG0(long, urls, "count"); i++)
        if (!CTFontManagerRegisterFontsForURL(MSG(id, urls, "objectAtIndex:", (u64)i), scope, 0)) { all = 0; ct_add_error(errs); }
    if (errors) *errors = all ? 0 : objc_retain(errs);
    return (u8)all;
}
u8 CTFontManagerUnregisterFontsForURLs(id urls, u32 scope, id *errors) {
    int all = 1;
    for (long i = 0; urls && i < MSG0(long, urls, "count"); i++)
        if (!CTFontManagerUnregisterFontsForURL(MSG(id, urls, "objectAtIndex:", (u64)i), scope, 0)) all = 0;
    if (errors) *errors = 0;
    return (u8)all;
}
u8 CTFontManagerUnregisterGraphicsFont(void *font, id *error) {
    if (error) *error = 0;
    cgf_t *g = cgf_of(font);
    if (!g || !is_user(g->fid)) return 0;
    int i = g->fid - USER_BASE;
    free(user_bytes[i]);
    user_bytes[i] = 0;
    user_nbytes[i] = 0;
    memset(&unames[i], 0, sizeof unames[i]);
    return 1;
}
void CTFontManagerUnregisterFontDescriptors(id descs, u32 scope, void *block) {
    (void)scope;
    for (long i = 0; descs && i < MSG0(long, descs, "count"); i++) {
        id d = MSG(id, descs, "objectAtIndex:", (u64)i);
        id num = d ? MSG(id, d, "objectForKey:", cx_str(USER_ID_KEY)) : 0;
        int fid = num ? (int)MSG0(long long, num, "longLongValue") : -1;
        if (is_user(fid)) { free(user_bytes[fid - USER_BASE]); user_bytes[fid - USER_BASE] = 0; user_nbytes[fid - USER_BASE] = 0; memset(&unames[fid - USER_BASE], 0, sizeof unames[0]); }
    }
    ct_call_done(block, MSG0(id, cls("NSMutableArray"), "array"), 1);
}
void CTFontManagerEnableFontDescriptors(id descs, u8 enable) { (void)descs; (void)enable; }   /* toda fonte registrada fica ativa */
u32 CTFontManagerGetAutoActivationSetting(id bundle) { (void)bundle; return (u32)ct_auto_activation; }
void CTFontManagerSetAutoActivationSetting(id bundle, u32 setting) { (void)bundle; ct_auto_activation = (int)setting; }
/* pedido de fontes: as que não se resolvem por nome voltam na lista "não resolvidas" */
void CTFontManagerRequestFonts(id descs, void *block) {
    id unresolved = MSG0(id, cls("NSMutableArray"), "array");
    for (long i = 0; descs && i < MSG0(long, descs, "count"); i++) {
        id d = MSG(id, descs, "objectAtIndex:", (u64)i);
        id nm = d ? MSG(id, d, "objectForKey:", (id)kCTFontNameAttribute) : 0;
        void *cg = nm ? CGFontCreateWithFontName(nm) : 0;
        if (!cg) MSG(void, unresolved, "addObject:", d); else objc_release((id)cg);
    }
    ct_block_t *b = block;
    if (b && b->invoke) ((void (*)(void *, id))b->invoke)(block, unresolved);
}
void *CTFontManagerCreateFontRequestRunLoopSource(long order, void *createMatchesCallback) {
    (void)order; (void)createMatchesCallback;
    return 0;                                     /* pedidos de outros processos não existem no mOS */
}

/* ---- CTRun: avanços em ponteiro e avanços/origens de base */
static const ct_sz_t *ct_run_adv_cache(id run) {
    ct_run_t *r = ct_state(run);
    if (!r) return 0;
    static struct { id run; ct_sz_t *adv; long n; } cache[8];
    static int next;
    for (int i = 0; i < 8; i++) if (cache[i].run == run && cache[i].n == r->count) return cache[i].adv;
    ct_sz_t *a = malloc((size_t)(r->count ? r->count : 1) * sizeof *a);
    if (!a) return 0;
    ct_range_t all = {0, 0};
    CTRunGetAdvances(run, all, a);
    free(cache[next].adv);
    cache[next].run = run; cache[next].adv = a; cache[next].n = r->count;
    next = (next + 1) & 7;
    return a;
}
const ct_sz_t *CTRunGetAdvancesPtr(id run) { return ct_run_adv_cache(run); }
void CTRunGetBaseAdvancesAndOrigins(id run, ct_range_t range, ct_sz_t *advances, ct_pt_t *origins) {
    ct_run_t *r = ct_state(run);
    if (!r) return;
    long a = range.loc, n = range.len ? range.len : r->count - range.loc;
    if (a < 0 || n <= 0 || a + n > r->count) return;
    ct_sz_t *adv = malloc((size_t)n * sizeof *adv);
    ct_pt_t *pos = malloc((size_t)n * sizeof *pos);
    if (adv && pos) {
        ct_range_t rr = {a, n};
        CTRunGetAdvances(run, rr, adv);
        CTRunGetPositions(run, rr, pos);
        double pen = pos[0].x;
        for (long i = 0; i < n; i++) {
            if (advances) advances[i] = adv[i];
            if (origins) { origins[i].x = pos[i].x - pen; origins[i].y = pos[i].y; }   /* deslocamento do glifo em relação à pena */
            pen += adv[i].w;
        }
    }
    free(adv);
    free(pos);
}
