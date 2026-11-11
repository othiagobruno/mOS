/* mOS — o mínimo da Foundation que o ExtensionFoundation/ExtensionKit em Swift usa (NSXPCConnection, NSXPCListener),
 * para os nomes mangled serem os da Apple (__C.NSXPCConnection). Só para compilar os módulos Swift; as classes são as do
 * libSystem (xpc.c, foundation_more.c). */
#pragma once
#import <objc/NSObject.h>

@interface NSXPCConnection : NSObject
- (void)resume;
- (void)invalidate;
@end

@class NSXPCListener;
@protocol NSXPCListenerDelegate <NSObject>
@optional
- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection;
@end

@interface NSXPCListener : NSObject
+ (NSXPCListener *)serviceListener;
@property (nullable, weak) id<NSXPCListenerDelegate> delegate;
- (void)resume;
@end
