/*
 * mOS SDK (variante com interop ObjC, docs/swift-objc-runtime.md, fase 5) — as opções NS_OPTIONS do Foundation que, no
 * Foundation da Apple (FOUNDATION_FRAMEWORK), são os tipos de Data.ReadingOptions, String.CompareOptions etc. Os nomes
 * mangled dos binários da Apple levam o nome C delas (So20NSDataReadingOptionsV), então o FoundationEssentials do mOS
 * as usa como apelidos (tools\build-swift-foundation-objc.ps1 remenda só esses #if FOUNDATION_FRAMEWORK).
 * Módulo clang próprio (_MOSNSOptions) para o FoundationEssentials não depender do Foundation em ObjC; o Foundation.h o
 * inclui. Sem aninhamento por swift_name (NSData.ReadingOptions só muda o nome no código-fonte, não no ABI); os membros
 * levam os nomes Swift da Apple.
 */
#pragma once
#include <objc/NSObjCRuntime.h>

#define MOS_OPTIONS(T, N) enum __attribute__((flag_enum, enum_extensibility(open))) N : T N; enum N : T
#define MOS_SN(n) __attribute__((swift_name(#n)))

/* ---- NSData */
typedef MOS_OPTIONS(NSUInteger, NSDataReadingOptions) {
    NSDataReadingMappedIfSafe MOS_SN(mappedIfSafe) = 1UL << 0,
    NSDataReadingUncached MOS_SN(uncached) = 1UL << 1,
    NSDataReadingMappedAlways MOS_SN(alwaysMapped) = 1UL << 3,
};

typedef MOS_OPTIONS(NSUInteger, NSDataWritingOptions) {
    NSDataWritingAtomic MOS_SN(atomic) = 1UL << 0,
    NSDataWritingWithoutOverwriting MOS_SN(withoutOverwriting) = 1UL << 1,
    NSDataWritingFileProtectionNone MOS_SN(noFileProtection) = 0x10000000,
    NSDataWritingFileProtectionComplete MOS_SN(completeFileProtection) = 0x20000000,
    NSDataWritingFileProtectionCompleteUnlessOpen MOS_SN(completeFileProtectionUnlessOpen) = 0x30000000,
    NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication MOS_SN(completeFileProtectionUntilFirstUserAuthentication) = 0x40000000,
    NSDataWritingFileProtectionCompleteWhenUserInactive MOS_SN(completeFileProtectionWhenUserInactive) = 0x50000000,
    NSDataWritingFileProtectionMask MOS_SN(fileProtectionMask) = 0xf0000000,
};

typedef MOS_OPTIONS(NSUInteger, NSDataBase64EncodingOptions) {
    NSDataBase64Encoding64CharacterLineLength MOS_SN(lineLength64Characters) = 1UL << 0,
    NSDataBase64Encoding76CharacterLineLength MOS_SN(lineLength76Characters) = 1UL << 1,
    NSDataBase64EncodingEndLineWithCarriageReturn MOS_SN(endLineWithCarriageReturn) = 1UL << 4,
    NSDataBase64EncodingEndLineWithLineFeed MOS_SN(endLineWithLineFeed) = 1UL << 5,
};

typedef MOS_OPTIONS(NSUInteger, NSDataBase64DecodingOptions) {
    NSDataBase64DecodingIgnoreUnknownCharacters MOS_SN(ignoreUnknownCharacters) = 1UL << 0,
};

/* ---- NSString */
typedef MOS_OPTIONS(NSUInteger, NSStringCompareOptions) {
    NSCaseInsensitiveSearch MOS_SN(caseInsensitive) = 1,
    NSLiteralSearch MOS_SN(literal) = 2,
    NSBackwardsSearch MOS_SN(backwards) = 4,
    NSAnchoredSearch MOS_SN(anchored) = 8,
    NSNumericSearch MOS_SN(numeric) = 64,
    NSDiacriticInsensitiveSearch MOS_SN(diacriticInsensitive) = 128,
    NSWidthInsensitiveSearch MOS_SN(widthInsensitive) = 256,
    NSForcedOrderingSearch MOS_SN(forcedOrdering) = 512,
    NSRegularExpressionSearch MOS_SN(regularExpression) = 1024,
};

/* ---- NSURL (bookmarks) */
typedef MOS_OPTIONS(NSUInteger, NSURLBookmarkCreationOptions) {
    NSURLBookmarkCreationPreferFileIDResolution MOS_SN(preferFileIDResolution) = 1UL << 8,
    NSURLBookmarkCreationMinimalBookmark MOS_SN(minimalBookmark) = 1UL << 9,
    NSURLBookmarkCreationSuitableForBookmarkFile MOS_SN(suitableForBookmarkFile) = 1UL << 10,
    NSURLBookmarkCreationWithSecurityScope MOS_SN(withSecurityScope) = 1UL << 11,
    NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess MOS_SN(securityScopeAllowOnlyReadAccess) = 1UL << 12,
    NSURLBookmarkCreationWithoutImplicitSecurityScope MOS_SN(withoutImplicitSecurityScope) = 1UL << 29,
};

typedef MOS_OPTIONS(NSUInteger, NSURLBookmarkResolutionOptions) {
    NSURLBookmarkResolutionWithoutUI MOS_SN(withoutUI) = 1UL << 8,
    NSURLBookmarkResolutionWithoutMounting MOS_SN(withoutMounting) = 1UL << 9,
    NSURLBookmarkResolutionWithSecurityScope MOS_SN(withSecurityScope) = 1UL << 10,
    NSURLBookmarkResolutionWithoutImplicitStartAccessing MOS_SN(withoutImplicitStartAccessing) = 1UL << 15,
};

#undef MOS_SN
#undef MOS_OPTIONS
