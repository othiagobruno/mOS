/*
 * mOS — CloudKit.framework (Mach-O, /System/Library/Frameworks/CloudKit.framework). O mOS não tem iCloud nem conta
 * da Apple: o contêiner responde "sem conta" (CKAccountStatusNoAccount, sem erro, como um Mac sem iCloud) e toda
 * operação de rede termina, na fila global, com CKErrorNotAuthenticated (código 9) nos blocos de conclusão. Os objetos
 * locais (CKRecord, CKRecordID, CKRecordZone, CKQuery, CKReference, CKAsset, assinaturas, compartilhamento) são de
 * verdade: guardam valores, comparam, copiam e se codificam. Nada fica pendurado: todo bloco de conclusão é chamado
 * uma vez, depois o completionBlock do NSOperation.
 * O overlay Swift (os nomes $s…8CloudKit…) está em CloudKit.swift. Interface: include/CloudKit_mos.h.
 */
#import "include/CloudKit_mos.h"
/* as implementações aceitam nil onde a interface pede objeto (como o CloudKit da Apple, por defesa) e trocam os
 * genéricos dos blocos por NSArray (o mesmo tipo em execução) */
#pragma clang diagnostic ignored "-Wnonnull"
#pragma clang diagnostic ignored "-Wmismatched-parameter-types"

/* ---- o que o Foundation.h mínimo do mOS (userland/sdk/include) não declara */
@interface NSError (mOSCloudKit)
+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)dict;
@property (readonly) NSInteger code;
@property (readonly, copy) NSString *domain;
@property (readonly, copy) NSDictionary *userInfo;
@end
@interface NSUUID : NSObject
+ (instancetype)UUID;
@property (readonly, copy) NSString *UUIDString;
@end
@interface NSURL : NSObject <NSCopying>
@end
@interface NSString (mOSCloudKit)
- (BOOL)hasPrefix:(NSString *)p;
- (NSString *)substringFromIndex:(NSUInteger)i;
- (NSString *)stringByAppendingString:(NSString *)s;
@end
@interface NSBundle (mOSCloudKit)
+ (NSBundle *)mainBundle;
@end
@interface NSNumber (mOSCloudKit)
+ (NSNumber *)numberWithInteger:(NSInteger)v;
@end
@interface NSCoder (mOSCloudKit)
- (void)encodeObject:(id)o forKey:(NSString *)k;
@end
@interface NSDictionary (mOSCloudKitKeys)
- (NSArray *)allKeys;
@end
typedef struct dispatch_queue_s *dispatch_queue_t;
dispatch_queue_t dispatch_get_global_queue(long identifier, unsigned long flags);
void dispatch_async(dispatch_queue_t queue, void (^block)(void));
typedef struct objc_ivar *Ivar;
Ivar *class_copyIvarList(Class cls, unsigned int *count);
const char *ivar_getTypeEncoding(Ivar v);
ptrdiff_t ivar_getOffset(Ivar v);
Class class_getSuperclass(Class c);
void free(void *p);
void objc_release(id o);

/* ---- constantes (CKErrorDomain, CKCurrentUserDefaultName e CKAccountChangedNotification vêm do libSystem) */
NSString *const CKPartialErrorsByItemIDKey = @"CKPartialErrors";
NSString *const CKRecordChangedErrorAncestorRecordKey = @"AncestorRecord";
NSString *const CKRecordChangedErrorServerRecordKey = @"ServerRecord";
NSString *const CKRecordChangedErrorClientRecordKey = @"ClientRecord";
NSString *const CKErrorUserDidResetEncryptedDataKey = @"CKErrorUserDidResetEncryptedDataKey";
NSString *const CKErrorRetryAfterKey = @"CKRetryAfter";
NSString *const CKRecordZoneDefaultName = @"_defaultZone";
NSString *const CKOwnerDefaultName = @"__defaultOwner__";
NSString *const CKRecordTypeUserRecord = @"Users";
NSString *const CKRecordTypeShare = @"cloudkit.share";
NSString *const CKRecordNameZoneWideShare = @"cloudkit.zoneshare";
NSString *const CKShareTitleKey = @"cloudkit.title";
NSString *const CKShareThumbnailImageDataKey = @"cloudkit.thumbnailImageData";
NSString *const CKShareTypeKey = @"cloudkit.type";
NSString *const CKRecordCreationDateKey = @"___createTime";
NSString *const CKRecordCreatorUserRecordIDKey = @"___createdBy";
NSString *const CKRecordLastModifiedUserRecordIDKey = @"___modifiedBy";
NSString *const CKRecordModificationDateKey = @"___modTime";
NSString *const CKRecordParentKey = @"___parent";
NSString *const CKRecordRecordIDKey = @"___recordID";
NSString *const CKRecordShareKey = @"___share";
const NSUInteger CKQueryOperationMaximumResults = 0;

static NSError *ck_no_account(void) {
    NSDictionary *info = [NSDictionary dictionaryWithObject:@"Nenhuma conta do iCloud configurada neste computador (o mOS não tem iCloud)."
                                                     forKey:@"NSLocalizedDescription"];
    return [NSError errorWithDomain:CKErrorDomain code:CKErrorNotAuthenticated userInfo:info];
}
static void ck_later(void (^b)(void)) { dispatch_async(dispatch_get_global_queue(0, 0), b); }

