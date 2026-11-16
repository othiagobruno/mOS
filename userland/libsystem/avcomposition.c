/*
 * mOS — AVComposition, AVMutableComposition, AVCompositionTrack, AVMutableCompositionTrack e AVCompositionTrackSegment.
 * Cada faixa é uma lista de trechos contíguos no tempo de destino (trecho de arquivo ou vazio); inserir parte o trecho
 * no ponto e empurra o resto, remover parte e puxa, escalar muda a duração no destino. O AVAssetExportSession exporta a
 * composição pelo avx_compose (cópia, sem recodificar): trechos escalados ou de configurações diferentes falham com o
 * motivo. O AVPlayer ainda não toca composição (isPlayable = NO). O estado fica numa tabela à parte (o layout do
 * AVAsset/AVAssetTrack é do avfaudio.c). Código original do mOS (MIT).
 */
#include "foundation.h"
#include "avexport.h"
#include "mp4mux.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memmove(void *d, const void *s, size_t n);
size_t strlen(const char *s);
char *strdup(const char *s);
int strcmp(const char *a, const char *b);
const char *cx_audio_url_path(id url);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id objc_msgSend_stret(id self, SEL op, ...);   /* struct de mais de 16 bytes (CMTime, CGAffineTransform) no x86_64 */

typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmt;
typedef struct { cmt start, duration; } cmrange;
typedef struct { cmrange source, target; } cmmapping;
typedef struct { double w, h; } szd;
typedef struct { double a, b, c, d, tx, ty; } aff;
static double secs(cmt t) { return (t.flags & 1) && t.timescale > 0 ? (double)t.value / t.timescale : 0; }
static int valid(cmt t) { return (t.flags & 1) && t.timescale > 0; }
static cmt mk(double s) { cmt t = {(i64)(s * 1000000 + (s >= 0 ? 0.5 : -0.5)), 1000000, 1, 0}; return t; }
static cmt invalid(void) { cmt t = {0, 0, 0, 0}; return t; }

typedef struct { char *path; id url; i32 src_tid; int empty; double src, dur, tgt, tdur; } seg;
typedef struct ctrack { id obj; int video; i32 tid; seg *s; int n, cap; aff tf; szd nat; BOOL enabled; i32 mts; id comp; struct ctrack *next; } ctrack;
typedef struct comp { id obj; szd nat; struct comp *next; } comp;
static ctrack *g_tracks;
static comp *g_comps;
static ctrack *ct_of(id o) { for (ctrack *t = g_tracks; t; t = t->next) if (t->obj == o) return t; return 0; }
static comp *cp_of(id o) { for (comp *c = g_comps; c; c = c->next) if (c->obj == o) return c; return 0; }
cx_class_t AVComposition_c __asm__("OBJC_CLASS_$_AVComposition"), AVComposition_m __asm__("OBJC_METACLASS_$_AVComposition");
cx_class_t AVMutableComposition_c __asm__("OBJC_CLASS_$_AVMutableComposition"), AVMutableComposition_m __asm__("OBJC_METACLASS_$_AVMutableComposition");
cx_class_t AVCompositionTrack_c __asm__("OBJC_CLASS_$_AVCompositionTrack"), AVCompositionTrack_m __asm__("OBJC_METACLASS_$_AVCompositionTrack");
cx_class_t AVMutableCompositionTrack_c __asm__("OBJC_CLASS_$_AVMutableCompositionTrack"), AVMutableCompositionTrack_m __asm__("OBJC_METACLASS_$_AVMutableCompositionTrack");
cx_class_t AVCompositionTrackSegment_c __asm__("OBJC_CLASS_$_AVCompositionTrackSegment"), AVCompositionTrackSegment_m __asm__("OBJC_METACLASS_$_AVCompositionTrackSegment");
typedef struct { cx_class_t *isa; id url; i32 tid; int empty; double src, dur, tgt, tdur; } segobj_t;
#define SO(o) ((segobj_t *)(o))
static id cls(const char *n) { return (id)cx_class_named(n); }
static id error_with(i64 code, const char *msg) {
    id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), code, info);
}

