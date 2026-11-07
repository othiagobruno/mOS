/*
 * mOS SDK (variante com interop ObjC, docs/swift-objc-runtime.md, fase 5) — UniformTypeIdentifiers em ObjC para o
 * overlay Swift (userland/sdk/swift-objc/UniformTypeIdentifiers -> libswiftUniformTypeIdentifiers.dylib). A classe é a
 * UTType do libSystem do mOS (userland/libsystem/vscode_media.c). Como na Apple, a classe ObjC fica escondida do Swift
 * (o struct UTType do overlay toma o nome); os nomes mangled da ponte saem do nome ObjC, __C.UTType.
 */
#pragma once
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/* as classes de tag: o struct UTTagClass é do overlay (o nome mangled é UniformTypeIdentifiers.UTTagClass); aqui só as
 * constantes, escondidas do Swift */
FOUNDATION_EXPORT NSString *const UTTagClassFilenameExtension NS_REFINED_FOR_SWIFT;
FOUNDATION_EXPORT NSString *const UTTagClassMIMEType NS_REFINED_FOR_SWIFT;

__attribute__((swift_name("_UTTypeObjC")))
@interface UTType : NSObject
+ (nullable instancetype)typeWithIdentifier:(NSString *)identifier;
+ (instancetype)exportedTypeWithIdentifier:(NSString *)identifier conformingToType:(UTType *)parentType;
+ (instancetype)exportedTypeWithIdentifier:(NSString *)identifier;
+ (nullable instancetype)typeWithFilenameExtension:(NSString *)filenameExtension conformingToType:(UTType *)supertype;
+ (nullable instancetype)typeWithMIMEType:(NSString *)mimeType conformingToType:(UTType *)supertype;
+ (NSArray<UTType *> *)typesWithTag:(NSString *)tag tagClass:(NSString *)tagClass conformingToType:(nullable UTType *)supertype;
@property (readonly, copy) NSString *identifier;
@property (readonly, nullable, copy) NSString *preferredFilenameExtension;
@property (readonly, nullable, copy) NSString *preferredMIMEType;
@property (readonly, nullable, copy) NSString *localizedDescription;
@property (readonly, getter=isDynamic) BOOL dynamic;
- (BOOL)conformsToType:(UTType *)type;
@end

NS_ASSUME_NONNULL_END
