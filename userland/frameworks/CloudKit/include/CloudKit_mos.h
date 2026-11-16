/* mOS — interface ObjC do CloudKit.framework (userland/frameworks/CloudKit/CloudKit.m) com os nomes da Apple.
 * Serve ao .m e ao overlay Swift (CloudKit.swift, pelo module.modulemap): os membros marcados com CK_REFINED são os
 * que o overlay da Apple redefine em Swift; com eles privados aqui, os nomes mangled do overlay saem iguais aos dela.
 * O mOS não tem iCloud: a conta é "sem conta" e toda operação de rede termina com CKErrorNotAuthenticated. */
#pragma once
#import <Foundation/Foundation.h>

#ifndef CK_REFINED
#define CK_REFINED __attribute__((swift_private))
#endif
#ifndef CK_SWIFT_NAME
#define CK_SWIFT_NAME(n) __attribute__((swift_name(#n)))
#endif
#ifndef CK_EXTERN
#define CK_EXTERN extern __attribute__((visibility("default")))
#endif

#pragma clang assume_nonnull begin

@class NSURL, NSUUID, NSPredicate, NSSortDescriptor, NSOperationQueue, NSCoder;
@class CKRecord, CKRecordID, CKRecordZone, CKRecordZoneID, CKDatabase, CKContainer, CKShare, CKShareMetadata,
    CKShareParticipant, CKUserIdentity, CKUserIdentityLookupInfo, CKQuery, CKQueryCursor, CKServerChangeToken,
    CKSubscription, CKNotificationInfo, CKOperationConfiguration, CKOperationGroup, CKAsset, CKReference;

/* NSOperation: os dois Foundation.h (o do libSystem e o do importador Swift) não declaram */
@interface NSOperation : NSObject
- (void)start;
- (void)main;
- (void)cancel;
@property (readonly, getter=isCancelled) BOOL cancelled;
@property (readonly, getter=isExecuting) BOOL executing;
@property (readonly, getter=isFinished) BOOL finished;
@property (nullable, copy) void (^completionBlock)(void);
- (void)waitUntilFinished;
@end

/* ---------------------------------------------------------------- erros */
CK_EXTERN NSString *const CKErrorDomain;
CK_EXTERN NSString *const CKPartialErrorsByItemIDKey;
CK_EXTERN NSString *const CKRecordChangedErrorAncestorRecordKey;
CK_EXTERN NSString *const CKRecordChangedErrorServerRecordKey;
CK_EXTERN NSString *const CKRecordChangedErrorClientRecordKey;
CK_EXTERN NSString *const CKErrorUserDidResetEncryptedDataKey;
CK_EXTERN NSString *const CKErrorRetryAfterKey;
typedef enum __attribute__((ns_error_domain(CKErrorDomain))) CKErrorCode : NSInteger CKErrorCode;
enum __attribute__((ns_error_domain(CKErrorDomain))) CKErrorCode : NSInteger {
    CKErrorInternalError = 1, CKErrorPartialFailure = 2, CKErrorNetworkUnavailable = 3, CKErrorNetworkFailure = 4,
    CKErrorBadContainer = 5, CKErrorServiceUnavailable = 6, CKErrorRequestRateLimited = 7, CKErrorMissingEntitlement = 8,
    CKErrorNotAuthenticated = 9, CKErrorPermissionFailure = 10, CKErrorUnknownItem = 11, CKErrorInvalidArguments = 12,
    CKErrorResultsTruncated = 13, CKErrorServerRecordChanged = 14, CKErrorServerRejectedRequest = 15,
    CKErrorAssetFileNotFound = 16, CKErrorAssetFileModified = 17, CKErrorIncompatibleVersion = 18,
    CKErrorConstraintViolation = 19, CKErrorOperationCancelled = 20, CKErrorChangeTokenExpired = 21,
    CKErrorBatchRequestFailed = 22, CKErrorZoneBusy = 23, CKErrorBadDatabase = 24, CKErrorQuotaExceeded = 25,
    CKErrorZoneNotFound = 26, CKErrorLimitExceeded = 27, CKErrorUserDeletedZone = 28, CKErrorTooManyParticipants = 29,
    CKErrorAlreadyShared = 30, CKErrorReferenceViolation = 31, CKErrorManagedAccountRestricted = 32,
    CKErrorParticipantMayNeedVerification = 33, CKErrorServerResponseLost = 34, CKErrorAssetNotAvailable = 35,
    CKErrorAccountTemporarilyUnavailable = 36, CKErrorParticipantAlreadyInvited = 37
};

