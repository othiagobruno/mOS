/*
 * mOS — MediaPlayer: MPNowPlayingInfoCenter, MPRemoteCommandCenter, MPRemoteCommand (e as subclasses de taxa, salto,
 * posição...), os eventos de comando, MPMediaItemArtwork e MPVolumeView.
 * O app registra handlers (bloco devolvendo MPRemoteCommandHandlerStatus, ou alvo/ação) e publica o "tocando agora".
 * Quem recebe a tecla de mídia ou o controle do sistema chama cx_mp_remote_command(); a área de trabalho lê o estado por
 * cx_mp_now_playing(). Código original do mOS (MIT).
 */
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *realloc(void *p, size_t n);
void free(void *p);
int strcmp(const char *a, const char *b);
double cx_now(void);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
#define BLK_FN(b) (((void **)(b))[2])

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } mpc_t;
#define SAME(var) static mpc_t var##_s = {cx_cfconst_isa, 0x7c8, #var, sizeof(#var) - 1}; const void *var = &var##_s;
SAME(MPNowPlayingInfoPropertyMediaType) SAME(MPNowPlayingInfoPropertyAssetURL) SAME(MPNowPlayingInfoPropertyChapterCount)
SAME(MPNowPlayingInfoPropertyChapterNumber) SAME(MPNowPlayingInfoPropertyDefaultPlaybackRate) SAME(MPNowPlayingInfoPropertyPlaybackQueueCount)
SAME(MPNowPlayingInfoPropertyPlaybackQueueIndex)

/* comandos: o índice é o tipo usado por cx_mp_remote_command */
enum { C_PLAY, C_PAUSE, C_STOP, C_TOGGLE, C_NEXT, C_PREV, C_RATE, C_SKIP_FWD, C_SKIP_BACK, C_POSITION, C_SEEK_FWD, C_SEEK_BACK,
       C_REPEAT, C_SHUFFLE, C_LIKE, C_DISLIKE, C_BOOKMARK, C_LANG_ON, C_LANG_OFF, C_COUNT };
static const char *const k_cmd_sel[C_COUNT] = {"playCommand", "pauseCommand", "stopCommand", "togglePlayPauseCommand", "nextTrackCommand",
    "previousTrackCommand", "changePlaybackRateCommand", "skipForwardCommand", "skipBackwardCommand", "changePlaybackPositionCommand",
    "seekForwardCommand", "seekBackwardCommand", "changeRepeatModeCommand", "changeShuffleModeCommand", "likeCommand", "dislikeCommand",
    "bookmarkCommand", "enableLanguageOptionCommand", "disableLanguageOptionCommand"};
