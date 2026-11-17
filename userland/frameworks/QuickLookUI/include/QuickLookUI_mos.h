/* mOS — o mínimo do QuickLookUI para o overlay Swift (QuickLookUI.swift). As classes são do libSystem.mos
 * (quicklook_fw.c); os métodos marcados swift_private ficam com "__" no Swift e o overlay põe a forma da Apple. */
#pragma once
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

@interface QLPreviewReply : NSObject
- (nonnull instancetype)initWithDataOfContentType:(nonnull id)contentType contentSize:(CGSize)contentSize
                                dataCreationBlock:(NSData * _Nullable (^ _Nonnull)(QLPreviewReply * _Nonnull reply, NSError * _Nullable * _Nullable error))block
    __attribute__((swift_private));
- (nonnull instancetype)initWithFileURL:(nonnull NSURL *)fileURL;
@property (nonatomic) unsigned long stringEncoding __attribute__((swift_private));
@property (nonatomic, copy, nullable) NSString *title;
@end