/* ---------------------------------------------------------------- constantes */
CK_EXTERN NSString *const CKRecordZoneDefaultName;
CK_EXTERN NSString *const CKCurrentUserDefaultName;
CK_EXTERN NSString *const CKOwnerDefaultName;
CK_EXTERN NSString *const CKAccountChangedNotification;
CK_EXTERN NSString *const CKRecordTypeUserRecord;
CK_EXTERN NSString *const CKRecordTypeShare;
CK_EXTERN NSString *const CKRecordNameZoneWideShare;
CK_EXTERN NSString *const CKShareTitleKey;
CK_EXTERN NSString *const CKShareThumbnailImageDataKey;
CK_EXTERN NSString *const CKShareTypeKey;
CK_EXTERN NSString *const CKRecordCreationDateKey;
CK_EXTERN NSString *const CKRecordCreatorUserRecordIDKey;
CK_EXTERN NSString *const CKRecordLastModifiedUserRecordIDKey;
CK_EXTERN NSString *const CKRecordModificationDateKey;
CK_EXTERN NSString *const CKRecordParentKey;
CK_EXTERN NSString *const CKRecordRecordIDKey;
CK_EXTERN NSString *const CKRecordShareKey;
CK_EXTERN const NSUInteger CKQueryOperationMaximumResults;

typedef NS_ENUM(NSInteger, CKAccountStatus) {
    CKAccountStatusCouldNotDetermine = 0, CKAccountStatusAvailable = 1, CKAccountStatusRestricted = 2,
    CKAccountStatusNoAccount = 3, CKAccountStatusTemporarilyUnavailable = 4
};
typedef NS_ENUM(NSInteger, CKDatabaseScope) { CKDatabaseScopePublic = 1, CKDatabaseScopePrivate = 2, CKDatabaseScopeShared = 3 };
typedef NS_OPTIONS(NSUInteger, CKApplicationPermissions) { CKApplicationPermissionUserDiscoverability = 1 << 0 };
typedef NS_ENUM(NSInteger, CKApplicationPermissionStatus) {
    CKApplicationPermissionStatusInitialState = 0, CKApplicationPermissionStatusCouldNotComplete = 1,
    CKApplicationPermissionStatusDenied = 2, CKApplicationPermissionStatusGranted = 3
};
typedef NS_ENUM(NSInteger, CKRecordSavePolicy) { CKRecordSaveIfServerRecordUnchanged = 0, CKRecordSaveChangedKeys = 1, CKRecordSaveAllKeys = 2 };
typedef NS_ENUM(NSUInteger, CKReferenceAction) { CKReferenceActionNone = 0, CKReferenceActionDeleteSelf = 1 };
typedef NS_OPTIONS(NSUInteger, CKRecordZoneCapabilities) {
    CKRecordZoneCapabilityFetchChanges = 1 << 0, CKRecordZoneCapabilityAtomic = 1 << 1, CKRecordZoneCapabilitySharing = 1 << 2,
    CKRecordZoneCapabilityZoneWideSharing = 1 << 3
};
typedef NS_ENUM(NSInteger, CKSubscriptionType) { CKSubscriptionTypeQuery = 1, CKSubscriptionTypeRecordZone = 2, CKSubscriptionTypeDatabase = 3 };
typedef NS_OPTIONS(NSUInteger, CKQuerySubscriptionOptions) {
    CKQuerySubscriptionOptionsFiresOnRecordCreation = 1 << 0, CKQuerySubscriptionOptionsFiresOnRecordUpdate = 1 << 1,
    CKQuerySubscriptionOptionsFiresOnRecordDeletion = 1 << 2, CKQuerySubscriptionOptionsFiresOnce = 1 << 3
};
typedef NS_ENUM(NSInteger, CKNotificationType) {
    CKNotificationTypeQuery = 1, CKNotificationTypeRecordZone = 2, CKNotificationTypeReadNotification = 3, CKNotificationTypeDatabase = 4
};
typedef NS_ENUM(NSInteger, CKShareParticipantPermission) {
    CKShareParticipantPermissionUnknown = 0, CKShareParticipantPermissionNone = 1, CKShareParticipantPermissionReadOnly = 2,
    CKShareParticipantPermissionReadWrite = 3
};
typedef NS_ENUM(NSInteger, CKOperationGroupTransferSize) { CKOperationGroupTransferSizeUnknown = 0 };

/* ---------------------------------------------------------------- valores de registro */
CK_SWIFT_NAME(__CKRecordObjCValue)
@protocol CKRecordValue <NSObject>
@end

@protocol CKRecordKeyValueSetting <NSObject>
- (nullable id)objectForKey:(NSString *)key;
- (void)setObject:(nullable id)object forKey:(NSString *)key;
- (nullable id)objectForKeyedSubscript:(NSString *)key CK_REFINED;
- (void)setObject:(nullable id)object forKeyedSubscript:(NSString *)key CK_REFINED;
- (NSArray<NSString *> *)allKeys CK_REFINED;
- (NSArray<NSString *> *)changedKeys;
@end