typedef struct { id target; SEL action; void *block; } mp_target;
typedef struct {
    cx_class_t *isa;
    int kind;
    BOOL enabled;
    mp_target *t;
    u32 nt, ct;
    id rates, intervals, title;   /* supportedPlaybackRates / preferredIntervals / localizedTitle */
    i64 mode;                     /* repeat/shuffle atuais */
    BOOL active;                  /* like/dislike/bookmark */
} mpcmd_t;
typedef struct { cx_class_t *isa; id command; double ts, value; i64 type; } mpev_t;
typedef struct { cx_class_t *isa; double bw, bh; void *handler; id image; } mpart_t;
#define CMD(o) ((mpcmd_t *)(o))
#define EV(o) ((mpev_t *)(o))
#define ART(o) ((mpart_t *)(o))
cx_class_t MPRemoteCommand_c __asm__("OBJC_CLASS_$_MPRemoteCommand"), MPRemoteCommand_m __asm__("OBJC_METACLASS_$_MPRemoteCommand");
cx_class_t MPChangePlaybackRateCommand_c __asm__("OBJC_CLASS_$_MPChangePlaybackRateCommand"), MPChangePlaybackRateCommand_m __asm__("OBJC_METACLASS_$_MPChangePlaybackRateCommand");
cx_class_t MPSkipIntervalCommand_c __asm__("OBJC_CLASS_$_MPSkipIntervalCommand"), MPSkipIntervalCommand_m __asm__("OBJC_METACLASS_$_MPSkipIntervalCommand");
cx_class_t MPChangePlaybackPositionCommand_c __asm__("OBJC_CLASS_$_MPChangePlaybackPositionCommand"), MPChangePlaybackPositionCommand_m __asm__("OBJC_METACLASS_$_MPChangePlaybackPositionCommand");
cx_class_t MPChangeRepeatModeCommand_c __asm__("OBJC_CLASS_$_MPChangeRepeatModeCommand"), MPChangeRepeatModeCommand_m __asm__("OBJC_METACLASS_$_MPChangeRepeatModeCommand");
cx_class_t MPChangeShuffleModeCommand_c __asm__("OBJC_CLASS_$_MPChangeShuffleModeCommand"), MPChangeShuffleModeCommand_m __asm__("OBJC_METACLASS_$_MPChangeShuffleModeCommand");
cx_class_t MPFeedbackCommand_c __asm__("OBJC_CLASS_$_MPFeedbackCommand"), MPFeedbackCommand_m __asm__("OBJC_METACLASS_$_MPFeedbackCommand");
cx_class_t MPRemoteCommandEvent_c __asm__("OBJC_CLASS_$_MPRemoteCommandEvent"), MPRemoteCommandEvent_m __asm__("OBJC_METACLASS_$_MPRemoteCommandEvent");
cx_class_t MPChangePlaybackPositionCommandEvent_c __asm__("OBJC_CLASS_$_MPChangePlaybackPositionCommandEvent"), MPChangePlaybackPositionCommandEvent_m __asm__("OBJC_METACLASS_$_MPChangePlaybackPositionCommandEvent");
cx_class_t MPChangePlaybackRateCommandEvent_c __asm__("OBJC_CLASS_$_MPChangePlaybackRateCommandEvent"), MPChangePlaybackRateCommandEvent_m __asm__("OBJC_METACLASS_$_MPChangePlaybackRateCommandEvent");
cx_class_t MPSkipIntervalCommandEvent_c __asm__("OBJC_CLASS_$_MPSkipIntervalCommandEvent"), MPSkipIntervalCommandEvent_m __asm__("OBJC_METACLASS_$_MPSkipIntervalCommandEvent");
cx_class_t MPSeekCommandEvent_c __asm__("OBJC_CLASS_$_MPSeekCommandEvent"), MPSeekCommandEvent_m __asm__("OBJC_METACLASS_$_MPSeekCommandEvent");
cx_class_t MPChangeRepeatModeCommandEvent_c __asm__("OBJC_CLASS_$_MPChangeRepeatModeCommandEvent"), MPChangeRepeatModeCommandEvent_m __asm__("OBJC_METACLASS_$_MPChangeRepeatModeCommandEvent");
cx_class_t MPChangeShuffleModeCommandEvent_c __asm__("OBJC_CLASS_$_MPChangeShuffleModeCommandEvent"), MPChangeShuffleModeCommandEvent_m __asm__("OBJC_METACLASS_$_MPChangeShuffleModeCommandEvent");
cx_class_t MPVolumeView_c __asm__("OBJC_CLASS_$_MPVolumeView"), MPVolumeView_m __asm__("OBJC_METACLASS_$_MPVolumeView");
/* as três abaixo moram em vscode_fwclasses.c (casca); este init roda antes e as define com o tamanho certo */
extern cx_class_t MPNowPlayingInfoCenter_c __asm__("OBJC_CLASS_$_MPNowPlayingInfoCenter"), MPNowPlayingInfoCenter_m __asm__("OBJC_METACLASS_$_MPNowPlayingInfoCenter");
extern cx_class_t MPRemoteCommandCenter_c __asm__("OBJC_CLASS_$_MPRemoteCommandCenter"), MPRemoteCommandCenter_m __asm__("OBJC_METACLASS_$_MPRemoteCommandCenter");
extern cx_class_t MPMediaItemArtwork_c __asm__("OBJC_CLASS_$_MPMediaItemArtwork"), MPMediaItemArtwork_m __asm__("OBJC_METACLASS_$_MPMediaItemArtwork");

