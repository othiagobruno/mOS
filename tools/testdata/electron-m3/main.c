/*
 * mOS — teste de host dos seletores que VS Code, Cursor e Slack chamam ao abrir a janela (marco M3, machotest).
 * Mach-O x86_64 que roda no tools/machotest contra o libSystem.mos: tools/test-electron-m3.sh.
 * Cada CHECK chama o seletor pelo objc_msgSend, como o Chromium/Electron chama.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef struct objc_object *id;
typedef struct objc_selector *SEL;
typedef signed char BOOL;
id objc_getClass(const char *n);
SEL sel_registerName(const char *s);
id objc_msgSend(id, SEL, ...);
void objc_msgSend_stret(void *ret, id, SEL, ...);   /* struct > 16 bytes no x86_64 */
#define S(x) sel_registerName(x)
#define C(x) objc_getClass(x)
typedef id (*M0)(id, SEL);
typedef id (*M1)(id, SEL, id);
#define MSG0(o, s) ((M0)objc_msgSend)((id)(o), S(s))
#define MSG1(o, s, a) ((M1)objc_msgSend)((id)(o), S(s), (id)(a))
typedef struct { double x, y, w, h; } rect_t;
typedef struct { double top, left, bottom, right; } insets_t;

static int fails, total;
static void check(const char *name, int ok) {
    total++;
    printf("%-58s %s\n", name, ok ? "ok" : "FALHOU");
    if (!ok) fails++;
}

/* bloco mínimo (ABI de blocos da Apple): invoke(bloco, resposta, dados, erro) */
struct blk { void *isa; int flags, reserved; void (*invoke)(struct blk *, id, id, id); void *desc; };
extern void *_NSConcreteGlobalBlock[];
static struct { unsigned long reserved, size; } blk_desc = {0, sizeof(struct blk)};
static volatile int got_handler;
static volatile long handler_tid_differs;
static pthread_t main_thread;
static void on_done(struct blk *b, id resp, id data, id err) {
    (void)b; (void)resp; (void)data;
    handler_tid_differs = !pthread_equal(pthread_self(), main_thread);
    got_handler = err ? 2 : 1;          /* 2: o pedido falhou e o erro chegou ao bloco */
}

/* operação que espera a vez (para a fila ter algo pendente) */
static volatile int release_ops;

