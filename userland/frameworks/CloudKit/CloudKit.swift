// mOS — overlay Swift do CloudKit.framework (no SDK da Apple ele fica dentro do próprio framework). As assinaturas são
// as do overlay da Apple, para os nomes mangled ($s…8CloudKit…) que os apps importam saírem iguais; por baixo, as
// classes ObjC de CloudKit.m (sem conta do iCloud: tudo termina com CKError.notAuthenticated).
// tools/build-frameworks-objc.sh (precisa do Foundation Swift real: arquivo swift-needs).
// Fora do módulo Foundation o importador do mOS não faz a ponte nos tipos importados (NSString, NSArray...): as
// conversões aqui são explícitas (as String, as NSString), como no overlay do UIKit.

@_exported import CloudKit
import Foundation

// ---------------------------------------------------------------- valores de registro
public protocol CKRecordValueProtocol {}
extension String: CKRecordValueProtocol {}
extension Date: CKRecordValueProtocol {}
extension Data: CKRecordValueProtocol {}
extension Bool: CKRecordValueProtocol {}
extension Double: CKRecordValueProtocol {}
extension Float: CKRecordValueProtocol {}
extension Int: CKRecordValueProtocol {}
extension Int8: CKRecordValueProtocol {}
extension Int16: CKRecordValueProtocol {}
extension Int32: CKRecordValueProtocol {}
extension Int64: CKRecordValueProtocol {}
extension UInt: CKRecordValueProtocol {}
extension UInt8: CKRecordValueProtocol {}
extension UInt16: CKRecordValueProtocol {}
extension UInt32: CKRecordValueProtocol {}
extension UInt64: CKRecordValueProtocol {}
extension Array: CKRecordValueProtocol where Element: CKRecordValueProtocol {}
extension CKAsset: CKRecordValueProtocol {}
extension CKReference: CKRecordValueProtocol {}
extension NSString: CKRecordValueProtocol {}
extension NSNumber: CKRecordValueProtocol {}
extension NSDate: CKRecordValueProtocol {}
extension NSData: CKRecordValueProtocol {}
extension NSArray: CKRecordValueProtocol {}

extension CKRecordKeyValueSetting {
    public subscript(key: String) -> CKRecordValueProtocol? {
        get { return self.object(forKey: key as NSString) as? CKRecordValueProtocol }
        set { self.setObject(newValue.map { $0 as AnyObject }, forKey: key as NSString) }
    }
    public subscript<T: CKRecordValueProtocol>(key: String) -> T? {
        return self.object(forKey: key as NSString) as? T
    }
}

extension CKRecord {
    public convenience init(recordType: String, recordID: CKRecordID = CKRecordID(recordName: UUID().uuidString)) {
        self.init(__recordType: recordType as NSString, recordID: recordID)
    }
    public var recordType: String { return __recordType as String }
    public func allKeys() -> [String] { return (__allKeys() as? [String]) ?? [] }
    public subscript(key: String) -> __CKRecordObjCValue? {
        get { return self.object(forKey: key as NSString) as? __CKRecordObjCValue }
        set { self.setObject(newValue, forKey: key as NSString) }
    }
}

extension CKRecordID {
    public convenience init(recordName: String, zoneID: CKRecordZoneID = CKRecordZoneID(zoneName: CKRecordZoneDefaultName as String, ownerName: CKCurrentUserDefaultName as String)) {
        self.init(__recordName: recordName as NSString, zoneID: zoneID)
    }
}

extension CKRecordZoneID {
    public convenience init(zoneName: String = CKRecordZoneDefaultName as String, ownerName: String = CKCurrentUserDefaultName as String) {
        self.init(__zoneName: zoneName as NSString, ownerName: ownerName as NSString)
    }
}

// ---------------------------------------------------------------- erros
extension CKError {
    public var clientRecord: CKRecord? { return userInfo[CKRecordChangedErrorClientRecordKey as String] as? CKRecord }
    public var serverRecord: CKRecord? { return userInfo[CKRecordChangedErrorServerRecordKey as String] as? CKRecord }
    public var ancestorRecord: CKRecord? { return userInfo[CKRecordChangedErrorAncestorRecordKey as String] as? CKRecord }
    public var retryAfterSeconds: Double? { return (userInfo[CKErrorRetryAfterKey as String] as? NSNumber)?.doubleValue }
    public var partialErrorsByItemID: [AnyHashable: Error]? {
        return userInfo[CKPartialErrorsByItemIDKey as String] as? [AnyHashable: Error]
    }
}

