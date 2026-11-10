/*
 * mOS — CoreData.framework (Mach-O, em /System/Library/Frameworks). Esta dylib continua só em memória:
 * é o caminho do Postico quando o arquivo está na imagem. O loader prefere este arquivo ao libSystem.
 * Slack, Claude Desktop e Cursor só têm o LC_LOAD_DYLIB (nenhum símbolo). Sem este arquivo no disco,
 * o libSystem atende com a loja MOSCD1 de userland/libsystem/coredata_store.inc (coredata_fw.c).
 * Esse arquivo não é SQLite. Ainda não há modelos compilados (.momd).
 * O overlay Swift (NSManagedObjectContext.fetch) está em CoreData.swift. tools/build-frameworks-objc.ps1
 */
#import <Foundation/Foundation.h>

NSString *const NSSQLiteStoreType = @"SQLite";
NSString *const NSInMemoryStoreType = @"InMemory";
NSString *const NSBinaryStoreType = @"Binary";
NSString *const NSXMLStoreType = @"XML";
NSString *const NSInferMappingModelAutomaticallyOption = @"NSInferMappingModelAutomaticallyOption";
NSString *const NSMigratePersistentStoresAutomaticallyOption = @"NSMigratePersistentStoresAutomaticallyOption";
NSString *const NSReadOnlyPersistentStoreOption = @"NSReadOnlyPersistentStoreOption";
NSString *const NSPersistentStoreFileProtectionKey = @"NSPersistentStoreFileProtectionKey";
NSString *const NSSQLitePragmasOption = @"NSSQLitePragmasOption";
NSString *const NSManagedObjectContextDidSaveNotification = @"NSManagedObjectContextDidSaveNotification";
NSString *const NSManagedObjectContextObjectsDidChangeNotification = @"NSManagedObjectContextObjectsDidChangeNotification";
NSString *const NSInsertedObjectsKey = @"inserted";
NSString *const NSUpdatedObjectsKey = @"updated";
NSString *const NSDeletedObjectsKey = @"deleted";

static void cd_warn_once(void) {
    static int w;
    if (!w++) fprintf(stderr, "CoreData[mOS]: armazenamento só em memória (sem SQLite ainda); nada é gravado em disco\n");
}

@interface NSEntityDescription : NSObject
@property (copy) NSString *name;
@property (copy) NSString *managedObjectClassName;
+ (instancetype)entityForName:(NSString *)name inManagedObjectContext:(id)ctx;
+ (id)insertNewObjectForEntityForName:(NSString *)name inManagedObjectContext:(id)ctx;
@end

@interface NSManagedObjectModel : NSObject
@property (copy) NSArray *entities;
- (instancetype)initWithContentsOfURL:(id)url;
+ (instancetype)mergedModelFromBundles:(NSArray *)bundles;
- (NSDictionary *)entitiesByName;
@end

@interface NSPersistentStoreCoordinator : NSObject
@property (readonly, strong) NSManagedObjectModel *managedObjectModel;
@property (readonly, copy) NSArray *persistentStores;
- (instancetype)initWithManagedObjectModel:(NSManagedObjectModel *)model;
- (id)addPersistentStoreWithType:(NSString *)type configuration:(NSString *)conf URL:(id)url options:(NSDictionary *)opts error:(id *)error;
@end

@interface NSFetchRequest : NSObject <NSCopying>
@property (copy) NSString *entityName;
@property (strong) NSEntityDescription *entity;
@property (strong) id predicate;
@property (copy) NSArray *sortDescriptors;
@property NSUInteger fetchLimit;
@property NSUInteger fetchOffset;
@property NSUInteger fetchBatchSize;
@property NSUInteger resultType;
@property BOOL returnsObjectsAsFaults;
+ (instancetype)fetchRequestWithEntityName:(NSString *)name;
- (instancetype)initWithEntityName:(NSString *)name;
@end