/* ---------------------------------------------------------------- lista de trechos */
static double track_end(const ctrack *t) { return t->n ? t->s[t->n - 1].tgt + t->s[t->n - 1].tdur : 0; }
static int grow(ctrack *t) {
    if (t->n < t->cap) return 1;
    int nc = t->cap ? t->cap * 2 : 8;
    seg *x = realloc(t->s, (size_t)nc * sizeof *x);
    if (!x) return 0;
    t->s = x; t->cap = nc;
    return 1;
}
static void seg_free(seg *s) { free(s->path); if (s->url) objc_release(s->url); }
/* garante uma fronteira em T (parte o trecho que o contém); devolve o índice do primeiro trecho que começa em >= T */
static int split_at(ctrack *t, double T) {
    for (int i = 0; i < t->n; i++) {
        seg *s = &t->s[i];
        if (T <= s->tgt + 1e-12) return i;
        if (T < s->tgt + s->tdur - 1e-12) {
            if (!grow(t)) return -1;
            s = &t->s[i];
            seg right = *s;
            double lt = T - s->tgt, ld = s->tdur > 0 ? s->dur * lt / s->tdur : 0;
            s->tdur = lt; s->dur = ld;
            right.tgt = T; right.tdur -= lt; right.src += ld; right.dur -= ld;
            if (right.path) right.path = strdup(right.path);
            if (right.url) objc_retain(right.url);
            memmove(t->s + i + 2, t->s + i + 1, (size_t)(t->n - i - 1) * sizeof *t->s);
            t->s[i + 1] = right;
            t->n++;
            return i + 1;
        }
    }
    return t->n;
}
static void shift_from(ctrack *t, int i, double d) { for (; i < t->n; i++) t->s[i].tgt += d; }
static int insert_seg(ctrack *t, double T, seg nw) {
    if (T > track_end(t) + 1e-12) {   /* depois do fim: vazio até T */
        seg gap = {0, 0, 0, 1, 0, T - track_end(t), track_end(t), T - track_end(t)};
        if (!grow(t)) return 0;
        t->s[t->n++] = gap;
    }
    int i = split_at(t, T);
    if (i < 0 || !grow(t)) return 0;
    shift_from(t, i, nw.tdur);
    memmove(t->s + i + 1, t->s + i, (size_t)(t->n - i) * sizeof *t->s);
    nw.tgt = T;
    t->s[i] = nw;
    t->n++;
    return 1;
}
static void remove_range(ctrack *t, double a, double b) {
    if (b <= a) return;
    int i = split_at(t, a), j = split_at(t, b);
    if (i < 0 || j < 0) return;
    for (int k = i; k < j; k++) seg_free(&t->s[k]);
    memmove(t->s + i, t->s + j, (size_t)(t->n - j) * sizeof *t->s);
    t->n -= j - i;
    shift_from(t, i, -(b - a));
}
static void scale_range(ctrack *t, double a, double b, double nd) {
    if (b <= a || nd <= 0) return;
    int i = split_at(t, a), j = split_at(t, b);
    if (i < 0 || j < 0) return;
    double f = nd / (b - a), at = a;
    for (int k = i; k < j; k++) { t->s[k].tgt = at; t->s[k].tdur *= f; at += t->s[k].tdur; }
    shift_from(t, j, nd - (b - a));
}

