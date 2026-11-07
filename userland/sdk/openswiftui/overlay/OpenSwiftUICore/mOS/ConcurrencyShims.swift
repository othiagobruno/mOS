//
//  ConcurrencyShims.swift
//  OpenSwiftUICore (mOS)
//
//  O mOS ainda não tem o runtime de concorrência (_Concurrency). O OpenSwiftUI usa pouco dele fora dos atributos
//  @MainActor/@isolated(any) (que o build tira dos fontes): MainActor.assumeIsolated, a isolação de um Binding,
//  extractIsolation e o ContinuousClock dos eventos do CAHostingLayer. Aqui ficam substitutos com a mesma forma,
//  visíveis só no pacote (OpenSwiftUICore + OpenSwiftUI). Tudo roda na thread de UI, então "assumir isolamento" é
//  simplesmente executar.

#if !canImport(_Concurrency) || OPENSWIFTUI_MOS

/// Substituto de `_Concurrency.Actor`.
package protocol Actor: AnyObject, Sendable {}

extension Actor {
    package func assumeIsolated<T>(_ operation: (Self) throws -> T, file: StaticString = #fileID, line: UInt = #line) rethrows -> T {
        try operation(self)
    }
}

/// Substituto de `_Concurrency.MainActor` (o ator global da UI).
package final class MainActor: Actor {
    package static let shared = MainActor()
    private init() {}

    package static func assumeIsolated<T>(_ operation: () throws -> T, file: StaticString = #fileID, line: UInt = #line) rethrows -> T {
        try operation()
    }
}

/// Sem atores, nenhuma função tem isolamento.
package func extractIsolation<F>(_ fn: F) -> (any Actor)? { nil }

@_silgen_name("mach_absolute_time") private func _mos_mach_absolute_time() -> UInt64

/// Substituto de `_Concurrency.ContinuousClock` (o relógio monotônico do mOS, em nanossegundos).
/// Público: os eventos do CAHostingLayer são API pública e recebem `Instant`.
public struct ContinuousClock: Sendable {
    public init() {}

    public struct Instant: Sendable, Hashable, Comparable {
        public var _nanoseconds: UInt64
        public init(_nanoseconds: UInt64) { self._nanoseconds = _nanoseconds }

        public static var now: Instant { Instant(_nanoseconds: _mos_mach_absolute_time()) }

        package func duration(to other: Instant) -> Duration {
            .nanoseconds(Int64(bitPattern: other._nanoseconds &- _nanoseconds))
        }
        package func advanced(by duration: Duration) -> Instant {
            let (s, a) = duration.components
            return Instant(_nanoseconds: _nanoseconds &+ UInt64(bitPattern: s &* 1_000_000_000 &+ a / 1_000_000_000))
        }
        public static func < (l: Instant, r: Instant) -> Bool { l._nanoseconds < r._nanoseconds }
    }

    package var now: Instant { .now }
    package static var continuous: ContinuousClock { ContinuousClock() }
}

#endif
