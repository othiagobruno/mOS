// mOS — teste do Foundation da variante com interop ObjC (docs/swift-objc-runtime.md, fase 5, marco B, passos 2–3): as
// pontes Data/URL/Date/UUID/Locale/TimeZone <-> NS*, IndexSet, Notification, URLRequest, URLResourceValues e KVO.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\foundation2\Fnd2 -Module Fnd2 -Sources userland\tests-swiftobjc\foundation2\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\foundation2 build\swiftobjctest\foundation2\Fnd2
// Imprime "foundation2: N/N ok".
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("foundation2: pontes dos tipos-valor e tipos do Foundation")

// Data <-> NSData
let d = Data([1, 2, 3, 250])
let nd = d as NSData
check(nd.length == 4, "Data -> NSData")
check((nd as Data) == d, "NSData -> Data")

// URL <-> NSURL
let u = URL(string: "https://exemplo.com/a?b=1")!
let nu = u as NSURL
check(nu.absoluteString.map { $0 as String } == "https://exemplo.com/a?b=1", "URL -> NSURL")
check((nu as URL) == u, "NSURL -> URL")
let fu = URL(fileURLWithPath: "/tmp/x.txt")
check(((fu as NSURL) as URL).path == "/tmp/x.txt", "URL de arquivo ida e volta")

// Date <-> NSDate
let t = Date(timeIntervalSince1970: 1234.5)
check(((t as NSDate) as Date).timeIntervalSince1970 == 1234.5, "Date <-> NSDate")

// UUID <-> NSUUID
let id = UUID()
check(((id as NSUUID) as UUID) == id, "UUID <-> NSUUID")

// TimeZone
let tz = TimeZone(secondsFromGMT: 3600)!
check(((tz as NSTimeZone) as TimeZone).secondsFromGMT() == 3600, "TimeZone <-> NSTimeZone")

// IndexSet
var ix = IndexSet(integer: 5)
ix.insert(7); ix.insert(6); ix.insert(1)
check(ix.count == 4 && ix.first == 1 && ix.contains(6) && !ix.contains(3), "IndexSet")
check(Array(ix) == [1, 5, 6, 7] && ix.rangeView.count == 2, "IndexSet: ordem e faixas")
check(!IndexSet().isEmpty == false, "IndexSet vazio")

// Notification
let nome = Notification.Name(rawValue: "mos.teste" as NSString)
let n = Notification(name: nome, object: nil, userInfo: ["k": 1])
let nn = n as NSNotification
check((nn.name.rawValue as String) == "mos.teste", "Notification -> NSNotification")
let back = nn as Notification
check(back.name == nome && (back.userInfo?["k"] as? Int) == 1, "NSNotification -> Notification")

// URLRequest
var req = URLRequest(url: u, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 5)
req.httpMethod = "POST"
req.setValue("application/json", forHTTPHeaderField: "Content-Type")
req.httpBody = Data("{}".utf8)
check(req.value(forHTTPHeaderField: "content-type") == "application/json", "URLRequest: cabeçalho")
let nreq = req as NSURLRequest
check((nreq.httpMethod.map { $0 as String }) == "POST", "URLRequest -> NSURLRequest")
check((nreq as URLRequest).httpBody == Data("{}".utf8), "NSURLRequest -> URLRequest")

// URLResourceValues
do {
    let dir = URL(fileURLWithPath: "/")
    let v = try dir.resourceValues(forKeys: [])
    check(v.isDirectory == true && v.isRegularFile == false, "URL.resourceValues de /")
} catch { check(false, "URL.resourceValues: \(error)") }

// KVO
class Modelo: NSObject {
    @objc dynamic var valor: Int = 0
    override init() { super.init() }
}
let m = Modelo()
var visto: [Int] = []
let obs = m.observe(\.valor, options: [.new]) { _, change in if let v = change.newValue { visto.append(v) } }
m.valor = 3
m.valor = 4
check(visto == [3, 4], "KVO observe (\(visto))")
obs.invalidate()
m.valor = 5
check(visto == [3, 4], "KVO invalidate")

print("foundation2: \(ok)/\(total) ok")