/* ---------------------------------------------------------------- AVCompositionTrack */
static id ct_new(id comp_obj, int video, i32 tid, int is_mutable) {
    id o = cx_alloc_instance(is_mutable ? &AVMutableCompositionTrack_c : &AVCompositionTrack_c);
    ctrack *t = calloc(1, sizeof *t);
    if (!o || !t) return 0;
    t->obj = o; t->video = video; t->tid = tid; t->enabled = YES; t->comp = comp_obj;
    t->tf = (aff){1, 0, 0, 1, 0, 0};
    t->next = g_tracks; g_tracks = t;
    return o;
}
static id ctm_media(id s, SEL c) { (void)c; ctrack *t = ct_of(s); return cx_str(t && t->video ? "vide" : "soun"); }
static i32 ctm_tid(id s, SEL c) { (void)c; ctrack *t = ct_of(s); return t ? t->tid : 0; }
static cmrange ctm_range(id s, SEL c) { (void)c; ctrack *t = ct_of(s); cmrange r = {mk(0), mk(t ? track_end(t) : 0)}; return r; }
static cmt ctm_duration(id s, SEL c) { (void)c; ctrack *t = ct_of(s); return mk(t ? track_end(t) : 0); }
static szd ctm_size(id s, SEL c) { (void)c; ctrack *t = ct_of(s); szd z = {0, 0}; return t ? t->nat : z; }
static aff ctm_tf(id s, SEL c) { (void)c; ctrack *t = ct_of(s); aff i = {1, 0, 0, 1, 0, 0}; return t ? t->tf : i; }
static void ctm_set_tf(id s, SEL c, aff v) { (void)c; ctrack *t = ct_of(s); if (t) t->tf = v; }
static BOOL ctm_enabled(id s, SEL c) { (void)c; ctrack *t = ct_of(s); return t && t->enabled; }
static void ctm_set_enabled(id s, SEL c, BOOL v) { (void)c; ctrack *t = ct_of(s); if (t) t->enabled = v; }
static i32 ctm_mts(id s, SEL c) { (void)c; ctrack *t = ct_of(s); return t && t->mts ? t->mts : 600; }
static void ctm_set_mts(id s, SEL c, i32 v) { (void)c; ctrack *t = ct_of(s); if (t) t->mts = v; }
static BOOL ctm_yes(id s, SEL c) { (void)s; (void)c; return YES; }
static id ctm_asset(id s, SEL c) { (void)c; ctrack *t = ct_of(s); return t ? t->comp : 0; }
static id seg_obj(const seg *g) {
    id o = cx_alloc_instance(&AVCompositionTrackSegment_c);
    SO(o)->url = g->url ? objc_retain(g->url) : 0;
    SO(o)->tid = g->src_tid; SO(o)->empty = g->empty;
    SO(o)->src = g->src; SO(o)->dur = g->dur; SO(o)->tgt = g->tgt; SO(o)->tdur = g->tdur;
    return objc_autorelease(o);
}
static id ctm_segments(id s, SEL c) {
    (void)c;
    ctrack *t = ct_of(s);
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (int i = 0; t && i < t->n; i++) MSG(void, a, "addObject:", seg_obj(&t->s[i]));
    return a;
}
static id ctm_segment_at(id s, SEL c, cmt when) {
    (void)c;
    ctrack *t = ct_of(s);
    double w = secs(when);
    for (int i = 0; t && i < t->n; i++) if (w >= t->s[i].tgt && w < t->s[i].tgt + t->s[i].tdur) return seg_obj(&t->s[i]);
    return 0;
}
/* o arquivo de uma faixa de origem (AVAssetTrack de um AVURLAsset) */
static int source_of(id track, char **path, id *url, int *video, i32 *tid, double *dur) {
    if (!track || ct_of(track)) return 0;   /* faixa de composição dentro de composição: não */
    id asset = MSG0(id, track, "asset");
    id u = asset ? MSG0(id, asset, "URL") : 0;
    const char *p = u ? cx_audio_url_path(u) : 0;
    if (!p) return 0;
    const char *m = cx_cstr(MSG0(id, track, "mediaType"));
    *video = m && !strcmp(m, "vide");
    *path = strdup(p);
    *url = objc_retain(u);
    *tid = MSG0(i32, track, "trackID");
    *dur = secs(((cmt (*)(id, SEL))objc_msgSend_stret)(track, sel_registerName("duration")));
    return 1;
}
static BOOL ctm_insert(id s, SEL c, cmrange r, id track, cmt at, id *err) {
    (void)c;
    ctrack *t = ct_of(s);
    seg g = {0};
    double sdur;
    if (!t || !valid(r.start) || !valid(r.duration) || !valid(at) || !source_of(track, &g.path, &g.url, &g.empty, &g.src_tid, &sdur) || g.empty != t->video) {
        if (g.path) { free(g.path); objc_release(g.url); }
        if (err) *err = error_with(-11800, "Faixa de origem incompatível com a faixa da composição (outro tipo de mídia ou sem arquivo).");
        return NO;
    }
    g.empty = 0;
    g.src = secs(r.start); g.dur = secs(r.duration); g.tdur = g.dur;
    if (g.src < 0 || g.dur <= 0 || g.src + g.dur > sdur + 1e-3) {
        seg_free(&g);
        if (err) *err = error_with(-11800, "O intervalo pedido passa da duração da faixa de origem.");
        return NO;
    }
    if (!t->nat.w && t->video) {
        szd z = ((szd (*)(id, SEL))objc_msgSend)(track, sel_registerName("naturalSize"));
        t->nat = z;
        t->tf = ((aff (*)(id, SEL))objc_msgSend_stret)(track, sel_registerName("preferredTransform"));
    }
    if (!insert_seg(t, secs(at), g)) { seg_free(&g); return NO; }
    if (err) *err = 0;
    return YES;
}
static BOOL ctm_insert_many(id s, SEL c, id ranges, id tracks, cmt at, id *err) {
    u64 n = MSG0(u64, ranges, "count");
    double T = secs(at);
    for (u64 i = 0; i < n; i++) {
        id v = MSG(id, ranges, "objectAtIndex:", i);
        cmrange r;
        MSG(void, v, "getValue:", &r);
        if (!ctm_insert(s, c, r, MSG(id, tracks, "objectAtIndex:", i), mk(T), err)) return NO;
        T += secs(r.duration);
    }
    return YES;
}
static void ctm_insert_empty(id s, SEL c, cmrange r) {
    (void)c;
    ctrack *t = ct_of(s);
    if (!t || secs(r.duration) <= 0) return;
    seg g = {0, 0, 0, 1, 0, secs(r.duration), 0, secs(r.duration)};
    insert_seg(t, secs(r.start), g);
}
static void ctm_remove(id s, SEL c, cmrange r) { (void)c; ctrack *t = ct_of(s); if (t) remove_range(t, secs(r.start), secs(r.start) + secs(r.duration)); }
static void ctm_scale(id s, SEL c, cmrange r, cmt d) { (void)c; ctrack *t = ct_of(s); if (t) scale_range(t, secs(r.start), secs(r.start) + secs(r.duration), secs(d)); }
static BOOL ctm_validate(id s, SEL c, id segs, id *err) {
    (void)s; (void)c;
    double end = 0;
    for (u64 i = 0, n = MSG0(u64, segs, "count"); i < n; i++) {
        id o = MSG(id, segs, "objectAtIndex:", i);
        if (SO(o)->tgt < end - 1e-9 || SO(o)->tdur <= 0) { if (err) *err = error_with(-11800, "Trechos fora de ordem, sobrepostos ou vazios."); return NO; }
        end = SO(o)->tgt + SO(o)->tdur;
    }
    if (err) *err = 0;
    return YES;
}
static void ctm_set_segments(id s, SEL c, id segs) {
    ctrack *t = ct_of(s);
    if (!t || !ctm_validate(s, c, segs, 0)) return;
    for (int i = 0; i < t->n; i++) seg_free(&t->s[i]);
    t->n = 0;
    for (u64 i = 0, n = MSG0(u64, segs, "count"); i < n; i++) {
        id o = MSG(id, segs, "objectAtIndex:", i);
        if (!grow(t)) return;
        seg g = {0};
        g.empty = SO(o)->empty; g.src = SO(o)->src; g.dur = SO(o)->dur; g.tgt = SO(o)->tgt; g.tdur = SO(o)->tdur; g.src_tid = SO(o)->tid;
        if (SO(o)->url) { g.url = objc_retain(SO(o)->url); const char *p = cx_audio_url_path(g.url); g.path = p ? strdup(p) : 0; }
        if (!g.empty && !g.path) { seg_free(&g); continue; }
        t->s[t->n++] = g;
    }
}

