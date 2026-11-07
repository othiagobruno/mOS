// mOS — teste do Foundation da variante com interop ObjC (docs/swift-objc-runtime.md, fase 5, marco C): erros Swift <->
// NSError (CocoaError/POSIXError/URLError como _BridgedStoredNSError, CustomNSError, LocalizedError, as conversões do
// runtime) e o StringProtocol sobre NSString.
//   .\tools\link-swift-objc.ps1 -Out build\swiftobjctest\foundation3\Fnd3 -Module Fnd3 -Sources userland\tests-swiftobjc\foundation3\main.swift
//   .\build\machotest.exe build\libSystem.mos build\swiftobjctest\foundation3 build\swiftobjctest\foundation3\Fnd3
// Imprime "foundation3: N/N ok".
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("foundation3: erros e NSError")

// CocoaError guardado como NSError
let ce = CocoaError(.fileNoSuchFile, userInfo: ["NSFilePath": "/nada"])
check(ce.code == .fileNoSuchFile && ce.errorCode == 4 && CocoaError.errorDomain == "NSCocoaErrorDomain", "CocoaError: código e domínio")
check(ce.filePath == "/nada", "CocoaError.filePath")
let cns = ce as NSError
check(cns.code == 4 && (cns.domain as String) == "NSCocoaErrorDomain", "CocoaError as NSError")

// NSError -> tipo Swift pelo runtime (_ObjectiveCBridgeableError)
let ns1 = NSError(domain: "NSCocoaErrorDomain", code: 260, userInfo: nil)
let e1: Error = ns1
check((e1 as? CocoaError)?.code == .fileReadNoSuchFile, "NSError as? CocoaError")
check(e1 is NSError && (e1 as NSError) === ns1, "NSError como Error e de volta")
check((e1 as? URLError) == nil, "NSError de outro domínio não vira URLError")

// POSIXError e URLError
let pe = POSIXError(.ENOENT)
check(pe.code == .ENOENT && (pe as NSError).code == Int(ENOENT), "POSIXError")
let ue = URLError(.timedOut)
check(ue.code == .timedOut && ue.errorCode == -1001 && URLError.errorDomain == "NSURLErrorDomain", "URLError")
let ue2: Error = NSError(domain: "NSURLErrorDomain", code: -1009, userInfo: nil)
check((ue2 as? URLError)?.code == URLError.notConnectedToInternet, "NSError as? URLError")
switch ue2 { case URLError.notConnectedToInternet: check(true, "~= de código contra Error"); default: check(false, "~= de código contra Error") }

// erro Swift próprio -> NSError
enum MeuErro: Error, LocalizedError, CustomNSError {
    case ruim(Int)
    static var errorDomain: String { "mos.meu" }
    var errorCode: Int { if case .ruim(let n) = self { return n }; return 0 }
    var errorUserInfo: [String: Any] { ["extra": "x"] }
    var errorDescription: String? { "deu ruim" }
}
let me = MeuErro.ruim(42) as NSError
check((me.domain as String) == "mos.meu" && me.code == 42, "CustomNSError: domínio e código no NSError")
check((me.localizedDescription as String) == "deu ruim", "LocalizedError.errorDescription vira localizedDescription")
check((me.userInfo.object(forKey: "extra" as NSString) as? NSString).map { $0 as String } == "x", "CustomNSError.errorUserInfo no NSError")
check(MeuErro.ruim(1).localizedDescription == "deu ruim", "Error.localizedDescription")
enum Simples: Error { case a, b }
let sn = Simples.b as NSError
check(sn.code == 1 && (sn.domain as String).hasSuffix("Simples"), "enum Error simples: código e domínio")
check(_convertErrorToNSError(Simples.a).code == 0 && (_convertNSErrorToError(ns1) as NSError) === ns1, "_convertErrorToNSError/_convertNSErrorToError")

// throw/catch atravessando
func lanca() throws { throw CocoaError(.fileWriteNoPermission) }
do { try lanca(); check(false, "throw CocoaError") } catch let e as CocoaError { check(e.code == .fileWriteNoPermission, "throw/catch CocoaError") } catch { check(false, "throw CocoaError") }