/* propriedades sintetizadas (MRC): libera os ivars de objeto de cada classe da cadeia até a base pedida */
static void ck_release_ivars(id self, Class stop) {
    for (Class c = [self class]; c && c != stop; c = class_getSuperclass(c)) {
        unsigned int n = 0;
        Ivar *iv = class_copyIvarList(c, &n);
        for (unsigned int i = 0; i < n; i++) {
            const char *t = ivar_getTypeEncoding(iv[i]);
            if (!t || t[0] != '@') continue;
            id *slot = (id *)((char *)self + ivar_getOffset(iv[i]));
            objc_release(*slot);
            *slot = nil;
        }
        free(iv);
    }
}
#define CK_DEALLOC(base) - (void)dealloc { ck_release_ivars(self, [base class]); [super dealloc]; }

/* ================================================================ identificadores */
@implementation CKRecordZoneID
- (instancetype)initWithZoneName:(NSString *)zoneName ownerName:(NSString *)ownerName {
    if ((self = [super init])) {
        _zoneName = [(zoneName ?: CKRecordZoneDefaultName) copy];
        _ownerName = [(ownerName ?: CKCurrentUserDefaultName) copy];
    }
    return self;
}
- (instancetype)init { return [self initWithZoneName:CKRecordZoneDefaultName ownerName:CKCurrentUserDefaultName]; }
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
- (id)copy { return [self retain]; }
- (BOOL)isEqual:(id)o {
    if (o == self) return YES;
    if (![o isKindOfClass:[CKRecordZoneID class]]) return NO;
    return [_zoneName isEqualToString:[o zoneName]] && [_ownerName isEqualToString:[o ownerName]];
}
- (NSUInteger)hash { return [_zoneName hash] ^ ([_ownerName hash] << 1); }
- (NSString *)description { return [NSString stringWithFormat:@"<CKRecordZoneID: %p; zoneName=%@, ownerName=%@>", self, _zoneName, _ownerName]; }
CK_DEALLOC(NSObject)
@end

@implementation CKRecordID
- (instancetype)initWithRecordName:(NSString *)recordName zoneID:(CKRecordZoneID *)zoneID {
    if ((self = [super init])) {
        _recordName = [(recordName ?: [[NSUUID UUID] UUIDString]) copy];
        _zoneID = [(zoneID ?: [[[CKRecordZoneID alloc] init] autorelease]) retain];
    }
    return self;
}
- (instancetype)initWithRecordName:(NSString *)recordName { return [self initWithRecordName:recordName zoneID:nil]; }
- (instancetype)init { return [self initWithRecordName:nil zoneID:nil]; }
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
- (id)copy { return [self retain]; }
- (BOOL)isEqual:(id)o {
    if (o == self) return YES;
    if (![o isKindOfClass:[CKRecordID class]]) return NO;
    return [_recordName isEqualToString:[o recordName]] && [_zoneID isEqual:[o zoneID]];
}
- (NSUInteger)hash { return [_recordName hash] ^ [_zoneID hash]; }
- (NSString *)description { return [NSString stringWithFormat:@"<CKRecordID: %p; recordName=%@, zoneID=%@:%@>", self, _recordName, [_zoneID zoneName], [_zoneID ownerName]]; }
CK_DEALLOC(NSObject)
@end

@implementation CKRecordZone
+ (CKRecordZone *)defaultRecordZone { return [[[CKRecordZone alloc] initWithZoneName:CKRecordZoneDefaultName] autorelease]; }
- (instancetype)initWithZoneID:(CKRecordZoneID *)zoneID {
    if ((self = [super init])) {
        _zoneID = [(zoneID ?: [[[CKRecordZoneID alloc] init] autorelease]) retain];
        _capabilities = [[_zoneID zoneName] isEqualToString:CKRecordZoneDefaultName] ? 0
                      : CKRecordZoneCapabilityFetchChanges | CKRecordZoneCapabilityAtomic | CKRecordZoneCapabilitySharing;
    }
    return self;
}
- (instancetype)initWithZoneName:(NSString *)zoneName {
    return [self initWithZoneID:[[[CKRecordZoneID alloc] initWithZoneName:zoneName ownerName:CKCurrentUserDefaultName] autorelease]];
}
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
- (id)copy { return [self retain]; }
- (BOOL)isEqual:(id)o { return o == self || ([o isKindOfClass:[CKRecordZone class]] && [_zoneID isEqual:[o zoneID]]); }
- (NSUInteger)hash { return [_zoneID hash]; }
CK_DEALLOC(NSObject)
@end

@implementation CKReference
- (instancetype)initWithRecordID:(CKRecordID *)recordID action:(CKReferenceAction)action {
    if ((self = [super init])) { _recordID = [recordID copy]; _referenceAction = action; }
    return self;
}
- (instancetype)initWithRecord:(CKRecord *)record action:(CKReferenceAction)action { return [self initWithRecordID:[record recordID] action:action]; }
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
- (id)copy { return [self retain]; }
- (BOOL)isEqual:(id)o { return o == self || ([o isKindOfClass:[CKReference class]] && [_recordID isEqual:[o recordID]] && _referenceAction == [o referenceAction]); }
- (NSUInteger)hash { return [_recordID hash]; }
CK_DEALLOC(NSObject)
@end

@implementation CKAsset
- (instancetype)initWithFileURL:(NSURL *)fileURL { if ((self = [super init])) _fileURL = [fileURL copy]; return self; }
CK_DEALLOC(NSObject)
@end

/* ================================================================ registro */
/* encryptedValues: uma visão do mesmo dicionário com chaves prefixadas (no mOS não há cifra de ponta a ponta:
 * sem iCloud o valor nunca sai do processo) */
