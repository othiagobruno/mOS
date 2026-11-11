/* mOS libSystem — o que o ChatGPT desktop (26.1103, x86_64) ainda importava (tools/machocheck.c, 10 nov. 2026).
 * Quase tudo vem do helper "Codex Computer Use". Cada função faz o que dá para fazer no mOS e devolve o erro do
 * macOS quando o recurso não existe. As classes ObjC ficam em agentes_classes.c. */
#include "foundation.h"
#include "sys.h"

typedef int OSStatus;
int cx_ax_copy(id e, id attr, id *v);
int cx_ax_actions(id e, id *names);
int strcmp(const char *a, const char *b);
id objc_retain(id o);
void objc_release(id o);
int IOPMAssertionCreateWithName(void *type, u32 level, void *name, u32 *id);
int IORegistryEntryGetName(u32 e, char *name);
int proc_pidinfo(int pid, int flavor, u64 arg, void *buf, int size);

/* ---------------------------------------------------------------- constantes (valores do macOS) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } az_const_t;
#define AZK(var, text) static az_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
AZK(kUTTypeVolume, "public.volume")
AZK(kUTTypeXML, "public.xml")
AZK(AVLayerVideoGravityResizeAspect, "AVLayerVideoGravityResizeAspect")
AZK(kODAttributeTypeJPEGPhoto, "dsAttrTypeStandard:JPEGPhoto")
AZK(kODAttributeTypePicture, "dsAttrTypeStandard:Picture")
AZK(kVTCompressionPropertyKey_PreserveAlphaChannel, "PreserveAlphaChannel")
AZK(kVTCompressionPropertyKey_TargetQualityForAlpha, "TargetQualityForAlpha")
AZK(AVAudioApplicationInputMuteStateChangeNotification, "AVAudioApplicationInputMuteStateChangeNotification")
AZK(AVAudioApplicationMuteStateKey, "AVAudioApplicationMuteStateKey")

/* ---------------------------------------------------------------- Process Manager
 * O número de série de processo do mOS é {0, pid}; pid que a lista de processos do kernel não conhece é
 * procNotFound. PROC_PIDTBSDINFO (3) é o mesmo pedido que o cli.c atende. */
typedef struct { u32 high, low; } psn_t;
enum { procNotFound = -600, paramErr = -50 };
static int pid_known(int pid) {
    u8 info[256];
    return pid > 0 && proc_pidinfo(pid, 3, 0, info, (int)sizeof info) > 0;
}
OSStatus GetProcessForPID(int pid, psn_t *psn) {
    if (!psn || pid <= 0) return paramErr;
    if (!pid_known(pid)) return procNotFound;
    psn->high = 0;
    psn->low = (u32)pid;
    return 0;
}
OSStatus GetProcessPID(const psn_t *psn, int *pid) {
    if (!psn || !pid) return paramErr;
    if (psn->high == 0 && psn->low == 2) { *pid = (int)SYS0(SYS_GETPID); return 0; }   /* kCurrentProcess */
    if (psn->high != 0 || !pid_known((int)psn->low)) return procNotFound;
    *pid = (int)psn->low;
    return 0;
}

/* ---------------------------------------------------------------- Carbon: atalhos simbólicos do sistema
 * O mOS não tem a tabela de atalhos simbólicos configuráveis (com.apple.symbolichotkeys): a lista vem vazia. */
OSStatus CopySymbolicHotKeys(id *out) {
    if (!out) return paramErr;
    *out = objc_retain(MSG0(id, (id)cx_class_named("NSArray"), "array"));
    return 0;
}

/* ---------------------------------------------------------------- AppleEvents: especificador de objeto
 * Monta o descritor 'obj ' com classe, forma da chave e cópias do contêiner e da chave. Não há outro app para
 * receber AppleEvents no mOS (AESendMessage devolve errAEEventNotHandled), mas o descritor é coerente. */