private func _ckFailure(_ e: Error?) -> Error { return e ?? CKError(.internalError) }

/// caixa para guardar um closure Swift no dicionário do CKOperation (CloudKit.m)
private final class _MOSBox<T> {
    let value: T
    init(_ v: T) { value = v }
}
extension CKOperation {
    fileprivate func _mosGet<T>(_ key: String, _ type: T.Type) -> T? {
        return (__mosSwiftValue(forKey: key as NSString) as? _MOSBox<T>)?.value
    }
    fileprivate func _mosSet<T>(_ key: String, _ v: T?) {
        __mosSetSwiftValue(v.map { _MOSBox($0) }, forKey: key as NSString)
    }
}

// ---------------------------------------------------------------- notificações e assinaturas
extension CKNotification {
    public var subscriptionID: String? { return __subscriptionID.map { $0 as String } }
}

extension CKDatabaseSubscription {
    public convenience init(subscriptionID: String = UUID().uuidString) {
        self.init(__subscriptionID: subscriptionID as NSString)
    }
    public var recordType: String? {
        get { return __recordType.map { $0 as String } }
        set { __recordType = newValue.map { $0 as NSString } }
    }
}

// ---------------------------------------------------------------- operações (blocos com Result, iOS 15 / macOS 12)
extension CKFetchRecordsOperation {
    public var desiredKeys: [String]? {
        get { return __desiredKeys.flatMap { $0 as? [String] } }
        set { __desiredKeys = newValue.map { $0._bridgeToObjectiveC() } }
    }
    public var perRecordResultBlock: ((_ recordID: CKRecordID, _ recordResult: Result<CKRecord, Error>) -> Void)? {
        get { return _mosGet("perRecordResultBlock", ((CKRecordID, Result<CKRecord, Error>) -> Void).self) }
        set {
            _mosSet("perRecordResultBlock", newValue)
            guard let b = newValue else { perRecordCompletionBlock = nil; return }
            perRecordCompletionBlock = { record, recordID, error in
                guard let id = recordID ?? record?.recordID else { return }
                if let r = record, error == nil { b(id, .success(r)) } else { b(id, .failure(_ckFailure(error))) }
            }
        }
    }
    public var fetchRecordsResultBlock: ((_ operationResult: Result<Void, Error>) -> Void)? {
        get { return _mosGet("fetchRecordsResultBlock", ((Result<Void, Error>) -> Void).self) }
        set {
            _mosSet("fetchRecordsResultBlock", newValue)
            guard let b = newValue else { fetchRecordsCompletionBlock = nil; return }
            fetchRecordsCompletionBlock = { _, error in b(error.map { .failure($0) } ?? .success(())) }
        }
    }
}

extension CKModifyRecordsOperation {
    public convenience init(recordsToSave: [CKRecord]? = nil, recordIDsToDelete: [CKRecordID]? = nil) {
        self.init(__recordsToSave: recordsToSave.map { $0._bridgeToObjectiveC() }, recordIDsToDelete: recordIDsToDelete.map { $0._bridgeToObjectiveC() })
    }
    public var perRecordSaveBlock: ((_ recordID: CKRecordID, _ saveResult: Result<CKRecord, Error>) -> Void)? {
        get { return _mosGet("perRecordSaveBlock", ((CKRecordID, Result<CKRecord, Error>) -> Void).self) }
        set {
            _mosSet("perRecordSaveBlock", newValue)
            guard let b = newValue else { __perRecordSaveBlock = nil; return }
            __perRecordSaveBlock = { id, record, error in
                if let r = record, error == nil { b(id, .success(r)) } else { b(id, .failure(_ckFailure(error))) }
            }
        }
    }
    public var perRecordDeleteBlock: ((_ recordID: CKRecordID, _ deleteResult: Result<Void, Error>) -> Void)? {
        get { return _mosGet("perRecordDeleteBlock", ((CKRecordID, Result<Void, Error>) -> Void).self) }
        set {
            _mosSet("perRecordDeleteBlock", newValue)
            guard let b = newValue else { __perRecordDeleteBlock = nil; return }
            __perRecordDeleteBlock = { id, error in b(id, error.map { .failure($0) } ?? .success(())) }
        }
    }
    public var modifyRecordsResultBlock: ((_ operationResult: Result<Void, Error>) -> Void)? {
        get { return _mosGet("modifyRecordsResultBlock", ((Result<Void, Error>) -> Void).self) }
        set {
            _mosSet("modifyRecordsResultBlock", newValue)
            guard let b = newValue else { modifyRecordsCompletionBlock = nil; return }
            modifyRecordsCompletionBlock = { _, _, error in b(error.map { .failure($0) } ?? .success(())) }
        }
    }
}

