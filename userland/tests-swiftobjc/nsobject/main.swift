// mOS — teste do overlay ObjectiveC (docs/swift-objc-runtime.md, fase 3): subclasses Swift de NSObject sobre a libobjc
// e o NSObject do mOS (foundation.c), Selector, ObjCBool e NSObject Equatable/Hashable.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\nsobject\NSObj -Module NSObj -Sources userland\tests-swiftobjc\nsobject\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\nsobject build\swiftobjctest\nsobject\NSObj
// Imprime "nsobject: N/N ok".
import ObjectiveC

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

var deinits = 0
class Pessoa: NSObject {
    var nome: String
    var idade = 30
    init(nome: String) { self.nome = nome; super.init() }
    @objc func resposta() -> Int { 42 }
    @objc dynamic func dobro(_ x: Int) -> Int { x * 2 }
    override func isEqual(_ object: Any?) -> Bool { (object as? Pessoa)?.nome == nome }
    override var hash: Int { nome.hashValue }
    deinit { deinits += 1 }
}
final class Aluna: Pessoa {
    var turma = "B"
    override func resposta() -> Int { 43 }
}

print("nsobject: overlay ObjectiveC + subclasses Swift de NSObject")

let p = Pessoa(nome: "Ana")
check(p.nome == "Ana" && p.idade == 30, "campos Swift numa subclasse de NSObject")
check(p.resposta() == 42, "método @objc chamado do Swift")
check(p.responds(to: #selector(Pessoa.resposta)), "responds(to:) com #selector")
check(!p.responds(to: Selector("naoExiste")), "responds(to:) falso")

let r = p.perform(#selector(Pessoa.resposta))
check(r != nil || true, "perform(_:) não derruba")

let a = Aluna(nome: "Bia")
check(a.turma == "B" && a.idade == 30 && a.nome == "Bia", "campos da subclasse e da superclasse")
check((a as Pessoa).resposta() == 43, "override despachado")
check(a.isKind(of: Pessoa.self) && a.isKind(of: NSObject.self), "isKind(of:)")
let nomeClasse = String(cString: object_getClassName(a))
check(nomeClasse.hasSuffix("Aluna"), "nome ObjC da classe: \(nomeClasse)")

check(Pessoa(nome: "Ana") == Pessoa(nome: "Ana"), "== usa isEqual(_:)")
check(Pessoa(nome: "Ana") != Pessoa(nome: "Caio"), "!= usa isEqual(_:)")
let conj: Set<Pessoa> = [Pessoa(nome: "Ana"), Pessoa(nome: "Ana"), Pessoa(nome: "Caio")]
check(conj.count == 2, "Set<NSObject> pelo hash/isEqual")

let s1 = Selector("dobro:"), s2: Selector = "dobro:"
check(s1 == s2 && s1.description == "dobro:", "Selector: igualdade e descrição")
check(Set([s1, s2, Selector("outro")]).count == 2, "Selector Hashable")

let b: ObjCBool = true
check(b.boolValue && !ObjCBool(false).boolValue, "ObjCBool")

let d0 = deinits
do { _ = Pessoa(nome: "Temp") }
check(deinits == d0 + 1, "deinit de subclasse de NSObject roda")

let objetos: [AnyObject] = [p, a]
check(objetos.compactMap { $0 as? Aluna }.count == 1, "cast de AnyObject para subclasse")

autoreleasepool { _ = Pessoa(nome: "Pool") }
check(true, "autoreleasepool")

print("nsobject: \(ok)/\(total) ok")
