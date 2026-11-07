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

print("foundation3: \(ok)/\(total) ok")
