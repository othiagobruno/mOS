// mOS SDK — Bundle e UserDefaults do Foundation clássico.
//
// Bundle: pasta com Contents/Info.plist (app do macOS) ou pasta simples; o principal é o .app que contém o executável
// (ou a pasta do executável). Info.plist em XML ou binário, lido com o PropertyListDecoder do FoundationEssentials.
// UserDefaults: domínios em memória (registro, app, argumentos `-Chave valor`). O domínio do app começa com o que
// houver em ~/Library/Preferences/<id>.plist; as mudanças ainda não são gravadas de volta.

@_silgen_name("_NSGetExecutablePath") private func _mos_NSGetExecutablePath(_ buf: UnsafeMutablePointer<CChar>, _ size: UnsafeMutablePointer<UInt32>) -> Int32

/// Valor de property list sem tipo fixo (o Info.plist mistura strings, números, listas e dicionários).
enum _MOSPlistValue: Decodable {
    case string(String), int(Int), double(Double), bool(Bool), date(Date), data(Data), array([_MOSPlistValue]), dict([String: _MOSPlistValue])
    init(from decoder: any Decoder) throws {
        if var a = try? decoder.unkeyedContainer() {
            var r: [_MOSPlistValue] = []
            while !a.isAtEnd { r.append(try a.decode(_MOSPlistValue.self)) }
            self = .array(r); return
        }
        if let k = try? decoder.container(keyedBy: _MOSAnyKey.self) {
            var r: [String: _MOSPlistValue] = [:]
            for key in k.allKeys { r[key.stringValue] = try k.decode(_MOSPlistValue.self, forKey: key) }
            self = .dict(r); return
        }
        let c = try decoder.singleValueContainer()
        if let b = try? c.decode(Bool.self) { self = .bool(b) }
        else if let i = try? c.decode(Int.self) { self = .int(i) }
        else if let d = try? c.decode(Double.self) { self = .double(d) }
        else if let s = try? c.decode(String.self) { self = .string(s) }
        else if let d = try? c.decode(Date.self) { self = .date(d) }
        else { self = .data(try c.decode(Data.self)) }
    }
    var any: Any {
        switch self {
        case .string(let s): s
        case .int(let i): i
        case .double(let d): d
        case .bool(let b): b
        case .date(let d): d
        case .data(let d): d
        case .array(let a): a.map(\.any)
        case .dict(let d): d.mapValues(\.any)
        }
    }
}
struct _MOSAnyKey: CodingKey {
    var stringValue: String
    var intValue: Int?
    init(stringValue: String) { self.stringValue = stringValue }
    init?(intValue: Int) { self.stringValue = "\(intValue)"; self.intValue = intValue }
}

func _mosReadPlist(_ path: String) -> [String: Any]? {
    guard let data = FileManager.default.contents(atPath: path),
          case let .dict(d)? = try? PropertyListDecoder().decode(_MOSPlistValue.self, from: data) else { return nil }
    return d.mapValues(\.any)
}

// MARK: - Bundle

open class Bundle: NSObject, @unchecked Sendable {
    public let bundleURL: URL
    private let _contents: URL?          // .../X.app/Contents (bundle do macOS) ou nil (pasta simples)
    private lazy var _info: [String: Any] = {
        let p = (_contents ?? bundleURL).appendingPathComponent("Info.plist").path
        return _mosReadPlist(p) ?? [:]
    }()

    public init?(path: String) {
        var isDir = false
        guard FileManager.default.fileExists(atPath: path, isDirectory: &isDir), isDir else { return nil }
        bundleURL = URL(fileURLWithPath: path, isDirectory: true)
        let c = bundleURL.appendingPathComponent("Contents")
        _contents = FileManager.default.fileExists(atPath: c.appendingPathComponent("Info.plist").path) ? c : nil
    }
    public convenience init?(url: URL) { self.init(path: url.path) }
    public convenience init(for aClass: AnyClass) { self.init(path: Bundle.main.bundlePath)! }
    public convenience init?(identifier: String) {
        guard Bundle.main.bundleIdentifier == identifier else { return nil }
        self.init(path: Bundle.main.bundlePath)
    }

    open class var main: Bundle { _main }
    private static let _main: Bundle = {
        var size: UInt32 = 1024
        var buf = [CChar](repeating: 0, count: Int(size))
        let exe = _mos_NSGetExecutablePath(&buf, &size) == 0 ? String(cString: buf) : (ProcessInfo.processInfo.arguments.first ?? "/")
        // .../X.app/Contents/MacOS/x -> X.app; senão a pasta do executável
        var dir = URL(fileURLWithPath: exe).deletingLastPathComponent()
        if dir.lastPathComponent == "MacOS", dir.deletingLastPathComponent().lastPathComponent == "Contents" {
            dir = dir.deletingLastPathComponent().deletingLastPathComponent()
        }
        return Bundle(path: dir.path) ?? Bundle(path: "/")!
    }()
    open class var allBundles: [Bundle] { [main] }
    open class var allFrameworks: [Bundle] { [] }

