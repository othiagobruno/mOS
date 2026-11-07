// mOS — teste do overlay Swift do UIKit do Catalyst (libswiftUIKit) sobre o UIKit.framework do mOS: IndexPath,
// snapshots "diffable", UIAction/UIMenu/UIKeyCommand/UIBarButtonItem/UIButton pelos inits Swift da Apple,
// UIControl.enumerateEventHandlers, UIWindowLevel, UIAccessibilityTraits e UIContentSizeCategory.
//   .\tools\test-uikit-swift.ps1
// Imprime "uikit: N/N ok".
import UIKit
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("uikit: overlay Swift do UIKit (Catalyst)")

let ip = IndexPath(row: 3, section: 1)
check(ip.row == 3 && ip.section == 1 && IndexPath(item: 2, section: 0).item == 2, "IndexPath(row:section:)/item")

var snap = NSDiffableDataSourceSnapshot<String, Int>()
snap.appendSections(["a", "b"])
snap.appendItems([1, 2], toSection: "a")
snap.appendItems([3])
check(snap.numberOfSections == 2 && snap.numberOfItems == 3 && snap.itemIdentifiers(inSection: "b") == [3], "snapshot: seções e itens")
check(snap.sectionIdentifier(containingItem: 2) == "a" && snap.indexOfItem(3) == 2, "snapshot: consultas")
snap.insertSections(["z"], beforeSection: "a")
snap.deleteSections(["b"])
check(snap.sectionIdentifiers == ["z", "a"] && snap.numberOfItems == 2, "snapshot: inserir e apagar seções")
var sec = NSDiffableDataSourceSectionSnapshot<String>()
sec.append(["pai"])
sec.append(["filho"], to: "pai")
check(sec.items == ["pai", "filho"] && sec.parent(of: "filho") == "pai" && !sec.isExpanded("pai"), "section snapshot")
sec.expand(["pai"])
check(sec.isExpanded("pai"), "section snapshot: expandir")

var disparou = 0
let acao = UIAction(title: "Enviar", attributes: [], state: .on) { _ in disparou += 1 }
check((acao.title as String) == "Enviar" && acao.state == .on, "UIAction(title:handler:)")
let botao = UIButton(type: .system, primaryAction: acao)
check(botao.buttonType == .system, "UIButton(type:primaryAction:)")
botao.sendActions(for: .primaryActionTriggered)
check(disparou == 1, "o UIAction roda quando o botão dispara")
var vistos = 0
botao.enumerateEventHandlers { a, _, ev, _ in if a != nil && ev.contains(.primaryActionTriggered) { vistos += 1 } }
check(vistos == 1, "UIControl.enumerateEventHandlers")

let menu = UIMenu(title: "Arquivo", options: .displayInline, children: [acao])
check((menu.title as String) == "Arquivo" && menu.children.count == 1 && menu.options.contains(.displayInline), "UIMenu(title:children:)")
let tecla = UIKeyCommand(title: "Novo", action: Selector(("novo:")), input: "n", modifierFlags: .command)
check((tecla.input as String?) == "n" && tecla.modifierFlags == .command, "UIKeyCommand(title:action:input:modifierFlags:)")
let item = UIBarButtonItem(title: "OK", primaryAction: acao)
check((item.title as String?) == "OK" && item.primaryAction === acao, "UIBarButtonItem(title:primaryAction:)")

var nivel = UIWindowLevel.normal + 1
nivel += 1
check(nivel.rawValue == 2 && UIWindowLevel.alert > nivel, "UIWindowLevel: aritmética e comparação")
let tracos: UIAccessibilityTraits = [.init(rawValue: 1), .init(rawValue: 4)]
check(tracos.contains(.init(rawValue: 4)) && tracos.rawValue == 5, "UIAccessibilityTraits como OptionSet")
check(UIContentSizeCategory.small < UIContentSizeCategory.large && !UIContentSizeCategory.medium.isAccessibilityCategory, "UIContentSizeCategory")

print("uikit: \(ok)/\(total) ok")
