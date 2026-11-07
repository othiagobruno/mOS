// mOS — Foundation (fase 5, marco B): URLResourceValues e os membros de URL que dependem dele (resourceValues,
// bookmarks, acesso com escopo de segurança).
// Tudo no mesmo módulo do URL, como na Apple (os nomes mangled batem).
// Os valores vêm do FileManager do swift-foundation (stat/lstat). O mOS ainda não tem bookmarks nem aliases do Finder:
// um "bookmark" daqui guarda o caminho em UTF-8, e um alias resolve para o próprio arquivo.

fileprivate func _mosKey(_ s: String) -> URLResourceKey { URLResourceKey(rawValue: _mosNSString(s)) }   // rawValue é NSString aqui

public struct URLResourceValues {
    internal var _values: [URLResourceKey: Any] = [:]
    internal var _keys: Set<URLResourceKey> = []

    public init() {}

    internal init(values: [URLResourceKey: Any]) {
        _values = values
        _keys = Set(values.keys)
    }

    public var allValues: [URLResourceKey: Any] { _values }

    internal func _get<T>(_ k: String) -> T? { _values[_mosKey(k)] as? T }
    internal mutating func _set(_ k: String, _ v: Any?) {
        let key = _mosKey(k)
        _keys.insert(key)
        _values[key] = v
    }

    public var name: String? { get { _get("NSURLNameKey") } set { _set("NSURLNameKey", newValue) } }
    public var localizedName: String? { _get("NSURLLocalizedNameKey") }
    public var path: String? { _get("_NSURLPathKey") }
    public var canonicalPath: String? { _get("NSURLCanonicalPathKey") }
    public var isRegularFile: Bool? { _get("NSURLIsRegularFileKey") }
    public var isDirectory: Bool? { _get("NSURLIsDirectoryKey") }
    public var isSymbolicLink: Bool? { _get("NSURLIsSymbolicLinkKey") }
    public var isAliasFile: Bool? { _get("NSURLIsAliasFileKey") }
    public var isPackage: Bool? { _get("NSURLIsPackageKey") }
    public var isHidden: Bool? { get { _get("NSURLIsHiddenKey") } set { _set("NSURLIsHiddenKey", newValue) } }
    public var isReadable: Bool? { _get("NSURLIsReadableKey") }
    public var isWritable: Bool? { _get("NSURLIsWritableKey") }
    public var isExecutable: Bool? { _get("NSURLIsExecutableKey") }
    public var hasHiddenExtension: Bool? { get { _get("NSURLHasHiddenExtensionKey") } set { _set("NSURLHasHiddenExtensionKey", newValue) } }
    public var fileSize: Int? { _get("NSURLFileSizeKey") }
    public var creationDate: Date? { get { _get("NSURLCreationDateKey") } set { _set("NSURLCreationDateKey", newValue) } }
    public var contentModificationDate: Date? { get { _get("NSURLContentModificationDateKey") } set { _set("NSURLContentModificationDateKey", newValue) } }
    public var typeIdentifier: String? { _get("NSURLTypeIdentifierKey") }
}

