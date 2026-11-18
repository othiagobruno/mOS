// Teste do ExtensionFoundation (lado do app hospedeiro): tools/test-frameworks-swift.sh ExtensionFoundation.
// O script monta a raiz com /Applications/HostTeste.app trazendo duas extensões: uma ExtensionKit
// (EXExtensionPointIdentifier = com.mos.teste.ponto) e uma NSExtension (NSExtensionPointIdentifier =
// com.mos.teste.antigo), e uma terceira de outro ponto que não deve aparecer.
import ExtensionFoundation

var ok = 0
var total = 0
func check(_ c: Bool, _ nome: String) {
    total += 1
    if c { ok += 1; print("  ok   \(nome)") } else { print("  FALHA \(nome)") }
}

struct Pontos {
    @AppExtensionPoint.Bind
    static var principal: AppExtensionPoint {
        AppExtensionPoint.Identifier("com.mos.teste.ponto")
    }
}

do {
    var it = try AppExtensionIdentity.matching(appExtensionPointIDs: "com.mos.teste.ponto").makeAsyncIterator()
    let lista = await it.next() ?? []
    check(lista.count == 1, "matching acha só a extensão do ponto pedido")
    check(lista.first?.bundleIdentifier == "com.mos.teste.ext", "bundleIdentifier vem do Info.plist")
    check(lista.first?.localizedName == "Extensão Teste", "localizedName vem do CFBundleDisplayName")

    var it2 = try AppExtensionIdentity.matching(appExtensionPointIDs: "com.mos.teste.ponto", "com.mos.teste.antigo")
        .makeAsyncIterator()
    let duas = await it2.next() ?? []
    check(duas.count == 2, "matching com dois pontos inclui a extensão NSExtension")
    check(duas.contains { $0.localizedName == "Antiga" }, "sem CFBundleDisplayName usa o CFBundleName")

    let p = try AppExtensionPoint(identifier: "com.mos.teste.ponto")
    check(p == Pontos.principal, "AppExtensionPoint.Bind monta o mesmo ponto que init(identifier:)")
    let m = try await AppExtensionPoint.Monitor(appExtensionPoint: p)
    check(m.identities.map(\.bundleIdentifier) == ["com.mos.teste.ext"], "Monitor.identities lista a extensão")
    check(m.identities.first == lista.first, "AppExtensionIdentity é Equatable")

    var av = AppExtensionIdentity.availabilityUpdates.makeAsyncIterator()
    let a = await av.next()
    check(a?.disabledCount == 0 && a?.unapprovedCount == 0, "availabilityUpdates: nada desligado nem pendente")

    // a sequência fica aberta (sem instalação nova) até a tarefa ser cancelada
    let t = Task { () -> Int in
        var i = try AppExtensionIdentity.matching(appExtensionPointIDs: "com.mos.teste.ponto").makeAsyncIterator()
        var n = 0
        while await i.next() != nil { n += 1 }
        return n
    }
    for _ in 0..<20 { await Task.yield() }
    t.cancel()
    check((try? await t.value) == 1, "Identities entrega a lista uma vez e termina no cancelamento")
} catch {
    check(false, "erro inesperado: \(error)")
}

do {
    _ = try AppExtensionIdentity.matching(appExtensionPointIDs: "")
    check(false, "identificador vazio lança erro")
} catch {
    check(true, "identificador vazio lança erro")
}

print("extensionfoundation: \(ok)/\(total) ok")