@interface CKRecordZoneID : NSObject <NSCopying>
- (instancetype)initWithZoneName:(NSString *)zoneName ownerName:(NSString *)ownerName CK_REFINED;
@property (readonly, copy) NSString *zoneName;
@property (readonly, copy) NSString *ownerName;
@end

@interface CKRecordID : NSObject <NSCopying>
- (instancetype)initWithRecordName:(NSString *)recordName;
- (instancetype)initWithRecordName:(NSString *)recordName zoneID:(CKRecordZoneID *)zoneID CK_REFINED;
@property (readonly, copy) NSString *recordName;
@property (readonly, strong) CKRecordZoneID *zoneID;
@end

@interface CKRecordZone : NSObject <NSCopying>
+ (CKRecordZone *)defaultRecordZone;
- (instancetype)initWithZoneName:(NSString *)zoneName;
- (instancetype)initWithZoneID:(CKRecordZoneID *)zoneID;
@property (readonly, strong) CKRecordZoneID *zoneID;
@property (readonly) CKRecordZoneCapabilities capabilities;
@property (nullable, readonly, strong) CKReference *share;
@end

@interface CKReference : NSObject <NSCopying, CKRecordValue>
- (instancetype)initWithRecordID:(CKRecordID *)recordID action:(CKReferenceAction)action;
- (instancetype)initWithRecord:(CKRecord *)record action:(CKReferenceAction)action;
@property (readonly) CKReferenceAction referenceAction;
@property (readonly, copy) CKRecordID *recordID;
@end

@interface CKAsset : NSObject <CKRecordValue>
- (instancetype)initWithFileURL:(NSURL *)fileURL;
@property (nullable, readonly, copy) NSURL *fileURL;
@end

@interface CKRecord : NSObject <NSCopying, CKRecordKeyValueSetting>
- (instancetype)initWithRecordType:(NSString *)recordType;
- (instancetype)initWithRecordType:(NSString *)recordType recordID:(CKRecordID *)recordID CK_REFINED;
- (instancetype)initWithRecordType:(NSString *)recordType zoneID:(CKRecordZoneID *)zoneID;
- (nullable instancetype)initWithCoder:(NSCoder *)coder;
@property (readonly, copy) NSString *recordType CK_REFINED;
@property (readonly, copy) CKRecordID *recordID;
@property (nullable, readonly, copy) NSString *recordChangeTag;
@property (nullable, readonly, copy) CKRecordID *creatorUserRecordID;
@property (nullable, readonly, copy) NSDate *creationDate;
@property (nullable, readonly, copy) CKRecordID *lastModifiedUserRecordID;
@property (nullable, readonly, copy) NSDate *modificationDate;
@property (nullable, copy) CKReference *parent;
@property (nullable, readonly, copy) CKReference *share;
@property (readonly, strong) id<CKRecordKeyValueSetting> encryptedValues;
- (void)setParentReferenceFromRecord:(nullable CKRecord *)parentRecord;
- (void)setParentReferenceFromRecordID:(nullable CKRecordID *)parentRecordID;
- (NSArray<NSString *> *)allTokens;
- (void)encodeSystemFieldsWithCoder:(NSCoder *)coder;
@end

@interface CKShare : CKRecord
- (instancetype)initWithRootRecord:(CKRecord *)rootRecord;
- (instancetype)initWithRootRecord:(CKRecord *)rootRecord shareID:(CKRecordID *)shareID;
- (instancetype)initWithRecordZoneID:(CKRecordZoneID *)recordZoneID;
@property CKShareParticipantPermission publicPermission;
@property (nullable, readonly, copy) NSURL *URL;
@property (readonly, copy) NSArray<CKShareParticipant *> *participants;
@property (nullable, readonly, copy) CKShareParticipant *owner;
@property (nullable, readonly, copy) CKShareParticipant *currentUserParticipant;
- (void)addParticipant:(CKShareParticipant *)participant;
- (void)removeParticipant:(CKShareParticipant *)participant;
@end

@interface CKShareParticipant : NSObject <NSCopying>
@property (readonly, copy) CKUserIdentity *userIdentity;
@property CKShareParticipantPermission permission;
@end
@interface CKUserIdentity : NSObject <NSCopying>
@property (nullable, readonly, copy) CKRecordID *userRecordID;
@property (nullable, readonly, copy) CKUserIdentityLookupInfo *lookupInfo;
@property (readonly) BOOL hasiCloudAccount;
@end
@interface CKUserIdentityLookupInfo : NSObject <NSCopying>
- (instancetype)initWithEmailAddress:(NSString *)emailAddress;
- (instancetype)initWithPhoneNumber:(NSString *)phoneNumber;
- (instancetype)initWithUserRecordID:(CKRecordID *)userRecordID;
@property (nullable, readonly, copy) NSString *emailAddress;
@property (nullable, readonly, copy) NSString *phoneNumber;
@property (nullable, readonly, copy) CKRecordID *userRecordID;
@end
@interface CKShareMetadata : NSObject <NSCopying>
@property (readonly, copy) NSString *containerIdentifier;
@property (readonly, copy) CKShare *share;
@property (nullable, readonly, copy) CKRecordID *hierarchicalRootRecordID;
@end