// StringProtocol sobre a API de NSString
let s = "  Olá, Mundo café  "
check(s.trimmingCharacters(in: .whitespaces) == "Olá, Mundo café", "trimmingCharacters(in:)")
check(s.range(of: "MUNDO", options: .caseInsensitive) != nil && s.range(of: "mundo") == nil, "range(of:options:)")
check(s.contains("Mundo") && s.localizedCaseInsensitiveContains("CAFÉ"), "contains / localizedCaseInsensitiveContains")
check("arquivo10".localizedStandardCompare("arquivo9") == .orderedDescending
      && "a".compare("B", options: .caseInsensitive, range: nil, locale: nil) == .orderedAscending, "compare numérico e sem caixa")
check("a-b-c".replacingOccurrences(of: "-", with: "+") == "a+b+c", "replacingOccurrences")
check("a,b;c".components(separatedBy: CharacterSet(charactersIn: ",;")) == ["a", "b", "c"], "components(separatedBy: CharacterSet)")
check("ab".padding(toLength: 5, withPad: "xy", startingAt: 0) == "abxyx", "padding")
check("café".folding(options: .diacriticInsensitive, locale: nil) == "cafe", "folding diacriticInsensitive")
check("a b".addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) == "a%20b" && "a%20b".removingPercentEncoding == "a b",
      "percent encoding")
check(String(format: "%d-%@", 7, "x") == "7-x" && "x".appendingFormat("%d", 3) == "x3", "String(format:) e appendingFormat")
var palavras: [String] = []
let frase = "um dois três"
frase.enumerateSubstrings(in: frase.startIndex..., options: .byWords) { w, _, _, _ in if let w { palavras.append(w) } }
check(palavras == ["um", "dois", "três"], "enumerateSubstrings(byWords)")
check("x1y".rangeOfCharacter(from: .decimalDigits).map { String("x1y"[$0]) } == "1", "rangeOfCharacter(from:)")
check(String("abc" as NSString) == "abc", "String(NSString)")

// FileManager (NSFileManager): enumerator(at:) e replaceItemAt
let raiz = "/mos-f3"   // na raiz do harness/VFS
_ = mkdir(raiz, 0o755); _ = mkdir(raiz + "/sub", 0o755)
_ = FileManagerTouch(raiz + "/a.txt"); _ = FileManagerTouch(raiz + "/sub/b.txt")
func FileManagerTouch(_ p: String) -> Bool { let f = fopen(p, "w"); if let f { fputs("x", f); fclose(f) }; return f != nil }
var achados: [String] = []
if let en = NSFileManager.default.enumerator(at: URL(fileURLWithPath: raiz), includingPropertiesForKeys: nil) {
    while let u = en.nextObject() as? NSURL { achados.append((u as URL).lastPathComponent) }
}
check(Set(achados) == ["a.txt", "sub", "b.txt"], "FileManager.enumerator(at:) (\(achados.sorted()))")
_ = FileManagerTouch(raiz + "/novo.txt")
var resErr = ""
let res: URL?
do { res = try NSFileManager.default.replaceItemAt(URL(fileURLWithPath: raiz + "/a.txt"), withItemAt: URL(fileURLWithPath: raiz + "/novo.txt")) } catch { res = nil; resErr = "\(error)" }
check(res?.lastPathComponent == "a.txt" && access(raiz + "/novo.txt", F_OK) != 0, "FileManager.replaceItemAt \(resErr)")

// IntegerFormatStyle
let fmtBR = 1234567.formatted(IntegerFormatStyle<Int>(locale: Locale(identifier: "pt_BR")))
check(fmtBR == "1.234.567", "IntegerFormatStyle (pt_BR) (\(fmtBR), \(Locale(identifier: "pt_BR").identifier))")
check((-42).formatted(IntegerFormatStyle<Int>(locale: Locale(identifier: "en_US"))) == "-42", "IntegerFormatStyle negativo")

print("foundation3: \(ok)/\(total) ok")
