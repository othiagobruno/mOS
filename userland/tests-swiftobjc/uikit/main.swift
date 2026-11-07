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

// terceira leva: configurações, data sources, registros, ponteiro, atributos
var cfg = UIButton.Configuration.filled()
cfg.title = "Entrar"
cfg.cornerStyle = .capsule
let b2 = UIButton(configuration: cfg, primaryAction: nil)
check(b2.configuration?.title == "Entrar" && b2.configuration?.cornerStyle == .capsule && b2.configuration == cfg, "UIButton.Configuration")
var lista = UIListContentConfiguration.subtitleCell()
lista.text = "Ana"; lista.secondaryText = "online"
let celula = UITableViewCell(frame: CGRect())
celula.contentConfiguration = lista
check((celula.contentConfiguration as? UIListContentConfiguration)?.secondaryText == "online", "UITableViewCell.contentConfiguration")
var fundo = UIBackgroundConfiguration.listGroupedCell()
fundo.cornerRadius = 12
celula.backgroundConfiguration = fundo
check(celula.backgroundConfiguration?.cornerRadius == 12, "UIBackgroundConfiguration")

let tabela = UITableView(frame: CGRect())
let ds = UITableViewDiffableDataSource<Int, String>(tableView: tabela) { tv, ip, item in
    let c = UITableViewCell(frame: CGRect()); c.tag = item.count; return c
}
var s2 = NSDiffableDataSourceSnapshot<Int, String>()
s2.appendSections([0, 1]); s2.appendItems(["a", "bb"], toSection: 0); s2.appendItems(["ccc"], toSection: 1)
ds.apply(s2, animatingDifferences: false)
check(ds.numberOfSections(in: tabela) == 2 && ds.tableView(tabela, numberOfRowsInSection: 0) == 2, "UITableViewDiffableDataSource: contagens")
check(ds.itemIdentifier(for: IndexPath(row: 1, section: 0)) == "bb" && ds.indexPath(for: "ccc") == IndexPath(row: 0, section: 1), "UITableViewDiffableDataSource: itens")
check(ds.tableView(tabela, cellForRowAt: IndexPath(row: 0, section: 1)).tag == 3, "UITableViewDiffableDataSource: célula do cellProvider")

let colecao = UICollectionView(frame: CGRect())
let reg = UICollectionView.CellRegistration<UICollectionViewListCell, String> { c, _, item in c.tag = item.count * 10 }
let cds = UICollectionViewDiffableDataSource<Int, String>(collectionView: colecao) { cv, ip, item in
    cv.dequeueConfiguredReusableCell(using: reg, for: ip, item: item)
}
var s3 = NSDiffableDataSourceSnapshot<Int, String>()
s3.appendSections([7]); s3.appendItems(["xy"])
cds.apply(s3)
check(cds.snapshot().numberOfItems == 1 && cds.sectionIdentifier(for: 0) == 7, "UICollectionViewDiffableDataSource")
let cel = colecao.dequeueConfiguredReusableCell(using: reg, for: IndexPath(item: 0, section: 0), item: "abc")
check(cel.tag == 30, "CellRegistration + dequeueConfiguredReusableCell")
let lc = cel
lc.accessories = [UICellAccessory.outlineDisclosure()]
check(lc.accessories.count == 1, "UICollectionViewListCell.accessories")

// a UICollectionView do mOS pergunta ao data source diffable e desenha as células visíveis
var area = CGRect(); area.size.width = 300; area.size.height = 300
let grade = UICollectionView(frame: area)   // layout em fluxo padrão (50x50)
var pedidas = 0
let gds = UICollectionViewDiffableDataSource<Int, String>(collectionView: grade) { cv, ip, item in
    pedidas += 1
    return cv.dequeueConfiguredReusableCell(using: reg, for: ip, item: item)
}
var s4 = NSDiffableDataSourceSnapshot<Int, String>()
s4.appendSections([0, 1]); s4.appendItems(["a", "b", "c"], toSection: 0); s4.appendItems(["d", "e"], toSection: 1)
gds.apply(s4)
check(grade.numberOfSections() == 2 && grade.numberOfItems(inSection: 0) == 3 && grade.numberOfItems(inSection: 1) == 2,
      "UICollectionView consulta o data source diffable")
check(grade.visibleCells.count == 5 && pedidas == 5, "UICollectionView cria as células visíveis pelo cellProvider (\(pedidas))")

var texto = AttributedString("oi")
texto.uiKit.foregroundColor = UIColor()
check(texto.uiKit.foregroundColor != nil, "AttributeScopes.UIKitAttributes")

print("uikit: \(ok)/\(total) ok")
