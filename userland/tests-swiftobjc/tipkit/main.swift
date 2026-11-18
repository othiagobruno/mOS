// Teste do TipKit: tools/test-frameworks-swift.sh TipKit (compila com o SwiftUI do mOS: swiftui-needs).
import Foundation
import SwiftUI
import TipKit

var ok = 0
var total = 0
func check(_ c: Bool, _ nome: String) {
    total += 1
    if c { ok += 1; print("  ok   \(nome)") } else { print("  FALHA \(nome)") }
}

struct Favoritos: Tip {
    var title: Text { Text("Favoritos") }
    var message: Text? { Text("Marque com a estrela") }
}

struct Limitada: Tip {
    var title: Text { Text("Limitada") }
    var options: [any TipOption] { [Tips.MaxDisplayCount(2)] }
}

nonisolated(unsafe) var regraLigada = false
struct ComRegra: Tip {
    var title: Text { Text("Com regra") }
    var rules: [Tips.Rule] { [Tips.Rule { regraLigada }] }
}

let dir = URL(filePath: "/tmp/tipkit-teste")
try? FileManager.default.removeItem(at: dir)

check(Favoritos().id == "Favoritos", "id padrão é o nome do tipo")
check(Favoritos().status == .pending, "antes de Tips.configure a dica não aparece")

do {
    try Tips.configure([.datastoreLocation(.url(dir)), .displayFrequency(.immediate)])
    check(true, "Tips.configure")
} catch {
    check(false, "Tips.configure: \(error)")
}
check(Favoritos().shouldDisplay, "configurado: a dica aparece")

Favoritos().invalidate(reason: .actionPerformed)
check(Favoritos().status == .invalidated(.actionPerformed), "invalidate(reason:) grava o motivo")
let json = (try? String(contentsOf: dir.appending(path: "tips.json"), encoding: .utf8)) ?? ""
check(json.contains("Favoritos") && json.contains("actionPerformed"), "o datastore guarda a invalidação no disco")

let anon = AnyTip(Limitada())
check(anon.id == "Limitada" && AnyTip(anon).id == "Limitada", "AnyTip guarda a dica original")
check(Limitada().shouldDisplay, "MaxDisplayCount(2): disponível no começo")

check(!ComRegra().shouldDisplay, "regra falsa: não aparece")
regraLigada = true
check(ComRegra().shouldDisplay, "regra verdadeira: aparece")

let v1 = TipView(Limitada() as (any Tip)?, arrowEdge: .top) { _ in }
let v2 = TipView(Favoritos())
_ = v1.body
_ = v2.body
check(true, "TipView monta com (any Tip)? e com o tipo concreto")

try? Tips.resetDatastore()
check(Favoritos().shouldDisplay, "resetDatastore apaga as invalidações")

Tips.hideAllTipsForTesting()
check(!ComRegra().shouldDisplay, "hideAllTipsForTesting esconde")
Tips.showAllTipsForTesting()
check(Favoritos().shouldDisplay, "showAllTipsForTesting mostra")

print("tipkit: \(ok)/\(total) ok")