extension CKModifyRecordZonesOperation {
    public convenience init(recordZonesToSave: [CKRecordZone]? = nil, recordZoneIDsToDelete: [CKRecordZoneID]? = nil) {
        self.init(__recordZonesToSave: recordZonesToSave.map { $0._bridgeToObjectiveC() }, recordZoneIDsToDelete: recordZoneIDsToDelete.map { $0._bridgeToObjectiveC() })
    }
    public var perRecordZoneSaveBlock: ((_ recordZoneID: CKRecordZoneID, _ recordZoneResult: Result<CKRecordZone, Error>) -> Void)? {
        get { return _mosGet("perRecordZoneSaveBlock", ((CKRecordZoneID, Result<CKRecordZone, Error>) -> Void).self) }
        set {
            _mosSet("perRecordZoneSaveBlock", newValue)
            guard let b = newValue else { __perRecordZoneSaveBlock = nil; return }
            __perRecordZoneSaveBlock = { id, zone, error in
                if let z = zone, error == nil { b(id, .success(z)) } else { b(id, .failure(_ckFailure(error))) }
            }
        }
    }
    public var perRecordZoneDeleteBlock: ((_ recordZoneID: CKRecordZoneID, _ deleteResult: Result<Void, Error>) -> Void)? {
        get { return _mosGet("perRecordZoneDeleteBlock", ((CKRecordZoneID, Result<Void, Error>) -> Void).self) }
        set {
            _mosSet("perRecordZoneDeleteBlock", newValue)
            guard let b = newValue else { __perRecordZoneDeleteBlock = nil; return }
            __perRecordZoneDeleteBlock = { id, error in b(id, error.map { .failure($0) } ?? .success(())) }
        }
    }
    public var modifyRecordZonesResultBlock: ((_ operationResult: Result<Void, Error>) -> Void)? {
        get { return _mosGet("modifyRecordZonesResultBlock", ((Result<Void, Error>) -> Void).self) }
        set {
            _mosSet("modifyRecordZonesResultBlock", newValue)
            guard let b = newValue else { modifyRecordZonesCompletionBlock = nil; return }
            modifyRecordZonesCompletionBlock = { _, _, error in b(error.map { .failure($0) } ?? .success(())) }
        }
    }
}

extension CKFetchDatabaseChangesOperation {
    public var fetchDatabaseChangesResultBlock: ((_ operationResult: Result<(serverChangeToken: CKServerChangeToken, moreComing: Bool), Error>) -> Void)? {
        get { return _mosGet("fetchDatabaseChangesResultBlock", ((Result<(serverChangeToken: CKServerChangeToken, moreComing: Bool), Error>) -> Void).self) }
        set {
            _mosSet("fetchDatabaseChangesResultBlock", newValue)
            guard let b = newValue else { fetchDatabaseChangesCompletionBlock = nil; return }
            fetchDatabaseChangesCompletionBlock = { token, more, error in
                if let t = token, error == nil { b(.success((serverChangeToken: t, moreComing: more))) }
                else { b(.failure(_ckFailure(error))) }
            }
        }
    }
}

extension CKFetchRecordZoneChangesConfiguration {
    public convenience init(previousServerChangeToken: CKServerChangeToken? = nil, resultsLimit: Int? = nil, desiredKeys: [String]? = nil) {
        self.init()
        self.previousServerChangeToken = previousServerChangeToken
        if let l = resultsLimit { self.resultsLimit = l }
        self.desiredKeys = desiredKeys.map { $0._bridgeToObjectiveC() }
    }
}

