/* App de exemplo do K19/K20: um NSStatusItem "Usage" com menu "Abrir".
 * flagsChanged: troca o título para F1, F2, … para o screendump mostrar o evento. */
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
extern id objc_getClass(const char *);
extern SEL sel_registerName(const char *);
extern id objc_msgSend(id, SEL, ...);
extern id objc_retain(id);
extern void *objc_allocateClassPair(void *, const char *, unsigned long);
extern void objc_registerClassPair(void *);
extern signed char class_addMethod(void *, SEL, void *, const char *);
extern int printf(const char *, ...);
extern int fflush(void *);

static id item;
static int nflag;

static id nsstr(const char *s) {
    return ((id (*)(id, SEL, const char *))objc_msgSend)(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), s);
}

static void on_flags(id self, SEL s, id ev) {
    unsigned long long mods;
    char buf[8];
    (void)self; (void)s;
    mods = ((unsigned long long (*)(id, SEL))objc_msgSend)(ev, sel_registerName("modifierFlags"));
    if (nflag < 9) nflag++;
    buf[0] = 'F';
    buf[1] = (char)('0' + nflag);
    buf[2] = 0;
    ((void (*)(id, SEL, id))objc_msgSend)(item, sel_registerName("setTitle:"), nsstr(buf));
    printf("flagsChanged n=%d mods=0x%llx\n", nflag, mods);
    fflush(0);
}

int main(void) {
    id app, bar, menu, win, view;
    void *cls;
    app = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
    ((void (*)(id, SEL, long long))objc_msgSend)(app, sel_registerName("setActivationPolicy:"), 0);
    bar = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSStatusBar"), sel_registerName("systemStatusBar"));
    item = ((id (*)(id, SEL, double))objc_msgSend)(bar, sel_registerName("statusItemWithLength:"), (double)-1.0);
    item = objc_retain(item);
    ((void (*)(id, SEL, id))objc_msgSend)(item, sel_registerName("setTitle:"), nsstr("Usage"));
    menu = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSMenu"), sel_registerName("alloc")), sel_registerName("init"));
    menu = objc_retain(menu);
    ((id (*)(id, SEL, id, SEL, id))objc_msgSend)(menu, sel_registerName("addItemWithTitle:action:keyEquivalent:"), nsstr("Abrir"), (SEL)0, nsstr(""));
    ((void (*)(id, SEL, id))objc_msgSend)(item, sel_registerName("setMenu:"), menu);
    cls = objc_allocateClassPair(objc_getClass("NSView"), "StatusFlagsView", 0);
    if (cls) {
        class_addMethod(cls, sel_registerName("flagsChanged:"), (void *)on_flags, "v@:@");
        objc_registerClassPair(cls);
        view = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cls, sel_registerName("alloc")), sel_registerName("init"));
        view = objc_retain(view);
        win = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSWindow"), sel_registerName("alloc")), sel_registerName("init"));
        win = objc_retain(win);
        ((void (*)(id, SEL, id))objc_msgSend)(win, sel_registerName("setContentView:"), view);
        ((void (*)(id, SEL, id))objc_msgSend)(win, sel_registerName("setTitle:"), nsstr("Status"));
        ((void (*)(id, SEL, id))objc_msgSend)(win, sel_registerName("makeKeyAndOrderFront:"), (id)0);
    }
    printf("statusbar-demo: item Usage\n");
    fflush(0);
    ((void (*)(id, SEL))objc_msgSend)(app, sel_registerName("run"));
    return 0;
}