    open var bundlePath: String { bundleURL.path }
    open var resourceURL: URL? { _contents.map { $0.appendingPathComponent("Resources", isDirectory: true) } ?? bundleURL }
    open var resourcePath: String? { resourceURL?.path }
    open var executableURL: URL? {
        guard let name = _info["CFBundleExecutable"] as? String else { return nil }
        return (_contents ?? bundleURL).appendingPathComponent(_contents == nil ? name : "MacOS/\(name)")
    }
    open var executablePath: String? { executableURL?.path }
    open var privateFrameworksURL: URL? { _contents?.appendingPathComponent("Frameworks") }
    open var builtInPlugInsURL: URL? { _contents?.appendingPathComponent("PlugIns") }
    open var bundleIdentifier: String? { _info["CFBundleIdentifier"] as? String }
    open var infoDictionary: [String: Any]? { _info }
    open var localizedInfoDictionary: [String: Any]? { _info }
    open func object(forInfoDictionaryKey key: String) -> Any? { _info[key] }
    open var developmentLocalization: String? { _info["CFBundleDevelopmentRegion"] as? String ?? "en" }
    open var localizations: [String] {
        guard let r = resourcePath, let items = try? FileManager.default.contentsOfDirectory(atPath: r) else { return [] }
        return items.filter { $0.hasSuffix(".lproj") }.map { String($0.dropLast(6)) }
    }
    open var preferredLocalizations: [String] {
        let l = localizations
        if let dev = developmentLocalization, l.contains(dev) || l.isEmpty { return [dev] }
        return l.first.map { [$0] } ?? ["en"]
    }
    open var isLoaded: Bool { true }
    @discardableResult open func load() -> Bool { true }

    open func url(forResource name: String?, withExtension ext: String?, subdirectory subpath: String? = nil) -> URL? {
        guard let base = resourceURL else { return nil }
        let file = (name ?? "") + (ext.map { $0.isEmpty ? "" : ".\($0)" } ?? "")
        var candidates: [URL] = []
        var dir = base
        if let subpath { dir = dir.appendingPathComponent(subpath) }
        candidates.append(dir.appendingPathComponent(file))
        for l in preferredLocalizations + ["Base"] { candidates.append(dir.appendingPathComponent("\(l).lproj").appendingPathComponent(file)) }
        if name == nil, let ext, let items = try? FileManager.default.contentsOfDirectory(atPath: dir.path),
           let f = items.first(where: { $0.hasSuffix(".\(ext)") }) {
            return dir.appendingPathComponent(f)
        }
        return candidates.first { FileManager.default.fileExists(atPath: $0.path) }
    }
    open func urls(forResourcesWithExtension ext: String?, subdirectory subpath: String?) -> [URL]? {
        guard var dir = resourceURL else { return nil }
        if let subpath { dir = dir.appendingPathComponent(subpath) }
        guard let items = try? FileManager.default.contentsOfDirectory(atPath: dir.path) else { return [] }
        return items.filter { ext == nil || $0.hasSuffix(".\(ext!)") }.map { dir.appendingPathComponent($0) }
    }
    open func path(forResource name: String?, ofType ext: String?) -> String? { url(forResource: name, withExtension: ext)?.path }
    open func path(forResource name: String?, ofType ext: String?, inDirectory subpath: String?) -> String? {
        url(forResource: name, withExtension: ext, subdirectory: subpath)?.path
    }

    private var _tables: [String: [String: String]] = [:]
    private let _tablesLock = NSLock()
    /// Strings de `<tabela>.strings` (em <idioma>.lproj); sem tradução devolve `value` (ou a chave).
    open func localizedString(forKey key: String, value: String?, table tableName: String?) -> String {
        let name = tableName ?? "Localizable"
        _tablesLock.lock()
        if _tables[name] == nil {
            var t: [String: String] = [:]
            if let u = url(forResource: name, withExtension: "strings"), let d = _mosReadPlist(u.path) {
                for (k, v) in d { if let s = v as? String { t[k] = s } }
            }
            _tables[name] = t
        }
        let s = _tables[name]?[key]
        _tablesLock.unlock()
        if let s { return s }
        if let value, !value.isEmpty { return value }
        return key
    }

    open override var description: String { "\(bundlePath) (\(isLoaded ? "loaded" : "not yet loaded"))" }
    open override func isEqual(_ object: Any?) -> Bool { (object as? Bundle)?.bundleURL == bundleURL }
    open override var hash: Int { bundleURL.hashValue }
}

public func NSLocalizedString(_ key: String, tableName: String? = nil, bundle: Bundle = .main, value: String = "", comment: String) -> String {
    bundle.localizedString(forKey: key, value: value, table: tableName)
}

// MARK: - UserDefaults