extension CKFetchRecordZoneChangesOperation {
    public convenience init(recordZoneIDs: [CKRecordZoneID]? = nil, configurationsByRecordZoneID: [CKRecordZoneID: CKFetchRecordZoneChangesConfiguration]? = nil) {
        self.init(__recordZoneIDs: (recordZoneIDs ?? [])._bridgeToObjectiveC(), configurationsByRecordZoneID: configurationsByRecordZoneID.map { $0._bridgeToObjectiveC() })
    }
    public var recordWasChangedBlock: ((_ recordID: CKRecordID, _ recordResult: Result<CKRecord, Error>) -> Void)? {
        get { return _mosGet("recordWasChangedBlock", ((CKRecordID, Result<CKRecord, Error>) -> Void).self) }
        set {
            _mosSet("recordWasChangedBlock", newValue)
            guard let b = newValue else { __recordWasChangedBlock = nil; return }
            __recordWasChangedBlock = { id, record, error in
                if let r = record, error == nil { b(id, .success(r)) } else { b(id, .failure(_ckFailure(error))) }
            }
        }
    }
    public var recordWithIDWasDeletedBlock: ((CKRecordID, String) -> Void)? {
        get { return _mosGet("recordWithIDWasDeletedBlock", ((CKRecordID, String) -> Void).self) }
        set {
            _mosSet("recordWithIDWasDeletedBlock", newValue)
            guard let b = newValue else { __recordWithIDWasDeletedBlock = nil; return }
            __recordWithIDWasDeletedBlock = { id, type in b(id, type as String) }
        }
    }
    public var recordZoneFetchResultBlock: ((_ recordZoneID: CKRecordZoneID, _ fetchChangesResult: Result<(serverChangeToken: CKServerChangeToken, clientChangeTokenData: Data?, moreComing: Bool), Error>) -> Void)? {
        get { return _mosGet("recordZoneFetchResultBlock", ((CKRecordZoneID, Result<(serverChangeToken: CKServerChangeToken, clientChangeTokenData: Data?, moreComing: Bool), Error>) -> Void).self) }
        set {
            _mosSet("recordZoneFetchResultBlock", newValue)
            guard let b = newValue else { recordZoneFetchCompletionBlock = nil; return }
            recordZoneFetchCompletionBlock = { id, token, data, more, error in
                if let t = token, error == nil { b(id, .success((serverChangeToken: t, clientChangeTokenData: data.map { $0 as Data }, moreComing: more))) }
                else { b(id, .failure(_ckFailure(error))) }
            }
        }
    }
    public var fetchRecordZoneChangesResultBlock: ((_ operationResult: Result<Void, Error>) -> Void)? {
        get { return _mosGet("fetchRecordZoneChangesResultBlock", ((Result<Void, Error>) -> Void).self) }
        set {
            _mosSet("fetchRecordZoneChangesResultBlock", newValue)
            guard let b = newValue else { fetchRecordZoneChangesCompletionBlock = nil; return }
            fetchRecordZoneChangesCompletionBlock = { error in b(error.map { .failure($0) } ?? .success(())) }
        }
    }
}

// ---------------------------------------------------------------- async
extension CKDatabase {
    public func modifyRecordZones(saving recordZonesToSave: [CKRecordZone], deleting recordZoneIDsToDelete: [CKRecordZoneID]) async throws
        -> (saveResults: [CKRecordZoneID: Result<CKRecordZone, Error>], deleteResults: [CKRecordZoneID: Result<Void, Error>]) {
        return try await withCheckedThrowingContinuation { cont in
            let op = CKModifyRecordZonesOperation(recordZonesToSave: recordZonesToSave, recordZoneIDsToDelete: recordZoneIDsToDelete)
            var saves: [CKRecordZoneID: Result<CKRecordZone, Error>] = [:]
            var deletes: [CKRecordZoneID: Result<Void, Error>] = [:]
            op.perRecordZoneSaveBlock = { id, r in saves[id] = r }
            op.perRecordZoneDeleteBlock = { id, r in deletes[id] = r }
            op.modifyRecordZonesResultBlock = { r in
                switch r {
                case .success: cont.resume(returning: (saveResults: saves, deleteResults: deletes))
                case .failure(let e): cont.resume(throwing: e)
                }
            }
            self.add(op)
        }
    }
}

extension CKContainer {
    /// Sem servidor não há configuração de rede a aplicar: o corpo roda com este contêiner.
    public func configuredWith<R>(configuration: CKOperationConfiguration? = nil, group: CKOperationGroup? = nil,
                                  body: (CKContainer) async throws -> R) async throws -> R {
        return try await body(self)
    }
}