@interface CKQuery : NSObject <NSCopying>
- (instancetype)initWithRecordType:(NSString *)recordType predicate:(NSPredicate *)predicate;
@property (readonly, copy) NSString *recordType;
@property (readonly, copy) NSPredicate *predicate;
@property (nullable, copy) NSArray<NSSortDescriptor *> *sortDescriptors;
@end
@interface CKQueryCursor : NSObject <NSCopying>
@end
@interface CKServerChangeToken : NSObject <NSCopying>
@end

/* ---------------------------------------------------------------- assinaturas e notificações */
@interface CKNotificationInfo : NSObject <NSCopying>
@property (nullable, copy) NSString *alertBody;
@property (nullable, copy) NSString *alertLocalizationKey;
@property (nullable, copy) NSString *title;
@property (nullable, copy) NSString *subtitle;
@property (nullable, copy) NSString *soundName;
@property (nullable, copy) NSArray<NSString *> *desiredKeys;
@property BOOL shouldBadge;
@property BOOL shouldSendContentAvailable;
@property BOOL shouldSendMutableContent;
@property (nullable, copy) NSString *category;
@property (nullable, copy) NSString *collapseIDKey;
@end
@interface CKSubscription : NSObject <NSCopying>
@property (readonly, copy) NSString *subscriptionID;
@property (readonly) CKSubscriptionType subscriptionType;
@property (nullable, copy) CKNotificationInfo *notificationInfo;
@end
@interface CKQuerySubscription : CKSubscription
- (instancetype)initWithRecordType:(NSString *)recordType predicate:(NSPredicate *)predicate options:(CKQuerySubscriptionOptions)options;
- (instancetype)initWithRecordType:(NSString *)recordType predicate:(NSPredicate *)predicate subscriptionID:(NSString *)subscriptionID
                           options:(CKQuerySubscriptionOptions)options;
@property (readonly, copy) NSString *recordType;
@property (readonly, copy) NSPredicate *predicate;
@property (nullable, copy) CKRecordZoneID *zoneID;
@property (readonly) CKQuerySubscriptionOptions querySubscriptionOptions;
@end
@interface CKRecordZoneSubscription : CKSubscription
- (instancetype)initWithZoneID:(CKRecordZoneID *)zoneID;
- (instancetype)initWithZoneID:(CKRecordZoneID *)zoneID subscriptionID:(NSString *)subscriptionID;
@property (readonly, copy) CKRecordZoneID *zoneID;
@property (nullable, copy) NSString *recordType;
@end
@interface CKDatabaseSubscription : CKSubscription
- (instancetype)init;
- (instancetype)initWithSubscriptionID:(NSString *)subscriptionID CK_REFINED;
@property (nullable, copy) NSString *recordType CK_REFINED;
@end
@interface CKNotification : NSObject
+ (nullable instancetype)notificationFromRemoteNotificationDictionary:(NSDictionary *)notificationDictionary;
@property (readonly) CKNotificationType notificationType;
@property (nullable, readonly, copy) NSString *containerIdentifier;
@property (nullable, readonly, copy) NSString *subscriptionID CK_REFINED;
@property (readonly) BOOL isPruned;
@end

/* ---------------------------------------------------------------- contêiner e banco */
@interface CKOperationConfiguration : NSObject <NSCopying>
@property (nullable, strong) CKContainer *container;
@property NSInteger qualityOfService;
@property BOOL allowsCellularAccess;
@property (getter=isLongLived) BOOL longLived;
@property double timeoutIntervalForRequest;
@property double timeoutIntervalForResource;
@end
@interface CKOperationGroup : NSObject
@property (readonly, copy) NSString *operationGroupID;
@property (nonatomic, null_resettable, copy) CKOperationConfiguration *defaultConfiguration;
@property (nullable, copy) NSString *name;
@property NSUInteger quantity;
@property CKOperationGroupTransferSize expectedSendSize;
@property CKOperationGroupTransferSize expectedReceiveSize;
@end