@interface MOSCKEncryptedValues : NSObject <CKRecordKeyValueSetting> { CKRecord *_rec; }
- (instancetype)initWithRecord:(CKRecord *)r;
@end

@implementation CKRecord {
    NSMutableDictionary *_values;
    NSMutableArray *_changed;
    MOSCKEncryptedValues *_enc;
}
- (instancetype)initWithRecordType:(NSString *)recordType recordID:(CKRecordID *)recordID {
    if ((self = [super init])) {
        _recordType = [recordType copy];
        _recordID = [(recordID ?: [[[CKRecordID alloc] initWithRecordName:nil zoneID:nil] autorelease]) copy];
        _values = [[NSMutableDictionary alloc] init];
        _changed = [[NSMutableArray alloc] init];
        _creationDate = nil;
    }
    return self;
}
- (instancetype)initWithRecordType:(NSString *)recordType { return [self initWithRecordType:recordType recordID:nil]; }
- (instancetype)initWithRecordType:(NSString *)recordType zoneID:(CKRecordZoneID *)zoneID {
    return [self initWithRecordType:recordType recordID:[[[CKRecordID alloc] initWithRecordName:nil zoneID:zoneID] autorelease]];
}
- (instancetype)init { return [self initWithRecordType:@"Record" recordID:nil]; }
/* campos de sistema (encodeSystemFieldsWithCoder:/initWithCoder:): tipo, nome, zona, dono */
- (void)encodeSystemFieldsWithCoder:(NSCoder *)coder {
    [coder encodeObject:_recordType forKey:@"RecordType"];
    [coder encodeObject:[_recordID recordName] forKey:@"RecordName"];
    [coder encodeObject:[[_recordID zoneID] zoneName] forKey:@"ZoneName"];
    [coder encodeObject:[[_recordID zoneID] ownerName] forKey:@"OwnerName"];
}
- (void)encodeWithCoder:(NSCoder *)coder {
    [self encodeSystemFieldsWithCoder:coder];
    [coder encodeObject:_values forKey:@"Values"];
}
- (nullable instancetype)initWithCoder:(NSCoder *)coder {
    NSString *type = [coder decodeObjectForKey:@"RecordType"];
    if (!type) { [self release]; return nil; }
    CKRecordZoneID *z = [[[CKRecordZoneID alloc] initWithZoneName:[coder decodeObjectForKey:@"ZoneName"]
                                                          ownerName:[coder decodeObjectForKey:@"OwnerName"]] autorelease];
    CKRecordID *rid = [[[CKRecordID alloc] initWithRecordName:[coder decodeObjectForKey:@"RecordName"] zoneID:z] autorelease];
    if ((self = [self initWithRecordType:type recordID:rid])) {
        NSDictionary *v = [coder decodeObjectForKey:@"Values"];
        if ([v isKindOfClass:[NSDictionary class]]) for (NSString *k in [v allKeys]) [_values setObject:[v objectForKey:k] forKey:k];
    }
    return self;
}
- (nullable id)objectForKey:(NSString *)key { return key ? [_values objectForKey:key] : nil; }
- (void)setObject:(nullable id)object forKey:(NSString *)key {
    if (!key) return;
    if (object) [_values setObject:object forKey:key];
    else [_values removeObjectForKey:key];
    if (![_changed containsObject:key]) [_changed addObject:key];
}
- (nullable id)objectForKeyedSubscript:(NSString *)key { return [self objectForKey:key]; }
- (void)setObject:(nullable id)object forKeyedSubscript:(NSString *)key { [self setObject:object forKey:key]; }
- (NSArray<NSString *> *)allKeys {
    NSMutableArray *out = [NSMutableArray array];
    for (NSString *k in [_values allKeys]) if (![k hasPrefix:@"\x01enc:"]) [out addObject:k];
    return out;
}
- (NSArray<NSString *> *)changedKeys { return [[_changed copy] autorelease]; }
- (NSArray<NSString *> *)allTokens { return [NSArray array]; }
- (id<CKRecordKeyValueSetting>)encryptedValues {
    if (!_enc) _enc = [[MOSCKEncryptedValues alloc] initWithRecord:self];
    return _enc;
}
- (void)setParentReferenceFromRecord:(nullable CKRecord *)parentRecord {
    [self setParent:parentRecord ? [[[CKReference alloc] initWithRecord:parentRecord action:CKReferenceActionNone] autorelease] : nil];
}
- (void)setParentReferenceFromRecordID:(nullable CKRecordID *)parentRecordID {
    [self setParent:parentRecordID ? [[[CKReference alloc] initWithRecordID:parentRecordID action:CKReferenceActionNone] autorelease] : nil];
}
- (id)copyWithZone:(void *)z {
    (void)z;
    CKRecord *c = [[[self class] alloc] initWithRecordType:_recordType recordID:_recordID];
    for (NSString *k in [_values allKeys]) [c->_values setObject:[_values objectForKey:k] forKey:k];
    [c->_changed addObjectsFromArray:_changed];
    [c setParent:[self parent]];
    return c;
}
- (id)copy { return [self copyWithZone:0]; }
- (NSString *)description { return [NSString stringWithFormat:@"<%@: %p; recordType=%@, recordID=%@, values=%@>", NSStringFromClass([self class]), self, _recordType, _recordID, _values]; }
/* uso interno de MOSCKEncryptedValues */
- (NSMutableDictionary *)__mosValues { return _values; }
- (NSMutableArray *)__mosChanged { return _changed; }
CK_DEALLOC(NSObject)
@end

