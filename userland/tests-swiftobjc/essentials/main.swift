// mOS — teste em execução do FoundationEssentials da variante com interop ObjC (docs/swift-objc-runtime.md, fase 5,
// marco B): o swift-foundation compilado com o ABI da Apple (-module-abi-name Foundation, DATA_LEGACY_ABI, library
// evolution), reexportado pelo overlay Foundation. Usa Data, URL, Date, UUID, JSON, PropertyList, Calendar e String.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\essentials\Ess -Module Ess -Sources userland\tests-swiftobjc\essentials\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\essentials build\swiftobjctest\essentials\Ess
// Imprime "essentials: N/N ok".
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("essentials: swift-foundation com o ABI da Apple")

// Data (representações inline, slice e grande)
var d = Data([1, 2, 3])
d.append(contentsOf: [4, 5])
check(d.count == 5 && d[4] == 5, "Data inline: append e subscript")
let big = Data(repeating: 7, count: 100_000)
check(big.count == 100_000 && big[99_999] == 7, "Data grande")
let sl = big[10..<20]
check(sl.count == 10 && sl.startIndex == 10, "fatia de Data")
check(Data("olá".utf8).base64EncodedString() == "b2zDoQ==", "Data.base64EncodedString")
check(Data(base64Encoded: "b2zDoQ==").map { String(decoding: $0, as: UTF8.self) } == "olá", "Data(base64Encoded:)")

// URL
let u = URL(string: "https://exemplo.com:8443/a/b.txt?q=1#frag")!
check(u.scheme == "https" && u.host == "exemplo.com" && u.port == 8443, "URL: scheme/host/port")
check(u.path == "/a/b.txt" && u.lastPathComponent == "b.txt" && u.pathExtension == "txt", "URL: path")
check(u.query == "q=1", "URL: query")
let f = URL(fileURLWithPath: "/Users/Visitante/doc.pdf")
check(f.isFileURL && f.deletingPathExtension().lastPathComponent == "doc", "URL de arquivo")
check(f.appendingPathComponent("x").path == "/Users/Visitante/doc.pdf/x", "URL.appendingPathComponent")
var comps = URLComponents()
comps.scheme = "http"; comps.host = "h"; comps.path = "/p"
comps.queryItems = [URLQueryItem(name: "a", value: "1")]
check(comps.url?.absoluteString == "http://h/p?a=1", "URLComponents")

// Date
let t0 = Date(timeIntervalSince1970: 1_000_000)
check(t0.timeIntervalSince1970 == 1_000_000 && Date() > t0, "Date")

// UUID
let id = UUID()
check(UUID(uuidString: id.uuidString) == id && id.uuidString.count == 36, "UUID")

// JSON
struct Ponto: Codable, Equatable { var x: Int; var nome: String; var tags: [String] }
let p = Ponto(x: 3, nome: "ä", tags: ["a", "b"])
do {
    let enc = JSONEncoder()
    enc.outputFormatting = .sortedKeys
    let j = try enc.encode(p)
    check(String(decoding: j, as: UTF8.self) == #"{"nome":"ä","tags":["a","b"],"x":3}"#, "JSONEncoder")
    check(try JSONDecoder().decode(Ponto.self, from: j) == p, "JSONDecoder")
} catch { check(false, "JSON: \(error)") }

// PropertyList
do {
    let pl = try PropertyListEncoder().encode(p)
    check(try PropertyListDecoder().decode(Ponto.self, from: pl) == p, "PropertyList ida e volta")
} catch { check(false, "PropertyList: \(error)") }

// Calendar (gregoriano, UTC)
var cal = Calendar(identifier: .gregorian)
cal.timeZone = TimeZone(secondsFromGMT: 0)!
check(cal.component(.year, from: Date(timeIntervalSince1970: 0)) == 1970, "Calendar.component(.year)")

// String (extensões do swift-foundation)
check("a,b,,c".split(separator: ",").count == 3, "split")
check("Olá Mundo".lowercased() == "olá mundo", "lowercased")
check("abc".data(using: .utf8) == Data([97, 98, 99]), "String.data(using:)")
check(String(data: Data([0x6f, 0x69]), encoding: .utf8) == "oi", "String(data:encoding:)")

print("essentials: \(ok)/\(total) ok")