@class CKOperation, CKDatabaseOperation;
@interface CKContainer : NSObject
+ (CKContainer *)defaultContainer;
+ (CKContainer *)containerWithIdentifier:(NSString *)containerIdentifier;
@property (nullable, readonly, copy) NSString *containerIdentifier;
@property (readonly, strong) CKDatabase *privateCloudDatabase;
@property (readonly, strong) CKDatabase *publicCloudDatabase;
@property (readonly, strong) CKDatabase *sharedCloudDatabase;
- (CKDatabase *)databaseWithDatabaseScope:(CKDatabaseScope)databaseScope;
- (void)addOperation:(CKOperation *)operation;
- (void)accountStatusWithCompletionHandler:(void (^)(CKAccountStatus accountStatus, NSError *_Nullable error))completionHandler;
- (void)fetchUserRecordIDWithCompletionHandler:(void (^)(CKRecordID *_Nullable recordID, NSError *_Nullable error))completionHandler;
- (void)statusForApplicationPermission:(CKApplicationPermissions)applicationPermission
                     completionHandler:(void (^)(CKApplicationPermissionStatus applicationPermissionStatus, NSError *_Nullable error))completionHandler;
- (void)requestApplicationPermission:(CKApplicationPermissions)applicationPermission
                   completionHandler:(void (^)(CKApplicationPermissionStatus applicationPermissionStatus, NSError *_Nullable error))completionHandler;
- (void)discoverAllIdentitiesWithCompletionHandler:(void (^)(NSArray<CKUserIdentity *> *_Nullable userIdentities, NSError *_Nullable error))completionHandler;
- (void)discoverUserIdentityWithEmailAddress:(NSString *)email completionHandler:(void (^)(CKUserIdentity *_Nullable userInfo, NSError *_Nullable error))completionHandler;
- (void)discoverUserIdentityWithPhoneNumber:(NSString *)phoneNumber completionHandler:(void (^)(CKUserIdentity *_Nullable userInfo, NSError *_Nullable error))completionHandler;
- (void)discoverUserIdentityWithUserRecordID:(CKRecordID *)userRecordID completionHandler:(void (^)(CKUserIdentity *_Nullable userInfo, NSError *_Nullable error))completionHandler;
- (void)fetchShareParticipantWithEmailAddress:(NSString *)emailAddress completionHandler:(void (^)(CKShareParticipant *_Nullable shareParticipant, NSError *_Nullable error))completionHandler;
- (void)fetchShareParticipantWithPhoneNumber:(NSString *)phoneNumber completionHandler:(void (^)(CKShareParticipant *_Nullable shareParticipant, NSError *_Nullable error))completionHandler;
- (void)fetchShareParticipantWithUserRecordID:(CKRecordID *)userRecordID completionHandler:(void (^)(CKShareParticipant *_Nullable shareParticipant, NSError *_Nullable error))completionHandler;
- (void)fetchShareMetadataWithURL:(NSURL *)url completionHandler:(void (^)(CKShareMetadata *_Nullable metadata, NSError *_Nullable error))completionHandler;
- (void)acceptShareMetadata:(CKShareMetadata *)metadata completionHandler:(void (^)(CKShare *_Nullable acceptedShare, NSError *_Nullable error))completionHandler;
- (void)fetchAllLongLivedOperationIDsWithCompletionHandler:(void (^)(NSArray<NSString *> *_Nullable outstandingOperationIDs, NSError *_Nullable error))completionHandler;
- (void)fetchLongLivedOperationWithID:(NSString *)operationID completionHandler:(void (^)(CKOperation *_Nullable outstandingOperation, NSError *_Nullable error))completionHandler;
@end

@interface CKDatabase : NSObject
@property (readonly) CKDatabaseScope databaseScope;
- (void)addOperation:(CKDatabaseOperation *)operation;
- (void)fetchRecordWithID:(CKRecordID *)recordID completionHandler:(void (^)(CKRecord *_Nullable record, NSError *_Nullable error))completionHandler;
- (void)saveRecord:(CKRecord *)record completionHandler:(void (^)(CKRecord *_Nullable record, NSError *_Nullable error))completionHandler;
- (void)deleteRecordWithID:(CKRecordID *)recordID completionHandler:(void (^)(CKRecordID *_Nullable recordID, NSError *_Nullable error))completionHandler;
- (void)performQuery:(CKQuery *)query inZoneWithID:(nullable CKRecordZoneID *)zoneID
   completionHandler:(void (^)(NSArray<CKRecord *> *_Nullable results, NSError *_Nullable error))completionHandler;
