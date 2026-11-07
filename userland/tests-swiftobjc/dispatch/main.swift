// mOS — teste do overlay Dispatch da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3): DispatchQueue/
// DispatchGroup/DispatchSemaphore com os tipos da Apple (classes OS_dispatch_*) sobre o libdispatch em C do mOS.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\dispatch\Disp -Module Disp -Sources userland\tests-swiftobjc\dispatch\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\dispatch build\swiftobjctest\dispatch\Disp
// Imprime "dispatch: N/N ok".
import Dispatch

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("dispatch: overlay Dispatch com os tipos da Apple")

let q = DispatchQueue(label: "mos.teste.serial")
check(q.label == "mos.teste.serial", "DispatchQueue(label:) e .label")
let v = q.sync { 6 * 7 }
check(v == 42, "sync devolve valor")

enum Falha: Error { case x }
do {
    try q.sync { throw Falha.x }
    check(false, "sync repassa erro")
} catch { check(error is Falha, "sync repassa erro") }

let cq = DispatchQueue(label: "mos.teste.concorrente", qos: .userInitiated, attributes: .concurrent)
check(cq.label == "mos.teste.concorrente", "fila concorrente")

let sem = DispatchSemaphore(value: 0)
var feito = false
DispatchQueue.global(qos: .utility).async {
    feito = true
    _ = sem.signal()
}
sem.wait()
check(feito, "global().async + semáforo")

let g = DispatchGroup()
var n = 0
for _ in 0..<3 { q.async(group: g) { n += 1 } }
let r = g.wait(timeout: .now() + 2.0)
check(r == .success && n == 3, "DispatchGroup com async(group:) e wait(timeout:)")

var notificado = false
g.notify(queue: q) { notificado = true }
q.sync {}
check(notificado, "DispatchGroup.notify")

let item = DispatchWorkItem(flags: .barrier) { n += 10 }
q.async(execute: item)
q.sync {}
check(n == 13, "DispatchWorkItem(flags:block:) executado")
let item2 = DispatchWorkItem { n += 100 }
item2.cancel()
q.async(execute: item2)
q.sync {}
check(item2.isCancelled && n == 13, "DispatchWorkItem.cancel()")

let t0 = DispatchTime.now()
let t1 = t0 + 0.5
check(t1 > t0 && t1.uptimeNanoseconds - t0.uptimeNanoseconds >= 400_000_000, "DispatchTime + Double e Comparable")

var depois = false
let sem2 = DispatchSemaphore(value: 0)
DispatchQueue.global().asyncAfter(deadline: .now() + 0.05) { depois = true; _ = sem2.signal() }
_ = sem2.wait(timeout: .now() + 3)
check(depois, "asyncAfter(deadline:)")

check(DispatchQoS.unspecified.qosClass == .unspecified && DispatchQoS.QoSClass(rawValue: QOS_CLASS_USER_INITIATED) == .userInitiated,
      "DispatchQoS")
check(DispatchWorkItemFlags.barrier.contains(.barrier) && !DispatchWorkItemFlags([]).contains(.barrier), "DispatchWorkItemFlags")
check(DispatchQueue.Attributes.concurrent.contains(.concurrent), "DispatchQueue.Attributes")

// valores por fila
let chave = DispatchSpecificKey<String>()
let qs = DispatchQueue(label: "mos.especifico")
qs.setSpecific(key: chave, value: "oi")
var dentro: String?
qs.sync { dentro = DispatchQueue.getSpecific(key: chave) }
check(qs.getSpecific(key: chave) == "oi" && dentro == "oi" && DispatchQueue.getSpecific(key: chave) == nil, "setSpecific/getSpecific")
qs.setSpecific(key: chave, value: nil)
check(qs.getSpecific(key: chave) == nil, "setSpecific(nil)")

var soma = 0
DispatchQueue.concurrentPerform(iterations: 5) { soma += $0 }
check(soma == 10, "concurrentPerform")

let dd = ([1, 2, 3] as [UInt8]).withUnsafeBytes { DispatchData(bytes: $0) }
check(dd.count == 3 && dd[2] == 3 && Array(dd) == [1, 2, 3], "DispatchData")

check(_dispatchPreconditionTest(.onQueue(.main)), "_dispatchPreconditionTest")

print("dispatch: \(ok)/\(total) ok")
