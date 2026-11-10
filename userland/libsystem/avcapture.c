/*
 * AVCapture em cima das classes de vscode_fwclasses.c e do AVCaptureAudioDataOutput do Slack.
 *
 * authorizationStatusForMediaType: e requestAccessForMediaType: consultam cx_media_tcc
 * (kTCCServiceMicrophone para áudio, kTCCServiceCamera para vídeo). Sem concessão, a sessão não corre.
 *
 * Há um dispositivo de microfone. O áudio sai do anel de captura do kernel (cx_capture_i16), o mesmo
 * do AudioUnit de entrada. Zero quadros não viram sinal. Não há câmera no kernel: um input de vídeo
 * impede startRunning e devicesWithMediaType: de vídeo fica vazio. Não se inventa quadro.
 */
#include "foundation.h"
#include "avf_core.h"

const char *cx_cstr(id o);
int cx_media_tcc(const char *service);
int strcmp(const char *a, const char *b);
void *memcpy(void *d, const void *s, size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
id objc_retain(id o);
void objc_release(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
extern id AVMediaTypeAudio;
extern void *cx_cm_audio_sample(const short *pcm, unsigned frames, double rate);

static const char *svc_for(id type) {
    const char *s = type ? cx_cstr(type) : "";
    if (!strcmp(s, "soun") || !strcmp(s, "AVMediaTypeAudio")) return "kTCCServiceMicrophone";
    return "kTCCServiceCamera";
}
static int is_audio_type(id type) {
    const char *s = type ? cx_cstr(type) : "";
    return !strcmp(s, "soun") || !strcmp(s, "AVMediaTypeAudio");
}

static long auth_status(id cls, SEL c, id type) {
    (void)cls; (void)c;
    int p = cx_media_tcc(svc_for(type));
    if (p < 0) return 0;
    return p == 1 ? 3 : 2;
}
static void request_access(id cls, SEL c, id type, void *blk) {
    (void)cls; (void)c;
    if (!blk) return;
    void *inv = *(void **)((u8 *)blk + 16);
    ((void (*)(void *, u8))inv)(blk, cx_media_tcc(svc_for(type)) == 1);
}

typedef struct { cx_class_t *isa; int kind; int position; char uid[20]; } avdev_t;
static id dev_make(int kind) {
    avdev_t *d = (avdev_t *)cx_alloc_instance(cx_class_named("AVCaptureDevice"));
    if (!d) return 0;
    d->kind = kind;
    d->position = 0;
    const char *u = kind == 1 ? "mos-mic" : "mos-cam";
    for (int i = 0; u[i] && i < 19; i++) d->uid[i] = u[i];
    return (id)d;
}
static id dev_list(id type) {
    cx_class_t *arr = cx_class_named("NSMutableArray");
    id a = arr ? MSG0(id, (id)arr, "array") : 0;
    if (!a) return 0;
    if (type && !is_audio_type(type)) return a;
    int kinds[4];
    int n = avf_device_kinds(kinds, 4);
    for (int i = 0; i < n; i++) {
        if (type && kinds[i] == 1 && !is_audio_type(type)) continue;
        id d = dev_make(kinds[i]);
        if (d) MSG(void, a, "addObject:", d);
    }
    return a;
}
static id dev_devices(id cls, SEL c) { (void)cls; (void)c; return dev_list(0); }
static id dev_typed(id cls, SEL c, id type) { (void)cls; (void)c; return dev_list(type); }
static id dev_default(id cls, SEL c, id type) {
    (void)cls; (void)c;
    if (!is_audio_type(type)) return 0;
    int kinds[2];
    if (avf_device_kinds(kinds, 2) < 1) return 0;
    return dev_make(kinds[0]);
}
static id dev_discovery(id cls, SEL c, id types, id pos, id media) {
    (void)cls; (void)c; (void)types; (void)pos;
    id s = cx_alloc_instance(cx_class_named("AVCaptureDeviceDiscoverySession"));
    if (s && media) objc_setAssociatedObject(s, "media", media, 1);
    return s;
}
static id disc_devices(id s, SEL c) {
    (void)c;
    return dev_list(objc_getAssociatedObject(s, "media"));
}
static id dev_name(id s, SEL c) { (void)c; return cx_str(((avdev_t *)s)->kind == 1 ? "Microfone do mOS" : "Câmera"); }
static id dev_uid(id s, SEL c) { (void)c; return cx_str(((avdev_t *)s)->uid); }
static id dev_model(id s, SEL c) { (void)s; (void)c; return cx_str("mOS"); }
static i64 dev_position(id s, SEL c) { (void)c; return ((avdev_t *)s)->position; }
static u8 dev_has_media(id s, SEL c, id type) {
    (void)c;
    int k = ((avdev_t *)s)->kind;
    if (k == 1) return (u8)is_audio_type(type);
    return 0;
}

typedef struct { id obj; int is_input; int kind; } ioitem_t;
typedef struct { cx_class_t *isa; int running; int err; ioitem_t *items; int n; int cap; } avsess_t;

static void sess_deliver(avsess_t *s) {
    short pcm[1024];
    int err = 0;
    unsigned n = avf_cap_pull(s->running, pcm, 1024, &err);
    if (!n) return;
    void *sample = cx_cm_audio_sample(pcm, n, 48000);
    if (!sample) return;
    for (int i = 0; i < s->n; i++) {
        if (s->items[i].is_input || !s->items[i].obj) continue;
        id del = 0;
        if (cx_responds(object_getClass(s->items[i].obj), sel_registerName("sampleBufferDelegate")))
            del = MSG0(id, s->items[i].obj, "sampleBufferDelegate");
        if (del && cx_responds(object_getClass(del), sel_registerName("captureOutput:didOutputSampleBuffer:fromConnection:")))
            MSG(void, del, "captureOutput:didOutputSampleBuffer:fromConnection:", s->items[i].obj, (id)sample, (id)0);
    }
    objc_release((id)sample);
}
static void sess_start(id s, SEL c) {
    (void)c;
    avsess_t *ss = (avsess_t *)s;
    int mic = 0, cam = 0;
    for (int i = 0; i < ss->n; i++) if (ss->items[i].is_input) {
        if (ss->items[i].kind == 1) mic = 1;
        if (ss->items[i].kind == 2) cam = 1;
    }
    int err = 0;
    ss->running = avf_cap_start(mic, cam, &err);
    ss->err = err;
    if (ss->running) sess_deliver(ss);
}
static void sess_stop(id s, SEL c) { (void)c; ((avsess_t *)s)->running = 0; }
static u8 sess_running(id s, SEL c) { (void)c; return (u8)((avsess_t *)s)->running; }
static u8 sess_can_add(id s, SEL c, id io) { (void)s; (void)c; return io != 0; }
static int kind_of_input(id io) {
    if (!io || !cx_responds(object_getClass(io), sel_registerName("device"))) return 0;
    id d = MSG0(id, io, "device");
    if (!d) return 0;
    return ((avdev_t *)d)->kind;
}
static void sess_add(id s, SEL c, id io) {
    (void)c;
    if (!io) return;
    avsess_t *ss = (avsess_t *)s;
    if (ss->n >= ss->cap) {
        int cap = ss->cap ? ss->cap * 2 : 4;
        ioitem_t *ni = calloc((size_t)cap, sizeof *ni);
        if (!ni) return;
        if (ss->items && ss->n) memcpy(ni, ss->items, (size_t)ss->n * sizeof *ni);
        free(ss->items);
        ss->items = ni;
        ss->cap = cap;
    }
    int input = cx_responds(object_getClass(io), sel_registerName("device"));
    ss->items[ss->n].obj = objc_retain(io);
    ss->items[ss->n].is_input = input;
    ss->items[ss->n].kind = input ? kind_of_input(io) : 0;
    ss->n++;
}
static void sess_remove(id s, SEL c, id io) {
    (void)c;
    avsess_t *ss = (avsess_t *)s;
    for (int i = 0; i < ss->n; i++) if (ss->items[i].obj == io) {
        objc_release(io);
        ss->items[i] = ss->items[ss->n - 1];
        ss->n--;
        return;
    }
}
static void sess_cfg(id s, SEL c) { (void)s; (void)c; }
static id sess_preset(id s, SEL c) {
    (void)c;
    id p = objc_getAssociatedObject(s, "preset");
    return p ? p : cx_str("AVCaptureSessionPresetHigh");
}
static void sess_set_preset(id s, SEL c, id p) { (void)c; if (p) objc_setAssociatedObject(s, "preset", p, 1); }
static i64 sess_err(id s, SEL c) { (void)c; return ((avsess_t *)s)->err; }

typedef struct { cx_class_t *isa; id device; } avin_t;
static id input_with_device(id cls, SEL c, id dev, id *err) {
    (void)cls; (void)c;
    if (err) *err = 0;
    if (!dev) return 0;
    avin_t *in = (avin_t *)cx_alloc_instance(cx_class_named("AVCaptureDeviceInput"));
    if (!in) return 0;
    in->device = objc_retain(dev);
    return (id)in;
}
static id input_device(id s, SEL c) { (void)c; return ((avin_t *)s)->device; }

static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(cls);
    if (!k) return;
    if (meta) cx_add_cmethod(k, sel, fn); else cx_add_method(k, sel, fn);
}

void cx_avcapture_init(void) {
    add("AVCaptureDevice", "authorizationStatusForMediaType:", (void *)auth_status, 1);
    add("AVCaptureDevice", "requestAccessForMediaType:completionHandler:", (void *)request_access, 1);
    add("AVCaptureDevice", "devices", (void *)dev_devices, 1);
    add("AVCaptureDevice", "devicesWithMediaType:", (void *)dev_typed, 1);
    add("AVCaptureDevice", "defaultDeviceWithMediaType:", (void *)dev_default, 1);
    add("AVCaptureDevice", "localizedName", (void *)dev_name, 0);
    add("AVCaptureDevice", "uniqueID", (void *)dev_uid, 0);
    add("AVCaptureDevice", "modelID", (void *)dev_model, 0);
    add("AVCaptureDevice", "position", (void *)dev_position, 0);
    add("AVCaptureDevice", "hasMediaType:", (void *)dev_has_media, 0);
    add("AVCaptureDeviceDiscoverySession", "discoverySessionWithDeviceTypes:mediaType:position:", (void *)dev_discovery, 1);
    add("AVCaptureDeviceDiscoverySession", "devices", (void *)disc_devices, 0);
    add("AVCaptureSession", "startRunning", (void *)sess_start, 0);
    add("AVCaptureSession", "stopRunning", (void *)sess_stop, 0);
    add("AVCaptureSession", "isRunning", (void *)sess_running, 0);
    add("AVCaptureSession", "canAddInput:", (void *)sess_can_add, 0);
    add("AVCaptureSession", "addInput:", (void *)sess_add, 0);
    add("AVCaptureSession", "removeInput:", (void *)sess_remove, 0);
    add("AVCaptureSession", "canAddOutput:", (void *)sess_can_add, 0);
    add("AVCaptureSession", "addOutput:", (void *)sess_add, 0);
    add("AVCaptureSession", "removeOutput:", (void *)sess_remove, 0);
    add("AVCaptureSession", "beginConfiguration", (void *)sess_cfg, 0);
    add("AVCaptureSession", "commitConfiguration", (void *)sess_cfg, 0);
    add("AVCaptureSession", "sessionPreset", (void *)sess_preset, 0);
    add("AVCaptureSession", "setSessionPreset:", (void *)sess_set_preset, 0);
    add("AVCaptureSession", "mosCaptureError", (void *)sess_err, 0);
    add("AVCaptureDeviceInput", "deviceInputWithDevice:error:", (void *)input_with_device, 1);
    add("AVCaptureDeviceInput", "device", (void *)input_device, 0);
}