static id g_cmds[C_COUNT];
static id g_center, g_info_center, g_info;
static i64 g_state;   /* MPNowPlayingPlaybackState: 0 desconhecido, 1 tocando, 2 pausado, 3 parado, 4 interrompido */
static id cls(const char *n) { return (id)cx_class_named(n); }
static void setobj(id *s, id v) { if (*s == v) return; if (v) objc_retain(v); if (*s) objc_release(*s); *s = v; }

/* ---------------------------------------------------------------- MPRemoteCommand */
static cx_class_t *cmd_class(int k) {
    switch (k) {
    case C_RATE: return &MPChangePlaybackRateCommand_c;
    case C_SKIP_FWD: case C_SKIP_BACK: return &MPSkipIntervalCommand_c;
    case C_POSITION: return &MPChangePlaybackPositionCommand_c;
    case C_REPEAT: return &MPChangeRepeatModeCommand_c;
    case C_SHUFFLE: return &MPChangeShuffleModeCommand_c;
    case C_LIKE: case C_DISLIKE: case C_BOOKMARK: return &MPFeedbackCommand_c;
    default: return &MPRemoteCommand_c;
    }
}
static id cmd_get(int k) {
    if (!g_cmds[k]) {
        id c = cx_alloc_instance(cmd_class(k));
        CMD(c)->kind = k;
        CMD(c)->enabled = YES;
        g_cmds[k] = c;
    }
    return g_cmds[k];
}
static int add_target(id s, id target, SEL act, void *blk) {
    mpcmd_t *c = CMD(s);
    if (c->nt == c->ct) {
        u32 nc = c->ct ? c->ct * 2 : 4;
        mp_target *t = realloc(c->t, nc * sizeof *t);
        if (!t) return 0;
        c->t = t; c->ct = nc;
    }
    c->t[c->nt].target = target;   /* o alvo não é retido, como na Apple */
    c->t[c->nt].action = act;
    c->t[c->nt].block = blk;
    c->nt++;
    return 1;
}
static void cmd_add_target(id s, SEL c, id target, SEL act) { (void)c; if (target && act) add_target(s, target, act, 0); }
/* o objeto devolvido identifica o handler em removeTarget: */
static id cmd_add_handler(id s, SEL c, void *blk) {
    (void)c;
    if (!blk) return 0;
    void *b = _Block_copy(blk);
    id token = MSG0(id, MSG0(id, cls("NSObject"), "alloc"), "init");
    if (!add_target(s, token, 0, b)) { _Block_release(b); return 0; }
    return token;
}
static void cmd_remove(id s, SEL c, id target) {
    (void)c;
    mpcmd_t *m = CMD(s);
    for (u32 i = 0; i < m->nt;) {
        if (!target || m->t[i].target == target) { if (m->t[i].block) _Block_release(m->t[i].block); m->t[i] = m->t[--m->nt]; }
        else i++;
    }
}
static void cmd_remove_action(id s, SEL c, id target, SEL act) {
    (void)c;
    mpcmd_t *m = CMD(s);
    for (u32 i = 0; i < m->nt;) {
        if (m->t[i].target == target && (!act || m->t[i].action == act)) m->t[i] = m->t[--m->nt];
        else i++;
    }
}
static BOOL cmd_enabled(id s, SEL c) { (void)c; return CMD(s)->enabled; }
static void cmd_set_enabled(id s, SEL c, BOOL v) { (void)c; CMD(s)->enabled = v; }
static id cmd_rates(id s, SEL c) { (void)c; return CMD(s)->rates; }
static void cmd_set_rates(id s, SEL c, id v) { (void)c; setobj(&CMD(s)->rates, v); }
static id cmd_intervals(id s, SEL c) { (void)c; return CMD(s)->intervals; }
static void cmd_set_intervals(id s, SEL c, id v) { (void)c; setobj(&CMD(s)->intervals, v); }
static i64 cmd_mode(id s, SEL c) { (void)c; return CMD(s)->mode; }
static void cmd_set_mode(id s, SEL c, i64 v) { (void)c; CMD(s)->mode = v; }
static BOOL cmd_active(id s, SEL c) { (void)c; return CMD(s)->active; }
static void cmd_set_active(id s, SEL c, BOOL v) { (void)c; CMD(s)->active = v; }
static id cmd_title(id s, SEL c) { (void)c; return CMD(s)->title; }
static void cmd_set_title(id s, SEL c, id v) { (void)c; setobj(&CMD(s)->title, v); }

