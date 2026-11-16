/*
 * mOS libSystem — Speech.framework.
 *
 * O mOS não tem modelo de reconhecimento de fala (nem no aparelho nem num servidor). O framework se comporta como num Mac
 * em que o serviço não está disponível: a autorização passa pelo TCC (kTCCServiceSpeechRecognition, com o diálogo do
 * sistema), supportedLocales vem vazio, o SFSpeechRecognizer existe mas isAvailable é NO, e uma tarefa termina logo, no
 * resultHandler/delegate, com erro e sem resultado. Os pedidos (buffer de áudio e URL), resultados, transcrições e
 * segmentos guardam o que recebem, para o app que os monta à mão. Código original do mOS (MIT).
 */
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *dispatch_get_main_queue(void);
void *dispatch_get_global_queue(long prio, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *calloc(size_t n, size_t m);
void free(void *p);
int cx_tcc_preflight(const char *service);
void cx_tcc_request_async(const char *service, void (*cb)(int granted, void *ctx), void *ctx);
#define BLK_FN(b) (((void **)(b))[2])

#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n), n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(SFSpeechRecognizer) CLS(SFSpeechRecognitionRequest) CLS(SFSpeechAudioBufferRecognitionRequest) CLS(SFSpeechURLRecognitionRequest)
CLS(SFSpeechRecognitionTask) CLS(SFSpeechRecognitionResult) CLS(SFTranscription) CLS(SFTranscriptionSegment)

typedef struct { cx_class_t *isa; id locale, delegate; void *queue; i64 task_hint; BOOL on_device; } rec_t;
typedef struct { cx_class_t *isa; id url, context; i64 hint; BOOL partial, on_device, punct; id native_fmt; u64 appended; int ended; } req_t;
typedef struct { cx_class_t *isa; i64 state; BOOL cancelled, finishing; id error; } task_t;
typedef struct { cx_class_t *isa; id best, all; BOOL final; } res_t;
typedef struct { cx_class_t *isa; id text, segments; } tr_t;
typedef struct { cx_class_t *isa; id text, alts; double ts, dur; float conf; u64 loc, len; } seg_t;
#define REC(o) ((rec_t *)(o))
#define RQ(o) ((req_t *)(o))
#define TK(o) ((task_t *)(o))
static void keep(id *slot, id v) { if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id arr(void) { return MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static id sp_error(void) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str("O reconhecimento de fala não está disponível no mOS."),
                cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("SFSpeechErrorDomain"), (long)1, ui);
}

/* ---------------------------------------------------------------- autorização */
static i64 auth_status(id k, SEL c) {   /* notDetermined 0, denied 1, restricted 2, authorized 3 */
    (void)k; (void)c;
    int p = cx_tcc_preflight("kTCCServiceSpeechRecognition");
    return p < 0 ? 0 : p == 1 ? 3 : 1;
}
static void auth_done(int granted, void *blk) {
    ((void (*)(void *, i64))BLK_FN(blk))(blk, granted ? 3 : 1);
    _Block_release(blk);
}
static void request_auth(id k, SEL c, void *blk) {
    (void)k; (void)c;
    if (!blk) return;
    cx_tcc_request_async("kTCCServiceSpeechRecognition", auth_done, _Block_copy(blk));
}

/* ---------------------------------------------------------------- SFSpeechRecognizer */
static id rec_init_locale(id s, SEL c, id locale) {
    (void)c;
    if (!locale) locale = MSG0(id, (id)cx_class_named("NSLocale"), "currentLocale");
    keep(&REC(s)->locale, locale);
    return s;
}
static id rec_init(id s, SEL c) { return rec_init_locale(s, c, 0); }
static id rec_supported(id k, SEL c) { (void)k; (void)c; return MSG0(id, (id)cx_class_named("NSSet"), "set"); }
static id rec_locale(id s, SEL c) { (void)c; return REC(s)->locale; }
static BOOL rec_available(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL rec_supports_on_device(id s, SEL c) { (void)s; (void)c; return NO; }
static id rec_delegate(id s, SEL c) { (void)c; return REC(s)->delegate; }
static void rec_set_delegate(id s, SEL c, id d) { (void)c; REC(s)->delegate = d; }
static void *rec_queue(id s, SEL c) { (void)c; return REC(s)->queue; }
static void rec_set_queue(id s, SEL c, void *q) { (void)c; REC(s)->queue = q; }
static i64 rec_hint(id s, SEL c) { (void)c; return REC(s)->task_hint; }
static void rec_set_hint(id s, SEL c, i64 v) { (void)c; REC(s)->task_hint = v; }
static BOOL rec_on_device(id s, SEL c) { (void)c; return REC(s)->on_device; }
static void rec_set_on_device(id s, SEL c, BOOL v) { (void)c; REC(s)->on_device = v; }

/* a tarefa termina logo: o handler recebe (nil, erro); o delegate, didFinishSuccessfully: NO */
typedef struct { void *blk; id task, delegate; } fin_t;
static void finish(void *a) {
    fin_t *f = a;
    id err = TK(f->task)->error;
    if (f->blk) ((void (*)(void *, id, id))BLK_FN(f->blk))(f->blk, (id)0, err);
    if (f->delegate) {
        SEL fs = sel_registerName("speechRecognitionTask:didFinishSuccessfully:");
        if (cx_responds(object_getClass(f->delegate), fs)) MSG(void, f->delegate, "speechRecognitionTask:didFinishSuccessfully:", f->task, (BOOL)NO);
    }
    if (f->blk) _Block_release(f->blk);
    if (f->delegate) objc_release(f->delegate);
    objc_release(f->task);
    free(f);
}
static id start_task(id rec, void *blk, id delegate) {
    task_t *t = (task_t *)cx_alloc_instance(&SFSpeechRecognitionTask_c);
    if (!t) return 0;
    t->state = 4;   /* SFSpeechRecognitionTaskStateCompleted */
    t->error = objc_retain(sp_error());
    fin_t *f = calloc(1, sizeof *f);
    if (f) {
        f->blk = blk ? _Block_copy(blk) : 0; f->task = objc_retain((id)t); f->delegate = delegate ? objc_retain(delegate) : 0;
        void *q = REC(rec)->queue ? REC(rec)->queue : dispatch_get_main_queue();
        dispatch_async_f(q, f, finish);
    }
    return objc_autorelease((id)t);
}
static id rec_task_handler(id s, SEL c, id req, void *blk) { (void)c; (void)req; return start_task(s, blk, 0); }
static id rec_task_delegate(id s, SEL c, id req, id d) { (void)c; (void)req; return start_task(s, 0, d); }

/* ---------------------------------------------------------------- pedidos */
static id url_init(id s, SEL c, id url) { (void)c; keep(&RQ(s)->url, url); return s; }
static id rq_url(id s, SEL c) { (void)c; return RQ(s)->url; }
static id rq_context(id s, SEL c) { (void)c; return RQ(s)->context ? RQ(s)->context : arr(); }
static void rq_set_context(id s, SEL c, id v) { (void)c; keep(&RQ(s)->context, v); }
static i64 rq_hint(id s, SEL c) { (void)c; return RQ(s)->hint; }
static void rq_set_hint(id s, SEL c, i64 v) { (void)c; RQ(s)->hint = v; }
static BOOL rq_partial(id s, SEL c) { (void)c; return RQ(s)->partial; }
static void rq_set_partial(id s, SEL c, BOOL v) { (void)c; RQ(s)->partial = v; }
static BOOL rq_on_device(id s, SEL c) { (void)c; return RQ(s)->on_device; }
static void rq_set_on_device(id s, SEL c, BOOL v) { (void)c; RQ(s)->on_device = v; }
static BOOL rq_punct(id s, SEL c) { (void)c; return RQ(s)->punct; }
static void rq_set_punct(id s, SEL c, BOOL v) { (void)c; RQ(s)->punct = v; }
static id rq_init(id s, SEL c) { (void)c; RQ(s)->partial = YES; return s; }
static void ab_append(id s, SEL c, id buf) { (void)c; if (buf && !RQ(s)->ended) RQ(s)->appended++; }
static void ab_end(id s, SEL c) { (void)c; RQ(s)->ended = 1; }
static id ab_format(id s, SEL c) { (void)c; return RQ(s)->native_fmt; }

/* ---------------------------------------------------------------- tarefa, resultado, transcrição, segmento */
static i64 tk_state(id s, SEL c) { (void)c; return TK(s)->state; }
static BOOL tk_cancelled(id s, SEL c) { (void)c; return TK(s)->cancelled; }
static BOOL tk_finishing(id s, SEL c) { (void)c; return TK(s)->finishing; }
static void tk_cancel(id s, SEL c) { (void)c; TK(s)->cancelled = YES; }
static void tk_finish(id s, SEL c) { (void)c; TK(s)->finishing = YES; }
static id tk_error(id s, SEL c) { (void)c; return TK(s)->error; }
static id res_best(id s, SEL c) { (void)c; return ((res_t *)s)->best; }
static id res_all(id s, SEL c) { (void)c; return ((res_t *)s)->all ? ((res_t *)s)->all : arr(); }
static BOOL res_final(id s, SEL c) { (void)c; return ((res_t *)s)->final; }
static id tr_text(id s, SEL c) { (void)c; return ((tr_t *)s)->text ? ((tr_t *)s)->text : cx_str(""); }
static id tr_segments(id s, SEL c) { (void)c; return ((tr_t *)s)->segments ? ((tr_t *)s)->segments : arr(); }
static id sg_text(id s, SEL c) { (void)c; return ((seg_t *)s)->text ? ((seg_t *)s)->text : cx_str(""); }
static id sg_alts(id s, SEL c) { (void)c; return ((seg_t *)s)->alts ? ((seg_t *)s)->alts : arr(); }
static double sg_ts(id s, SEL c) { (void)c; return ((seg_t *)s)->ts; }
static double sg_dur(id s, SEL c) { (void)c; return ((seg_t *)s)->dur; }
static float sg_conf(id s, SEL c) { (void)c; return ((seg_t *)s)->conf; }
typedef struct { u64 loc, len; } nsrange_t;
static nsrange_t sg_range(id s, SEL c) { (void)c; nsrange_t r = {((seg_t *)s)->loc, ((seg_t *)s)->len}; return r; }

static void K(cx_class_t *k, cx_class_t *m, const char *n, cx_class_t *sup, u32 sz) { if (!cx_class_named(n)) cx_define_class(k, m, n, sup, sz); }
#define A(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define AC(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
void cx_speech_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    K(&SFSpeechRecognizer_c, &SFSpeechRecognizer_m, "SFSpeechRecognizer", ob, sizeof(rec_t) - 8);
    AC(SFSpeechRecognizer, "authorizationStatus", auth_status); AC(SFSpeechRecognizer, "requestAuthorization:", request_auth);
    AC(SFSpeechRecognizer, "supportedLocales", rec_supported);
    A(SFSpeechRecognizer, "init", rec_init); A(SFSpeechRecognizer, "initWithLocale:", rec_init_locale); A(SFSpeechRecognizer, "locale", rec_locale);
    A(SFSpeechRecognizer, "isAvailable", rec_available); A(SFSpeechRecognizer, "supportsOnDeviceRecognition", rec_supports_on_device);
    A(SFSpeechRecognizer, "setSupportsOnDeviceRecognition:", rec_set_on_device); (void)rec_on_device;
    A(SFSpeechRecognizer, "delegate", rec_delegate); A(SFSpeechRecognizer, "setDelegate:", rec_set_delegate);
    A(SFSpeechRecognizer, "queue", rec_queue); A(SFSpeechRecognizer, "setQueue:", rec_set_queue);
    A(SFSpeechRecognizer, "defaultTaskHint", rec_hint); A(SFSpeechRecognizer, "setDefaultTaskHint:", rec_set_hint);
    A(SFSpeechRecognizer, "recognitionTaskWithRequest:resultHandler:", rec_task_handler);
    A(SFSpeechRecognizer, "recognitionTaskWithRequest:delegate:", rec_task_delegate);
    K(&SFSpeechRecognitionRequest_c, &SFSpeechRecognitionRequest_m, "SFSpeechRecognitionRequest", ob, sizeof(req_t) - 8);
    A(SFSpeechRecognitionRequest, "init", rq_init);
    A(SFSpeechRecognitionRequest, "contextualStrings", rq_context); A(SFSpeechRecognitionRequest, "setContextualStrings:", rq_set_context);
    A(SFSpeechRecognitionRequest, "taskHint", rq_hint); A(SFSpeechRecognitionRequest, "setTaskHint:", rq_set_hint);
    A(SFSpeechRecognitionRequest, "shouldReportPartialResults", rq_partial); A(SFSpeechRecognitionRequest, "setShouldReportPartialResults:", rq_set_partial);
    A(SFSpeechRecognitionRequest, "requiresOnDeviceRecognition", rq_on_device); A(SFSpeechRecognitionRequest, "setRequiresOnDeviceRecognition:", rq_set_on_device);
    A(SFSpeechRecognitionRequest, "addsPunctuation", rq_punct); A(SFSpeechRecognitionRequest, "setAddsPunctuation:", rq_set_punct);
    K(&SFSpeechAudioBufferRecognitionRequest_c, &SFSpeechAudioBufferRecognitionRequest_m, "SFSpeechAudioBufferRecognitionRequest", &SFSpeechRecognitionRequest_c, 0);
    A(SFSpeechAudioBufferRecognitionRequest, "appendAudioPCMBuffer:", ab_append); A(SFSpeechAudioBufferRecognitionRequest, "appendAudioSampleBuffer:", ab_append);
    A(SFSpeechAudioBufferRecognitionRequest, "endAudio", ab_end); A(SFSpeechAudioBufferRecognitionRequest, "nativeAudioFormat", ab_format);
    K(&SFSpeechURLRecognitionRequest_c, &SFSpeechURLRecognitionRequest_m, "SFSpeechURLRecognitionRequest", &SFSpeechRecognitionRequest_c, 0);
    A(SFSpeechURLRecognitionRequest, "initWithURL:", url_init); A(SFSpeechURLRecognitionRequest, "URL", rq_url);
    K(&SFSpeechRecognitionTask_c, &SFSpeechRecognitionTask_m, "SFSpeechRecognitionTask", ob, sizeof(task_t) - 8);
    A(SFSpeechRecognitionTask, "state", tk_state); A(SFSpeechRecognitionTask, "isCancelled", tk_cancelled);
    A(SFSpeechRecognitionTask, "isFinishing", tk_finishing); A(SFSpeechRecognitionTask, "cancel", tk_cancel);
    A(SFSpeechRecognitionTask, "finish", tk_finish); A(SFSpeechRecognitionTask, "error", tk_error);
    K(&SFSpeechRecognitionResult_c, &SFSpeechRecognitionResult_m, "SFSpeechRecognitionResult", ob, sizeof(res_t) - 8);
    A(SFSpeechRecognitionResult, "bestTranscription", res_best); A(SFSpeechRecognitionResult, "transcriptions", res_all);
    A(SFSpeechRecognitionResult, "isFinal", res_final);
    K(&SFTranscription_c, &SFTranscription_m, "SFTranscription", ob, sizeof(tr_t) - 8);
    A(SFTranscription, "formattedString", tr_text); A(SFTranscription, "segments", tr_segments);
    K(&SFTranscriptionSegment_c, &SFTranscriptionSegment_m, "SFTranscriptionSegment", ob, sizeof(seg_t) - 8);
    A(SFTranscriptionSegment, "substring", sg_text); A(SFTranscriptionSegment, "alternativeSubstrings", sg_alts);
    A(SFTranscriptionSegment, "timestamp", sg_ts); A(SFTranscriptionSegment, "duration", sg_dur);
    A(SFTranscriptionSegment, "confidence", sg_conf); A(SFTranscriptionSegment, "substringRange", sg_range);
}