/* ---------------------------------------------------------------- AVCompositionTrackSegment */
static id sg_make(cx_class_t *k, id url, i32 tid, cmrange src, cmrange tgt, int empty) {
    id o = cx_alloc_instance(k);
    SO(o)->url = url ? objc_retain(url) : 0;
    SO(o)->tid = tid; SO(o)->empty = empty;
    SO(o)->src = secs(src.start); SO(o)->dur = secs(src.duration); SO(o)->tgt = secs(tgt.start); SO(o)->tdur = secs(tgt.duration);
    return o;
}
static id sg_with_url(id k, SEL c, id url, i32 tid, cmrange src, cmrange tgt) { (void)k; (void)c; return objc_autorelease(sg_make(&AVCompositionTrackSegment_c, url, tid, src, tgt, 0)); }
static id sg_with_range(id k, SEL c, cmrange tgt) { (void)k; (void)c; cmrange z = {invalid(), invalid()}; return objc_autorelease(sg_make(&AVCompositionTrackSegment_c, 0, 0, z, tgt, 1)); }
static id sg_init_url(id s, SEL c, id url, i32 tid, cmrange src, cmrange tgt) {
    (void)c;
    SO(s)->url = url ? objc_retain(url) : 0; SO(s)->tid = tid;
    SO(s)->src = secs(src.start); SO(s)->dur = secs(src.duration); SO(s)->tgt = secs(tgt.start); SO(s)->tdur = secs(tgt.duration);
    return s;
}
static id sg_init_range(id s, SEL c, cmrange tgt) { (void)c; SO(s)->empty = 1; SO(s)->tgt = secs(tgt.start); SO(s)->tdur = secs(tgt.duration); return s; }
static id sg_url(id s, SEL c) { (void)c; return SO(s)->url; }
static i32 sg_tid(id s, SEL c) { (void)c; return SO(s)->tid; }
static BOOL sg_empty(id s, SEL c) { (void)c; return SO(s)->empty; }
static cmmapping sg_mapping(id s, SEL c) {
    (void)c;
    cmmapping m;
    m.source.start = SO(s)->empty ? invalid() : mk(SO(s)->src); m.source.duration = SO(s)->empty ? invalid() : mk(SO(s)->dur);
    m.target.start = mk(SO(s)->tgt); m.target.duration = mk(SO(s)->tdur);
    return m;
}
static void sg_dealloc(id s, SEL c) {
    if (SO(s)->url) objc_release(SO(s)->url);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}

