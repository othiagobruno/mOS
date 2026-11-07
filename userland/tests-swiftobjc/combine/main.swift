// mOS — teste do Combine da variante com interop ObjC (OpenCombine como módulo Combine + CombineLatest/Merge do mOS;
// tools/build-swift-combine-objc.ps1).
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\combine\Comb -Module Comb -Sources userland\tests-swiftobjc\combine\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\combine build\swiftobjctest\combine\Comb
// Imprime "combine: N/N ok".
import Combine
import Dispatch

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("combine: publicadores, assuntos e operadores")
var bag = Set<AnyCancellable>()

// Just + map + sink
var got: [Int] = []
Just(3).map { $0 * 2 }.sink { got.append($0) }.store(in: &bag)
check(got == [6], "Just.map.sink")

// Sequence + filter + collect
var col: [[Int]] = []
[1, 2, 3, 4, 5].publisher.filter { $0 % 2 == 1 }.collect().sink { col.append($0) }.store(in: &bag)
check(col == [[1, 3, 5]], "Sequence.filter.collect")

// PassthroughSubject + removeDuplicates + completion
let ps = PassthroughSubject<Int, Never>()
var pv: [Int] = []
var fim = false
ps.removeDuplicates().sink(receiveCompletion: { _ in fim = true }, receiveValue: { pv.append($0) }).store(in: &bag)
ps.send(1); ps.send(1); ps.send(2); ps.send(completion: .finished)
check(pv == [1, 2] && fim, "PassthroughSubject.removeDuplicates")

// CurrentValueSubject
let cv = CurrentValueSubject<String, Never>("a")
var cvv: [String] = []
cv.sink { cvv.append($0) }.store(in: &bag)
cv.value = "b"
check(cvv == ["a", "b"] && cv.value == "b", "CurrentValueSubject")

// combineLatest
let a = PassthroughSubject<Int, Never>(), b = PassthroughSubject<String, Never>()
var cl: [String] = []
a.combineLatest(b) { "\($0)\($1)" }.sink { cl.append($0) }.store(in: &bag)
a.send(1); b.send("x"); a.send(2); b.send("y")
check(cl == ["1x", "2x", "2y"], "combineLatest (\(cl))")

// merge
let m1 = PassthroughSubject<Int, Never>(), m2 = PassthroughSubject<Int, Never>()
var mv: [Int] = []
var mfim = false
m1.merge(with: m2).sink(receiveCompletion: { _ in mfim = true }, receiveValue: { mv.append($0) }).store(in: &bag)
m1.send(1); m2.send(2); m1.send(3); m1.send(completion: .finished)
check(mv == [1, 2, 3] && !mfim, "merge: valores")
m2.send(completion: .finished)
check(mfim, "merge: termina quando todos terminam")
var mm: [Int] = []
Publishers.MergeMany([Just(1), Just(2), Just(3)]).collect().sink { mm = $0 }.store(in: &bag)
check(mm == [1, 2, 3], "MergeMany")

// Optional.publisher / Result.Publisher
var op: [Int] = []
Optional(7).publisher.sink { op.append($0) }.store(in: &bag)
check(op == [7], "Optional.publisher")
enum E: Error { case x }
var rf = false
Result<Int, E>.failure(.x).publisher.sink(receiveCompletion: { if case .failure = $0 { rf = true } }, receiveValue: { _ in }).store(in: &bag)
check(rf, "Result.Publisher falha")

// tryMap + catch
var tc: [Int] = []
Just(1).tryMap { _ -> Int in throw E.x }.catch { _ in Just(9) }.sink { tc.append($0) }.store(in: &bag)
check(tc == [9], "tryMap.catch")

// @Published + ObservableObject
final class Modelo: ObservableObject {
    @Published var n = 0
}
let mo = Modelo()
var avisos = 0
var ns: [Int] = []
mo.objectWillChange.sink { avisos += 1 }.store(in: &bag)
mo.$n.sink { ns.append($0) }.store(in: &bag)
mo.n = 5
check(avisos == 1 && ns == [0, 5], "@Published/ObservableObject")

// cancelar
let cs = PassthroughSubject<Int, Never>()
var cn: [Int] = []
let c = cs.sink { cn.append($0) }
cs.send(1); c.cancel(); cs.send(2)
check(cn == [1], "AnyCancellable.cancel")

// DispatchQueue como Scheduler (overlay Dispatch)
let fila = DispatchQueue(label: "mos.combine")
var rq: [Int] = []
let sem = DispatchSemaphore(value: 0)
[1, 2].publisher.receive(on: fila).sink(receiveCompletion: { _ in _ = sem.signal() }, receiveValue: { rq.append($0) }).store(in: &bag)
_ = sem.wait(timeout: .now() + 3)
check(rq == [1, 2], "receive(on: DispatchQueue)")
let st = DispatchQueue.SchedulerTimeType.Stride.milliseconds(1500)
check(st.magnitude == 1_500_000_000 && st == .seconds(1.5) && fila.now.advanced(by: st) > fila.now, "SchedulerTimeType.Stride")
var dbn: [Int] = []
let semd = DispatchSemaphore(value: 0)
let dsub = PassthroughSubject<Int, Never>()
dsub.debounce(for: .milliseconds(50), scheduler: fila).sink { dbn.append($0); _ = semd.signal() }.store(in: &bag)
dsub.send(1); dsub.send(2)
_ = semd.wait(timeout: .now() + 3)
check(dbn == [2], "debounce(for:scheduler: DispatchQueue) (\(dbn))")

print("combine: \(ok)/\(total) ok")