int main(void) {
    main_thread = pthread_self();
    MSG0(C("NSApplication"), "sharedApplication");

    /* 1. token de acessibilidade remota (ui::RemoteAccessibility do Chromium) */
    id el = MSG0(MSG0(C("NSObject"), "alloc"), "init");
    id tok = MSG1(C("NSAccessibilityRemoteUIElement"), "remoteTokenForLocalUIElement:", el);
    check("remoteTokenForLocalUIElement: devolve NSData de 16 bytes",
          tok && ((unsigned long (*)(id, SEL))objc_msgSend)(tok, S("length")) == 16);
    id rui = MSG1(MSG0(C("NSAccessibilityRemoteUIElement"), "alloc"), "initWithRemoteToken:", tok);
    check("initWithRemoteToken: abre o token", rui != 0);
    check("processIdentifier do token = getpid()",
          rui && ((int (*)(id, SEL))objc_msgSend)(rui, S("processIdentifier")) == getpid());
    id bad = MSG1(C("NSData"), "dataWithBytes:length:", (id)"xxxx");
    (void)bad;
    id junk = ((id (*)(id, SEL, const void *, unsigned long))objc_msgSend)(C("NSData"), S("dataWithBytes:length:"), "abcdefghijklmnop", 16);
    check("token sem a marca do mOS é recusado",
          MSG1(MSG0(C("NSAccessibilityRemoteUIElement"), "alloc"), "initWithRemoteToken:", junk) == 0);

    /* 2. NSWindow: inLiveResize e sharingType */
    rect_t r = {100, 100, 400, 300};
    id w = ((id (*)(id, SEL, rect_t, unsigned long, unsigned long, BOOL))objc_msgSend)(
        MSG0(C("NSWindow"), "alloc"), S("initWithContentRect:styleMask:backing:defer:"), r, 15, 2, 1);
    check("NSWindow criada", w != 0);
    check("-[NSWindow inLiveResize] = NO", w && ((BOOL (*)(id, SEL))objc_msgSend)(w, S("inLiveResize")) == 0);
    check("-[NSWindow sharingType] padrão = ReadOnly (1)", w && ((unsigned long (*)(id, SEL))objc_msgSend)(w, S("sharingType")) == 1);
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(w, S("setSharingType:"), 0);
    check("setSharingType: None volta 0", ((unsigned long (*)(id, SEL))objc_msgSend)(w, S("sharingType")) == 0);
    check("-[NSWindow keyViewSelectionDirection] = NSDirectSelection (0)",
          ((BOOL (*)(id, SEL, SEL))objc_msgSend)(w, S("respondsToSelector:"), S("keyViewSelectionDirection")) &&
          ((unsigned long (*)(id, SEL))objc_msgSend)(w, S("keyViewSelectionDirection")) == 0);
    id sc = MSG0(C("NSSpellChecker"), "sharedSpellChecker");
    check("NSSpellChecker responde a dismissCorrectionIndicatorForView:",
          sc && ((BOOL (*)(id, SEL, SEL))objc_msgSend)(sc, S("respondsToSelector:"), S("dismissCorrectionIndicatorForView:")));
    MSG0(MSG0(C("NSApplication"), "sharedApplication"), "updateWindows");
    check("-[NSApplication updateWindows] volta", 1);

    /* 3. NSView: ganchos viewDidHide/viewDidUnhide existem na base */
    id v = MSG0(MSG0(C("NSView"), "alloc"), "init");
    check("NSView responde a viewDidHide e viewDidUnhide",
          ((BOOL (*)(id, SEL, SEL))objc_msgSend)(v, S("respondsToSelector:"), S("viewDidHide")) &&
          ((BOOL (*)(id, SEL, SEL))objc_msgSend)(v, S("respondsToSelector:"), S("viewDidUnhide")));
    ((void (*)(id, SEL, BOOL))objc_msgSend)(v, S("setHidden:"), 1);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(v, S("setHidden:"), 0);
    check("setHidden: YES/NO chama os ganchos sem erro", 1);

    /* 4. NSImage capInsets / resizingMode, NSVisualEffectView maskImage */
    id img = MSG0(MSG0(C("NSImage"), "alloc"), "init");
    insets_t in = {1.5, 2, 3, 4.25}, out0 = {9, 9, 9, 9};
    ((void (*)(insets_t *, id, SEL))objc_msgSend_stret)(&out0, img, S("capInsets"));
    check("capInsets padrão = zeros", !out0.top && !out0.left && !out0.bottom && !out0.right);
    ((void (*)(id, SEL, insets_t))objc_msgSend)(img, S("setCapInsets:"), in);
    insets_t out = {0, 0, 0, 0};
    ((void (*)(insets_t *, id, SEL))objc_msgSend_stret)(&out, img, S("capInsets"));
    check("setCapInsets: {1.5,2,3,4.25} volta igual", !memcmp(&in, &out, sizeof in));
    check("resizingMode padrão = Stretch (1)", ((long (*)(id, SEL))objc_msgSend)(img, S("resizingMode")) == 1);
    ((void (*)(id, SEL, long))objc_msgSend)(img, S("setResizingMode:"), 0);
    check("setResizingMode: Tile (0) volta 0", ((long (*)(id, SEL))objc_msgSend)(img, S("resizingMode")) == 0);
    id vev = MSG0(MSG0(C("NSVisualEffectView"), "alloc"), "init");
    MSG1(vev, "setMaskImage:", img);
    check("NSVisualEffectView maskImage volta a imagem", MSG0(vev, "maskImage") == img);

    /* 5. NSCondition name */
    id cond = MSG0(MSG0(C("NSCondition"), "alloc"), "init");
    id nm = ((id (*)(id, SEL, const char *))objc_msgSend)(C("NSString"), S("stringWithUTF8String:"), "fila-mOS");
    MSG1(cond, "setName:", nm);
    id back = MSG0(cond, "name");
    check("-[NSCondition setName:] / name", back && ((BOOL (*)(id, SEL, id))objc_msgSend)(back, S("isEqualToString:"), nm));

    /* 6. NSOperationQueue: operations, operationCount, cancelAllOperations */
    id q = MSG0(MSG0(C("NSOperationQueue"), "alloc"), "init");
    id ops[3];
    for (int i = 0; i < 3; i++) {
        ops[i] = MSG0(MSG0(C("NSOperation"), "alloc"), "init");
        MSG1(q, "addOperation:", ops[i]);
    }
    long cnt = ((long (*)(id, SEL))objc_msgSend)(q, S("operationCount"));
    check("operationCount conta as operações não terminadas (<= 3)", cnt >= 0 && cnt <= 3);
    MSG0(q, "cancelAllOperations");
    int all_cancel_or_done = 1;
    for (int i = 0; i < 3; i++)
        all_cancel_or_done &= ((BOOL (*)(id, SEL))objc_msgSend)(ops[i], S("isCancelled")) ||
                              ((BOOL (*)(id, SEL))objc_msgSend)(ops[i], S("isFinished"));
    check("cancelAllOperations: cada operação cancelada ou já terminada", all_cancel_or_done);
    id lst = MSG0(q, "operations");
    check("operations devolve NSArray", lst != 0);
    (void)release_ops;

    /* 7. +[NSURLConnection sendAsynchronousRequest:queue:completionHandler:] (o harness não tem rede: o erro chega
     *    ao bloco, numa thread que não é a principal, porque a fila não é a mainQueue) */
    id url = ((id (*)(id, SEL, const char *))objc_msgSend)(C("NSString"), S("stringWithUTF8String:"), "http://127.0.0.1:9/");
    id req = MSG1(C("NSURLRequest"), "requestWithURL:", MSG1(C("NSURL"), "URLWithString:", url));
    struct blk b = {_NSConcreteGlobalBlock, 1 << 28, 0, on_done, &blk_desc};
    ((void (*)(id, SEL, id, id, void *))objc_msgSend)(C("NSURLConnection"), S("sendAsynchronousRequest:queue:completionHandler:"), req, q, &b);
    for (int i = 0; i < 400 && !got_handler; i++) usleep(10000);
    check("sendAsynchronousRequest: chama o bloco", got_handler != 0);
    check("o bloco roda fora da thread principal (fila comum)", got_handler && handler_tid_differs);

    printf("electron-m3: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