@interface NSManagedObjectContext : NSObject
@property (strong) NSPersistentStoreCoordinator *persistentStoreCoordinator;
@property (assign) NSManagedObjectContext *parentContext;
@property BOOL automaticallyMergesChangesFromParent;
@property (strong) id mergePolicy;
@property (strong) id undoManager;
- (instancetype)initWithConcurrencyType:(NSUInteger)type;
- (void)insertObject:(id)obj;
- (void)deleteObject:(id)obj;
- (NSArray *)executeFetchRequest:(NSFetchRequest *)request error:(id *)error;
- (NSUInteger)countForFetchRequest:(NSFetchRequest *)request error:(id *)error;
- (BOOL)save:(id *)error;
- (BOOL)hasChanges;
- (void)performBlock:(void (^)(void))block;
- (void)performBlockAndWait:(void (^)(void))block;
- (void)rollback;
- (void)reset;
- (id)existingObjectWithID:(id)oid error:(id *)error;
- (id)objectWithID:(id)oid;
@end

@interface NSManagedObject : NSObject
@property (readonly, strong) NSEntityDescription *entity;
@property (readonly, assign) NSManagedObjectContext *managedObjectContext;
- (instancetype)initWithEntity:(NSEntityDescription *)entity insertIntoManagedObjectContext:(NSManagedObjectContext *)ctx;
- (instancetype)initWithContext:(NSManagedObjectContext *)ctx;
+ (NSFetchRequest *)fetchRequest;
+ (NSEntityDescription *)entity;
- (id)objectID;
- (BOOL)isInserted;
- (BOOL)isDeleted;
- (BOOL)isFault;
@end

/* ---------------------------------------------------------------- implementação */

@implementation NSEntityDescription
+ (instancetype)entityForName:(NSString *)name inManagedObjectContext:(id)ctx {
    NSEntityDescription *e = [[[self alloc] init] autorelease];
    e.name = name;
    e.managedObjectClassName = name;
    return e;
}
+ (id)insertNewObjectForEntityForName:(NSString *)name inManagedObjectContext:(id)ctx {
    NSEntityDescription *e = [self entityForName:name inManagedObjectContext:ctx];
    Class cls = NSClassFromString(name);
    if (!cls || ![cls isSubclassOfClass:[NSManagedObject class]]) cls = [NSManagedObject class];
    return [[[cls alloc] initWithEntity:e insertIntoManagedObjectContext:ctx] autorelease];
}
@end

@implementation NSManagedObjectModel
- (instancetype)initWithContentsOfURL:(id)url { cd_warn_once(); return [self init]; }
+ (instancetype)mergedModelFromBundles:(NSArray *)bundles { return [[[self alloc] init] autorelease]; }
- (NSDictionary *)entitiesByName { return [NSDictionary dictionary]; }
@end

@implementation NSPersistentStoreCoordinator {
    NSMutableArray *_stores;
}
- (instancetype)initWithManagedObjectModel:(NSManagedObjectModel *)model {
    if ((self = [super init])) { _managedObjectModel = [model retain]; _stores = [[NSMutableArray alloc] init]; }
    return self;
}
- (NSArray *)persistentStores { return _stores; }
- (id)addPersistentStoreWithType:(NSString *)type configuration:(NSString *)conf URL:(id)url options:(NSDictionary *)opts error:(id *)error {
    cd_warn_once();
    id store = [[[NSObject alloc] init] autorelease];
    [_stores addObject:store];
    return store;
}
@end

@implementation NSFetchRequest
+ (instancetype)fetchRequestWithEntityName:(NSString *)name { return [[[self alloc] initWithEntityName:name] autorelease]; }
- (instancetype)initWithEntityName:(NSString *)name { if ((self = [super init])) _entityName = [name copy]; return self; }
- (id)copyWithZone:(void *)zone {
    NSFetchRequest *r = [[NSFetchRequest alloc] initWithEntityName:_entityName];
    r.predicate = _predicate; r.sortDescriptors = _sortDescriptors; r.fetchLimit = _fetchLimit; r.resultType = _resultType;
    return r;
}
- (NSString *)entityName { return _entityName ? _entityName : _entity.name; }
@end