/* ---------------------------------------------------------------- eventos */
static id ev_command(id s, SEL c) { (void)c; return EV(s)->command; }
static double ev_ts(id s, SEL c) { (void)c; return EV(s)->ts; }
static double ev_value(id s, SEL c) { (void)c; return EV(s)->value; }
static float ev_value_f(id s, SEL c) { (void)c; return (float)EV(s)->value; }
static i64 ev_type(id s, SEL c) { (void)c; return EV(s)->type; }
static BOOL ev_should_negate(id s, SEL c) { (void)s; (void)c; return NO; }

/* despacha um comando: devolve o MPRemoteCommandHandlerStatus do último handler (0 sucesso, 100 sem conteúdo,
 * 120 sem ação, 200 falha; 120 também quando não há handler ou o comando está desligado) */
long cx_mp_remote_command(int kind, double value, long type) {
    if (kind < 0 || kind >= C_COUNT) return 200;
    id c = cmd_get(kind);
    mpcmd_t *m = CMD(c);
    if (!m->enabled || !m->nt) return 120;
    cx_class_t *ec = &MPRemoteCommandEvent_c;
    if (kind == C_POSITION) ec = &MPChangePlaybackPositionCommandEvent_c;
    else if (kind == C_RATE) ec = &MPChangePlaybackRateCommandEvent_c;
    else if (kind == C_SKIP_FWD || kind == C_SKIP_BACK) ec = &MPSkipIntervalCommandEvent_c;
    else if (kind == C_SEEK_FWD || kind == C_SEEK_BACK) ec = &MPSeekCommandEvent_c;
    else if (kind == C_REPEAT) ec = &MPChangeRepeatModeCommandEvent_c;
    else if (kind == C_SHUFFLE) ec = &MPChangeShuffleModeCommandEvent_c;
    id ev = cx_alloc_instance(ec);
    EV(ev)->command = c;
    EV(ev)->ts = cx_now();
    EV(ev)->value = value;
    EV(ev)->type = type;
    if (kind == C_REPEAT || kind == C_SHUFFLE) m->mode = (i64)value;
    long st = 120;
    u32 n = m->nt;
    mp_target copy[16];
    if (n > 16) n = 16;
    for (u32 i = 0; i < n; i++) copy[i] = m->t[i];
    for (u32 i = 0; i < n; i++) {
        if (copy[i].block) st = ((long (*)(void *, id))BLK_FN(copy[i].block))(copy[i].block, ev);
        else st = MSG(long, copy[i].target, (const char *)copy[i].action, ev);
    }
    objc_release(ev);
    return st;
}

/* ---------------------------------------------------------------- centros */
static id rcc_shared(id k, SEL c) {
    (void)k; (void)c;
    if (!g_center) g_center = cx_alloc_instance((cx_class_t *)cx_class_named("MPRemoteCommandCenter"));
    return g_center;
}
static id rcc_command(id s, SEL c) {
    (void)s;
    for (int i = 0; i < C_COUNT; i++) if (!strcmp(k_cmd_sel[i], c)) return cmd_get(i);
    return 0;
}
static id npc_default(id k, SEL c) {
    (void)k; (void)c;
    if (!g_info_center) g_info_center = cx_alloc_instance((cx_class_t *)cx_class_named("MPNowPlayingInfoCenter"));
    return g_info_center;
}
static id npc_info(id s, SEL c) { (void)s; (void)c; return g_info; }
static void npc_set_info(id s, SEL c, id v) { (void)s; (void)c; id cp = v ? MSG0(id, v, "copy") : 0; setobj(&g_info, cp); if (cp) objc_release(cp); }
static i64 npc_state(id s, SEL c) { (void)s; (void)c; return g_state; }
static void npc_set_state(id s, SEL c, i64 v) { (void)s; (void)c; g_state = v; }
/* para a área de trabalho: estado e o dicionário publicado pelo app (sem retenção extra) */
id cx_mp_now_playing(i64 *state) { if (state) *state = g_state; return g_info; }