open class UserDefaults: NSObject, @unchecked Sendable {
    public static let argumentDomain = "NSArgumentDomain"
    public static let globalDomain = "NSGlobalDomain"
    public static let registrationDomain = "NSRegistrationDomain"
    public static let didChangeNotification = "NSUserDefaultsDidChangeNotification"

    private let suite: String?
    private let lock = NSRecursiveLock()
    private var app: [String: Any] = [:]
    private var registered: [String: Any] = [:]
    private static let arguments: [String: Any] = {
        // -Chave valor (o mesmo domínio de argumentos do macOS)
        var r: [String: Any] = [:]
        let a = ProcessInfo.processInfo.arguments
        var i = 1
        while i + 1 < a.count {
            if a[i].hasPrefix("-"), a[i].count > 1, !a[i + 1].hasPrefix("-") {
                let v = a[i + 1]
                let value: Any
                if let n = Int(v) { value = n }
                else if let d = Double(v) { value = d }
                else if v == "YES" || v == "true" { value = true }
                else if v == "NO" || v == "false" { value = false }
                else { value = v }
                r[String(a[i].dropFirst())] = value
                i += 2
            } else { i += 1 }
        }
        return r
    }()

    open class var standard: UserDefaults { _standard }
    private static let _standard = UserDefaults(suiteName: nil)!
    open class func resetStandardUserDefaults() {}

    public init?(suiteName: String?) {
        if suiteName == UserDefaults.globalDomain { return nil }
        suite = suiteName
        super.init()
        if let p = plistPath, let d = _mosReadPlist(p) { app = d }
    }
    public override convenience init() { self.init(suiteName: nil)! }

    private var plistPath: String? {
        guard let id = suite ?? Bundle.main.bundleIdentifier else { return nil }
        return FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Preferences/\(id).plist").path
    }

    open func object(forKey key: String) -> Any? {
        lock.lock(); defer { lock.unlock() }
        return UserDefaults.arguments[key] ?? app[key] ?? registered[key]
    }
    open func set(_ value: Any?, forKey key: String) {
        lock.lock(); app[key] = value; lock.unlock()
    }
    open func set(_ value: Bool, forKey key: String) { set(value as Any?, forKey: key) }
    open func set(_ value: Int, forKey key: String) { set(value as Any?, forKey: key) }
    open func set(_ value: Double, forKey key: String) { set(value as Any?, forKey: key) }
    open func set(_ value: Float, forKey key: String) { set(Double(value) as Any?, forKey: key) }
    open func set(_ url: URL?, forKey key: String) { set(url?.absoluteString as Any?, forKey: key) }
    open func removeObject(forKey key: String) { lock.lock(); app[key] = nil; lock.unlock() }
    open func register(defaults: [String: Any]) { lock.lock(); registered.merge(defaults) { _, n in n }; lock.unlock() }

    open func string(forKey key: String) -> String? {
        switch object(forKey: key) {
        case let s as String: s
        case let n as Int: String(n)
        case let d as Double: String(d)
        default: nil
        }
    }
    open func array(forKey key: String) -> [Any]? { object(forKey: key) as? [Any] }
    open func dictionary(forKey key: String) -> [String: Any]? { object(forKey: key) as? [String: Any] }
    open func data(forKey key: String) -> Data? { object(forKey: key) as? Data }
    open func stringArray(forKey key: String) -> [String]? { object(forKey: key) as? [String] }
    open func url(forKey key: String) -> URL? { string(forKey: key).flatMap { URL(string: $0) } }
    open func integer(forKey key: String) -> Int {
        switch object(forKey: key) {
        case let n as Int: n
        case let d as Double: Int(d)
        case let b as Bool: b ? 1 : 0
        case let s as String: Int(s) ?? 0
        default: 0
        }
    }
    open func double(forKey key: String) -> Double {
        switch object(forKey: key) {
        case let d as Double: d
        case let n as Int: Double(n)
        case let b as Bool: b ? 1 : 0
        case let s as String: Double(s) ?? 0
        default: 0
        }
    }
    open func float(forKey key: String) -> Float { Float(double(forKey: key)) }
    open func bool(forKey key: String) -> Bool {
        switch object(forKey: key) {
        case let b as Bool: b
        case let n as Int: n != 0
        case let d as Double: d != 0
        case let s as String: ["YES", "yes", "true", "TRUE", "1"].contains(s)
        default: false
        }
    }
    open func dictionaryRepresentation() -> [String: Any] {
        lock.lock(); defer { lock.unlock() }
        return registered.merging(app) { _, n in n }.merging(UserDefaults.arguments) { _, n in n }
    }
    open func persistentDomain(forName domainName: String) -> [String: Any]? { domainName == (suite ?? Bundle.main.bundleIdentifier) ? app : nil }
    open func volatileDomain(forName domainName: String) -> [String: Any] { domainName == UserDefaults.argumentDomain ? UserDefaults.arguments : [:] }
    @discardableResult open func synchronize() -> Bool { true }
}
