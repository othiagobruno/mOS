// mOS — teste do _Concurrency da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3): async/await, Task,
// TaskGroup, actor, continuations e MainActor, com o executor global do Darwin sobre o dispatch do mOS.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\concurrency\Conc -Module Conc -Sources userland\tests-swiftobjc\concurrency\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\concurrency build\swiftobjctest\concurrency\Conc
// Imprime "concurrency: N/N ok".
import Dispatch

nonisolated(unsafe) var ok = 0
nonisolated(unsafe) var total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

actor Contador {
    var n = 0
    func soma(_ k: Int) -> Int { n += k; return n }
}

func dobro(_ x: Int) async -> Int { x * 2 }

func lento() async throws -> String {
    try await withCheckedThrowingContinuation { (c: CheckedContinuation<String, Error>) in
        DispatchQueue.global().async { c.resume(returning: "continuação") }
    }
}

enum Falha: Error { case x }

@main
struct Principal {
    static func main() async {
        print("concurrency: _Concurrency com o executor do Darwin")

        check(await dobro(21) == 42, "função async")

        let t = Task { await dobro(5) }
        check(await t.value == 10, "Task.value")

        let c = Contador()
        for i in 1...4 { _ = await c.soma(i) }
        check(await c.n == 10, "actor serializa as mensagens")

        let soma = await withTaskGroup(of: Int.self, returning: Int.self) { g in
            for i in 1...5 { g.addTask { await dobro(i) } }
            var s = 0
            for await v in g { s += v }
            return s
        }
        check(soma == 30, "withTaskGroup")

        do {
            let s = try await lento()
            check(s == "continuação", "withCheckedThrowingContinuation")
        } catch { check(false, "withCheckedThrowingContinuation") }

        do {
            _ = try await withCheckedThrowingContinuation { (c: CheckedContinuation<Int, Error>) in c.resume(throwing: Falha.x) }
            check(false, "continuação com erro")
        } catch { check(error is Falha, "continuação com erro") }

        let cancelada = Task { () -> Bool in
            try? await Task.sleep(nanoseconds: 50_000_000)
            return Task.isCancelled
        }
        cancelada.cancel()
        check(await cancelada.value, "Task.cancel / isCancelled")

        let m = await MainActor.run { 7 }
        check(m == 7, "MainActor.run")

        let antes = DispatchTime.now()
        try? await Task.sleep(nanoseconds: 20_000_000)
        let dt = DispatchTime.now().uptimeNanoseconds - antes.uptimeNanoseconds
        let c0 = ContinuousClock.now
        try? await Task.sleep(for: .milliseconds(30))
        let dc = ContinuousClock.now - c0
        check(dt >= 15_000_000 && dc >= .milliseconds(25), "Task.sleep (dispatch \(dt / 1_000_000) ms, ContinuousClock \(dc))")

        print("concurrency: \(ok)/\(total) ok")
    }
}
