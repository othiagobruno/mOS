// mOS — teste de execução das classes do Foundation clássico do mOS (RunLoop/Timer sobre o CFRunLoop do libSystem,
// JSONSerialization, NSKeyedArchiver, String(format:), IndexSet, formatadores, Scanner, pontes `as`).
// tools\test-openswiftui-deps.ps1 -Main userland\sdk\openswiftui\tests\foundation-classic.swift
import Foundation

var falhas = 0
func check(_ nome: String, _ ok: Bool, _ detalhe: @autoclosure () -> String = "") {
    if ok { print("  ok: \(nome)") } else { falhas += 1; print("  FALHOU: \(nome) \(detalhe())") }
}

// String(format:) com %@, %s, %d, %.2f e posições
let s1 = String(format: "%@ tem %d anos e %.2f m (%s)", "Ana", 30, 1.655, "ok")
check("String(format:) %@ %d %.2f %s", s1 == "Ana tem 30 anos e 1.66 m (ok)", s1)
check("String(format:) posicional", String(format: "%2$@-%1$@", "a", "b") == "b-a")
check("CGFloat em %f", String(format: "%.1f", CGFloat(2.5)) == "2.5")

// pontes `as`
let ns = "olá" as NSString
check("String as NSString", ns.length == 3 && (ns as String) == "olá")
let nd = ["a": 1, "b": 2] as NSDictionary
check("Dictionary as NSDictionary", nd.count == 2 && (nd["a"] as? Int) == 1)
check("Array as NSArray", (["x", "y"] as NSArray).count == 2)
check("Date as NSDate", (Date(timeIntervalSinceReferenceDate: 10) as NSDate).timeIntervalSinceReferenceDate == 10)

// JSONSerialization
let obj: [String: Any] = ["nome": "mOS", "n": 3, "lista": [1, 2.5, true], "nada": NSNull()]
let json = try! JSONSerialization.data(withJSONObject: obj, options: .sortedKeys)
check("JSONSerialization.data", String(decoding: json, as: UTF8.self) == #"{"lista":[1,2.5,true],"n":3,"nada":null,"nome":"mOS"}"#,
      String(decoding: json, as: UTF8.self))
let back = try! JSONSerialization.jsonObject(with: json) as! [String: Any]
check("JSONSerialization.jsonObject", (back["nome"] as? String) == "mOS" && (back["n"] as? NSNumber)?.intValue == 3)

// NSKeyedArchiver / NSKeyedUnarchiver
let arch = NSKeyedArchiver(requiringSecureCoding: true)
arch.encode(NSNumber(value: 42), forKey: "num")
arch.encode(NSValue(cgRect: CGRect(x: 1, y: 2, width: 3, height: 4)), forKey: "val")
arch.encode("texto", forKey: "str")
let un = try! NSKeyedUnarchiver(forReadingFrom: arch.encodedData)
check("NSKeyedArchiver NSNumber", (un.decodeObject(forKey: "num") as? NSNumber)?.intValue == 42)
check("NSKeyedArchiver NSValue", (un.decodeObject(forKey: "val") as? NSValue)?.rectValue == CGRect(x: 1, y: 2, width: 3, height: 4))
check("NSKeyedArchiver String", (un.decodeObject(forKey: "str") as? String) == "texto")
let root = try! NSKeyedArchiver.archivedData(withRootObject: NSNumber(value: 7.5), requiringSecureCoding: true)
check("archivedData/unarchivedObject", (try? NSKeyedUnarchiver.unarchivedObject(ofClass: NSNumber.self, from: root))??.doubleValue == 7.5)

// IndexSet
var iset = IndexSet([1, 2, 3, 7])
iset.insert(4)
iset.remove(2)
check("IndexSet", Array(iset) == [1, 3, 4, 7] && iset.rangeView.count == 3 && iset.contains(4))

// formatadores
let nf = NumberFormatter(); nf.numberStyle = .decimal
check("NumberFormatter decimal", nf.string(from: 1234567.891) == "1,234,567.891", nf.string(from: 1234567.891) ?? "nil")
check("NumberFormatter percent", NumberFormatter.localizedString(from: 0.25, number: .percent) == "25%")
let df = DateFormatter(); df.timeZone = TimeZone(identifier: "GMT"); df.dateFormat = "yyyy-MM-dd HH:mm:ss"
let d = Date(timeIntervalSince1970: 1_700_000_000)
check("DateFormatter", df.string(from: d) == "2023-11-14 22:13:20", df.string(from: d))
check("DateFormatter.date(from:)", df.date(from: "2023-11-14 22:13:20") == d)
check("ISO8601DateFormatter", ISO8601DateFormatter().string(from: d) == "2023-11-14T22:13:20Z", ISO8601DateFormatter().string(from: d))

// Scanner
let sc = Scanner(string: "%12% resto")
check("Scanner", sc.scanCharacter() == "%" && sc.scanInt() == 12 && sc.scanCharacter() == "%")

// String com opções
check("range(of:options:)", "Cor Verde".range(of: "verde", options: .caseInsensitive) != nil && "abc".range(of: "x") == nil)

// Bundle e UserDefaults
check("Bundle.main", !Bundle.main.bundlePath.isEmpty)
UserDefaults.standard.set(5, forKey: "k")
check("UserDefaults", UserDefaults.standard.integer(forKey: "k") == 5 && !UserDefaults.standard.bool(forKey: "nada"))

// Thread e locks
check("Thread.isMainThread", Thread.isMainThread)
let lk = NSLock()
check("NSLock", lk.try() && { lk.unlock(); return true }())

// RunLoop + Timer (CFRunLoop do libSystem)
var disparos = 0
var executou = false
let t = Timer(timeInterval: 0.02, repeats: true) { tm in
    disparos += 1
    if disparos == 3 { tm.invalidate() }
}
RunLoop.main.add(t, forMode: .common)
RunLoop.main.perform { executou = true }
let limite = Date(timeIntervalSinceNow: 2)
while (disparos < 3 || !executou) && Date() < limite { _ = RunLoop.main.run(mode: .default, before: Date(timeIntervalSinceNow: 0.05)) }
check("Timer repetido no RunLoop", disparos == 3 && !t.isValid, "disparos=\(disparos)")
check("RunLoop.perform", executou)

print(falhas == 0 ? "TUDO OK" : "\(falhas) FALHAS")
