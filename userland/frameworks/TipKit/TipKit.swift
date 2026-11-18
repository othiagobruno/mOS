// mOS — TipKit.framework (Swift, macOS 14+): dicas de uso que o app mostra uma vez e depois some.
// Reimplementação a partir da documentação pública. O estado de cada dica (vezes mostrada, invalidada e por quê) fica
// num JSON em ~/Library/Application Support/<bundle id>/TipKit/tips.json (ou no local de Tips.configure), e
// sobrevive a reinícios do app, como o datastore da Apple. Nenhuma dica aparece antes de Tips.configure().

public import Foundation
public import OpenSwiftUI   // o SwiftUI do mOS; o ABI name do módulo é "SwiftUI" (sem -module-alias, que trocaria o nome mangled)
import Darwin

// MARK: - Protocolos

/// Uma opção de exibição de uma dica (Tips.MaxDisplayCount, Tips.IgnoresDisplayFrequency).
public protocol TipOption: Sendable {}

/// Uma dica. O app declara o título e, se quiser, mensagem, imagem, ações, regras e opções.
public protocol Tip: Identifiable, Sendable {
    var id: String { get }
    var title: Text { get }
    var message: Text? { get }
    var image: Image? { get }
    var actions: [Tips.Action] { get }
    var rules: [Tips.Rule] { get }
    var options: [any TipOption] { get }
}

extension Tip {
    public var id: String { String(describing: Self.self) }
    public var message: Text? { nil }
    public var image: Image? { nil }
    public var actions: [Tips.Action] { [] }
    public var rules: [Tips.Rule] { [] }
    public var options: [any TipOption] { [] }

    /// A dica não aparece mais (gravado no datastore).
    public func invalidate(reason: Tips.InvalidationReason) {
        _MOSTipStore.shared.invalidate(id, reason)
    }

    public var status: Tips.Status { _MOSTipStore.shared.status(of: self) }
    public var shouldDisplay: Bool { status == .available }
}

/// Uma dica qualquer, com o tipo apagado.
public struct AnyTip: Tip {
    let base: any Tip
    public init(_ tip: any Tip) { base = (tip as? AnyTip)?.base ?? tip }
    public var id: String { base.id }
    public var title: Text { base.title }
    public var message: Text? { base.message }
    public var image: Image? { base.image }
    public var actions: [Tips.Action] { base.actions }
    public var rules: [Tips.Rule] { base.rules }
    public var options: [any TipOption] { base.options }
}

// MARK: - Tips

public enum Tips {
    public enum InvalidationReason: String, Codable, Sendable, Hashable {
        /// o usuário fez o que a dica ensinava
        case actionPerformed
        /// o usuário fechou a dica
        case tipClosed
        /// a dica passou de Tips.MaxDisplayCount
        case displayCountExceeded
    }

    public enum Status: Sendable, Hashable {
        case pending
        case available
        case invalidated(InvalidationReason)
    }

    /// Um botão da dica.
    public struct Action: Identifiable, Sendable {
        public let id: String
        public let index: Int?
        public let title: Text
        let handler: @Sendable () -> Void

        public init(id: String? = nil, title: String, perform handler: @escaping @Sendable () -> Void = {}) {
            self.id = id ?? title
            index = nil
            self.title = Text(title)
            self.handler = handler
        }

        public init(id: String? = nil, perform handler: @escaping @Sendable () -> Void = {}, _ label: () -> Text) {
            let t = label()
            self.id = id ?? UUID().uuidString
            index = nil
            title = t
            self.handler = handler
        }

        public func callAsFunction() { handler() }
    }

    /// Uma condição para a dica aparecer; todas precisam ser verdadeiras.
    public struct Rule: Sendable {
        let check: @Sendable () -> Bool
        public init(_ check: @escaping @Sendable () -> Bool) { self.check = check }
        var isSatisfied: Bool { check() }
    }

    /// Mostra a dica no máximo N vezes; depois ela é invalidada com .displayCountExceeded.
    public struct MaxDisplayCount: TipOption {
        let count: Int
        public init(_ count: Int) { self.count = count }
    }

