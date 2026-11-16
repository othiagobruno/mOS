// mOS — probe do runtime Swift na VM (tools/rtprobe.sh): Concurrency sobre a libdispatch, Dispatch, Observation,
// Synchronization, os, System e Regex/RegexBuilder. Escreve "RT-OK n/m" no fim e "RT-FAIL <item>" por falha.
import Dispatch
import Observation
import Synchronization
import os
import System
import RegexBuilder
import Darwin

nonisolated(unsafe) var ok = 0
nonisolated(unsafe) var total = 0
func check(_ c: Bool, _ name: String) {
    total += 1
    if c { ok += 1 } else { fputs("RT-FAIL \(name)\n", stderr) }
}

actor Contador {
    var n = 0
    func soma(_ k: Int) { n += k }
}

@Observable final class Modelo { var valor = 0 }

@main struct Probe {
    static func main() async {
        // async/await e Task.sleep (timer da libdispatch)
        let t0 = DispatchTime.now().uptimeNanoseconds
        try? await Task.sleep(nanoseconds: 50_000_000)
        let dt = (DispatchTime.now().uptimeNanoseconds - t0) / 1_000_000
        check(dt >= 45 && dt < 2000, "Task.sleep 50 ms (\(dt) ms)")

        // TaskGroup e actor
        let c = Contador()
        let soma = await withTaskGroup(of: Int.self) { g -> Int in
            for i in 1...100 { g.addTask { await c.soma(i); return i } }
            var s = 0
            for await v in g { s += v }
            return s
        }
        check(soma == 5050, "TaskGroup soma \(soma)")
        check(await c.n == 5050, "actor serializa 100 tarefas")

        // cancelamento
        let tarefa = Task { () -> Bool in
            try? await Task.sleep(nanoseconds: 5_000_000_000)
            return Task.isCancelled
        }
        tarefa.cancel()
        check(await tarefa.value, "Task.cancel acorda o sleep")

        // MainActor na fila principal
        let naPrincipal = await MainActor.run { pthread_main_np() != 0 }
        check(naPrincipal, "MainActor roda na thread principal")

        // AsyncStream
        let (stream, cont) = AsyncStream.makeStream(of: Int.self)
        Task { for i in 0..<10 { cont.yield(i) }; cont.finish() }
        var lidos = 0
        for await _ in stream { lidos += 1 }
        check(lidos == 10, "AsyncStream entrega 10 itens")

        // continuation com Dispatch
        let v: Int = await withCheckedContinuation { k in
            DispatchQueue.global().asyncAfter(deadline: .now() + .milliseconds(10)) { k.resume(returning: 7) }
        }
        check(v == 7, "withCheckedContinuation + asyncAfter")

        // Dispatch: grupo, concorrente, semáforo
        let q = DispatchQueue(label: "rt.conc", attributes: .concurrent)
        let g = DispatchGroup()
        let m = Mutex(0)
        for _ in 0..<64 { q.async(group: g) { m.withLock { $0 += 1 } } }
        check(g.wait(timeout: .now() + 5) == .success && m.withLock { $0 } == 64, "DispatchGroup + Mutex 64")
        let s = DispatchSemaphore(value: 0)
        DispatchQueue.global().async { s.signal() }
        check(s.wait(timeout: .now() + 2) == .success, "DispatchSemaphore")
        dispatchPrecondition(condition: .notOnQueue(q))
        check(true, "dispatchPrecondition")

        // Synchronization.Atomic
        let a = Atomic<Int>(0)
        DispatchQueue.concurrentPerform(iterations: 1000) { _ in a.wrappingAdd(1, ordering: .relaxed) }
        check(a.load(ordering: .relaxed) == 1000, "Atomic + concurrentPerform")

        // Observation
        let modelo = Modelo()
        nonisolated(unsafe) var mudou = false
        withObservationTracking { _ = modelo.valor } onChange: { mudou = true }
        modelo.valor = 1
        check(mudou, "withObservationTracking")

        // os
        let log = Logger(subsystem: "com.mos.rtprobe", category: "teste")
        log.info("rtprobe: Logger")
        let sp = OSSignposter(logger: log)
        let st = sp.beginInterval("intervalo")
        sp.endInterval("intervalo", st)
        check(true, "Logger e OSSignposter")

        // System
        let p = FilePath("/tmp/rtprobe.txt")
        check(p.lastComponent?.string == "rtprobe.txt" && p.extension == "txt", "FilePath")
        if let fd = try? FileDescriptor.open(p, .readWrite, options: [.create, .truncate], permissions: .ownerReadWrite) {
            let w = (try? fd.writeAll("mOS".utf8)) ?? 0
            try? fd.seek(offset: 0, from: .start)
            var buf = [UInt8](repeating: 0, count: 3)
            let r = (try? buf.withUnsafeMutableBytes { try fd.read(into: $0) }) ?? 0
            try? fd.close()
            check(w == 3 && r == 3 && buf == Array("mOS".utf8), "FileDescriptor write/read")
        } else { check(false, "FileDescriptor.open") }

        // Regex + RegexBuilder
        let rx = Regex { Capture { OneOrMore(.digit) } ; "-" ; Capture { OneOrMore(.word) } }
        if let mm = "id 42-abc".firstMatch(of: rx) { check(mm.1 == "42" && mm.2 == "abc", "RegexBuilder") }
        else { check(false, "RegexBuilder") }

        // String/Unicode na libswiftCore
        check("é" == "e\u{301}" && "ÇÃO".lowercased() == "ção", "String: NFC e caixa")

        print("RT-OK \(ok)/\(total)")
        fflush(stdout)
        exit(ok == total ? 0 : 1)
    }
}
