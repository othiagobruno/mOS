/*
 * mOS SDK — incluído antes de cada fonte C/C++ das dependências do OpenSwiftUI (-include).
 * O CoreFoundation que vem com o OpenAttributeGraph (SwiftCorelibs) é um subconjunto: completa aqui
 * os tipos opacos que os cabeçalhos públicos citam.
 *
 * CFRetain/CFRelease/CFGetRetainCount passam por mos_oag_CF* (userland/sdk/openswiftui/src/oag_mos.cpp):
 * os objetos CF do OpenAttributeGraph são objetos Swift nativos (o Swift sem ObjC os retém com
 * swift_retain), enquanto os do libSystem são objetos ObjC do mOS; a função certa é escolhida pelo isa.
 */
#pragma once
#include <CoreFoundation/CFBase.h>

typedef const struct __CFURL *CFURLRef;

#ifdef __cplusplus
extern "C" {
#endif
CFTypeRef mos_oag_CFRetain(CFTypeRef cf);
void mos_oag_CFRelease(CFTypeRef cf);
CFIndex mos_oag_CFGetRetainCount(CFTypeRef cf);
CFTypeRef CFAutorelease(CFTypeRef cf);   /* cf_mos.cpp (falta no CoreFoundation do swift-corelibs) */
#ifdef __cplusplus
}
#endif
#define CFRetain mos_oag_CFRetain
#define CFRelease mos_oag_CFRelease
#define CFGetRetainCount mos_oag_CFGetRetainCount
