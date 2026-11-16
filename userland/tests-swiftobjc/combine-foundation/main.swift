// mOS — teste do Combine no módulo Foundation (userland/sdk/swift-objc/Foundation/CombinePublishers.swift):
// NotificationCenter.Publisher, Timer.publish/connect, NSObject.publisher(for:) (KVO), RunLoop e OperationQueue como
// Scheduler e o URLSession.DataTaskPublisher (falha com URLError e não começa sem demanda). tools/test-combine-foundation.sh.
// Imprime "combine-foundation: N/N ok".
import Foundation
import Combine
import Dispatch

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}
func postar(_ n: Notification.Name, _ o: AnyObject?) {
    NotificationCenter.default.post(Notification(name: n, object: o)._bridgeToObjectiveC())
}
func girar(_ s: Double) { _ = RunLoop.current.runMode(RunLoop.Mode.defaultRunLoopMode, before: Date().addingTimeInterval(s)._bridgeToObjectiveC()) }
func girarAte(_ limite: Double, _ cond: () -> Bool) {
    let fim = Date().addingTimeInterval(limite)
    while !cond() && Date() < fim { girar(0.02) }
}

print("combine-foundation: publishers do Foundation")
var bag = Set<AnyCancellable>()

// NotificationCenter
let nome = Notification.Name("mos.teste.combine")
var recebidas: [String] = []
NotificationCenter.default.publisher(for: nome)
    .map { ($0.object as? NSString).map { String._unconditionallyBridgeFromObjectiveC($0) } ?? "?" }
    .sink { recebidas.append($0) }
    .store(in: &bag)
postar(nome, "um" as NSString)
postar(Notification.Name("outra"), nil as AnyObject?)
postar(nome, "dois" as NSString)
check(recebidas == ["um", "dois"], "NotificationCenter.Publisher: só o nome pedido, na ordem")
bag.removeAll()
postar(nome, "tres" as NSString)
check(recebidas.count == 2, "NotificationCenter.Publisher: cancelar tira o observador")

// Timer.publish
var ticks = 0
let pub = Timer.publish(every: 0.05, on: .current, in: RunLoop.Mode.defaultRunLoopMode)
pub.sink { _ in ticks += 1 }.store(in: &bag)
girar(0.12)
check(ticks == 0, "TimerPublisher: nada antes do connect")
let conexao = pub.connect()
girarAte(2) { ticks >= 3 }
check(ticks >= 3, "TimerPublisher.connect: dispara (\(ticks))")
conexao.cancel()
let parado = ticks
girar(0.2)
check(ticks == parado, "TimerPublisher: cancelar a conexão para o timer")
var auto = 0
Timer.publish(every: 0.03, on: .main, in: RunLoop.Mode.commonModes).autoconnect().prefix(2).sink { _ in auto += 1 }.store(in: &bag)
girarAte(2) { auto >= 2 }
check(auto == 2, "TimerPublisher.autoconnect + prefix(2)")

// KVO
final class Modelo: NSObject {
    @objc dynamic var contador: Int = 0
}
let m = Modelo()!   // no SDK da variante o -[NSObject init] é anulável
var vistos: [Int] = []
m.publisher(for: \.contador).sink { vistos.append($0) }.store(in: &bag)
m.contador = 5
m.contador = 7
check(vistos == [0, 5, 7], "KeyValueObservingPublisher: inicial e mudanças (\(vistos))")
var semInicial: [Int] = []
m.publisher(for: \.contador, options: [.new]).sink { semInicial.append($0) }.store(in: &bag)
m.contador = 8
check(semInicial == [8], "KeyValueObservingPublisher sem .initial")
check(m.publisher(for: \.contador) == m.publisher(for: \.contador), "KeyValueObservingPublisher: Equatable")

// RunLoop como Scheduler
var noLaco: [Int] = []
Just(1).receive(on: RunLoop.main).sink { noLaco.append($0) }.store(in: &bag)
check(noLaco.isEmpty, "receive(on: RunLoop): não entrega na hora")
girarAte(1) { !noLaco.isEmpty }
check(noLaco == [1], "receive(on: RunLoop): entrega no laço")
var atrasado = false
let t0 = Date()
RunLoop.main.schedule(after: RunLoop.main.now.advanced(by: .milliseconds(80))) { atrasado = true }
girarAte(2) { atrasado }
check(atrasado && Date().timeIntervalSince(t0) >= 0.07, "RunLoop.schedule(after:)")
let d = RunLoop.SchedulerTimeType(Date(timeIntervalSinceReferenceDate: 10))
check(d.distance(to: d.advanced(by: .seconds(2))) == .seconds(2), "RunLoop.SchedulerTimeType: distância")

// OperationQueue como Scheduler
let fila = OperationQueue()!
fila.maxConcurrentOperationCount = 1   // receive(on:) pede agendador serial
var naFila = 0
let sem = DispatchSemaphore(value: 0)
[1, 2, 3].publisher.receive(on: fila).sink { naFila += $0; if naFila == 6 { sem.signal() } }.store(in: &bag)
check(sem.wait(timeout: .now() + 2) == .success && naFila == 6, "receive(on: OperationQueue)")
let sem2 = DispatchSemaphore(value: 0)
fila.schedule(after: fila.now.advanced(by: .milliseconds(50))) { sem2.signal() }
check(sem2.wait(timeout: .now() + 2) == .success, "OperationQueue.schedule(after:)")

// URLSession.DataTaskPublisher: o caminho de erro e a demanda
var falhou: URLError?
var valores = 0
URLSession.shared.dataTaskPublisher(for: URL(string: "mos-esquema-inexistente://x/y")!)
    .sink(receiveCompletion: { if case .failure(let e) = $0 { falhou = e } }, receiveValue: { _ in valores += 1 })
    .store(in: &bag)
girarAte(5) { falhou != nil }
check(falhou != nil && valores == 0, "DataTaskPublisher: esquema sem suporte termina com URLError")
let semDemanda = URLSession.shared.dataTaskPublisher(for: URL(string: "mos-esquema-inexistente://z")!)
check(semDemanda.request.url?.absoluteString == "mos-esquema-inexistente://z", "DataTaskPublisher(for: URL): pedido guardado")

print("combine-foundation: \(ok)/\(total) ok")
