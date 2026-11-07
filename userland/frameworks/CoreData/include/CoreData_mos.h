/* mOS — o mínimo das interfaces do CoreData para o overlay Swift (userland/frameworks/CoreData/CoreData.swift) ter os
 * nomes mangled da Apple (__C.NSManagedObjectContext, __C.NSFetchRequest<A>, __C.NSFetchRequestResult). Só para
 * compilar o overlay; o NSArray daqui é só o que ele usa para converter o resultado. */
#pragma once
#import <objc/NSObject.h>

@interface NSArray : NSObject
- (unsigned long)count;
- (id)objectAtIndex:(unsigned long)index;
@end

@protocol NSFetchRequestResult <NSObject>
@end

@interface NSFetchRequest<__covariant ResultType: id<NSFetchRequestResult>> : NSObject
@end

@interface NSManagedObjectContext : NSObject
- (nullable NSArray *)executeFetchRequest:(NSFetchRequest *)request error:(id _Nullable * _Nullable)error;
@end