/* ---------------------------------------------------------------- AVComposition */
static id cp_tracks_of(id s, int filter) {   /* filter: 0 todas, 1 áudio, 2 vídeo */
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s && (!filter || (filter == 2) == t->video)) MSG(void, a, "insertObject:atIndex:", t->obj, (u64)0);
    return a;
}
static id cp_new(cx_class_t *k) {
    id o = cx_alloc_instance(k);
    comp *c = calloc(1, sizeof *c);
    if (!o || !c) return 0;
    c->obj = o; c->next = g_comps; g_comps = c;
    return o;
}
static id cp_composition(id k, SEL c) { (void)k; (void)c; return objc_autorelease(cp_new(&AVMutableComposition_c)); }
static id cp_composition_opts(id k, SEL c, id opts) { (void)opts; return cp_composition(k, c); }
static id cp_tracks(id s, SEL c) { (void)c; return cp_tracks_of(s, 0); }
static id cp_tracks_media(id s, SEL c, id m) { (void)c; const char *x = m ? cx_cstr(m) : ""; return cp_tracks_of(s, x && !strcmp(x, "vide") ? 2 : x && !strcmp(x, "soun") ? 1 : -1); }
static id cp_track_id(id s, SEL c, i32 tid) { (void)c; for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s && t->tid == tid) return t->obj; return 0; }
static cmt cp_duration(id s, SEL c) { (void)c; double d = 0; for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s && track_end(t) > d) d = track_end(t); return mk(d); }
static szd cp_size(id s, SEL c) {
    (void)c;
    comp *cp = cp_of(s);
    if (cp && cp->nat.w) return cp->nat;
    for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s && t->video && t->nat.w) return t->nat;
    szd z = {0, 0};
    return z;
}
static void cp_set_size(id s, SEL c, szd z) { (void)c; comp *cp = cp_of(s); if (cp) cp->nat = z; }
static BOOL cp_no(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL cp_yes(id s, SEL c) { (void)s; (void)c; return YES; }
static id cp_url(id s, SEL c) { (void)s; (void)c; return 0; }
static id cp_add_track(id s, SEL c, id media, i32 pref) {
    (void)c;
    const char *m = media ? cx_cstr(media) : "";
    if (!m || (strcmp(m, "vide") && strcmp(m, "soun"))) return 0;   /* texto/legenda: não */
    i32 tid = pref > 0 && !cp_track_id(s, 0, pref) ? pref : 1;
    while (cp_track_id(s, 0, tid)) tid++;
    return ct_new(s, !strcmp(m, "vide"), tid, 1);
}
static void cp_remove_track(id s, SEL c, id trk) {
    (void)c;
    for (ctrack **pp = &g_tracks; *pp; pp = &(*pp)->next)
        if ((*pp)->obj == trk && (*pp)->comp == s) { ctrack *t = *pp; *pp = t->next; for (int i = 0; i < t->n; i++) seg_free(&t->s[i]); free(t->s); free(t); return; }
}
static id cp_compatible(id s, SEL c, id trk) {
    (void)c;
    const char *m = trk ? cx_cstr(MSG0(id, trk, "mediaType")) : 0;
    for (ctrack *t = g_tracks; t && m; t = t->next) if (t->comp == s && t->video == !strcmp(m, "vide")) return t->obj;
    return 0;
}
static BOOL cp_insert_asset(id s, SEL c, cmrange r, id asset, cmt at, id *err) {
    (void)c;
    id tracks = asset ? MSG0(id, asset, "tracks") : 0;
    u64 n = tracks ? MSG0(u64, tracks, "count") : 0;
    if (!n) { if (err) *err = error_with(-11800, "O asset não tem faixas."); return NO; }
    double T = secs(at), dur = secs(r.duration);
    /* as faixas da composição são as compatíveis (uma por tipo); as que faltam são criadas */
    for (u64 i = 0; i < n; i++) {
        id src = MSG(id, tracks, "objectAtIndex:", i);
        id dst = cp_compatible(s, 0, src);
        if (!dst) dst = cp_add_track(s, 0, MSG0(id, src, "mediaType"), 0);
        if (!dst) continue;
        ctrack *t = ct_of(dst);
        double sd = secs(((cmt (*)(id, SEL))objc_msgSend_stret)(src, sel_registerName("duration")));
        cmrange rr = {r.start, mk(dur < sd - secs(r.start) ? dur : sd - secs(r.start))};
        if (secs(rr.duration) <= 0) continue;
        if (t && T > track_end(t) + 1e-12) ctm_insert_empty(dst, 0, (cmrange){mk(track_end(t)), mk(T - track_end(t))});
        if (!ctm_insert(dst, 0, rr, src, mk(T), err)) return NO;
    }
    /* as outras faixas da composição ganham o espaço vazio correspondente (o resto anda junto) */
    for (ctrack *t = g_tracks; t; t = t->next) {
        if (t->comp != s) continue;
        int touched = 0;
        for (u64 i = 0; i < n; i++) if (cp_compatible(s, 0, MSG(id, tracks, "objectAtIndex:", i)) == t->obj) touched = 1;
        if (!touched && T < track_end(t)) ctm_insert_empty(t->obj, 0, (cmrange){mk(T), mk(dur)});
    }
    if (err) *err = 0;
    return YES;
}
static void cp_insert_empty(id s, SEL c, cmrange r) { (void)c; for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s) ctm_insert_empty(t->obj, 0, r); }
static void cp_remove(id s, SEL c, cmrange r) { (void)c; for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s) ctm_remove(t->obj, 0, r); }
static void cp_scale(id s, SEL c, cmrange r, cmt d) { (void)c; for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == s) ctm_scale(t->obj, 0, r, d); }
static id cp_copy(id s, SEL c, void *z) { (void)c; (void)z; return objc_retain(s); }   /* a cópia compartilha (imutável na prática) */

