/* mOS — o mínimo das interfaces do NaturalLanguage para o overlay Swift (userland/frameworks/NaturalLanguage/
 * NaturalLanguage.swift) ter os nomes mangled da Apple (__C.NLLanguageRecognizer, __C.NLLanguage). Só para compilar
 * o overlay: as classes da Foundation daqui têm só o que ele usa para converter o dicionário. */
#pragma once
#import <objc/NSObject.h>

@interface NSString : NSObject
- (nonnull const char *)UTF8String;
@end

@interface NSNumber : NSObject
- (double)doubleValue;
@end

@interface NSArray : NSObject
- (unsigned long)count;
- (id)objectAtIndex:(unsigned long)index;
@end

@interface NSDictionary : NSObject
- (nonnull NSArray *)allKeys;
- (nullable id)objectForKey:(id)key;
@end

typedef NSString *NLLanguage __attribute__((swift_wrapper(struct)));

@interface NLLanguageRecognizer : NSObject
- (void)processString:(NSString *)string;
- (nullable NSDictionary *)languageHypothesesWithMaximum:(unsigned long)maxHypotheses __attribute__((swift_private));
@end
