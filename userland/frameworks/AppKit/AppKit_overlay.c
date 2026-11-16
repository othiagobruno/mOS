/* mOS — funções do overlay Swift do AppKit escritas em C com a convenção Swift (libswiftAppKit.dylib, junto do
 * AppKit.swift). Os nomes são os mangled da Apple e o corpo chama o AppKit do libSystem.mos. CGRect/CGPoint/CGSize vão
 * em registradores de ponto flutuante; o contexto de um closure vai em swiftself (swift_context). Em Mach-O o rótulo
 * do __asm__ é o nome exato do símbolo: por isso o "_" inicial. */
typedef struct { double x, y, w, h; } ak_rect;
typedef struct { double x, y; } ak_point;
typedef struct { double w, h; } ak_size;
typedef unsigned long u64;

void NSRectClip(ak_rect r);
void NSRectFill(ak_rect r);
void NSRectFillUsingOperation(ak_rect r, u64 op);
void NSFrameRectWithWidthUsingOperation(ak_rect r, double width, u64 op);
void NSBeep(void);
void NSShowAnimationEffect(u64 effect, ak_point center, ak_size size, void *delegate, void *did_end, void *ctx);
int NSApplicationMain(int argc, const char **argv);

/* NSApplicationMain(_:_:) do Swift */
int ak_swift_main(int argc, char **argv) __asm__("_$s6AppKit17NSApplicationMainys5Int32VAD_SpySpys4Int8VGSgGtF");
int ak_swift_main(int argc, char **argv) { return NSApplicationMain(argc, (const char **)argv); }

/* CGRect.fill(using:), CGRect.frame(withWidth:using:), CGRect.clip() */
void ak_rect_fill(double x, double y, double w, double h, u64 op) __asm__("_$sSo6CGRectV6AppKitE4fill5usingySo22NSCompositingOperationV_tF");
void ak_rect_fill(double x, double y, double w, double h, u64 op) { NSRectFillUsingOperation((ak_rect){x, y, w, h}, op); }
void ak_rect_frame(double x, double y, double w, double h, double width, u64 op)
    __asm__("_$sSo6CGRectV6AppKitE5frame9withWidth5usingy12CoreGraphics7CGFloatV_So22NSCompositingOperationVtF");
void ak_rect_frame(double x, double y, double w, double h, double width, u64 op) { NSFrameRectWithWidthUsingOperation((ak_rect){x, y, w, h}, width, op); }
void ak_rect_clip(double x, double y, double w, double h) __asm__("_$sSo6CGRectV6AppKitE4clipyyF");
void ak_rect_clip(double x, double y, double w, double h) { NSRectClip((ak_rect){x, y, w, h}); }

/* NSSound.beep() */
void ak_sound_beep(void) __asm__("_$sSo7NSSoundC6AppKitE4beepyyFZ");
void ak_sound_beep(void) { NSBeep(); }

/* NSAnimationEffect.show(centeredAt:size:completionHandler:): o efeito roda e depois o closure */
typedef __attribute__((swiftcall)) void (*ak_closure_fn)(__attribute__((swift_context)) void *ctx);
__attribute__((swiftcall)) void ak_effect_show(double px, double py, double w, double h, ak_closure_fn fn, void *ctx, u64 effect)
    __asm__("_$sSo17NSAnimationEffectV6AppKitE4show10centeredAt4size17completionHandlerySo7CGPointV_So6CGSizeVyyctF");
__attribute__((swiftcall)) void ak_effect_show(double px, double py, double w, double h, ak_closure_fn fn, void *ctx, u64 effect) {
    NSShowAnimationEffect(effect, (ak_point){px, py}, (ak_size){w, h}, 0, 0, 0);
    if (fn) fn(ctx);
}