extension URL {
    /// os valores pedidos (todos os que o mOS sabe calcular vêm juntos, como cache)
    public func resourceValues(forKeys keys: Set<URLResourceKey>) throws -> URLResourceValues {
        guard isFileURL else { throw CocoaError(.fileReadUnsupportedScheme) }
        let p = path
        let fm = FileManager.default
        var isDir: Bool = false
        guard fm.fileExists(atPath: p, isDirectory: &isDir) || (try? fm.destinationOfSymbolicLink(atPath: p)) != nil else {
            throw CocoaError(.fileReadNoSuchFile)
        }
        var v = URLResourceValues()
        let attrs = (try? fm.attributesOfItem(atPath: p)) ?? [:]
        let type = attrs[.type] as? FileAttributeType
        v._set("NSURLNameKey", lastPathComponent)
        v._set("NSURLLocalizedNameKey", lastPathComponent)
        v._set("_NSURLPathKey", p)
        v._set("NSURLCanonicalPathKey", resolvingSymlinksInPath().path)
        v._set("NSURLIsDirectoryKey", isDir)
        v._set("NSURLIsRegularFileKey", type == .typeRegular || (!isDir && type == nil))
        v._set("NSURLIsSymbolicLinkKey", type == .typeSymbolicLink)
        v._set("NSURLIsAliasFileKey", type == .typeSymbolicLink)
        v._set("NSURLIsPackageKey", isDir && ["app", "bundle", "framework", "plugin"].contains(pathExtension))
        v._set("NSURLIsHiddenKey", lastPathComponent.hasPrefix("."))
        v._set("NSURLIsReadableKey", fm.isReadableFile(atPath: p))
        v._set("NSURLIsWritableKey", fm.isWritableFile(atPath: p))
        v._set("NSURLIsExecutableKey", fm.isExecutableFile(atPath: p))
        v._set("NSURLHasHiddenExtensionKey", false)
        if let n = attrs[.size] as? Int { v._set("NSURLFileSizeKey", n) }
        else if let n = attrs[.size] as? UInt64 { v._set("NSURLFileSizeKey", Int(n)) }
        if let d = attrs[.creationDate] as? Date { v._set("NSURLCreationDateKey", d) }
        if let d = attrs[.modificationDate] as? Date { v._set("NSURLContentModificationDateKey", d) }
        v._set("NSURLTypeIdentifierKey", isDir ? "public.folder" : _mosUTI(pathExtension))
        return v
    }

    /// o mOS ainda não guarda atributos extras (extensão oculta, datas): aceita e não muda nada
    public mutating func setResourceValues(_ values: URLResourceValues) throws {
        guard isFileURL else { throw CocoaError(.fileWriteUnsupportedScheme) }
    }

    public mutating func removeCachedResourceValue(forKey key: URLResourceKey) {}
    public mutating func removeAllCachedResourceValues() {}

    public static func resourceValues(forKeys keys: Set<URLResourceKey>, fromBookmarkData data: Data) -> URLResourceValues? {
        guard let u = _mosURLFromBookmark(data) else { return nil }
        return try? u.resourceValues(forKeys: keys)
    }

    public init(resolvingBookmarkData data: __shared Data, options: BookmarkResolutionOptions = [], relativeTo url: __shared URL? = nil,
                bookmarkDataIsStale: inout Bool) throws {
        guard let u = URL._mosURLFromBookmark(data) else { throw CocoaError(.fileReadCorruptFile) }
        bookmarkDataIsStale = false
        self = u
    }

    public init(resolvingAliasFileAt url: __shared URL, options: BookmarkResolutionOptions = []) throws {
        self = url.resolvingSymlinksInPath()
    }

    public func bookmarkData(options: BookmarkCreationOptions = [], includingResourceValuesForKeys keys: Set<URLResourceKey>? = nil,
                             relativeTo url: URL? = nil) throws -> Data {
        guard isFileURL else { throw CocoaError(.fileReadUnsupportedScheme) }
        return Data(("mos-bookmark:" + path).utf8)
    }

    public func startAccessingSecurityScopedResource() -> Bool { true }
    public func stopAccessingSecurityScopedResource() {}

    internal static func _mosURLFromBookmark(_ data: Data) -> URL? {
        let s = String(decoding: data, as: UTF8.self)
        guard s.hasPrefix("mos-bookmark:") else { return nil }
        return URL(fileURLWithPath: String(s.dropFirst("mos-bookmark:".count)))
    }
}

/// identificador de tipo pela extensão (o mínimo; o UTType do libSystem é a fonte completa)
fileprivate func _mosUTI(_ ext: String) -> String {
    switch ext.lowercased() {
    case "txt", "text": return "public.plain-text"
    case "sql": return "public.sql"
    case "csv": return "public.comma-separated-values-text"
    case "json": return "public.json"
    case "png": return "public.png"
    case "jpg", "jpeg": return "public.jpeg"
    case "pdf": return "com.adobe.pdf"
    case "html", "htm": return "public.html"
    case "app": return "com.apple.application-bundle"
    case "": return "public.data"
    default: return "public.data"
    }
}
