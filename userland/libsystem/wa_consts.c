/*
 * mOS libSystem — constantes NSString de notificação (CoreFoundation/Foundation e AppKit) que o WhatsApp importa e que
 * faltavam. Sem elas, no modo tolerante do carregador o nome chega nil, e um observador com nome nil recebe todas
 * as notificações. O contexto de i18n do WhatsApp observava NSCurrentLocaleDidChangeNotification, recebia o próprio
 * "didChange" e entrava em laço. Os valores são os do macOS.
 */
#include "sys.h"
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } wa_const_t;
#define WK(var, text) static wa_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
WK(NSCurrentLocaleDidChangeNotification, "kCFLocaleCurrentLocaleDidChangeNotification")
WK(NSSystemClockDidChangeNotification, "NSSystemClockDidChangeNotification")
WK(NSWindowDidMiniaturizeNotification, "NSWindowDidMiniaturizeNotification")
WK(NSWindowDidDeminiaturizeNotification, "NSWindowDidDeminiaturizeNotification")
WK(NSWindowDidUpdateNotification, "NSWindowDidUpdateNotification")
WK(NSWorkspaceDidWakeNotification, "NSWorkspaceDidWakeNotification")
WK(NSWorkspaceWillSleepNotification, "NSWorkspaceWillSleepNotification")
