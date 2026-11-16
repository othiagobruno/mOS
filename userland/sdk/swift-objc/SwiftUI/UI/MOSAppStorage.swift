// mOS — AppStorage e defaultAppStorage do SwiftUI.framework, com as assinaturas da Apple (o upstream não tem).
// O valor fica no UserDefaults (o NSUserDefaults do libSystem). A caixa de cada chave é observável (Observation):
// a view que lê `wrappedValue` é redesenhada quando a chave muda, por esta ou por outra AppStorage do processo.
public import Foundation
public import OpenSwiftUICore
import Observation

/// Conversões entre o valor Swift e o que o UserDefaults guarda.
struct _MOSDefaultsCodec<Value> {
    let read: (Any) -> Value?
    let write: (Value) -> Any?
}

final class _MOSAppStorageHolder<Value> {
    var box: _MOSAppStorageBox<Value>?
}

/// Caixa observável de uma chave: lê e grava no UserDefaults e avisa as outras caixas da mesma chave.
final class _MOSAppStorageBox<Value>: Observation.Observable {
    private let registrar = ObservationRegistrar()
    let key: String
    let store: UserDefaults
    let defaultValue: Value
    let codec: _MOSDefaultsCodec<Value>
    private var cached: Value
    private var token: NSObjectProtocol?

    static var changed: Notification.Name { Notification.Name("_MOSAppStorageDidChange") }

    init(key: String, store: UserDefaults, defaultValue: Value, codec: _MOSDefaultsCodec<Value>) {
        self.key = key
        self.store = store
        self.defaultValue = defaultValue
        self.codec = codec
        cached = store.object(forKey: key).flatMap(codec.read) ?? defaultValue
        token = NotificationCenter.default.addObserver(forName: Self.changed, object: nil, queue: nil) { [weak self] n in
            guard let self, (n.object as? NSString).map({ $0 as String }) == key else { return }
            self.reload()
        }
    }

    deinit { if let token { NotificationCenter.default.removeObserver(token) } }

    var value: Value {
        get {
            registrar.access(self, keyPath: \.value)
            return cached
        }
        set {
            registrar.withMutation(of: self, keyPath: \.value) { cached = newValue }
            if let o = codec.write(newValue) { store.setObject(o, forKey: key) } else { store.removeObject(forKey: key) }
            NotificationCenter.default.post(Notification(name: Self.changed, object: key as NSString))
        }
    }

    func reload() {
        let v = store.object(forKey: key).flatMap(codec.read) ?? defaultValue
        registrar.withMutation(of: self, keyPath: \.value) { cached = v }
    }
}

private struct MOSDefaultAppStorageKey: EnvironmentKey {
    static var defaultValue: UserDefaults? { nil }
}

extension EnvironmentValues {
    var _mosDefaultAppStorage: UserDefaults? {
        get { self[MOSDefaultAppStorageKey.self] }
        set { self[MOSDefaultAppStorageKey.self] = newValue }
    }
}

@available(macOS 11.0, *)
extension View {
    /// O UserDefaults que as AppStorage desta hierarquia usam quando não recebem `store:`.
    public func defaultAppStorage(_ store: UserDefaults) -> some View {
        environment(\._mosDefaultAppStorage, store)
    }
}

/// Um property wrapper que lê e grava um valor no UserDefaults e redesenha a view quando ele muda.
@available(macOS 11.0, *)
@propertyWrapper
public struct AppStorage<Value>: DynamicProperty {
    /// guarda a caixa da chave entre as atualizações da view (referência: resolvê-la não escreve estado)
    @State private var holder = _MOSAppStorageHolder<Value>()
    @Environment(\._mosDefaultAppStorage) private var environmentStore
    let key: String
    let explicitStore: UserDefaults?
    let defaultValue: Value
    let codec: _MOSDefaultsCodec<Value>

    init(key: String, store: UserDefaults?, defaultValue: Value, codec: _MOSDefaultsCodec<Value>) {
        self.key = key
        explicitStore = store
        self.defaultValue = defaultValue
        self.codec = codec
    }

    /// a caixa da chave (criada na primeira leitura, com o store explícito, o do ambiente ou o padrão)
    private var resolved: _MOSAppStorageBox<Value> {
        if let b = holder.box, b.key == key { return b }
        let b = _MOSAppStorageBox(key: key, store: explicitStore ?? environmentStore ?? UserDefaults.standard,
                                  defaultValue: defaultValue, codec: codec)
        holder.box = b
        return b
    }

    public var wrappedValue: Value {
        get { resolved.value }
        nonmutating set { resolved.value = newValue }
    }

    public var projectedValue: Binding<Value> {
        let b = resolved
        return Binding(get: { b.value }, set: { b.value = $0 })
    }
}