    /// A dica aparece mesmo que outra tenha aparecido há pouco (ignora displayFrequency).
    public struct IgnoresDisplayFrequency: TipOption {
        let ignores: Bool
        public init(_ ignores: Bool) { self.ignores = ignores }
    }

    public struct ConfigurationOption: Sendable {
        enum Kind: Sendable {
            case location(DatastoreLocation)
            case frequency(DisplayFrequency)
        }
        let kind: Kind

        public struct DatastoreLocation: Sendable {
            let url: URL?
            public static var applicationDefault: DatastoreLocation { DatastoreLocation(url: nil) }
            public static func url(_ url: URL) -> DatastoreLocation { DatastoreLocation(url: url) }
            public static func groupContainer(identifier: String) -> DatastoreLocation {
                DatastoreLocation(url: URL.homeDirectory.appending(path: "Library/Group Containers/\(identifier)/TipKit"))
            }
        }

        public struct DisplayFrequency: Sendable {
            let interval: TimeInterval
            public static var immediate: DisplayFrequency { DisplayFrequency(interval: 0) }
            public static var hourly: DisplayFrequency { DisplayFrequency(interval: 3600) }
            public static var daily: DisplayFrequency { DisplayFrequency(interval: 86400) }
            public static var weekly: DisplayFrequency { DisplayFrequency(interval: 7 * 86400) }
            public static var monthly: DisplayFrequency { DisplayFrequency(interval: 30 * 86400) }
            public init(_ interval: TimeInterval) { self.interval = interval }
            init(interval: TimeInterval) { self.interval = interval }
        }

        public static func datastoreLocation(_ location: DatastoreLocation) -> ConfigurationOption {
            ConfigurationOption(kind: .location(location))
        }
        public static func displayFrequency(_ frequency: DisplayFrequency) -> ConfigurationOption {
            ConfigurationOption(kind: .frequency(frequency))
        }
    }

    /// Liga o TipKit: abre o datastore e passa a mostrar as dicas.
    public static func configure(_ configuration: [ConfigurationOption] = []) throws {
        try _MOSTipStore.shared.configure(configuration)
    }

    /// Apaga o estado de todas as dicas.
    public static func resetDatastore() throws { try _MOSTipStore.shared.reset() }

    public static func showAllTipsForTesting() { _MOSTipStore.shared.override = true }
    public static func hideAllTipsForTesting() { _MOSTipStore.shared.override = false }
    public static func showTipsForTesting(_ tips: [any Tip.Type]) { _MOSTipStore.shared.override = nil }
}

// MARK: - Datastore

final class _MOSTipStore: @unchecked Sendable {
    static let shared = _MOSTipStore()

    struct Entry: Codable {
        var displayCount = 0
        var invalidated: Tips.InvalidationReason?
    }
    struct File: Codable {
        var tips: [String: Entry] = [:]
        var lastShown: Date?
    }

    private let lock = _MOSMutex()
    private var file = File()
    private var url: URL?
    private var frequency: TimeInterval = 0
    private(set) var configured = false
    /// showAllTipsForTesting (true) / hideAllTipsForTesting (false)
    var override: Bool? {
        get { lock.lock(); defer { lock.unlock() }; return _override }
        set { lock.lock(); _override = newValue; lock.unlock() }
    }
    private var _override: Bool?

    static func defaultURL() -> URL {
        let app = Bundle.main.bundleIdentifier ?? ProcessInfo.processInfo.processName
        return URL.homeDirectory.appending(path: "Library/Application Support/\(app)/TipKit")
    }

    func configure(_ opts: [Tips.ConfigurationOption]) throws {
        lock.lock()
        defer { lock.unlock() }
        var dir = Self.defaultURL()
        for o in opts {
            switch o.kind {
            case .location(let l): if let u = l.url { dir = u }
            case .frequency(let f): frequency = f.interval
            }
        }
        let u = dir.appending(path: "tips.json")
        if FileManager.default.fileExists(atPath: u.path) {
            file = (try? JSONDecoder().decode(File.self, from: Data(contentsOf: u))) ?? File()
        }
        url = u
        configured = true
    }

