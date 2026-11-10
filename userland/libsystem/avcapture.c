/*
 * AVCapture em cima das classes vazias de vscode_fwclasses.c (e da AVCaptureAudioDataOutput do Slack).
 * Não cria uma segunda classe com o mesmo nome.
 *
 * O que funciona: o portão de permissão. requestAccessForMediaType: e authorizationStatusForMediaType:
 * consultam cx_tcc_request da frente de TCC ("kTCCServiceMicrophone" para "soun"/áudio, "kTCCServiceCamera"
 * para o resto). Sem concessão, startRunning não liga a sessão.
 *
 * Limitação: não há driver de câmera. devices devolve lista vazia, defaultDevice devolve nil, e uma sessão
 * autorizada pode ficar isRunning sem produzir quadro nenhum. Não se inventa imagem.
 */
#include "foundation.h"

const char *cx_cstr(id o);
int cx_media_tcc(const char *service);
int cx_tcc_preflight(const char *service);
int strcmp(const char *a, const char *b);

static const char *svc_for(id type) {
    const char *s = type ? cx_cstr(type) : "";
    if (!strcmp(s, "soun") || !strcmp(s, "AVMediaTypeAudio")) return "kTCCServiceMicrophone";
    return "kTCCServiceCamera";
}

/* AVAuthorizationStatus: 0 notDetermined, 2 denied, 3 authorized. Sem o símbolo de TCC, notDetermined. */
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

static id empty_array(void) {
    cx_class_t *arr = cx_class_named("NSArray");
    return arr ? MSG0(id, (id)arr, "array") : 0;
}
static id dev_devices(id cls, SEL c) { (void)cls; (void)c; return empty_array(); }
static id dev_typed(id cls, SEL c, id type) { (void)type; return dev_devices(cls, c); }
static id dev_default(id cls, SEL c, id type) { (void)cls; (void)c; (void)type; return 0; }
static id dev_discovery(id cls, SEL c, id types, id pos, id media) {
    (void)cls; (void)c; (void)types; (void)pos; (void)media;
    return cx_alloc_instance(cx_class_named("AVCaptureDeviceDiscoverySession"));
}
static id disc_devices(id s, SEL c) { (void)s; (void)c; return empty_array(); }

typedef struct { cx_class_t *isa; int running; int screen; } avsess_t;

cx_class_t AVCaptureScreenInput_c __asm__("OBJC_CLASS_$_AVCaptureScreenInput");
cx_class_t AVCaptureScreenInput_m __asm__("OBJC_METACLASS_$_AVCaptureScreenInput");
typedef struct { cx_class_t *isa; u32 display; } avscreen_t;

static void sess_start(id s, SEL c) {
    (void)c;
    avsess_t *x = (avsess_t *)s;
    /* Entrada de tela exige gravação de tela. Sem ela, o portão continua sendo a câmera.
     * Nos dois casos a sessão não fabrica quadro: a captura contínua é o SCStream. */
    if (x->screen) x->running = cx_tcc_preflight("kTCCServiceScreenCapture") == 1;
    else x->running = cx_media_tcc("kTCCServiceCamera") == 1;
}
static void sess_stop(id s, SEL c) { (void)c; ((avsess_t *)s)->running = 0; }
static u8 sess_running(id s, SEL c) { (void)c; return (u8)((avsess_t *)s)->running; }
static u8 sess_can_add(id s, SEL c, id io) { (void)s; (void)c; return io != 0; }
static void sess_add(id s, SEL c, id io) {
    (void)c;
    if (io && *(cx_class_t **)io == &AVCaptureScreenInput_c) ((avsess_t *)s)->screen = 1;
}
static void sess_remove(id s, SEL c, id io) { (void)s; (void)c; (void)io; }
static void sess_cfg(id s, SEL c) { (void)s; (void)c; }
static id sess_preset(id s, SEL c) { (void)s; (void)c; return 0; }
static void sess_set_preset(id s, SEL c, id p) { (void)s; (void)c; (void)p; }

static id input_with_device(id cls, SEL c, id dev, id *err) {
    (void)cls; (void)c; (void)dev;
    if (err) *err = 0;
    return 0;   /* não há AVCaptureDevice de verdade para embrulhar */
}
static id input_device(id s, SEL c) { (void)s; (void)c; return 0; }
static id screen_init(id s, SEL c, u32 display) {
    (void)c;
    ((avscreen_t *)s)->display = display ? display : 1;
    return s;
}

static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(cls);
    if (!k) return;
    if (meta) cx_add_cmethod(k, sel, fn); else cx_add_method(k, sel, fn);
}

void cx_avcapture_init(void) {
    if (!cx_class_named("AVCaptureScreenInput")) {
        cx_define_class(&AVCaptureScreenInput_c, &AVCaptureScreenInput_m, "AVCaptureScreenInput", cx_class_named("NSObject"), 16);
        cx_add_method(&AVCaptureScreenInput_c, "initWithDisplayID:", (void *)screen_init);
    }
    add("AVCaptureDevice", "authorizationStatusForMediaType:", (void *)auth_status, 1);
    add("AVCaptureDevice", "requestAccessForMediaType:completionHandler:", (void *)request_access, 1);
    add("AVCaptureDevice", "devices", (void *)dev_devices, 1);
    add("AVCaptureDevice", "devicesWithMediaType:", (void *)dev_typed, 1);
    add("AVCaptureDevice", "defaultDeviceWithMediaType:", (void *)dev_default, 1);
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
    add("AVCaptureDeviceInput", "deviceInputWithDevice:error:", (void *)input_with_device, 1);
    add("AVCaptureDeviceInput", "device", (void *)input_device, 0);
}