- (void)fetchAllRecordZonesWithCompletionHandler:(void (^)(NSArray<CKRecordZone *> *_Nullable zones, NSError *_Nullable error))completionHandler;
- (void)fetchRecordZoneWithID:(CKRecordZoneID *)zoneID completionHandler:(void (^)(CKRecordZone *_Nullable zone, NSError *_Nullable error))completionHandler;
- (void)saveRecordZone:(CKRecordZone *)zone completionHandler:(void (^)(CKRecordZone *_Nullable zone, NSError *_Nullable error))completionHandler;
- (void)deleteRecordZoneWithID:(CKRecordZoneID *)zoneID completionHandler:(void (^)(CKRecordZoneID *_Nullable zoneID, NSError *_Nullable error))completionHandler;
- (void)fetchSubscriptionWithID:(NSString *)subscriptionID completionHandler:(void (^)(CKSubscription *_Nullable subscription, NSError *_Nullable error))completionHandler;
- (void)fetchAllSubscriptionsWithCompletionHandler:(void (^)(NSArray<CKSubscription *> *_Nullable subscriptions, NSError *_Nullable error))completionHandler;
- (void)saveSubscription:(CKSubscription *)subscription completionHandler:(void (^)(CKSubscription *_Nullable subscription, NSError *_Nullable error))completionHandler;
- (void)deleteSubscriptionWithID:(NSString *)subscriptionID completionHandler:(void (^)(NSString *_Nullable subscriptionID, NSError *_Nullable error))completionHandler;
@end

/* ---------------------------------------------------------------- operações */
@interface CKOperation : NSOperation
@property (nonatomic, null_resettable, copy) CKOperationConfiguration *configuration;
@property (nullable, strong) CKOperationGroup *group;
@property (readonly, copy) NSString *operationID;
@property (nullable, copy) void (^longLivedOperationWasPersistedBlock)(void);
/* armazenamento dos blocos do overlay Swift (CloudKit.swift) */
- (nullable id)__mosSwiftValueForKey:(NSString *)key;
- (void)__mosSetSwiftValue:(nullable id)value forKey:(NSString *)key;
@end
@interface CKDatabaseOperation : CKOperation
@property (nullable, strong) CKDatabase *database;
@end

@interface CKFetchRecordsOperation : CKDatabaseOperation
+ (instancetype)fetchCurrentUserRecordOperation;
- (instancetype)initWithRecordIDs:(NSArray<CKRecordID *> *)recordIDs;
@property (nullable, copy) NSArray<CKRecordID *> *recordIDs;
@property (nullable, copy) NSArray<NSString *> *desiredKeys CK_REFINED;
@property (nullable, copy) void (^perRecordProgressBlock)(CKRecordID *recordID, double progress);
@property (nullable, copy) void (^perRecordCompletionBlock)(CKRecord *_Nullable record, CKRecordID *_Nullable recordID, NSError *_Nullable error);
@property (nullable, copy) void (^fetchRecordsCompletionBlock)(NSDictionary<CKRecordID *, CKRecord *> *_Nullable recordsByRecordID, NSError *_Nullable operationError);
@end

@interface CKModifyRecordsOperation : CKDatabaseOperation
- (instancetype)initWithRecordsToSave:(nullable NSArray<CKRecord *> *)records recordIDsToDelete:(nullable NSArray<CKRecordID *> *)recordIDs CK_REFINED;
@property (nullable, copy) NSArray<CKRecord *> *recordsToSave;
@property (nullable, copy) NSArray<CKRecordID *> *recordIDsToDelete;
@property CKRecordSavePolicy savePolicy;
@property (nullable, copy) NSData *clientChangeTokenData;
@property BOOL atomic;
@property (nullable, copy) void (^perRecordProgressBlock)(CKRecord *record, double progress);
@property (nullable, copy) void (^perRecordCompletionBlock)(CKRecord *record, NSError *_Nullable error);
@property (nullable, copy) void (^perRecordSaveBlock)(CKRecordID *recordID, CKRecord *_Nullable record, NSError *_Nullable error) CK_REFINED;
@property (nullable, copy) void (^perRecordDeleteBlock)(CKRecordID *recordID, NSError *_Nullable error) CK_REFINED;
@property (nullable, copy) void (^modifyRecordsCompletionBlock)(NSArray<CKRecord *> *_Nullable savedRecords, NSArray<CKRecordID *> *_Nullable deletedRecordIDs, NSError *_Nullable operationError);
@end

@interface CKQueryOperation : CKDatabaseOperation
- (instancetype)initWithQuery:(CKQuery *)query;
- (instancetype)initWithCursor:(CKQueryCursor *)cursor;
@property (nullable, copy) CKQuery *query;
@property (nullable, copy) CKQueryCursor *cursor;
@property (nullable, copy) CKRecordZoneID *zoneID;
@property NSUInteger resultsLimit;
@property (nullable, copy) NSArray<NSString *> *desiredKeys;
@property (nullable, copy) void (^recordFetchedBlock)(CKRecord *record);
@property (nullable, copy) void (^recordMatchedBlock)(CKRecordID *recordID, CKRecord *_Nullable record, NSError *_Nullable error);
@property (nullable, copy) void (^queryCompletionBlock)(CKQueryCursor *_Nullable cursor, NSError *_Nullable operationError);
@end