    func reset() throws {
        lock.lock()
        defer { lock.unlock() }
        file = File()
        try writeLocked()
    }

    private func writeLocked() throws {
        guard let url else { return }
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try JSONEncoder().encode(file).write(to: url, options: .atomic)
    }

    func invalidate(_ id: String, _ r: Tips.InvalidationReason) {
        lock.lock()
        defer { lock.unlock() }
        file.tips[id, default: Entry()].invalidated = r
        try? writeLocked()
    }

    func status<T: Tip>(of tip: T) -> Tips.Status {
        let rulesOK = tip.rules.allSatisfy { $0.isSatisfied }
        let max = tip.options.compactMap { $0 as? Tips.MaxDisplayCount }.last?.count
        let ignoraFreq = tip.options.compactMap { $0 as? Tips.IgnoresDisplayFrequency }.last?.ignores ?? false
        lock.lock()
        defer { lock.unlock() }
        if let o = _override { return o ? .available : .pending }
        let e = file.tips[tip.id] ?? Entry()
        if let r = e.invalidated { return .invalidated(r) }
        guard configured, rulesOK else { return .pending }
        if let max, e.displayCount >= max { return .invalidated(.displayCountExceeded) }
        // displayFrequency: outra dica apareceu há pouco (a que já está em exibição continua)
        if !ignoraFreq, e.displayCount == 0, frequency > 0, let last = file.lastShown,
           Date().timeIntervalSince(last) < frequency { return .pending }
        return .available
    }

    /// a dica apareceu na tela
    func didDisplay(_ id: String, max: Int?) {
        lock.lock()
        defer { lock.unlock() }
        var e = file.tips[id] ?? Entry()
        e.displayCount += 1
        if let max, e.displayCount > max { e.invalidated = .displayCountExceeded }
        file.tips[id] = e
        file.lastShown = Date()
        try? writeLocked()
    }

    func displayCount(_ id: String) -> Int {
        lock.lock()
        defer { lock.unlock() }
        return file.tips[id]?.displayCount ?? 0
    }
}

// MARK: - TipView

/// A dica embutida na interface: ícone, título, mensagem, ações e o botão de fechar.
public struct TipView<Content: Tip>: View {
    let tip: AnyTip?
    let arrowEdge: Edge?
    let action: (Tips.Action) -> Void
    @State private var closed = false

    public init(_ tip: Content, arrowEdge: Edge? = nil, action: @escaping (Tips.Action) -> Void = { _ in }) {
        self.tip = AnyTip(tip)
        self.arrowEdge = arrowEdge
        self.action = action
    }

    public init(_ tip: (any Tip)?, arrowEdge: Edge? = nil, action: @escaping (Tips.Action) -> Void = { _ in })
        where Content == AnyTip {
        self.tip = tip.map { AnyTip($0) }
        self.arrowEdge = arrowEdge
        self.action = action
    }

    public var body: some View {
        if let tip, !closed, tip.shouldDisplay {
            HStack(alignment: .top, spacing: 10) {
                if let image = tip.image { image }
                VStack(alignment: .leading, spacing: 4) {
                    tip.title.bold()
                    if let m = tip.message { m }
                    ForEach(tip.actions) { a in
                        Button { a(); action(a) } label: { a.title }
                    }
                }
                Spacer()
                Button {
                    tip.invalidate(reason: .tipClosed)
                    closed = true
                } label: { Text("✕") }
            }
            .padding(12)
            .onAppear {
                let max = tip.options.compactMap { $0 as? Tips.MaxDisplayCount }.last?.count
                _MOSTipStore.shared.didDisplay(tip.id, max: max)
            }
        }
    }
}

/// trava simples sobre pthread_mutex (o Foundation do mOS não traz NSLock)
final class _MOSMutex: @unchecked Sendable {
    private let m = UnsafeMutablePointer<pthread_mutex_t>.allocate(capacity: 1)
    init() { pthread_mutex_init(m, nil) }
    deinit { pthread_mutex_destroy(m); m.deallocate() }
    func lock() { pthread_mutex_lock(m) }
    func unlock() { pthread_mutex_unlock(m) }
}
