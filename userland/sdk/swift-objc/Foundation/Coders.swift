// mOS — overlay Foundation (marco C): as extensões Swift da Apple sobre NSCoder, NSFileManager e NSUndoManager, e o
// IntegerFormatStyle (o do FoundationInternationalization, que depende de ICU; aqui só o agrupamento de milhares do
// locale). Nomes e assinaturas da Apple.
import Darwin

// MARK: - NSCoder

extension NSCoder {
    public func decodeObject<DecodedObjectType>(of cls: DecodedObjectType.Type, forKey key: String) -> DecodedObjectType?
        where DecodedObjectType: NSCoding, DecodedObjectType: NSObject {
        decodeObject(of: cls as AnyClass, forKey: _mosNSString(key)) as? DecodedObjectType
    }

    public func decodeTopLevelObject(of classes: [AnyClass]?, forKey key: String) throws -> Any? {
        decodeObject(forKey: _mosNSString(key))
    }

    public func decodeDictionary<Key, Value>(withKeyClass keyCls: Key.Type, objectClass objectCls: Value.Type,
                                             forKey key: String) -> [Key: Value]?
        where Key: NSObject, Key: NSCopying, Key: NSSecureCoding, Value: NSObject, Value: NSSecureCoding {
        guard let d = decodeObject(forKey: _mosNSString(key)) as? NSDictionary else { return nil }
        var out: [Key: Value] = [:]
        for k in d.allKeys {
            guard let kk = k as? Key, let v = d.object(forKey: k) as? Value else { return nil }
            out[kk] = v
        }
        return out
    }
}

// MARK: - NSFileManager

extension NSFileManager {
    public func enumerator(at url: URL, includingPropertiesForKeys keys: [URLResourceKey]?,
                           options mask: NSDirectoryEnumerationOptions = [],
                           errorHandler handler: ((URL, Error) -> Bool)? = nil) -> NSDirectoryEnumerator? {
        __enumerator(at: url._bridgeToObjectiveC(), includingPropertiesForKeys: keys.map { $0._mosKeys() },
                     options: mask, errorHandler: nil)
    }

    /// troca o item original pelo novo (rename); o backup, se pedido, é feito por cópia antes
    public func replaceItemAt(_ originalItemURL: URL, withItemAt newItemURL: URL, backupItemName: String? = nil,
                              options: NSFileManagerItemReplacementOptions = []) throws -> URL? {
        if let b = backupItemName {
            let backup = originalItemURL.deletingLastPathComponent().appendingPathComponent(b).path
            _ = Darwin.unlink(backup)
            if Darwin.link(originalItemURL.path, backup) != 0 && errno != ENOENT {
                throw CocoaError(.fileWriteUnknown, userInfo: ["NSFilePath": backup])
            }
        }
        // rename substitui o destino no POSIX; se o sistema recusar (o harness no Windows), apaga e tenta de novo
        if Darwin.rename(newItemURL.path, originalItemURL.path) != 0 && (Darwin.unlink(originalItemURL.path) != 0 || Darwin.rename(newItemURL.path, originalItemURL.path) != 0) {
            throw CocoaError(.fileWriteUnknown, userInfo: ["NSFilePath": originalItemURL.path])
        }
        if let b = backupItemName, !options.contains(.withoutDeletingBackupItem) {
            _ = Darwin.unlink(originalItemURL.deletingLastPathComponent().appendingPathComponent(b).path)
        }
        return originalItemURL
    }
}

extension Array where Element == URLResourceKey {
    internal func _mosKeys() -> NSArray { (map { $0.rawValue } as [NSString])._bridgeToObjectiveC() }
}

// MARK: - NSUndoManager

/// alvo ObjC do -registerUndoWithTarget:selector:object: do libSystem; roda o bloco Swift
internal final class _MOSUndoAction: NSObject {
    let run: (AnyObject) -> Void
    init(_ run: @escaping (AnyObject) -> Void) { self.run = run; super.init() }
    @objc func perform(_ target: AnyObject) { run(target) }
}

extension NSUndoManager {
    public func registerUndo<TargetType: AnyObject>(withTarget target: TargetType, handler: @escaping (TargetType) -> Void) {
        let action = _MOSUndoAction { handler($0 as! TargetType) }
        // a ação fica viva enquanto o alvo vive (como o bloco guardado pela Apple)
        objc_setAssociatedObject(target, Unmanaged.passUnretained(action).toOpaque(), action, .OBJC_ASSOCIATION_RETAIN)
        registerUndo(withTarget: action, selector: #selector(_MOSUndoAction.perform(_:)), object: target)
    }
}

// MARK: - IntegerFormatStyle (sem ICU: agrupamento de milhares do locale)

public struct IntegerFormatStyle<Value: BinaryInteger>: FormatStyle, Codable, Hashable, Sendable {
    public var locale: Locale

    public init(locale: Locale = .autoupdatingCurrent) { self.locale = locale }

    public func format(_ value: Value) -> String {
        let neg = value < 0
        var digits = String(value.magnitude)
        let sep: Character = locale.identifier.hasPrefix("en") ? "," : "."
        var out = ""
        while digits.count > 3 { out = String(sep) + digits.suffix(3) + out; digits.removeLast(3) }
        return (neg ? "-" : "") + digits + out
    }

    public func locale(_ locale: Locale) -> IntegerFormatStyle { IntegerFormatStyle(locale: locale) }
}

extension BinaryInteger {
    public func formatted<S: FormatStyle>(_ format: S) -> S.FormatOutput where Self == S.FormatInput {
        format.format(self)
    }

    public func formatted() -> String { IntegerFormatStyle<Self>(locale: .autoupdatingCurrent).format(self) }
}