/* ---------------------------------------------------------------- MPMediaItemArtwork */
typedef struct { double w, h; } sz_t;
typedef struct { double x, y, w, h; } rc_t;
static id art_init_handler(id s, SEL c, sz_t size, void *blk) { (void)c; ART(s)->bw = size.w; ART(s)->bh = size.h; ART(s)->handler = blk ? _Block_copy(blk) : 0; return s; }
static id art_init_image(id s, SEL c, id img) {
    (void)c;
    setobj(&ART(s)->image, img);
    if (img) { sz_t z = ((sz_t (*)(id, SEL))objc_msgSend)(img, sel_registerName("size")); ART(s)->bw = z.w; ART(s)->bh = z.h; }
    return s;
}
static id art_image(id s, SEL c, sz_t size) {
    (void)c;
    if (ART(s)->handler) return ((id (*)(void *, sz_t))BLK_FN(ART(s)->handler))(ART(s)->handler, size);
    return ART(s)->image;
}
static rc_t art_bounds(id s, SEL c) { (void)c; rc_t r = {0, 0, ART(s)->bw, ART(s)->bh}; return r; }
static void art_dealloc(id s, SEL c) {
    if (ART(s)->handler) _Block_release(ART(s)->handler);
    setobj(&ART(s)->image, 0);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
/* MPVolumeView: no Mac é uma NSView sem controles próprios (o volume do sistema fica na barra de menus) */
static BOOL vv_yes(id s, SEL c) { (void)s; (void)c; return YES; }
static BOOL vv_no(id s, SEL c) { (void)s; (void)c; return NO; }
static void vv_set(id s, SEL c, BOOL v) { (void)s; (void)c; (void)v; }

void cx_mediaplayer_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
#define DEF(k, n, sup, sz) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, sup, (u32)(sz))
    DEF(MPNowPlayingInfoCenter, "MPNowPlayingInfoCenter", ob, 0);
    DEF(MPRemoteCommandCenter, "MPRemoteCommandCenter", ob, 0);
    DEF(MPMediaItemArtwork, "MPMediaItemArtwork", ob, sizeof(mpart_t) - 8);
    DEF(MPRemoteCommand, "MPRemoteCommand", ob, sizeof(mpcmd_t) - 8);
    DEF(MPChangePlaybackRateCommand, "MPChangePlaybackRateCommand", &MPRemoteCommand_c, 0);
    DEF(MPSkipIntervalCommand, "MPSkipIntervalCommand", &MPRemoteCommand_c, 0);
    DEF(MPChangePlaybackPositionCommand, "MPChangePlaybackPositionCommand", &MPRemoteCommand_c, 0);
    DEF(MPChangeRepeatModeCommand, "MPChangeRepeatModeCommand", &MPRemoteCommand_c, 0);
    DEF(MPChangeShuffleModeCommand, "MPChangeShuffleModeCommand", &MPRemoteCommand_c, 0);
    DEF(MPFeedbackCommand, "MPFeedbackCommand", &MPRemoteCommand_c, 0);
    DEF(MPRemoteCommandEvent, "MPRemoteCommandEvent", ob, sizeof(mpev_t) - 8);
    DEF(MPChangePlaybackPositionCommandEvent, "MPChangePlaybackPositionCommandEvent", &MPRemoteCommandEvent_c, 0);
    DEF(MPChangePlaybackRateCommandEvent, "MPChangePlaybackRateCommandEvent", &MPRemoteCommandEvent_c, 0);
    DEF(MPSkipIntervalCommandEvent, "MPSkipIntervalCommandEvent", &MPRemoteCommandEvent_c, 0);
    DEF(MPSeekCommandEvent, "MPSeekCommandEvent", &MPRemoteCommandEvent_c, 0);
    DEF(MPChangeRepeatModeCommandEvent, "MPChangeRepeatModeCommandEvent", &MPRemoteCommandEvent_c, 0);
    DEF(MPChangeShuffleModeCommandEvent, "MPChangeShuffleModeCommandEvent", &MPRemoteCommandEvent_c, 0);
    cx_class_t *nsview = cx_class_named("NSView");
    if (nsview) DEF(MPVolumeView, "MPVolumeView", nsview, 0);
#undef DEF
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
    M(MPRemoteCommand, "addTarget:action:", cmd_add_target); M(MPRemoteCommand, "addTargetWithHandler:", cmd_add_handler);
    M(MPRemoteCommand, "removeTarget:", cmd_remove); M(MPRemoteCommand, "removeTarget:action:", cmd_remove_action);
    M(MPRemoteCommand, "isEnabled", cmd_enabled); M(MPRemoteCommand, "setEnabled:", cmd_set_enabled);
    M(MPChangePlaybackRateCommand, "supportedPlaybackRates", cmd_rates); M(MPChangePlaybackRateCommand, "setSupportedPlaybackRates:", cmd_set_rates);
    M(MPSkipIntervalCommand, "preferredIntervals", cmd_intervals); M(MPSkipIntervalCommand, "setPreferredIntervals:", cmd_set_intervals);
    M(MPChangeRepeatModeCommand, "currentRepeatType", cmd_mode); M(MPChangeRepeatModeCommand, "setCurrentRepeatType:", cmd_set_mode);
    M(MPChangeShuffleModeCommand, "currentShuffleType", cmd_mode); M(MPChangeShuffleModeCommand, "setCurrentShuffleType:", cmd_set_mode);
    M(MPFeedbackCommand, "isActive", cmd_active); M(MPFeedbackCommand, "setActive:", cmd_set_active);
    M(MPFeedbackCommand, "localizedTitle", cmd_title); M(MPFeedbackCommand, "setLocalizedTitle:", cmd_set_title);
    M(MPRemoteCommandEvent, "command", ev_command); M(MPRemoteCommandEvent, "timestamp", ev_ts);
    M(MPChangePlaybackPositionCommandEvent, "positionTime", ev_value);
    M(MPChangePlaybackRateCommandEvent, "playbackRate", ev_value_f);
    M(MPSkipIntervalCommandEvent, "interval", ev_value);
    M(MPSeekCommandEvent, "type", ev_type);
    M(MPChangeRepeatModeCommandEvent, "repeatType", ev_type); M(MPChangeRepeatModeCommandEvent, "preservesRepeatMode", ev_should_negate);
    M(MPChangeShuffleModeCommandEvent, "shuffleType", ev_type); M(MPChangeShuffleModeCommandEvent, "preservesShuffleMode", ev_should_negate);
#undef M
    cx_class_t *rcc = cx_class_named("MPRemoteCommandCenter");
    if (rcc) {
        cx_add_cmethod(rcc, "sharedCommandCenter", (void *)rcc_shared);
        for (int i = 0; i < C_COUNT; i++) cx_add_method(rcc, k_cmd_sel[i], (void *)rcc_command);
    }
    cx_class_t *npc = cx_class_named("MPNowPlayingInfoCenter");
    if (npc) {
        cx_add_cmethod(npc, "defaultCenter", (void *)npc_default);
        cx_add_method(npc, "nowPlayingInfo", (void *)npc_info); cx_add_method(npc, "setNowPlayingInfo:", (void *)npc_set_info);
        cx_add_method(npc, "playbackState", (void *)npc_state); cx_add_method(npc, "setPlaybackState:", (void *)npc_set_state);
    }
    cx_class_t *art = cx_class_named("MPMediaItemArtwork");
    if (art) {
        cx_add_method(art, "initWithBoundsSize:requestHandler:", (void *)art_init_handler);
        cx_add_method(art, "initWithImage:", (void *)art_init_image);
        cx_add_method(art, "imageWithSize:", (void *)art_image);
        cx_add_method(art, "bounds", (void *)art_bounds);
        cx_add_method(art, "dealloc", (void *)art_dealloc);
    }
    if (nsview) {
        cx_add_method(&MPVolumeView_c, "showsVolumeSlider", (void *)vv_yes); cx_add_method(&MPVolumeView_c, "setShowsVolumeSlider:", (void *)vv_set);
        cx_add_method(&MPVolumeView_c, "showsRouteButton", (void *)vv_no); cx_add_method(&MPVolumeView_c, "setShowsRouteButton:", (void *)vv_set);
        cx_add_method(&MPVolumeView_c, "areWirelessRoutesAvailable", (void *)vv_no); cx_add_method(&MPVolumeView_c, "isWirelessRouteActive", (void *)vv_no);
    }
}