/* ---------------------------------------------------------------- exportação (avexport_objc.c) */
int cx_avcomposition_is(id a) { return a && cp_of(a) != 0; }
int cx_avcomposition_info(id a, int *has_video, int *has_audio, double *dur) {
    int v = 0, au = 0;
    double d = 0;
    for (ctrack *t = g_tracks; t; t = t->next) if (t->comp == a && t->enabled && t->n) { if (t->video) v = 1; else au = 1; if (track_end(t) > d) d = track_end(t); }
    if (has_video) *has_video = v;
    if (has_audio) *has_audio = au;
    if (dur) *dur = d;
    return cp_of(a) ? 0 : -1;
}
int avx_track_index(const char *path, int media);
/* exporta [start, start + dur) da composição (dur < 0: tudo) */
int cx_avcomposition_export(id a, const char *out, int container, int fast, double start, double dur, int want, avx_ctl *ctl, const char **why) {
    avx_ctrack ct[16];
    avx_segment *all = 0;
    int nct = 0, nall = 0, cap = 0, rc = 0;
    double end = dur < 0 ? 1e300 : start + dur;
    for (ctrack *t = g_tracks; t && nct < 16; t = t->next) {
        if (t->comp != a || !t->enabled || !t->n) continue;
        int media = t->video ? AVX_VIDEO : AVX_AUDIO;
        if (!(want & media) || (container == MP4W_M4A && t->video)) continue;
        int first = nall;
        for (int i = 0; i < t->n; i++) {
            seg *g = &t->s[i];
            if (g->empty) continue;
            if (g->tdur > 0 && (g->tdur < g->dur - 1e-6 || g->tdur > g->dur + 1e-6)) { *why = "Trecho com o tempo escalado: exportar exige recodificar, o que o mOS ainda não faz."; rc = AVX_ERR_CODEC; goto done; }
            double a0 = g->tgt > start ? g->tgt : start, b0 = g->tgt + g->tdur < end ? g->tgt + g->tdur : end;
            if (b0 <= a0) continue;
            if (nall == cap) { cap = cap ? cap * 2 : 32; avx_segment *x = realloc(all, (size_t)cap * sizeof *x); if (!x) { rc = AVX_ERR_WRITE; goto done; } all = x; }
            int idx = avx_track_index(g->path, media);
            if (idx < 0) { *why = "Arquivo de origem da composição ilegível."; rc = AVX_ERR_OPEN; goto done; }
            all[nall++] = (avx_segment){g->path, idx, g->src + (a0 - g->tgt), b0 - a0, a0 - start};
        }
        if (nall > first) { ct[nct].media = media; ct[nct].seg = 0; ct[nct].nseg = nall - first; nct++; }
    }
    for (int k = 0, off = 0; k < nct; k++) { ct[k].seg = all + off; off += ct[k].nseg; }   /* depois do realloc */
    if (!nct) { *why = "Nada para exportar na composição."; rc = AVX_ERR_EMPTY; goto done; }
    rc = avx_compose(out, container, fast, ct, nct, ctl, why);
done:
    free(all);
    return rc;
}