func _mosCodec<V>(_ read: @escaping (Any) -> V?, _ write: @escaping (V) -> Any?) -> _MOSDefaultsCodec<V> { .init(read: read, write: write) }
func _mosNumber(_ o: Any) -> NSNumber? { o as? NSNumber }

@available(macOS 11.0, *)
extension AppStorage {
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value == Bool {
        self.init(key: key, store: store, defaultValue: wrappedValue, codec: _mosCodec({ _mosNumber($0)?.boolValue }, { NSNumber(value: $0) }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value == Int {
        self.init(key: key, store: store, defaultValue: wrappedValue, codec: _mosCodec({ _mosNumber($0)?.intValue }, { NSNumber(value: $0) }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value == Double {
        self.init(key: key, store: store, defaultValue: wrappedValue, codec: _mosCodec({ _mosNumber($0)?.doubleValue }, { NSNumber(value: $0) }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value == String {
        self.init(key: key, store: store, defaultValue: wrappedValue,
                  codec: _mosCodec({ ($0 as? NSString).map { $0 as String } }, { $0 as NSString }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value == URL {
        self.init(key: key, store: store, defaultValue: wrappedValue,
                  codec: _mosCodec({ ($0 as? NSString).flatMap { URL(string: $0 as String) } }, { $0.absoluteString as NSString }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value == Data {
        self.init(key: key, store: store, defaultValue: wrappedValue, codec: _mosCodec({ ($0 as? NSData).map { $0 as Data } }, { $0 as NSData }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value: RawRepresentable, Value.RawValue == Int {
        self.init(key: key, store: store, defaultValue: wrappedValue,
                  codec: _mosCodec({ _mosNumber($0).flatMap { Value(rawValue: $0.intValue) } }, { NSNumber(value: $0.rawValue) }))
    }
    public init(wrappedValue: Value, _ key: String, store: UserDefaults? = nil) where Value: RawRepresentable, Value.RawValue == String {
        self.init(key: key, store: store, defaultValue: wrappedValue,
                  codec: _mosCodec({ ($0 as? NSString).flatMap { Value(rawValue: $0 as String) } }, { $0.rawValue as NSString }))
    }
}

@available(macOS 11.0, *)
extension AppStorage where Value: ExpressibleByNilLiteral {
    public init(_ key: String, store: UserDefaults? = nil) where Value == Bool? {
        self.init(key: key, store: store, defaultValue: nil, codec: _mosCodec({ _mosNumber($0)?.boolValue }, { $0.map { NSNumber(value: $0) } }))
    }
    public init(_ key: String, store: UserDefaults? = nil) where Value == Int? {
        self.init(key: key, store: store, defaultValue: nil, codec: _mosCodec({ _mosNumber($0)?.intValue }, { $0.map { NSNumber(value: $0) } }))
    }
    public init(_ key: String, store: UserDefaults? = nil) where Value == Double? {
        self.init(key: key, store: store, defaultValue: nil, codec: _mosCodec({ _mosNumber($0)?.doubleValue }, { $0.map { NSNumber(value: $0) } }))
    }
    public init(_ key: String, store: UserDefaults? = nil) where Value == String? {
        self.init(key: key, store: store, defaultValue: nil,
                  codec: _mosCodec({ ($0 as? NSString).map { $0 as String } }, { $0.map { $0 as NSString } }))
    }
    public init(_ key: String, store: UserDefaults? = nil) where Value == URL? {
        self.init(key: key, store: store, defaultValue: nil,
                  codec: _mosCodec({ ($0 as? NSString).flatMap { URL(string: $0 as String) } }, { $0.map { $0.absoluteString as NSString } }))
    }
    public init(_ key: String, store: UserDefaults? = nil) where Value == Data? {
        self.init(key: key, store: store, defaultValue: nil, codec: _mosCodec({ ($0 as? NSData).map { $0 as Data } }, { $0.map { $0 as NSData } }))
    }
}

@available(macOS 11.0, *)
extension AppStorage {
    public init<R>(_ key: String, store: UserDefaults? = nil) where Value == R?, R: RawRepresentable, R.RawValue == String {
        self.init(key: key, store: store, defaultValue: nil,
                  codec: _mosCodec({ ($0 as? NSString).flatMap { R(rawValue: $0 as String) } }, { $0.map { $0.rawValue as NSString } }))
    }
    public init<R>(_ key: String, store: UserDefaults? = nil) where Value == R?, R: RawRepresentable, R.RawValue == Int {
        self.init(key: key, store: store, defaultValue: nil,
                  codec: _mosCodec({ _mosNumber($0).flatMap { R(rawValue: $0.intValue) } }, { $0.map { NSNumber(value: $0.rawValue) } }))
    }
}
