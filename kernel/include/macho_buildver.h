#pragma once
#include <stdint.h>
/* PLATFORM_MACCATALYST no load command LC_BUILD_VERSION (mach-o/loader.h). */
#define MOS_PLATFORM_MACCATALYST 6u
/* Plataforma 0 (comando ausente no campo, ou curto) vira macCatalyst para apps iPad/Catalyst. */
static inline uint32_t mos_build_version_platform(uint32_t raw) {
    return raw ? raw : MOS_PLATFORM_MACCATALYST;
}