@implementation NSManagedObject {
    NSMutableDictionary *_values;
    NSEntityDescription *_entity;
    NSManagedObjectContext *_ctx;
    BOOL _deleted;
}
- (instancetype)initWithEntity:(NSEntityDescription *)entity insertIntoManagedObjectContext:(NSManagedObjectContext *)ctx {
    if ((self = [super init])) {
        _values = [[NSMutableDictionary alloc] init];
        _entity = [entity retain];
        if (ctx) [ctx insertObject:self];
    }
    return self;
}
- (instancetype)initWithContext:(NSManagedObjectContext *)ctx {
    return [self initWithEntity:[[self class] entity] insertIntoManagedObjectContext:ctx];
}
+ (NSEntityDescription *)entity { return [NSEntityDescription entityForName:NSStringFromClass(self) inManagedObjectContext:nil]; }
+ (NSFetchRequest *)fetchRequest { return [NSFetchRequest fetchRequestWithEntityName:NSStringFromClass(self)]; }
- (NSEntityDescription *)entity { return _entity; }
- (NSManagedObjectContext *)managedObjectContext { return _ctx; }
- (void)_mos_setContext:(NSManagedObjectContext *)c { _ctx = c; }
- (void)_mos_setDeleted:(BOOL)d { _deleted = d; }
- (id)objectID { return self; }
- (BOOL)isInserted { return _ctx != nil; }
- (BOOL)isDeleted { return _deleted; }
- (BOOL)isFault { return NO; }
- (id)valueForKey:(NSString *)key { return [_values objectForKey:key]; }
- (void)setValue:(id)value forKey:(NSString *)key {
    if (value) [_values setObject:value forKey:key]; else [_values removeObjectForKey:key];
}
- (id)primitiveValueForKey:(NSString *)key { return [_values objectForKey:key]; }
- (void)setPrimitiveValue:(id)value forKey:(NSString *)key { [self setValue:value forKey:key]; }
- (void)willAccessValueForKey:(NSString *)key {}
- (void)didAccessValueForKey:(NSString *)key {}
- (void)willChangeValueForKey:(NSString *)key {}
- (void)didChangeValueForKey:(NSString *)key {}
@end

@implementation NSManagedObjectContext {
    NSMutableArray *_objects;
    BOOL _changed;
}
- (instancetype)init { return [self initWithConcurrencyType:0]; }
- (instancetype)initWithConcurrencyType:(NSUInteger)type {
    if ((self = [super init])) _objects = [[NSMutableArray alloc] init];
    return self;
}
- (void)insertObject:(id)obj {
    if (!obj || [_objects containsObject:obj]) return;
    [_objects addObject:obj];
    if ([obj respondsToSelector:@selector(_mos_setContext:)]) [obj _mos_setContext:self];
    _changed = YES;
}
- (void)deleteObject:(id)obj {
    if ([obj respondsToSelector:@selector(_mos_setDeleted:)]) [obj _mos_setDeleted:YES];
    [_objects removeObject:obj];
    _changed = YES;
}
- (NSArray *)executeFetchRequest:(NSFetchRequest *)request error:(id *)error {
    NSString *name = request.entityName;
    NSMutableArray *r = [NSMutableArray array];
    for (NSManagedObject *o in _objects) {
        NSString *en = [o respondsToSelector:@selector(entity)] ? o.entity.name : nil;
        if (!name || (en && [en isEqual:name])) [r addObject:o];
        if (request.fetchLimit && r.count >= request.fetchLimit) break;
    }
    if (request.resultType == 4) return @[@(r.count)];   /* NSCountResultType */
    return r;
}
- (NSUInteger)countForFetchRequest:(NSFetchRequest *)request error:(id *)error {
    return [[self executeFetchRequest:request error:error] count];
}
- (BOOL)save:(id *)error { _changed = NO; return YES; }
- (BOOL)hasChanges { return _changed; }
- (void)performBlock:(void (^)(void))block { if (block) block(); }
- (void)performBlockAndWait:(void (^)(void))block { if (block) block(); }
- (void)rollback { _changed = NO; }
- (void)reset { [_objects removeAllObjects]; _changed = NO; }
- (id)existingObjectWithID:(id)oid error:(id *)error { return [_objects containsObject:oid] ? oid : nil; }
- (id)objectWithID:(id)oid { return oid; }
@end
