/*
 * mOS — CoreFoundation mínimo para compilar o runtime Swift com interop ObjC (tools/build-swift-runtime-objc.ps1).
 * Só os tipos e funções que o runtime e os stubs usam; as funções vêm do CF do libSystem (foundation.c).
 * O CoreFoundation completo do SDK do mOS fica em userland/sdk/openswiftui/importer (para o importador Swift).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef const void *CFTypeRef;
typedef unsigned long CFTypeID;
typedef unsigned long CFOptionFlags;
typedef unsigned long CFHashCode;
typedef signed long CFIndex;
typedef unsigned char Boolean;
typedef const struct __CFString *CFStringRef;
typedef struct __CFString *CFMutableStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef struct __CFError *CFErrorRef;
typedef CFStringRef CFErrorDomain;
typedef struct { CFIndex location, length; } CFRange;
typedef unsigned int CFStringEncoding;
#define kCFStringEncodingUTF8 0x08000100

CFTypeID CFGetTypeID(CFTypeRef cf);
CFTypeID CFStringGetTypeID(void);
CFTypeID CFErrorGetTypeID(void);
CFTypeRef CFRetain(CFTypeRef cf);
void CFRelease(CFTypeRef cf);
Boolean CFEqual(CFTypeRef a, CFTypeRef b);
CFHashCode CFHash(CFTypeRef cf);
CFIndex CFStringGetLength(CFStringRef s);
CFErrorDomain CFErrorGetDomain(CFErrorRef e);
CFIndex CFErrorGetCode(CFErrorRef e);
CFDictionaryRef CFErrorCopyUserInfo(CFErrorRef e);
#ifdef __cplusplus
}
#endif
