/*
 * mOS — CoreData.framework (Mach-O, em /System/Library/Frameworks). É o caminho do Postico quando o
 * arquivo está na imagem. NSSQLiteStoreType grava o mesmo SQLite do libSystem (mos_object, mos_attr,
 * cd_glue.c). NSInMemoryStoreType não toca disco. Um arquivo MOSCD1 é migrado na abertura.
 * Ainda não há modelos compilados (.momd). Binary e XML não gravam.
 * O overlay Swift (NSManagedObjectContext.fetch) está em CoreData.swift. tools/build-frameworks-objc.ps1
 */
#import <Foundation/Foundation.h>
#include "cd_api.h"
#include <stdlib.h>

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

static const char *cd_path(id url) {
    if (!url) return 0;
    if ([url isKindOfClass:[NSString class]]) return [(NSString *)url UTF8String];
    if ([url respondsToSelector:@selector(path)]) {
        id p = [url path];
        if ([p isKindOfClass:[NSString class]]) return [(NSString *)p UTF8String];
    }
    return 0;
}
static void cd_fail(id *error, NSString *msg) {
    (void)msg;
    if (error) *error = nil;
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
- (cd_store *)mos_store;
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
- (void)_mos_note;
@end

@interface NSManagedObject : NSObject
@property (readonly, strong) NSEntityDescription *entity;
@property (readonly, assign) NSManagedObjectContext *managedObjectContext;
- (void)_mos_bindStore:(cd_store *)st oid:(unsigned long)oid;
- (unsigned long)_mos_oid;
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
- (instancetype)initWithContentsOfURL:(id)url { (void)url; return [self init]; }
+ (instancetype)mergedModelFromBundles:(NSArray *)bundles { return [[[self alloc] init] autorelease]; }
- (NSDictionary *)entitiesByName { return [NSDictionary dictionary]; }
@end

@implementation NSPersistentStoreCoordinator {
    NSMutableArray *_stores;
    cd_store *_st;
}
- (instancetype)initWithManagedObjectModel:(NSManagedObjectModel *)model {
    if ((self = [super init])) { _managedObjectModel = [model retain]; _stores = [[NSMutableArray alloc] init]; }
    return self;
}
- (NSArray *)persistentStores { return _stores; }
- (cd_store *)mos_store { return _st; }
- (id)addPersistentStoreWithType:(NSString *)type configuration:(NSString *)conf URL:(id)url options:(NSDictionary *)opts error:(id *)error {
    const char *path;
    int memory;
    (void)conf; (void)opts;
    memory = !type || [type isEqual:NSInMemoryStoreType] || [type isEqual:NSBinaryStoreType] || [type isEqual:NSXMLStoreType];
    if (!memory && ![type isEqual:NSSQLiteStoreType]) {
        cd_fail(error, @"tipo de loja desconhecido");
        return nil;
    }
    if ([type isEqual:NSBinaryStoreType] || [type isEqual:NSXMLStoreType])
        fprintf(stderr, "CoreData[mOS]: loja %s continua só em memória\n", type.UTF8String);
    path = cd_path(url);
    if (_st) { moscd_close(_st); _st = 0; }
    if ([type isEqual:NSSQLiteStoreType]) {
        if (!path || !path[0]) {
            cd_fail(error, @"NSSQLiteStoreType precisa de um caminho");
            return nil;
        }
        _st = moscd_open_file(path);
    } else _st = moscd_open_memory();
    if (!_st) {
        cd_fail(error, @"não foi possível abrir a loja do Core Data");
        return nil;
    }
    if (error) *error = nil;
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
    cd_store *_st;
    unsigned long _oid;
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
- (void)_mos_bindStore:(cd_store *)st oid:(unsigned long)oid { _st = st; _oid = oid; }
- (instancetype)initWithContext:(NSManagedObjectContext *)ctx {
    return [self initWithEntity:[[self class] entity] insertIntoManagedObjectContext:ctx];
}
+ (NSEntityDescription *)entity { return [NSEntityDescription entityForName:NSStringFromClass(self) inManagedObjectContext:nil]; }
+ (NSFetchRequest *)fetchRequest { return [NSFetchRequest fetchRequestWithEntityName:NSStringFromClass(self)]; }
- (NSEntityDescription *)entity { return _entity; }
- (NSManagedObjectContext *)managedObjectContext { return _ctx; }
- (void)_mos_setContext:(NSManagedObjectContext *)c { _ctx = c; }
- (unsigned long)_mos_oid { return _oid; }
- (void)_mos_setDeleted:(BOOL)d { _deleted = d; }
- (id)objectID { return self; }
- (BOOL)isInserted { return _ctx != nil; }
- (BOOL)isDeleted { return _deleted; }
- (BOOL)isFault { return NO; }
- (id)valueForKey:(NSString *)key {
    int kind;
    if (_st && key) {
        kind = moscd_get_kind(_st, _oid, key.UTF8String);
        if (kind == 1) return [NSString stringWithUTF8String:moscd_get_str(_st, _oid, key.UTF8String) ?: ""];
        if (kind == 2) return [NSNumber numberWithLongLong:moscd_get_i64(_st, _oid, key.UTF8String)];
        if (kind == 3) return [NSNumber numberWithDouble:moscd_get_f64(_st, _oid, key.UTF8String)];
    }
    return [_values objectForKey:key];
}
- (void)setValue:(id)value forKey:(NSString *)key {
    const char *k = key.UTF8String;
    if (value) [_values setObject:value forKey:key]; else [_values removeObjectForKey:key];
    if (!_st || !k) return;
    if (!value) moscd_set_str(_st, _oid, k, 0);
    else if ([value isKindOfClass:[NSString class]]) moscd_set_str(_st, _oid, k, [(NSString *)value UTF8String]);
    else if ([value isKindOfClass:[NSNumber class]]) {
        double d = [(NSNumber *)value doubleValue];
        long long i = [(NSNumber *)value longLongValue];
        if (d == (double)i) moscd_set_i64(_st, _oid, k, i);
        else moscd_set_f64(_st, _oid, k, d);
    }     else moscd_set_str(_st, _oid, k, [[value description] UTF8String]);
    if (_ctx && [_ctx respondsToSelector:@selector(_mos_note)]) [_ctx _mos_note];
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
- (cd_store *)_mos_st {
    id coord = self.persistentStoreCoordinator;
    if (coord && [coord respondsToSelector:@selector(mos_store)]) return [coord mos_store];
    return 0;
}
- (void)_mos_note { _changed = YES; }
- (void)insertObject:(id)obj {
    cd_store *st;
    const char *name = "Entity";
    if (!obj || [_objects containsObject:obj]) return;
    [_objects addObject:obj];
    if ([obj respondsToSelector:@selector(_mos_setContext:)]) [obj _mos_setContext:self];
    st = [self _mos_st];
    if (st && [obj respondsToSelector:@selector(entity)] && [obj entity].name)
        name = [obj entity].name.UTF8String;
    if (st && [obj respondsToSelector:@selector(_mos_bindStore:oid:)])
        [obj _mos_bindStore:st oid:moscd_insert(st, name)];
    _changed = YES;
}
- (void)deleteObject:(id)obj {
    cd_store *st = [self _mos_st];
    if (st && [obj respondsToSelector:@selector(_mos_bindStore:oid:)]) {
        /* o oid está no objeto; delete usa valueForKey só para não expor o campo */
    }
    if ([obj respondsToSelector:@selector(objectID)] && st) {
        unsigned long oid = 0;
        if ([obj respondsToSelector:@selector(_mos_oid)]) oid = [obj _mos_oid];
        if (oid) moscd_delete(st, oid);
    }
    if ([obj respondsToSelector:@selector(_mos_setDeleted:)]) [obj _mos_setDeleted:YES];
    [_objects removeObject:obj];
    _changed = YES;
}
- (NSArray *)executeFetchRequest:(NSFetchRequest *)request error:(id *)error {
    NSString *name = request.entityName;
    cd_store *st = [self _mos_st];
    NSMutableArray *r = [NSMutableArray array];
    id pred = request.predicate;
    char key[128], val[512];
    const char *pk = 0, *pv = 0;
    int parsed = 0, can_eval = 0;
    unsigned n = 0, i;
    unsigned long *ids;
    if (error) *error = nil;
    if (pred) {
        NSString *fmt = nil;
        can_eval = [pred respondsToSelector:@selector(evaluateWithObject:)];
        if ([pred respondsToSelector:@selector(predicateFormat)]) fmt = [pred predicateFormat];
        else if ([pred isKindOfClass:[NSString class]]) fmt = pred;
        if (!fmt && [pred respondsToSelector:@selector(leftExpression)]) {
            id left = [pred leftExpression];
            id right = [pred respondsToSelector:@selector(rightExpression)] ? [pred rightExpression] : nil;
            id kp = [left respondsToSelector:@selector(keyPath)] ? [left keyPath] : nil;
            id cv = [right respondsToSelector:@selector(constantValue)] ? [right constantValue] : nil;
            if ([kp isKindOfClass:[NSString class]] && cv)
                fmt = [NSString stringWithFormat:@"%@ == \"%@\"", kp, cv];
        }
        if (fmt && moscd_split_eq(fmt.UTF8String, key, sizeof key, val, sizeof val)) {
            pk = key; pv = val; parsed = 1;
        }
        if (!parsed && !can_eval) return r;
    }
    if (!st) {
        for (NSManagedObject *o in _objects) {
            NSString *en = [o respondsToSelector:@selector(entity)] ? o.entity.name : nil;
            if (name && !(en && [en isEqual:name])) continue;
            if (can_eval && ![pred evaluateWithObject:o]) continue;
            [r addObject:o];
            if (request.fetchLimit && r.count >= request.fetchLimit) break;
        }
        if (request.resultType == 4) return @[@(r.count)];
        return r;
    }
    ids = moscd_fetch(st, name ? name.UTF8String : 0, pk, pv, 0, (unsigned)request.fetchOffset, &n);
    for (i = 0; i < n; i++) {
        NSEntityDescription *e = [NSEntityDescription entityForName:name ?: [NSString stringWithUTF8String:moscd_entity_of(st, ids[i]) ?: ""] inManagedObjectContext:self];
        NSManagedObject *o = [[[NSManagedObject alloc] initWithEntity:e insertIntoManagedObjectContext:nil] autorelease];
        [o _mos_bindStore:st oid:ids[i]];
        [o _mos_setContext:self];
        if (can_eval && ![pred evaluateWithObject:o]) continue;
        [r addObject:o];
        if (request.fetchLimit && r.count >= request.fetchLimit) break;
    }
    free(ids);
    if (request.resultType == 4) return @[@(r.count)];
    return r;
}
- (NSUInteger)countForFetchRequest:(NSFetchRequest *)request error:(id *)error {
    return [[self executeFetchRequest:request error:error] count];
}
- (BOOL)save:(id *)error {
    cd_store *st = [self _mos_st];
    if (st && moscd_save(st)) {
        cd_fail(error, @"falha ao gravar a loja do Core Data");
        return NO;
    }
    _changed = NO;
    if (error) *error = nil;
    return YES;
}
- (BOOL)hasChanges { return _changed; }
- (void)performBlock:(void (^)(void))block { if (block) block(); }
- (void)performBlockAndWait:(void (^)(void))block { if (block) block(); }
- (void)rollback { _changed = NO; }
- (void)reset { [_objects removeAllObjects]; _changed = NO; }
- (id)existingObjectWithID:(id)oid error:(id *)error { return [_objects containsObject:oid] ? oid : nil; }
- (id)objectWithID:(id)oid { return oid; }
@end

/* ---- migração (NSMigrationManager, NSMappingModel, NSEntityMigrationPolicy, NSPropertyMapping)
 * A loja do mOS guarda objetos e atributos por nome (mos_object/mos_attr), sem esquema compilado: não há .cdm para
 * carregar. +mappingModelFromBundles:... devolve nil (nenhum modelo de mapeamento compilado); o inferido é um modelo
 * vazio, que basta porque os atributos seguem pelo nome. migrateStoreFromURL:... copia a loja de origem para o
 * destino (a mesma tabela serve aos dois modelos) e informa o progresso 1.0. */
id objc_msgSend(id, SEL, ...);
int strcmp(const char *a, const char *b);

@interface NSPropertyMapping : NSObject {
    id _name, _value, _userInfo;
}
- (id)name;
- (void)setName:(id)n;
- (id)valueExpression;
- (void)setValueExpression:(id)v;
- (id)userInfo;
- (void)setUserInfo:(id)u;
@end
@implementation NSPropertyMapping
- (id)name { return _name; }
- (void)setName:(id)n { id c = [n copy]; [_name release]; _name = c; }
- (id)valueExpression { return _value; }
- (void)setValueExpression:(id)v { [v retain]; [_value release]; _value = v; }
- (id)userInfo { return _userInfo; }
- (void)setUserInfo:(id)u { [u retain]; [_userInfo release]; _userInfo = u; }
- (void)dealloc { [_name release]; [_value release]; [_userInfo release]; [super dealloc]; }
@end

@interface NSMappingModel : NSObject {
    id _entityMappings;
}
+ (id)mappingModelFromBundles:(id)bundles forSourceModel:(id)source destinationModel:(id)destination;
+ (id)inferredMappingModelForSourceModel:(id)source destinationModel:(id)destination error:(id *)error;
- (id)initWithContentsOfURL:(id)url;
- (id)entityMappings;
- (void)setEntityMappings:(id)e;
- (id)entityMappingsByName;
@end
@implementation NSMappingModel
+ (id)mappingModelFromBundles:(id)bundles forSourceModel:(id)source destinationModel:(id)destination { return nil; }
+ (id)inferredMappingModelForSourceModel:(id)source destinationModel:(id)destination error:(id *)error {
    if (error) *error = nil;
    NSMappingModel *m = [[[self alloc] init] autorelease];
    [m setEntityMappings:[NSArray array]];
    return m;
}
- (id)initWithContentsOfURL:(id)url { [self release]; return nil; }      /* sem formato .cdm no mOS */
- (id)entityMappings { return _entityMappings; }
- (void)setEntityMappings:(id)e { [e retain]; [_entityMappings release]; _entityMappings = e; }
- (id)entityMappingsByName {
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    NSUInteger n = [_entityMappings count];
    for (NSUInteger i = 0; i < n; i++) {
        id e = [_entityMappings objectAtIndex:i];
        id k = [e valueForKey:@"name"];
        if (k) [d setObject:e forKey:k];
    }
    return d;
}
- (void)dealloc { [_entityMappings release]; [super dealloc]; }
@end

@interface NSMigrationManager : NSObject {
    id _src, _dst, _mapping, _userInfo;
    float _progress;
    BOOL _cancelled;
}
- (instancetype)initWithSourceModel:(id)source destinationModel:(id)destination;
- (BOOL)migrateStoreFromURL:(id)srcURL type:(id)srcType options:(id)srcOpts withMappingModel:(id)mapping
           toDestinationURL:(id)dstURL destinationType:(id)dstType destinationOptions:(id)dstOpts error:(id *)error;
- (id)sourceModel;
- (id)destinationModel;
- (id)mappingModel;
- (float)migrationProgress;
- (id)userInfo;
- (void)setUserInfo:(id)u;
- (void)cancelMigrationWithError:(id)error;
@end
@implementation NSMigrationManager
- (instancetype)initWithSourceModel:(id)source destinationModel:(id)destination {
    if ((self = [super init])) { _src = [source retain]; _dst = [destination retain]; }
    return self;
}
- (void)dealloc { [_src release]; [_dst release]; [_mapping release]; [_userInfo release]; [super dealloc]; }
- (id)sourceModel { return _src; }
- (id)destinationModel { return _dst; }
- (id)mappingModel { return _mapping; }
- (float)migrationProgress { return _progress; }
- (id)userInfo { return _userInfo; }
- (void)setUserInfo:(id)u { [u retain]; [_userInfo release]; _userInfo = u; }
- (void)cancelMigrationWithError:(id)error { _cancelled = YES; }
- (BOOL)migrateStoreFromURL:(id)srcURL type:(id)srcType options:(id)srcOpts withMappingModel:(id)mapping
           toDestinationURL:(id)dstURL destinationType:(id)dstType destinationOptions:(id)dstOpts error:(id *)error {
    [mapping retain]; [_mapping release]; _mapping = mapping;
    if (_cancelled) { cd_fail(error, @"migração cancelada"); return NO; }
    if ([dstType isEqual:NSInMemoryStoreType] || [srcType isEqual:NSInMemoryStoreType]) { _progress = 1; if (error) *error = nil; return YES; }
    const char *s = cd_path(srcURL), *d = cd_path(dstURL);
    if (!s || !d) { cd_fail(error, @"URL de loja inválida"); return NO; }
    if (strcmp(s, d)) {
        id fm = [NSFileManager defaultManager];
        id sp = [NSString stringWithUTF8String:s], dp = [NSString stringWithUTF8String:d];
        ((BOOL (*)(id, SEL, id, id *))objc_msgSend)(fm, @selector(removeItemAtPath:error:), dp, (id *)0);
        BOOL ok = ((BOOL (*)(id, SEL, id, id, id *))objc_msgSend)(fm, @selector(copyItemAtPath:toPath:error:), sp, dp, error);
        if (!ok) { cd_fail(error, @"não foi possível copiar a loja para o destino"); return NO; }
    }
    _progress = 1;
    if (error) *error = nil;
    return YES;
}
@end

@interface NSEntityMigrationPolicy : NSObject
- (BOOL)beginEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
- (BOOL)createDestinationInstancesForSourceInstance:(id)sInstance entityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
- (BOOL)endInstanceCreationForEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
- (BOOL)createRelationshipsForDestinationInstance:(id)dInstance entityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
- (BOOL)endRelationshipCreationForEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
- (BOOL)performCustomValidationForEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
- (BOOL)endEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error;
@end
/* a política padrão da Apple: cada etapa dá certo sem fazer nada; subclasses sobrescrevem a que precisam */
@implementation NSEntityMigrationPolicy
- (BOOL)beginEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
- (BOOL)createDestinationInstancesForSourceInstance:(id)sInstance entityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
- (BOOL)endInstanceCreationForEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
- (BOOL)createRelationshipsForDestinationInstance:(id)dInstance entityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
- (BOOL)endRelationshipCreationForEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
- (BOOL)performCustomValidationForEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
- (BOOL)endEntityMapping:(id)mapping manager:(NSMigrationManager *)manager error:(id *)error { return YES; }
@end
