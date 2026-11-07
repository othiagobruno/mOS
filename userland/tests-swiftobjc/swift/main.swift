// mOS — teste das fases 2-3 de docs/swift-objc-runtime.md: stdlib + runtime Swift com interop ObjC sobre a libobjc
// do mOS. Sem Foundation/ObjectiveC (overlays são outra fase): fala com a libobjc pelas funções C.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\swift\SwiftObjc -Module SwiftObjc userland\tests-swiftobjc\swift\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\swift build\swiftobjctest\swift\SwiftObjc
// Imprime "swiftobjc: N/N ok".

@_silgen_name("objc_retain") func objc_retain(_ o: UnsafeRawPointer) -> UnsafeRawPointer
@_silgen_name("objc_release") func objc_release(_ o: UnsafeRawPointer)
@_silgen_name("object_getClassName") func object_getClassName(_ o: UnsafeRawPointer) -> UnsafePointer<CChar>
@_silgen_name("objc_getClass") func objc_getClass(_ n: UnsafePointer<CChar>) -> UnsafeRawPointer?
@_silgen_name("class_getSuperclass") func class_getSuperclass(_ c: UnsafeRawPointer) -> UnsafeRawPointer?
@_silgen_name("class_getName") func class_getName(_ c: UnsafeRawPointer) -> UnsafePointer<CChar>
@_silgen_name("objc_msgSend") func objc_msgSend_desc(_ o: UnsafeRawPointer, _ s: UnsafeRawPointer) -> UnsafeRawPointer?
@_silgen_name("sel_registerName") func sel_registerName(_ n: UnsafePointer<CChar>) -> UnsafeRawPointer
@_silgen_name("objc_initWeak") func objc_initWeak(_ l: UnsafeMutablePointer<UnsafeRawPointer?>, _ o: UnsafeRawPointer?) -> UnsafeRawPointer?
@_silgen_name("objc_destroyWeak") func objc_destroyWeak(_ l: UnsafeMutablePointer<UnsafeRawPointer?>)

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

func tem(_ s: String, _ p: String) -> Bool {
    let a = Array(s.utf8), b = Array(p.utf8)
    if b.count > a.count { return false }
    for i in 0...(a.count - b.count) where Array(a[i..<(i + b.count)]) == b { return true }
    return false
}

var deinits = 0
class Caixa {
    var valor: Int
    init(_ v: Int) { valor = v }
    deinit { deinits += 1 }
}
final class Filha: Caixa {}

print("swiftobjc: stdlib + runtime Swift com interop ObjC")

let c = Caixa(7)
let p = UnsafeRawPointer(Unmanaged.passUnretained(c).toOpaque())
let nome = String(cString: object_getClassName(p))
check(tem(nome, "Caixa"), "classe Swift vista pela libobjc: \(nome)")
let cls = objc_getClass(object_getClassName(p))
check(cls != nil, "objc_getClass acha a classe Swift")
if let cls, let sup = class_getSuperclass(cls) {
    let sn = String(cString: class_getName(sup))
    check(tem(sn, "SwiftObject"), "superclasse ObjC é a raiz Swift: \(sn)")
} else { check(false, "superclasse ObjC é a raiz Swift") }

let antes = _getRetainCount(c)
_ = objc_retain(p)
check(_getRetainCount(c) == antes + 1, "objc_retain chega ao refcount Swift (\(antes) -> \(_getRetainCount(c)))")
objc_release(p)
check(_getRetainCount(c) == antes, "objc_release volta")

let a: AnyObject = Filha(3)
check((a as? Caixa)?.valor == 3, "cast de AnyObject para classe Swift")
check(!(a is Int), "AnyObject não é Int")

// mensagem ObjC a um objeto Swift: -description vem do SwiftObject
if let d = objc_msgSend_desc(p, sel_registerName("description")) {
    check(true, "-description respondido (\(d))")
} else { check(false, "-description respondido") }

do {
    var w: UnsafeRawPointer? = nil
    var x: Caixa? = Caixa(9)
    let xp = UnsafeRawPointer(Unmanaged.passUnretained(x!).toOpaque())
    _ = objc_initWeak(&w, xp)
    check(w == xp, "referência fraca ObjC a objeto Swift")
    let d0 = deinits
    x = nil
    check(deinits == d0 + 1, "deinit roda")
    check(w == nil, "referência fraca zerada no dealloc")
    objc_destroyWeak(&w)
}

let s = "olá, mOS"
check(s.count == 8 && s.uppercased() == "OLÁ, MOS", "String nativa")
let dict: [String: Int] = ["a": 1, "b": 2]
check(dict["b"] == 2, "Dictionary")

print("swiftobjc: \(ok)/\(total) ok")
