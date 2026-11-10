protocol Marcado {
    func marca() -> Int
}
struct Caixa<T: Marcado> {
    var item: T
}
enum Falha: Error {
    case esperada
}
final class Item: Marcado {
    func marca() -> Int { 7 }
    func calcular() throws -> Int { throw Falha.esperada }
}
let caixa = Caixa(item: Item())
do {
    _ = try caixa.item.calcular()
    print("SWIFT-FAIL sem erro")
} catch {
    print("SWIFT-OK marca=\(caixa.item.marca())")
}