void cx_avcomposition_init(void) {
    cx_class_t *asset = cx_class_named("AVAsset"), *track = cx_class_named("AVAssetTrack"), *ob = cx_class_named("NSObject");
    if (!asset || !track) return;
#define DEF(k, n, sup) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, sup, 0)
    DEF(AVComposition, "AVComposition", asset);
    DEF(AVMutableComposition, "AVMutableComposition", &AVComposition_c);
    DEF(AVCompositionTrack, "AVCompositionTrack", track);
    DEF(AVMutableCompositionTrack, "AVMutableCompositionTrack", &AVCompositionTrack_c);
#undef DEF
    if (!cx_class_named("AVCompositionTrackSegment"))
        cx_define_class(&AVCompositionTrackSegment_c, &AVCompositionTrackSegment_m, "AVCompositionTrackSegment", ob, (u32)(sizeof(segobj_t) - 8));
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
    CM(AVMutableComposition, "composition", cp_composition); CM(AVMutableComposition, "compositionWithURLAssetInitializationOptions:", cp_composition_opts);
    M(AVComposition, "tracks", cp_tracks); M(AVComposition, "tracksWithMediaType:", cp_tracks_media); M(AVComposition, "trackWithTrackID:", cp_track_id);
    M(AVComposition, "duration", cp_duration); M(AVComposition, "naturalSize", cp_size); M(AVComposition, "URL", cp_url);
    M(AVComposition, "isPlayable", cp_no); M(AVComposition, "isExportable", cp_yes); M(AVComposition, "isComposable", cp_yes); M(AVComposition, "isReadable", cp_yes);
    M(AVComposition, "copyWithZone:", cp_copy); M(AVComposition, "mutableCopyWithZone:", cp_copy);
    M(AVMutableComposition, "setNaturalSize:", cp_set_size);
    M(AVMutableComposition, "addMutableTrackWithMediaType:preferredTrackID:", cp_add_track);
    M(AVMutableComposition, "removeTrack:", cp_remove_track); M(AVMutableComposition, "mutableTrackCompatibleWithTrack:", cp_compatible);
    M(AVMutableComposition, "insertTimeRange:ofAsset:atTime:error:", cp_insert_asset);
    M(AVMutableComposition, "insertEmptyTimeRange:", cp_insert_empty); M(AVMutableComposition, "removeTimeRange:", cp_remove);
    M(AVMutableComposition, "scaleTimeRange:toDuration:", cp_scale);
    M(AVCompositionTrack, "mediaType", ctm_media); M(AVCompositionTrack, "trackID", ctm_tid); M(AVCompositionTrack, "timeRange", ctm_range);
    M(AVCompositionTrack, "duration", ctm_duration); M(AVCompositionTrack, "naturalSize", ctm_size); M(AVCompositionTrack, "preferredTransform", ctm_tf);
    M(AVCompositionTrack, "isEnabled", ctm_enabled); M(AVCompositionTrack, "naturalTimeScale", ctm_mts); M(AVCompositionTrack, "segments", ctm_segments);
    M(AVCompositionTrack, "segmentForTrackTime:", ctm_segment_at); M(AVCompositionTrack, "isPlayable", ctm_yes); M(AVCompositionTrack, "asset", ctm_asset);
    M(AVMutableCompositionTrack, "setPreferredTransform:", ctm_set_tf); M(AVMutableCompositionTrack, "setEnabled:", ctm_set_enabled);
    M(AVMutableCompositionTrack, "setNaturalTimeScale:", ctm_set_mts); M(AVMutableCompositionTrack, "setSegments:", ctm_set_segments);
    M(AVMutableCompositionTrack, "insertTimeRange:ofTrack:atTime:error:", ctm_insert);
    M(AVMutableCompositionTrack, "insertTimeRanges:ofTracks:atTime:error:", ctm_insert_many);
    M(AVMutableCompositionTrack, "insertEmptyTimeRange:", ctm_insert_empty); M(AVMutableCompositionTrack, "removeTimeRange:", ctm_remove);
    M(AVMutableCompositionTrack, "scaleTimeRange:toDuration:", ctm_scale); M(AVMutableCompositionTrack, "validateTrackSegments:error:", ctm_validate);
    CM(AVCompositionTrackSegment, "compositionTrackSegmentWithURL:trackID:sourceTimeRange:targetTimeRange:", sg_with_url);
    CM(AVCompositionTrackSegment, "compositionTrackSegmentWithTimeRange:", sg_with_range);
    M(AVCompositionTrackSegment, "initWithURL:trackID:sourceTimeRange:targetTimeRange:", sg_init_url);
    M(AVCompositionTrackSegment, "initWithTimeRange:", sg_init_range);
    M(AVCompositionTrackSegment, "sourceURL", sg_url); M(AVCompositionTrackSegment, "sourceTrackID", sg_tid);
    M(AVCompositionTrackSegment, "isEmpty", sg_empty); M(AVCompositionTrackSegment, "timeMapping", sg_mapping);
    M(AVCompositionTrackSegment, "dealloc", sg_dealloc);
#undef M
#undef CM
}