typedef struct { u32 type; void *data; } ae_desc_t;
typedef struct { u32 desired_class, key_form; ae_desc_t container, key; } ae_objspec_t;
void *calloc(size_t n, size_t s);
OSStatus CreateObjSpecifier(u32 desired_class, ae_desc_t *container, u32 key_form, ae_desc_t *key, u8 dispose, ae_desc_t *out) {
    if (!out || !key) return paramErr;
    ae_objspec_t *o = calloc(1, sizeof *o);
    if (!o) return -108;                               /* memFullErr */
    o->desired_class = desired_class;
    o->key_form = key_form;
    if (container) o->container = *container; else o->container.type = 0x6E756C6C;   /* 'null': o aplicativo */
    o->key = *key;
    out->type = 0x6F626A20;                            /* 'obj ' (typeObjectSpecifier) */
    out->data = o;
    if (dispose) {                                     /* o descritor novo fica com o conteúdo: só zera as entradas */
        if (container) { container->type = 0x6E756C6C; container->data = 0; }
        key->type = 0x6E756C6C;
        key->data = 0;
    }
    return 0;
}

/* ---------------------------------------------------------------- Acessibilidade */
enum { kAXErrorIllegalArgument = -25201, kAXErrorActionUnsupported = -25206, kAXErrorAttributeUnsupported = -25205 };
static const char *action_desc(const char *a) {
    static const char *t[][2] = {
        {"AXPress", "press"}, {"AXCancel", "cancel"}, {"AXConfirm", "confirm"}, {"AXDecrement", "decrement"},
        {"AXIncrement", "increment"}, {"AXPick", "pick"}, {"AXRaise", "raise"}, {"AXShowMenu", "show menu"},
        {"AXShowAlternateUI", "show alternate UI"}, {"AXShowDefaultUI", "show default UI"}, {"AXScrollToVisible", "scroll to visible"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if (!strcmp(t[i][0], a)) return t[i][1];
    return 0;
}
/* a descrição só existe para uma ação que o elemento tem (AXUIElementCopyActionNames) */
OSStatus AXUIElementCopyActionDescription(id e, id action, id *desc) {
    if (!desc) return kAXErrorIllegalArgument;
    *desc = 0;
    if (!e || !action || !cx_is_str(action)) return kAXErrorIllegalArgument;
    id names = 0;
    int st = cx_ax_actions(e, &names);
    if (st) return st;
    int has = 0;
    u64 n = names ? MSG0(u64, names, "count") : 0;
    for (u64 i = 0; i < n && !has; i++) {
        id s = MSG(id, names, "objectAtIndex:", i);
        if (s && cx_is_str(s) && !strcmp(cx_cstr(s), cx_cstr(action))) has = 1;
    }
    if (names) objc_release(names);
    if (!has) return kAXErrorActionUnsupported;
    const char *d = action_desc(cx_cstr(action));
    *desc = objc_retain(cx_str(d ? d : cx_cstr(action)));
    return 0;
}
/* quantos valores tem um atributo de lista (AXChildren, AXWindows...); atributo que não é lista é erro de argumento */
OSStatus AXUIElementGetAttributeValueCount(id e, id attr, long *count) {
    if (!count) return kAXErrorIllegalArgument;
    *count = 0;
    id v = 0;
    int st = cx_ax_copy(e, attr, &v);
    if (st) return st;
    if (!v) return kAXErrorAttributeUnsupported;
    int arr = MSG(BOOL, v, "respondsToSelector:", sel_registerName("count")) &&
              MSG(BOOL, v, "respondsToSelector:", sel_registerName("objectAtIndex:"));
    if (arr) *count = (long)MSG0(u64, v, "count");
    objc_release(v);
    return arr ? 0 : kAXErrorIllegalArgument;
}

/* ---------------------------------------------------------------- IOKit */
/* "o usuário está ativo": uma asserção de uso do sistema, como o macOS cria (o id volta para o release) */
int IOPMAssertionDeclareUserActivity(id name, u32 type, u32 *assertion) {
    if (!assertion) return (int)0xE00002C2;            /* kIOReturnBadArgument */
    static az_const_t kind = {cx_cfconst_isa, 0x7c8, "PreventUserIdleDisplaySleep", 27};
    return IOPMAssertionCreateWithName(&kind, 255, name, assertion);
}
/* o mOS tem um plano só (IOService): o nome nos outros planos é o mesmo */
int IORegistryEntryGetNameInPlane(u32 e, const char *plane, char *name) {
    (void)plane;
    if (!name || !e) return (int)0xE00002C2;
    return IORegistryEntryGetName(e, name);
}
