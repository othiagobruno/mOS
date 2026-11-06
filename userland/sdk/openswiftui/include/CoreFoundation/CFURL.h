/*
 * mOS SDK — CFURL mínimo para as deps do OpenSwiftUI (o CoreFoundation do swift-corelibs que vem com elas
 * não tem). CFURLRef já vem do prelude; CFURLCreateWithBytes está em userland/sdk/openswiftui/src/cf_mos.cpp.
 */
#pragma once
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>

CF_EXTERN_C_BEGIN
CF_EXPORT CFURLRef CFURLCreateWithBytes(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length, CFStringEncoding encoding, CFURLRef baseURL);
CF_EXTERN_C_END
