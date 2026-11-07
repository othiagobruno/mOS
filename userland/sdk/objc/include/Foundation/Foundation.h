/*
 * mOS — Foundation mínimo para compilar o runtime Swift com interop ObjC (tools/build-swift-runtime-objc.ps1),
 * sobre os cabeçalhos objc/*.h que o zig traz. Só as classes e métodos que o runtime e os stubs mandam;
 * as classes existem no libSystem (foundation.c). O Foundation dos apps é userland/sdk/include/Foundation.
 */
#pragma once
#include <objc/NSObject.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

typedef struct _NSZone NSZone;
@class NSString, NSDictionary, NSCoder;

@protocol NSCopying
- (id)copyWithZone:(NSZone *)zone;
@end

@interface NSObject (MOSFoundation)
- (id)copy;
- (id)mutableCopy;
@property (readonly, copy) NSString *description;
@property (readonly, copy) NSString *debugDescription;
@end

@interface NSString : NSObject <NSCopying>
+ (instancetype)stringWithUTF8String:(const char *)s;
- (instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)len encoding:(NSUInteger)enc;
- (const char *)UTF8String;
- (NSUInteger)length;
- (BOOL)isEqualToString:(NSString *)s;
@end
#define NSUTF8StringEncoding 4

@interface NSDictionary : NSObject <NSCopying>
- (NSUInteger)count;
- (id)objectForKey:(id)key;
@end

@interface NSError : NSObject <NSCopying>
+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)userInfo;
- (instancetype)initWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)userInfo;
@property (readonly, copy) NSString *domain;
@property (readonly) NSInteger code;
@property (readonly, copy) NSDictionary *userInfo;
@end

@interface NSNull : NSObject <NSCopying>
+ (NSNull *)null;
@end

@interface NSCoder : NSObject
@end

__attribute__((objc_root_class))
@interface NSProxy <NSObject> {
    Class isa;
}
+ (id)alloc;
+ (Class)class;
@end