@interface CKFetchRecordZonesOperation : CKDatabaseOperation
+ (instancetype)fetchAllRecordZonesOperation;
- (instancetype)initWithRecordZoneIDs:(NSArray<CKRecordZoneID *> *)zoneIDs;
@property (nullable, copy) NSArray<CKRecordZoneID *> *recordZoneIDs;
@property (nullable, copy) void (^perRecordZoneCompletionBlock)(CKRecordZoneID *zoneID, CKRecordZone *_Nullable recordZone, NSError *_Nullable error);
@property (nullable, copy) void (^fetchRecordZonesCompletionBlock)(NSDictionary<CKRecordZoneID *, CKRecordZone *> *_Nullable recordZonesByZoneID, NSError *_Nullable operationError);
@end

@interface CKModifyRecordZonesOperation : CKDatabaseOperation
- (instancetype)initWithRecordZonesToSave:(nullable NSArray<CKRecordZone *> *)recordZonesToSave
                    recordZoneIDsToDelete:(nullable NSArray<CKRecordZoneID *> *)recordZoneIDsToDelete CK_REFINED;
@property (nullable, copy) NSArray<CKRecordZone *> *recordZonesToSave;
@property (nullable, copy) NSArray<CKRecordZoneID *> *recordZoneIDsToDelete;
@property (nullable, copy) void (^perRecordZoneSaveBlock)(CKRecordZoneID *recordZoneID, CKRecordZone *_Nullable recordZone, NSError *_Nullable error) CK_REFINED;
@property (nullable, copy) void (^perRecordZoneDeleteBlock)(CKRecordZoneID *recordZoneID, NSError *_Nullable error) CK_REFINED;
@property (nullable, copy) void (^modifyRecordZonesCompletionBlock)(NSArray<CKRecordZone *> *_Nullable savedRecordZones, NSArray<CKRecordZoneID *> *_Nullable deletedRecordZoneIDs, NSError *_Nullable operationError);
@end

@interface CKFetchRecordZoneChangesConfiguration : NSObject <NSCopying>
@property (nullable, copy) CKServerChangeToken *previousServerChangeToken;
@property NSUInteger resultsLimit;
@property (nullable, copy) NSArray<NSString *> *desiredKeys;
@end

@interface CKFetchRecordZoneChangesOperation : CKDatabaseOperation
- (instancetype)initWithRecordZoneIDs:(NSArray<CKRecordZoneID *> *)recordZoneIDs
         configurationsByRecordZoneID:(nullable NSDictionary<CKRecordZoneID *, CKFetchRecordZoneChangesConfiguration *> *)configurationsByRecordZoneID CK_REFINED;
@property (nullable, copy) NSArray<CKRecordZoneID *> *recordZoneIDs;
@property (nullable, copy) NSDictionary<CKRecordZoneID *, CKFetchRecordZoneChangesConfiguration *> *configurationsByRecordZoneID;
@property BOOL fetchAllChanges;
@property (nullable, copy) void (^recordChangedBlock)(CKRecord *record);
@property (nullable, copy) void (^recordWasChangedBlock)(CKRecordID *recordID, CKRecord *_Nullable record, NSError *_Nullable error) CK_REFINED;
@property (nullable, copy) void (^recordWithIDWasDeletedBlock)(CKRecordID *recordID, NSString *recordType) CK_REFINED;
@property (nullable, copy) void (^recordZoneChangeTokensUpdatedBlock)(CKRecordZoneID *recordZoneID, CKServerChangeToken *_Nullable serverChangeToken, NSData *_Nullable clientChangeTokenData);
@property (nullable, copy) void (^recordZoneFetchCompletionBlock)(CKRecordZoneID *recordZoneID, CKServerChangeToken *_Nullable serverChangeToken, NSData *_Nullable clientChangeTokenData, BOOL moreComing, NSError *_Nullable recordZoneError);
@property (nullable, copy) void (^fetchRecordZoneChangesCompletionBlock)(NSError *_Nullable operationError);
@end

@interface CKFetchDatabaseChangesOperation : CKDatabaseOperation
- (instancetype)initWithPreviousServerChangeToken:(nullable CKServerChangeToken *)previousServerChangeToken;
@property (nullable, copy) CKServerChangeToken *previousServerChangeToken;
@property NSUInteger resultsLimit;
@property BOOL fetchAllChanges;
@property (nullable, copy) void (^recordZoneWithIDChangedBlock)(CKRecordZoneID *zoneID);
@property (nullable, copy) void (^recordZoneWithIDWasDeletedBlock)(CKRecordZoneID *zoneID);
@property (nullable, copy) void (^recordZoneWithIDWasPurgedBlock)(CKRecordZoneID *zoneID);
@property (nullable, copy) void (^changeTokenUpdatedBlock)(CKServerChangeToken *serverChangeToken);
@property (nullable, copy) void (^fetchDatabaseChangesCompletionBlock)(CKServerChangeToken *_Nullable serverChangeToken, BOOL moreComing, NSError *_Nullable operationError);
@end