@implementation MOSCKEncryptedValues
- (instancetype)initWithRecord:(CKRecord *)r { if ((self = [super init])) _rec = r; return self; }   /* fraco: o registro é dono */
static NSString *enc_key(NSString *k) { return [@"\x01enc:" stringByAppendingString:k]; }
- (nullable id)objectForKey:(NSString *)key { return key ? [[_rec __mosValues] objectForKey:enc_key(key)] : nil; }
- (void)setObject:(nullable id)object forKey:(NSString *)key {
    if (!key) return;
    if (object) [[_rec __mosValues] setObject:object forKey:enc_key(key)];
    else [[_rec __mosValues] removeObjectForKey:enc_key(key)];
    if (![[_rec __mosChanged] containsObject:key]) [[_rec __mosChanged] addObject:key];
}
- (nullable id)objectForKeyedSubscript:(NSString *)key { return [self objectForKey:key]; }
- (void)setObject:(nullable id)object forKeyedSubscript:(NSString *)key { [self setObject:object forKey:key]; }
- (NSArray<NSString *> *)allKeys {
    NSMutableArray *out = [NSMutableArray array];
    for (NSString *k in [[_rec __mosValues] allKeys]) if ([k hasPrefix:@"\x01enc:"]) [out addObject:[k substringFromIndex:5]];
    return out;
}
- (NSArray<NSString *> *)changedKeys { return [_rec changedKeys]; }
@end

@implementation CKShare {
    NSMutableArray *_parts;
}
- (instancetype)initWithRecordZoneID:(CKRecordZoneID *)recordZoneID {
    CKRecordID *rid = [[[CKRecordID alloc] initWithRecordName:CKRecordNameZoneWideShare zoneID:recordZoneID] autorelease];
    if ((self = [super initWithRecordType:CKRecordTypeShare recordID:rid])) _parts = [[NSMutableArray alloc] init];
    return self;
}
- (instancetype)initWithRootRecord:(CKRecord *)rootRecord shareID:(CKRecordID *)shareID {
    if ((self = [super initWithRecordType:CKRecordTypeShare recordID:shareID])) {
        _parts = [[NSMutableArray alloc] init];
        [rootRecord setObject:nil forKey:@"___share"];
    }
    return self;
}
- (instancetype)initWithRootRecord:(CKRecord *)rootRecord {
    CKRecordID *rid = [[[CKRecordID alloc] initWithRecordName:nil zoneID:[[rootRecord recordID] zoneID]] autorelease];
    return [self initWithRootRecord:rootRecord shareID:rid];
}
- (NSArray<CKShareParticipant *> *)participants { return [[_parts copy] autorelease]; }
- (void)addParticipant:(CKShareParticipant *)p { if (p && ![_parts containsObject:p]) [_parts addObject:p]; }
- (void)removeParticipant:(CKShareParticipant *)p { if (p) [_parts removeObject:p]; }
- (NSURL *)URL { return nil; }                  /* só o servidor do iCloud gera o link */
- (CKShareParticipant *)owner { return nil; }
- (CKShareParticipant *)currentUserParticipant { return nil; }
CK_DEALLOC(CKRecord)
@end

@implementation CKShareParticipant
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
CK_DEALLOC(NSObject)
@end
@implementation CKUserIdentity
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
- (BOOL)hasiCloudAccount { return NO; }
CK_DEALLOC(NSObject)
@end
@implementation CKUserIdentityLookupInfo
- (instancetype)initWithEmailAddress:(NSString *)e { if ((self = [super init])) _emailAddress = [e copy]; return self; }
- (instancetype)initWithPhoneNumber:(NSString *)p { if ((self = [super init])) _phoneNumber = [p copy]; return self; }
- (instancetype)initWithUserRecordID:(CKRecordID *)r { if ((self = [super init])) _userRecordID = [r copy]; return self; }
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
CK_DEALLOC(NSObject)
@end
@implementation CKShareMetadata
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
CK_DEALLOC(NSObject)
@end

@implementation CKQuery
- (instancetype)initWithRecordType:(NSString *)recordType predicate:(NSPredicate *)predicate {
    if ((self = [super init])) { _recordType = [recordType copy]; _predicate = [(id)predicate retain]; }
    return self;
}
- (id)copyWithZone:(void *)z {
    (void)z;
    CKQuery *q = [[CKQuery alloc] initWithRecordType:_recordType predicate:_predicate];
    [q setSortDescriptors:_sortDescriptors];
    return q;
}
- (id)copy { return [self copyWithZone:0]; }
CK_DEALLOC(NSObject)
@end
@implementation CKQueryCursor
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
@end
@implementation CKServerChangeToken
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
- (void)encodeWithCoder:(NSCoder *)c { (void)c; }
- (nullable instancetype)initWithCoder:(NSCoder *)c { (void)c; return [self init]; }
@end

/* ================================================================ assinaturas e notificações */
@implementation CKNotificationInfo
- (id)copyWithZone:(void *)z {
    (void)z;
    CKNotificationInfo *n = [[CKNotificationInfo alloc] init];
    [n setAlertBody:_alertBody]; [n setAlertLocalizationKey:_alertLocalizationKey]; [n setTitle:_title];
    [n setSubtitle:_subtitle]; [n setSoundName:_soundName]; [n setDesiredKeys:_desiredKeys]; [n setShouldBadge:_shouldBadge];
    [n setShouldSendContentAvailable:_shouldSendContentAvailable]; [n setShouldSendMutableContent:_shouldSendMutableContent];
    [n setCategory:_category]; [n setCollapseIDKey:_collapseIDKey];
    return n;
}
- (id)copy { return [self copyWithZone:0]; }
CK_DEALLOC(NSObject)
@end

