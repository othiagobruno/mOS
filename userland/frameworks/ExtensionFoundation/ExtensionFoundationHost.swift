// mOS — ExtensionFoundation.framework (Swift, macOS 13+): o lado do app hospedeiro.
// AppExtensionPoint descreve um ponto de extensão do app; AppExtensionIdentity é uma extensão instalada que o atende;
// AppExtensionPoint.Monitor e AppExtensionIdentity.matching(appExtensionPointIDs:) listam essas extensões.
// Reimplementação a partir da documentação pública. No mOS não há pluginkit: a lista sai do disco
// (ExtensionFoundation_scan.m), toda extensão instalada já vale como habilitada e aprovada, e não há outro processo
// avisando de instalações novas, então as sequências entregam a lista atual e ficam abertas até a tarefa ser cancelada.

import Darwin

/// Erro das APIs de hospedeiro (nome do ponto de extensão inválido).
public struct _MOSAppExtensionError: Error, CustomStringConvertible {
    public let description: String
}

/// Uma extensão de app instalada que atende um ponto de extensão.
public struct AppExtensionIdentity: Hashable, Sendable {
    public let bundleIdentifier: String
    public let localizedName: String
    /// o ponto de extensão que a extensão declara (EXExtensionPointIdentifier)
    public let extensionPointIdentifier: String
    let path: String

    public static func == (a: AppExtensionIdentity, b: AppExtensionIdentity) -> Bool {
        a.bundleIdentifier == b.bundleIdentifier && a.extensionPointIdentifier == b.extensionPointIdentifier
    }
    public func hash(into h: inout Hasher) {
        h.combine(bundleIdentifier)
        h.combine(extensionPointIdentifier)
    }

    /// Quantas extensões o usuário desligou ou ainda não aprovou.
    public struct Availability: Sendable, Equatable {
        public let disabledCount: Int
        public let unapprovedCount: Int
        public init(disabledCount: Int, unapprovedCount: Int) {
            self.disabledCount = disabledCount
            self.unapprovedCount = unapprovedCount
        }
    }

    /// No mOS toda extensão instalada está habilitada e aprovada: o valor é sempre (0, 0).
    public static var availabilityUpdates: AsyncStream<Availability> {
        AsyncStream { c in
            c.yield(Availability(disabledCount: 0, unapprovedCount: 0))
            _MOSStreams.keep(c)
        }
    }

    /// As extensões que atendem algum dos pontos pedidos; cada elemento é a lista inteira naquele momento.
    public static func matching(appExtensionPointIDs: String...) throws -> Identities {
        for id in appExtensionPointIDs where id.isEmpty {
            throw _MOSAppExtensionError(description: "ExtensionFoundation: identificador de ponto de extensão vazio")
        }
        return Identities(points: appExtensionPointIDs)
    }

    public struct Identities: AsyncSequence, Sendable {
        public typealias Element = [AppExtensionIdentity]
        let points: [String]

        public struct AsyncIterator: AsyncIteratorProtocol {
            let points: [String]
            var sent = false

            /// a primeira chamada devolve as extensões instaladas; as seguintes esperam até o cancelamento da tarefa
            /// (não há aviso de instalação nova no mOS) e então devolvem nil
            public mutating func next() async -> [AppExtensionIdentity]? {
                if !sent {
                    sent = true
                    return _MOSExtensionScan.identities(points: points)
                }
                await _MOSExtensionScan.waitForCancellation()
                return nil
            }
        }

        public func makeAsyncIterator() -> AsyncIterator { AsyncIterator(points: points) }
    }
}

/// Um ponto de extensão que o app hospedeiro oferece.
public struct AppExtensionPoint: Sendable, Hashable {
    public let identifier: String

    public struct Identifier: Sendable, Hashable {
        public let rawValue: String
        public init(_ identifier: StaticString) { rawValue = identifier.description }
    }

    public init(identifier: StaticString) throws {
        let s = identifier.description
        guard !s.isEmpty else {
            throw _MOSAppExtensionError(description: "ExtensionFoundation: identificador de ponto de extensão vazio")
        }
        self.identifier = s
    }

    init(_ id: Identifier) { identifier = id.rawValue }

    /// Construtor do `@AppExtensionPoint.Bind`: o corpo devolve o identificador do ponto.
    @resultBuilder
    public struct Bind {
        public static func buildBlock(_ id: AppExtensionPoint.Identifier) -> AppExtensionPoint { AppExtensionPoint(id) }
    }

    /// Acompanha as extensões instaladas para um ponto de extensão.
    public final class Monitor: @unchecked Sendable {
        public let appExtensionPoint: AppExtensionPoint

        public init(appExtensionPoint: AppExtensionPoint) async throws {
            self.appExtensionPoint = appExtensionPoint
        }

        /// as extensões instaladas agora (relidas do disco a cada acesso)
        public var identities: [AppExtensionIdentity] {
            _MOSExtensionScan.identities(points: [appExtensionPoint.identifier])
        }

        public enum State: Sendable, Equatable { case running, stopped }
        public var state: State { .running }
    }
}

enum _MOSExtensionScan {
    private final class Box { var all: [AppExtensionIdentity] = [] }

    static func identities(points: [String]) -> [AppExtensionIdentity] {
        let box = Box()
        let ctx = Unmanaged.passUnretained(box).toOpaque()
        mos_ef_enumerate(ctx) { ctx, bid, name, point, path in
            guard let ctx, let bid, let name, let point, let path else { return }
            let box = Unmanaged<Box>.fromOpaque(ctx).takeUnretainedValue()
            var nome = String(cString: name)
            if nome.hasSuffix(".appex") { nome.removeLast(6) }
            box.all.append(AppExtensionIdentity(bundleIdentifier: String(cString: bid), localizedName: nome,
                                                extensionPointIdentifier: String(cString: point),
                                                path: String(cString: path)))
        }
        var seen = Set<AppExtensionIdentity>()
        return box.all.filter { points.contains($0.extensionPointIdentifier) && seen.insert($0).inserted }
    }

    /// suspende até a tarefa atual ser cancelada (sem timer: a continuação volta pelo onCancel)
    static func waitForCancellation() async {
        let w = _MOSCancelWait()
        await withTaskCancellationHandler {
            await withCheckedContinuation { (c: CheckedContinuation<Void, Never>) in w.arm(c) }
        } onCancel: {
            w.fire()
        }
    }
}

/// A continuação de waitForCancellation; o cancelamento pode chegar antes ou depois de ela ser guardada.
final class _MOSCancelWait: @unchecked Sendable {
    private let lock = _MOSLock()
    private var c: CheckedContinuation<Void, Never>?
    private var fired = false

    func arm(_ k: CheckedContinuation<Void, Never>) {
        let now = lock.with { () -> Bool in
            if fired { return true }
            c = k
            return false
        }
        if now { k.resume() }
    }

    func fire() {
        let k = lock.with { () -> CheckedContinuation<Void, Never>? in
            fired = true
            defer { c = nil }
            return c
        }
        k?.resume()
    }
}

/// Mantém vivas as continuações de AsyncStream que nunca terminam (soltar a continuação encerraria a sequência).
enum _MOSStreams {
    nonisolated(unsafe) private static var kept: [Any] = []
    private static let lock = _MOSLock()
    static func keep(_ c: Any) { lock.with { kept.append(c) } }
}

final class _MOSLock: @unchecked Sendable {
    private var m = pthread_mutex_t()
    init() { pthread_mutex_init(&m, nil) }
    func with<T>(_ f: () -> T) -> T {
        pthread_mutex_lock(&m)
        defer { pthread_mutex_unlock(&m) }
        return f()
    }
}