@interface CKModifySubscriptionsOperation : CKDatabaseOperation
- (instancetype)initWithSubscriptionsToSave:(nullable NSArray<CKSubscription *> *)subscriptionsToSave
                    subscriptionIDsToDelete:(nullable NSArray<NSString *> *)subscriptionIDsToDelete;
@property (nullable, copy) NSArray<CKSubscription *> *subscriptionsToSave;
@property (nullable, copy) NSArray<NSString *> *subscriptionIDsToDelete;
@property (nullable, copy) void (^modifySubscriptionsCompletionBlock)(NSArray<CKSubscription *> *_Nullable savedSubscriptions, NSArray<NSString *> *_Nullable deletedSubscriptionIDs, NSError *_Nullable operationError);
@end

@interface CKFetchSubscriptionsOperation : CKDatabaseOperation
+ (instancetype)fetchAllSubscriptionsOperation;
- (instancetype)initWithSubscriptionIDs:(NSArray<NSString *> *)subscriptionIDs;
@property (nullable, copy) NSArray<NSString *> *subscriptionIDs;
@property (nullable, copy) void (^fetchSubscriptionCompletionBlock)(NSDictionary<NSString *, CKSubscription *> *_Nullable subscriptionsBySubscriptionID, NSError *_Nullable operationError);
@end

@interface CKFetchShareMetadataOperation : CKOperation
- (instancetype)initWithShareURLs:(NSArray<NSURL *> *)shareURLs;
@property (nullable, copy) NSArray<NSURL *> *shareURLs;
@property BOOL shouldFetchRootRecord;
@property (nullable, copy) void (^perShareMetadataBlock)(NSURL *shareURL, CKShareMetadata *_Nullable shareMetadata, NSError *_Nullable error);
@property (nullable, copy) void (^fetchShareMetadataCompletionBlock)(NSError *_Nullable operationError);
@end

@interface CKAcceptSharesOperation : CKOperation
- (instancetype)initWithShareMetadatas:(NSArray<CKShareMetadata *> *)shareMetadatas;
@property (nullable, copy) NSArray<CKShareMetadata *> *shareMetadatas;
@property (nullable, copy) void (^perShareCompletionBlock)(CKShareMetadata *shareMetadata, CKShare *_Nullable acceptedShare, NSError *_Nullable error);
@property (nullable, copy) void (^acceptSharesCompletionBlock)(NSError *_Nullable operationError);
@end

@interface CKFetchShareParticipantsOperation : CKOperation
- (instancetype)initWithUserIdentityLookupInfos:(NSArray<CKUserIdentityLookupInfo *> *)userIdentityLookupInfos;
@property (nullable, copy) NSArray<CKUserIdentityLookupInfo *> *userIdentityLookupInfos;
@property (nullable, copy) void (^shareParticipantFetchedBlock)(CKShareParticipant *participant);
@property (nullable, copy) void (^fetchShareParticipantsCompletionBlock)(NSError *_Nullable operationError);
@end

@interface CKDiscoverAllUserIdentitiesOperation : CKOperation
@property (nullable, copy) void (^userIdentityDiscoveredBlock)(CKUserIdentity *identity);
@property (nullable, copy) void (^discoverAllUserIdentitiesCompletionBlock)(NSError *_Nullable operationError);
@end

@interface CKDiscoverUserIdentitiesOperation : CKOperation
- (instancetype)initWithUserIdentityLookupInfos:(NSArray<CKUserIdentityLookupInfo *> *)userIdentityLookupInfos;
@property (nullable, copy) NSArray<CKUserIdentityLookupInfo *> *userIdentityLookupInfos;
@property (nullable, copy) void (^userIdentityDiscoveredBlock)(CKUserIdentity *identity, CKUserIdentityLookupInfo *lookupInfo);
@property (nullable, copy) void (^discoverUserIdentitiesCompletionBlock)(NSError *_Nullable operationError);
@end

@interface CKFetchWebAuthTokenOperation : CKDatabaseOperation
- (instancetype)initWithAPIToken:(NSString *)APIToken;
@property (nullable, copy) NSString *APIToken;
@property (nullable, copy) void (^fetchWebAuthTokenCompletionBlock)(NSString *_Nullable webToken, NSError *_Nullable operationError);
@end

#pragma clang assume_nonnull end