@interface CKSubscription ()
- (instancetype)__initWithID:(NSString *)sid type:(CKSubscriptionType)type;
@end
@implementation CKSubscription
- (instancetype)__initWithID:(NSString *)sid type:(CKSubscriptionType)type {
    if ((self = [super init])) { _subscriptionID = [(sid ?: [[NSUUID UUID] UUIDString]) copy]; _subscriptionType = type; }
    return self;
}
- (id)copyWithZone:(void *)z { (void)z; return [self retain]; }
CK_DEALLOC(NSObject)
@end
@implementation CKQuerySubscription
- (instancetype)initWithRecordType:(NSString *)recordType predicate:(NSPredicate *)predicate subscriptionID:(NSString *)sid
                           options:(CKQuerySubscriptionOptions)options {
    if ((self = [super __initWithID:sid type:CKSubscriptionTypeQuery])) {
        _recordType = [recordType copy];
        _predicate = [(id)predicate retain];
        _querySubscriptionOptions = options;
    }
    return self;
}
- (instancetype)initWithRecordType:(NSString *)recordType predicate:(NSPredicate *)predicate options:(CKQuerySubscriptionOptions)options {
    return [self initWithRecordType:recordType predicate:predicate subscriptionID:nil options:options];
}
CK_DEALLOC(CKSubscription)
@end
@implementation CKRecordZoneSubscription
- (instancetype)initWithZoneID:(CKRecordZoneID *)zoneID subscriptionID:(NSString *)sid {
    if ((self = [super __initWithID:sid type:CKSubscriptionTypeRecordZone])) _zoneID = [zoneID copy];
    return self;
}
- (instancetype)initWithZoneID:(CKRecordZoneID *)zoneID { return [self initWithZoneID:zoneID subscriptionID:nil]; }
CK_DEALLOC(CKSubscription)
@end
@implementation CKDatabaseSubscription
- (instancetype)initWithSubscriptionID:(NSString *)sid { return [super __initWithID:sid type:CKSubscriptionTypeDatabase]; }
- (instancetype)init { return [self initWithSubscriptionID:nil]; }
CK_DEALLOC(CKSubscription)
@end

@implementation CKNotification {
    CKNotificationType _type;
    NSString *_sid, *_cid;
}
/* o payload do push tem "ck": {"ce": contêiner, "qry"/"fet"/"met": {"sid": assinatura}}; no mOS nenhum push chega
 * do iCloud, mas um dicionário montado pelo app é lido do mesmo jeito */
+ (nullable instancetype)notificationFromRemoteNotificationDictionary:(NSDictionary *)d {
    NSDictionary *ck = [d isKindOfClass:[NSDictionary class]] ? [d objectForKey:@"ck"] : nil;
    if (![ck isKindOfClass:[NSDictionary class]]) return nil;
    CKNotification *n = [[[self alloc] init] autorelease];
    n->_cid = [[ck objectForKey:@"cid"] copy];
    NSString *kinds[] = {@"qry", @"fet", @"met"};
    CKNotificationType types[] = {CKNotificationTypeQuery, CKNotificationTypeRecordZone, CKNotificationTypeDatabase};
    n->_type = CKNotificationTypeQuery;
    for (int i = 0; i < 3; i++) {
        NSDictionary *k = [ck objectForKey:kinds[i]];
        if ([k isKindOfClass:[NSDictionary class]]) { n->_type = types[i]; n->_sid = [[k objectForKey:@"sid"] copy]; break; }
    }
    return n;
}
- (CKNotificationType)notificationType { return _type; }
- (NSString *)containerIdentifier { return _cid; }
- (NSString *)subscriptionID { return _sid; }
- (BOOL)isPruned { return NO; }
- (void)dealloc { [_sid release]; [_cid release]; [super dealloc]; }
@end

/* ================================================================ contêiner e banco */
@implementation CKOperationConfiguration
- (instancetype)init { if ((self = [super init])) { _allowsCellularAccess = YES; _timeoutIntervalForRequest = 60; _timeoutIntervalForResource = 604800; } return self; }
- (id)copyWithZone:(void *)z {
    (void)z;
    CKOperationConfiguration *c = [[CKOperationConfiguration alloc] init];
    [c setContainer:_container]; [c setQualityOfService:_qualityOfService]; [c setAllowsCellularAccess:_allowsCellularAccess];
    [c setLongLived:_longLived]; [c setTimeoutIntervalForRequest:_timeoutIntervalForRequest];
    [c setTimeoutIntervalForResource:_timeoutIntervalForResource];
    return c;
}
- (id)copy { return [self copyWithZone:0]; }
CK_DEALLOC(NSObject)
@end
@implementation CKOperationGroup
- (instancetype)init { if ((self = [super init])) { _operationGroupID = [[[NSUUID UUID] UUIDString] copy]; _quantity = 1; } return self; }
- (CKOperationConfiguration *)defaultConfiguration {
    if (!_defaultConfiguration) _defaultConfiguration = [[CKOperationConfiguration alloc] init];
    return _defaultConfiguration;
}
CK_DEALLOC(NSObject)
@end

@interface CKDatabase ()
- (instancetype)__initWithContainer:(CKContainer *)c scope:(CKDatabaseScope)scope;
@end

@implementation CKContainer {
    CKDatabase *_private, *_public, *_shared;
}
static CKContainer *g_default;
+ (CKContainer *)defaultContainer {
    @synchronized(self) {
        if (!g_default) {
            NSString *bid = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleIdentifier"];
            g_default = [[CKContainer alloc] __initWithIdentifier:[@"iCloud." stringByAppendingString:bid ?: @"mos.app"]];
        }
    }
    return g_default;
}
+ (CKContainer *)containerWithIdentifier:(NSString *)identifier { return [[[CKContainer alloc] __initWithIdentifier:identifier] autorelease]; }
- (instancetype)__initWithIdentifier:(NSString *)identifier {
    if ((self = [super init])) {
        _containerIdentifier = [identifier copy];
        _private = [[CKDatabase alloc] __initWithContainer:self scope:CKDatabaseScopePrivate];
        _public = [[CKDatabase alloc] __initWithContainer:self scope:CKDatabaseScopePublic];
        _shared = [[CKDatabase alloc] __initWithContainer:self scope:CKDatabaseScopeShared];
    }
    return self;
}
- (CKDatabase *)privateCloudDatabase { return _private; }
- (CKDatabase *)publicCloudDatabase { return _public; }
- (CKDatabase *)sharedCloudDatabase { return _shared; }
- (CKDatabase *)databaseWithDatabaseScope:(CKDatabaseScope)scope {
    return scope == CKDatabaseScopePublic ? _public : scope == CKDatabaseScopeShared ? _shared : _private;
}
- (void)addOperation:(CKOperation *)op {
    if (!op) return;
    [op retain];
    ck_later(^{ [op start]; [op release]; });
}
- (void)accountStatusWithCompletionHandler:(void (^)(CKAccountStatus, NSError *))h {
    if (!h) return;
    void (^b)(CKAccountStatus, NSError *) = [h copy];
    ck_later(^{ b(CKAccountStatusNoAccount, nil); [b release]; });
}
#define CK_FAIL2(h) do { if (!(h)) return; void (^b_)(id, NSError *) = [(id)(h) copy]; ck_later(^{ b_(nil, ck_no_account()); [b_ release]; }); } while (0)
- (void)fetchUserRecordIDWithCompletionHandler:(void (^)(CKRecordID *, NSError *))h { CK_FAIL2(h); }
- (void)statusForApplicationPermission:(CKApplicationPermissions)p completionHandler:(void (^)(CKApplicationPermissionStatus, NSError *))h {
    (void)p;
    if (!h) return;
    void (^b)(CKApplicationPermissionStatus, NSError *) = [h copy];
    ck_later(^{ b(CKApplicationPermissionStatusCouldNotComplete, ck_no_account()); [b release]; });
}
- (void)requestApplicationPermission:(CKApplicationPermissions)p completionHandler:(void (^)(CKApplicationPermissionStatus, NSError *))h {
    [self statusForApplicationPermission:p completionHandler:h];
}
- (void)discoverAllIdentitiesWithCompletionHandler:(void (^)(NSArray *, NSError *))h { CK_FAIL2(h); }
- (void)discoverUserIdentityWithEmailAddress:(NSString *)e completionHandler:(void (^)(CKUserIdentity *, NSError *))h { (void)e; CK_FAIL2(h); }
- (void)discoverUserIdentityWithPhoneNumber:(NSString *)p completionHandler:(void (^)(CKUserIdentity *, NSError *))h { (void)p; CK_FAIL2(h); }
- (void)discoverUserIdentityWithUserRecordID:(CKRecordID *)r completionHandler:(void (^)(CKUserIdentity *, NSError *))h { (void)r; CK_FAIL2(h); }
- (void)fetchShareParticipantWithEmailAddress:(NSString *)e completionHandler:(void (^)(CKShareParticipant *, NSError *))h { (void)e; CK_FAIL2(h); }
- (void)fetchShareParticipantWithPhoneNumber:(NSString *)p completionHandler:(void (^)(CKShareParticipant *, NSError *))h { (void)p; CK_FAIL2(h); }
- (void)fetchShareParticipantWithUserRecordID:(CKRecordID *)r completionHandler:(void (^)(CKShareParticipant *, NSError *))h { (void)r; CK_FAIL2(h); }
- (void)fetchShareMetadataWithURL:(NSURL *)u completionHandler:(void (^)(CKShareMetadata *, NSError *))h { (void)u; CK_FAIL2(h); }
- (void)acceptShareMetadata:(CKShareMetadata *)m completionHandler:(void (^)(CKShare *, NSError *))h { (void)m; CK_FAIL2(h); }
/* operações de longa duração: nenhuma sobrevive sem servidor; a lista é vazia, sem erro */
- (void)fetchAllLongLivedOperationIDsWithCompletionHandler:(void (^)(NSArray *, NSError *))h {
    if (!h) return;
    void (^b)(NSArray *, NSError *) = [h copy];
    ck_later(^{ b([NSArray array], nil); [b release]; });
}
- (void)fetchLongLivedOperationWithID:(NSString *)i completionHandler:(void (^)(CKOperation *, NSError *))h { (void)i; CK_FAIL2(h); }
- (void)dealloc { [_containerIdentifier release]; [_private release]; [_public release]; [_shared release]; [super dealloc]; }
@end

@implementation CKDatabase {
    CKContainer *_container;   /* fraco: o contêiner é dono */
}
- (instancetype)__initWithContainer:(CKContainer *)c scope:(CKDatabaseScope)scope {
    if ((self = [super init])) { _container = c; _databaseScope = scope; }
    return self;
}
- (void)addOperation:(CKDatabaseOperation *)op {
    if (!op) return;
    if (![op database]) [op setDatabase:self];
    [_container addOperation:op];
}
- (void)fetchRecordWithID:(CKRecordID *)r completionHandler:(void (^)(CKRecord *, NSError *))h { (void)r; CK_FAIL2(h); }
- (void)saveRecord:(CKRecord *)r completionHandler:(void (^)(CKRecord *, NSError *))h { (void)r; CK_FAIL2(h); }
- (void)deleteRecordWithID:(CKRecordID *)r completionHandler:(void (^)(CKRecordID *, NSError *))h { (void)r; CK_FAIL2(h); }
- (void)performQuery:(CKQuery *)q inZoneWithID:(CKRecordZoneID *)z completionHandler:(void (^)(NSArray *, NSError *))h { (void)q; (void)z; CK_FAIL2(h); }
- (void)fetchAllRecordZonesWithCompletionHandler:(void (^)(NSArray *, NSError *))h { CK_FAIL2(h); }
- (void)fetchRecordZoneWithID:(CKRecordZoneID *)z completionHandler:(void (^)(CKRecordZone *, NSError *))h { (void)z; CK_FAIL2(h); }
- (void)saveRecordZone:(CKRecordZone *)z completionHandler:(void (^)(CKRecordZone *, NSError *))h { (void)z; CK_FAIL2(h); }
- (void)deleteRecordZoneWithID:(CKRecordZoneID *)z completionHandler:(void (^)(CKRecordZoneID *, NSError *))h { (void)z; CK_FAIL2(h); }
- (void)fetchSubscriptionWithID:(NSString *)s completionHandler:(void (^)(CKSubscription *, NSError *))h { (void)s; CK_FAIL2(h); }
- (void)fetchAllSubscriptionsWithCompletionHandler:(void (^)(NSArray *, NSError *))h { CK_FAIL2(h); }
- (void)saveSubscription:(CKSubscription *)s completionHandler:(void (^)(CKSubscription *, NSError *))h { (void)s; CK_FAIL2(h); }
- (void)deleteSubscriptionWithID:(NSString *)s completionHandler:(void (^)(NSString *, NSError *))h { (void)s; CK_FAIL2(h); }
@end

/* ================================================================ operações
 * -main entrega o erro de "sem conta" ao bloco de conclusão da operação (os blocos por item não são chamados: o
 * servidor nem é contatado, como o CloudKit faz quando a conta falta). Depois o NSOperation marca o fim e chama o
 * completionBlock. */
@implementation CKOperation {
    NSMutableDictionary *_swift;
}
- (instancetype)init {
    if ((self = [super init])) {
        _operationID = [[[NSUUID UUID] UUIDString] copy];
        _swift = [[NSMutableDictionary alloc] init];
    }
    return self;
}
- (CKOperationConfiguration *)configuration {
    if (!_configuration) _configuration = [[CKOperationConfiguration alloc] init];
    return _configuration;
}
- (nullable id)__mosSwiftValueForKey:(NSString *)key { return key ? [_swift objectForKey:key] : nil; }
- (void)__mosSetSwiftValue:(nullable id)value forKey:(NSString *)key {
    if (!key) return;
    if (value) [_swift setObject:value forKey:key]; else [_swift removeObjectForKey:key];
}
- (void)__mosFinishWithError:(NSError *)err { (void)err; }
- (void)main {
    NSError *err = [self isCancelled] ? [NSError errorWithDomain:CKErrorDomain code:CKErrorOperationCancelled userInfo:nil] : ck_no_account();
    [self __mosFinishWithError:err];
}
CK_DEALLOC(NSOperation)
@end
@implementation CKDatabaseOperation
CK_DEALLOC(CKOperation)
@end

@implementation CKFetchRecordsOperation
+ (instancetype)fetchCurrentUserRecordOperation { return [[[self alloc] init] autorelease]; }
- (instancetype)initWithRecordIDs:(NSArray *)ids { if ((self = [self init])) [self setRecordIDs:ids]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_fetchRecordsCompletionBlock) _fetchRecordsCompletionBlock(nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKModifyRecordsOperation
- (instancetype)initWithRecordsToSave:(NSArray *)r recordIDsToDelete:(NSArray *)d {
    if ((self = [self init])) { [self setRecordsToSave:r]; [self setRecordIDsToDelete:d]; _atomic = YES; }
    return self;
}
- (void)__mosFinishWithError:(NSError *)e { if (_modifyRecordsCompletionBlock) _modifyRecordsCompletionBlock(nil, nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKQueryOperation
- (instancetype)initWithQuery:(CKQuery *)q { if ((self = [self init])) [self setQuery:q]; return self; }
- (instancetype)initWithCursor:(CKQueryCursor *)c { if ((self = [self init])) [self setCursor:c]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_queryCompletionBlock) _queryCompletionBlock(nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKFetchRecordZonesOperation
+ (instancetype)fetchAllRecordZonesOperation { return [[[self alloc] init] autorelease]; }
- (instancetype)initWithRecordZoneIDs:(NSArray *)ids { if ((self = [self init])) [self setRecordZoneIDs:ids]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_fetchRecordZonesCompletionBlock) _fetchRecordZonesCompletionBlock(nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKModifyRecordZonesOperation
- (instancetype)initWithRecordZonesToSave:(NSArray *)s recordZoneIDsToDelete:(NSArray *)d {
    if ((self = [self init])) { [self setRecordZonesToSave:s]; [self setRecordZoneIDsToDelete:d]; }
    return self;
}
- (void)__mosFinishWithError:(NSError *)e { if (_modifyRecordZonesCompletionBlock) _modifyRecordZonesCompletionBlock(nil, nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKFetchRecordZoneChangesConfiguration
- (id)copyWithZone:(void *)z {
    (void)z;
    CKFetchRecordZoneChangesConfiguration *c = [[CKFetchRecordZoneChangesConfiguration alloc] init];
    [c setPreviousServerChangeToken:_previousServerChangeToken]; [c setResultsLimit:_resultsLimit]; [c setDesiredKeys:_desiredKeys];
    return c;
}
- (id)copy { return [self copyWithZone:0]; }
CK_DEALLOC(NSObject)
@end
@implementation CKFetchRecordZoneChangesOperation
- (instancetype)initWithRecordZoneIDs:(NSArray *)ids configurationsByRecordZoneID:(NSDictionary *)c {
    if ((self = [self init])) { [self setRecordZoneIDs:ids]; [self setConfigurationsByRecordZoneID:c]; _fetchAllChanges = YES; }
    return self;
}
- (void)__mosFinishWithError:(NSError *)e { if (_fetchRecordZoneChangesCompletionBlock) _fetchRecordZoneChangesCompletionBlock(e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKFetchRecordChangesOperation
- (instancetype)initWithRecordZoneID:(CKRecordZoneID *)z previousServerChangeToken:(CKServerChangeToken *)t {
    if ((self = [self init])) { [self setRecordZoneID:z]; [self setPreviousServerChangeToken:t]; }
    return self;
}
- (void)__mosFinishWithError:(NSError *)e { if (_fetchRecordChangesCompletionBlock) _fetchRecordChangesCompletionBlock(nil, nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKFetchDatabaseChangesOperation
- (instancetype)initWithPreviousServerChangeToken:(CKServerChangeToken *)t {
    if ((self = [self init])) { [self setPreviousServerChangeToken:t]; _fetchAllChanges = YES; }
    return self;
}
- (void)__mosFinishWithError:(NSError *)e { if (_fetchDatabaseChangesCompletionBlock) _fetchDatabaseChangesCompletionBlock(nil, NO, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKModifySubscriptionsOperation
- (instancetype)initWithSubscriptionsToSave:(NSArray *)s subscriptionIDsToDelete:(NSArray *)d {
    if ((self = [self init])) { [self setSubscriptionsToSave:s]; [self setSubscriptionIDsToDelete:d]; }
    return self;
}
- (void)__mosFinishWithError:(NSError *)e { if (_modifySubscriptionsCompletionBlock) _modifySubscriptionsCompletionBlock(nil, nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKFetchSubscriptionsOperation
+ (instancetype)fetchAllSubscriptionsOperation { return [[[self alloc] init] autorelease]; }
- (instancetype)initWithSubscriptionIDs:(NSArray *)ids { if ((self = [self init])) [self setSubscriptionIDs:ids]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_fetchSubscriptionCompletionBlock) _fetchSubscriptionCompletionBlock(nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end
@implementation CKFetchShareMetadataOperation
- (instancetype)initWithShareURLs:(NSArray *)u { if ((self = [self init])) [self setShareURLs:u]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_fetchShareMetadataCompletionBlock) _fetchShareMetadataCompletionBlock(e); }
CK_DEALLOC(CKOperation)
@end
@implementation CKAcceptSharesOperation
- (instancetype)initWithShareMetadatas:(NSArray *)m { if ((self = [self init])) [self setShareMetadatas:m]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_acceptSharesCompletionBlock) _acceptSharesCompletionBlock(e); }
CK_DEALLOC(CKOperation)
@end
@implementation CKFetchShareParticipantsOperation
- (instancetype)initWithUserIdentityLookupInfos:(NSArray *)l { if ((self = [self init])) [self setUserIdentityLookupInfos:l]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_fetchShareParticipantsCompletionBlock) _fetchShareParticipantsCompletionBlock(e); }
CK_DEALLOC(CKOperation)
@end
@implementation CKDiscoverAllUserIdentitiesOperation
- (void)__mosFinishWithError:(NSError *)e { if (_discoverAllUserIdentitiesCompletionBlock) _discoverAllUserIdentitiesCompletionBlock(e); }
CK_DEALLOC(CKOperation)
@end
@implementation CKDiscoverUserIdentitiesOperation
- (instancetype)initWithUserIdentityLookupInfos:(NSArray *)l { if ((self = [self init])) [self setUserIdentityLookupInfos:l]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_discoverUserIdentitiesCompletionBlock) _discoverUserIdentitiesCompletionBlock(e); }
CK_DEALLOC(CKOperation)
@end
@implementation CKFetchWebAuthTokenOperation
- (instancetype)initWithAPIToken:(NSString *)t { if ((self = [self init])) [self setAPIToken:t]; return self; }
- (void)__mosFinishWithError:(NSError *)e { if (_fetchWebAuthTokenCompletionBlock) _fetchWebAuthTokenCompletionBlock(nil, e); }
CK_DEALLOC(CKDatabaseOperation)
@end

/* ================================================================ valores aceitos em CKRecord (protocolo CKRecordValue) */
@interface NSString (CKRecordValue) <CKRecordValue> @end
@implementation NSString (CKRecordValue) @end
@interface NSNumber (CKRecordValue) <CKRecordValue> @end
@implementation NSNumber (CKRecordValue) @end
@interface NSArray (CKRecordValue) <CKRecordValue> @end
@implementation NSArray (CKRecordValue) @end
@interface NSDate (CKRecordValue) <CKRecordValue> @end
@implementation NSDate (CKRecordValue) @end
@interface NSData (CKRecordValue) <CKRecordValue> @end
@implementation NSData (CKRecordValue) @end
