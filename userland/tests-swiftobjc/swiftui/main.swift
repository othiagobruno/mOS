// mOS — teste do SwiftUI.framework (tools/test-swiftui.sh). Liga como um app da Apple contra o SwiftUI.framework do
// mOS (OpenSwiftUI com -module-abi-name SwiftUI) e roda no build/machotest. As sondas medem com GeometryReader onde cada
// view caiu no layout; os valores esperados são os do SwiftUI da Apple para o mesmo código.
import AppKit
import OpenSwiftUI
import Observation

var total = 0, passou = 0
func check(_ ok: Bool, _ what: String) {
    total += 1
    if ok { passou += 1; print("  ok   \(what)") } else { print("  FALHA \(what)") }
}

nonisolated(unsafe) var medidas: [String: CGRect] = [:]

struct Sonda: View {
    let id: String
    var body: some View {
        GeometryReader { g in
            // mede a cada avaliação do corpo (o GeometryReader reavalia quando a geometria muda)
            let _ = { medidas[id] = g.frame(in: .global) }()
            Color.clear
        }
    }
}

/// hospeda a view num NSHostingView do tamanho dado e devolve as medidas das sondas
func medir<V: View>(_ w: CGFloat, _ h: CGFloat, @ViewBuilder _ v: () -> V) -> [String: CGRect] {
    medidas = [:]
    let host = NSHostingView(rootView: v())
    host.setFrameSize(NSSize(width: w, height: h))
    host.layout()
    return medidas
}

func perto(_ r: CGRect?, _ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat) -> Bool {
    guard let r else { return false }
    return abs(r.minX - x) < 0.5 && abs(r.minY - y) < 0.5 && abs(r.width - w) < 0.5 && abs(r.height - h) < 0.5
}

func desc(_ r: CGRect?) -> String { r.map { "(\($0.minX), \($0.minY), \($0.width), \($0.height))" } ?? "nil" }

// HStack centraliza o grupo e soma o espaçamento
var m = medir(200, 100) {
    HStack(spacing: 10) {
        Color.red.frame(width: 50, height: 20).background(Sonda(id: "a"))
        Color.blue.frame(width: 30, height: 20).background(Sonda(id: "b"))
    }
}
check(perto(m["a"], 55, 40, 50, 20) && perto(m["b"], 115, 40, 30, 20), "HStack(spacing: 10) centralizado: a \(desc(m["a"])) b \(desc(m["b"]))")

// VStack empilha sem espaço
m = medir(200, 100) {
    VStack(spacing: 0) {
        Color.red.frame(width: 40, height: 10).background(Sonda(id: "a"))
        Color.blue.frame(width: 40, height: 30).background(Sonda(id: "b"))
    }
}
check(perto(m["a"], 80, 30, 40, 10) && perto(m["b"], 80, 40, 40, 30), "VStack(spacing: 0): a \(desc(m["a"])) b \(desc(m["b"]))")

// padding cresce a view nos quatro lados
m = medir(200, 100) {
    Color.green.frame(width: 50, height: 50).padding(10).background(Sonda(id: "p"))
}
check(perto(m["p"], 65, 15, 70, 70), "padding(10) em 50x50: \(desc(m["p"]))")

// Spacer empurra para a borda
m = medir(200, 100) {
    HStack(spacing: 0) {
        Color.red.frame(width: 20, height: 20).background(Sonda(id: "s"))
        Spacer()
    }
}
check(perto(m["s"], 0, 40, 20, 20), "HStack com Spacer encosta à esquerda: \(desc(m["s"]))")

// ZStack com alinhamento topLeading dentro de um frame maior
m = medir(200, 100) {
    ZStack(alignment: .topLeading) {
        Color.gray.frame(width: 100, height: 60)
        Color.red.frame(width: 10, height: 10).background(Sonda(id: "z"))
    }
}
check(perto(m["z"], 50, 20, 10, 10), "ZStack(.topLeading): \(desc(m["z"]))")

// frame(maxWidth: .infinity, alignment: .trailing) ocupa a largura e alinha à direita
m = medir(200, 100) {
    Color.red.frame(width: 30, height: 30).background(Sonda(id: "t"))
        .frame(maxWidth: .infinity, alignment: .trailing)
}
check(perto(m["t"], 170, 35, 30, 30), "frame(maxWidth: .infinity, alignment: .trailing): \(desc(m["t"]))")

// offset só desloca o desenho; a sonda dentro dele vê o deslocamento no espaço global
m = medir(200, 100) {
    Color.red.frame(width: 20, height: 20).background(Sonda(id: "o")).offset(x: 15, y: -5)
}
check(perto(m["o"], 105, 35, 20, 20), "offset(x: 15, y: -5): \(desc(m["o"]))")

// VStack(alignment: .leading) alinha as bordas esquerdas
m = medir(200, 100) {
    VStack(alignment: .leading, spacing: 0) {
        Color.red.frame(width: 40, height: 10).background(Sonda(id: "a"))
        Color.blue.frame(width: 20, height: 10).background(Sonda(id: "b"))
    }
}
check(perto(m["a"], 80, 40, 40, 10) && perto(m["b"], 80, 50, 20, 10), "VStack(alignment: .leading): a \(desc(m["a"])) b \(desc(m["b"]))")

// ForEach gera uma view por elemento
m = medir(200, 100) {
    HStack(spacing: 0) {
        ForEach(0..<3, id: \.self) { i in
            Color.red.frame(width: 10, height: 10).background(Sonda(id: "f\(i)"))
        }
    }
}
check(perto(m["f0"], 85, 45, 10, 10) && perto(m["f1"], 95, 45, 10, 10) && perto(m["f2"], 105, 45, 10, 10),
      "ForEach(0..<3): \(desc(m["f0"])) \(desc(m["f1"])) \(desc(m["f2"]))")

// if/else escolhe um ramo
let mostrarA = true
m = medir(200, 100) {
    if mostrarA {
        Color.red.frame(width: 30, height: 30).background(Sonda(id: "A"))
    } else {
        Color.blue.frame(width: 30, height: 30).background(Sonda(id: "B"))
    }
}
check(perto(m["A"], 85, 35, 30, 30) && m["B"] == nil, "if/else: A \(desc(m["A"])) B \(desc(m["B"]))")

// GeometryReader recebe a proposta inteira
m = medir(200, 100) { Sonda(id: "g") }
check(perto(m["g"], 0, 0, 200, 100), "GeometryReader ocupa a proposta: \(desc(m["g"]))")

// aspectRatio(.fit) cabe na menor dimensão
m = medir(200, 200) { Color.red.aspectRatio(2, contentMode: .fit).background(Sonda(id: "r")) }
check(perto(m["r"], 0, 50, 200, 100), "aspectRatio(2, .fit) em 200x200: \(desc(m["r"]))")

// ScrollView: conteúdo no topo, centralizado na horizontal
m = medir(200, 100) {
    ScrollView {
        Color.red.frame(width: 50, height: 300).background(Sonda(id: "s"))
    }
}
check(perto(m["s"], 75, 0, 50, 300), "ScrollView vertical: \(desc(m["s"]))")

// @State mudado no onAppear redesenha com o valor novo
struct ComEstado: View {
    @State var largura: CGFloat = 10
    var body: some View {
        Color.red.frame(width: largura, height: 10).background(Sonda(id: "e")).onAppear { largura = 80 }
    }
}
// @Binding: o filho escreve no estado do pai
struct Filho: View {
    @Binding var valor: CGFloat
    var body: some View { Color.clear.frame(width: 1, height: 1).onAppear { valor = 40 } }
}
struct Pai: View {
    @State var valor: CGFloat = 5
    var body: some View {
        VStack(spacing: 0) {
            Color.red.frame(width: valor, height: 10).background(Sonda(id: "p"))
            Filho(valor: $valor)
        }
    }
}
// o que a macro @Observable gera (os apps da Apple chegam com ela já expandida)
final class Modelo: Observation.Observable {
    private let _registrar = ObservationRegistrar()
    private var _largura: CGFloat = 20
    var largura: CGFloat {
        get { _registrar.access(self, keyPath: \.largura); return _largura }
        set { _registrar.withMutation(of: self, keyPath: \.largura) { _largura = newValue } }
    }
}
struct ComModelo: View {
    let modelo: Modelo
    var body: some View { Color.red.frame(width: modelo.largura, height: 10).background(Sonda(id: "o")) }
}

medidas = [:]
let modelo = Modelo()
let hosts: [NSView] = [NSHostingView(rootView: ComEstado()), NSHostingView(rootView: Pai()), NSHostingView(rootView: ComModelo(modelo: modelo))]
for h in hosts { h.setFrameSize(NSSize(width: 200, height: 100)); h.layout() }
modelo.largura = 120
// uma segunda passada (outro tamanho) aplica o que mudou depois da primeira
medidas = [:]
for h in hosts { h.setFrameSize(NSSize(width: 200, height: 101)); h.layout() }
check(abs((medidas["e"]?.width ?? 0) - 80) < 0.5, "@State mudado no onAppear: \(desc(medidas["e"]))")
check(abs((medidas["p"]?.width ?? 0) - 40) < 0.5, "@Binding escreve no estado do pai: \(desc(medidas["p"]))")
check(abs((medidas["o"]?.width ?? 0) - 120) < 0.5, "@Observable redesenha com o valor novo: \(desc(medidas["o"]))")

// NSViewRepresentable: a NSView entra no NSHostingView no frame do layout
final class Contador { var feitas = 0, atualizadas = 0, desmontadas = 0, ultimo = 0 }
let cont = Contador()
final class Coord { let id = 7 }
struct Rep: NSViewRepresentable {
    var valor: Int
    func makeCoordinator() -> Coord { Coord() }
    func makeNSView(context: Context) -> NSView { cont.feitas += 1; return NSView(frame: .zero) }
    func updateNSView(_ nsView: NSView, context: Context) { cont.atualizadas += 1; cont.ultimo = valor * 10 + context.coordinator.id }
    static func dismantleNSView(_ nsView: NSView, coordinator: Coord) { cont.desmontadas += 1 }
    func sizeThatFits(_ proposal: ProposedViewSize, nsView: NSView, context: Context) -> CGSize? { CGSize(width: 40, height: 20) }
}
let hostRep = NSHostingView(rootView: AnyView(HStack(spacing: 0) {
    Rep(valor: 1)
    Color.red.frame(width: 30, height: 20)
}))
hostRep.setFrameSize(NSSize(width: 200, height: 100))
hostRep.layout()
let sub = hostRep.subviews.first { $0.frame.width == 40 }
check(sub != nil && abs(sub!.frame.minX - 65) < 0.5 && abs(sub!.frame.minY - 40) < 0.5 && sub!.frame.height == 20,
      "NSViewRepresentable no frame do layout: \(sub.map { "\($0.frame)" } ?? "sem subview")")
check(cont.feitas == 1 && cont.atualizadas >= 1 && cont.ultimo == 17, "makeNSView 1x, updateNSView com coordinator: \(cont.feitas) \(cont.atualizadas) \(cont.ultimo)")
hostRep.rootView = AnyView(HStack(spacing: 0) { Rep(valor: 2); Color.red.frame(width: 30, height: 20) })
hostRep.setFrameSize(NSSize(width: 200, height: 101))
hostRep.layout()
check(cont.ultimo == 27 && cont.feitas == 1, "updateNSView recebe o valor novo sem recriar a NSView: \(cont.ultimo) (feitas \(cont.feitas))")
hostRep.rootView = AnyView(Color.red.frame(width: 30, height: 20))
hostRep.setFrameSize(NSSize(width: 200, height: 100))
hostRep.layout()
check(cont.desmontadas >= 1 && !hostRep.subviews.contains { $0.frame.width == 40 }, "dismantleNSView ao sair da árvore: \(cont.desmontadas)")

// o grafo sobrevive a resize: @State não volta ao valor inicial
struct Persistente: View {
    @State var n = 0
    var body: some View {
        Color.red.frame(width: CGFloat(10 + n * 10), height: 10).background(Sonda(id: "per")).onAppear { n += 1 }
    }
}
let hostPer = NSHostingView(rootView: Persistente())
for h in [100.0, 101, 102, 103] { hostPer.setFrameSize(NSSize(width: 200, height: h)); hostPer.layout() }
check(abs((medidas["per"]?.width ?? 0) - 20) < 0.5, "@State sobrevive ao resize (onAppear 1x): \(desc(medidas["per"]))")

// NSViewControllerRepresentable sem sizeThatFits: ocupa a proposta
struct RepVC: NSViewControllerRepresentable {
    func makeNSViewController(context: Context) -> NSViewController {
        let c = NSViewController(nibName: nil, bundle: nil)
        c.view = NSView(frame: .zero)
        return c
    }
    func updateNSViewController(_ c: NSViewController, context: Context) {}
}
let hostVC = NSHostingView(rootView: RepVC().frame(width: 120, height: 50))
hostVC.setFrameSize(NSSize(width: 200, height: 100))
hostVC.layout()
let subVC = hostVC.subviews.first { $0.frame.width == 120 }
check(subVC != nil && abs(subVC!.frame.minX - 40) < 0.5 && abs(subVC!.frame.minY - 25) < 0.5, "NSViewControllerRepresentable: \(subVC.map { "\($0.frame)" } ?? "sem subview")")

// Picker: as opções vêm dos filhos com .tag; o menu (padrão) mostra só a selecionada
enum Sabor: Hashable { case baunilha, chocolate, morango }
struct OpcaoPicker: View {
    let s: Sabor
    var body: some View { Color.red.frame(width: 20, height: 10).background(Sonda(id: "op-\(s)")) }
}
struct TestePicker: View {
    @State var sabor: Sabor = .chocolate
    let estilo: Int
    var body: some View {
        let p = Picker(selection: $sabor) {
            OpcaoPicker(s: .baunilha).tag(Sabor.baunilha)
            OpcaoPicker(s: .chocolate).tag(Sabor.chocolate)
            OpcaoPicker(s: .morango).tag(Sabor.morango)
        } label: { EmptyView() }
        if estilo == 0 { p } else { p.pickerStyle(.radioGroup) }
    }
}
m = medir(300, 200) { TestePicker(estilo: 0) }
check(m["op-chocolate"] != nil && m["op-baunilha"] == nil && m["op-morango"] == nil,
      "Picker menu mostra só a opção selecionada: \(m.keys.filter { $0.hasPrefix("op-") }.sorted())")
m = medir(300, 200) { TestePicker(estilo: 1) }
if let a = m["op-baunilha"], let b = m["op-chocolate"], let c = m["op-morango"] {
    check(a.minY < b.minY && b.minY < c.minY && abs(a.minX - c.minX) < 0.5, "Picker radioGroup empilha as 3 opções: \(desc(a)) \(desc(b)) \(desc(c))")
} else { check(false, "Picker radioGroup: faltam opções \(m.keys.sorted())") }

// LazyVGrid: coluna fixa de 40 e uma flexível com o resto (200 - 40 - 8)
m = medir(200, 100) {
    LazyVGrid(columns: [GridItem(.fixed(40)), GridItem(.flexible())]) {
        ForEach(0..<4, id: \.self) { i in Color.red.frame(width: 20, height: 20).background(Sonda(id: "g\(i)")) }
    }
}
check(perto(m["g0"], 10, 26, 20, 20) && perto(m["g1"], 114, 26, 20, 20) && perto(m["g2"], 10, 54, 20, 20) && perto(m["g3"], 114, 54, 20, 20),
      "LazyVGrid fixed+flexible: \(desc(m["g0"])) \(desc(m["g1"])) \(desc(m["g2"])) \(desc(m["g3"]))")

// Grid: colunas com a largura da maior célula
m = medir(200, 100) {
    Grid {
        GridRow {
            Color.red.frame(width: 30, height: 10).background(Sonda(id: "A"))
            Color.red.frame(width: 10, height: 10).background(Sonda(id: "B"))
        }
        GridRow {
            Color.red.frame(width: 10, height: 10).background(Sonda(id: "C"))
            Color.red.frame(width: 40, height: 10).background(Sonda(id: "D"))
        }
    }
}
check(perto(m["A"], 61, 36, 30, 10) && perto(m["B"], 114, 36, 10, 10) && perto(m["C"], 71, 54, 10, 10) && perto(m["D"], 99, 54, 40, 10),
      "Grid/GridRow: \(desc(m["A"])) \(desc(m["B"])) \(desc(m["C"])) \(desc(m["D"]))")

// LazyVStack se comporta como VStack
m = medir(200, 100) {
    LazyVStack(spacing: 0) {
        Color.red.frame(width: 40, height: 10).background(Sonda(id: "la"))
        Color.red.frame(width: 40, height: 30).background(Sonda(id: "lb"))
    }
}
check(perto(m["la"], 80, 30, 40, 10) && perto(m["lb"], 80, 40, 40, 30), "LazyVStack: \(desc(m["la"])) \(desc(m["lb"]))")

// AppStorage: valor padrão, e a escrita por outra AppStorage da mesma chave redesenha a view
UserDefaults.standard.removeObject(forKey: "mos.teste.largura")
struct ComPreferencia: View {
    @AppStorage("mos.teste.largura") var largura: Double = 30
    var body: some View { Color.red.frame(width: largura, height: 10).background(Sonda(id: "pref")) }
}
let hostPref = NSHostingView(rootView: ComPreferencia())
hostPref.setFrameSize(NSSize(width: 200, height: 100)); hostPref.layout()
let larguraAntes = medidas["pref"]?.width ?? 0
let outra = AppStorage(wrappedValue: 30.0, "mos.teste.largura")
outra.wrappedValue = 70
hostPref.setFrameSize(NSSize(width: 200, height: 101)); hostPref.layout()
check(abs(larguraAntes - 30) < 0.5 && abs((medidas["pref"]?.width ?? 0) - 70) < 0.5,
      "AppStorage: padrão \(larguraAntes), depois de gravar 70: \(desc(medidas["pref"]))")

// Form (columns, o padrão do macOS): linhas alinhadas à esquerda, espaço 8, margem 20
m = medir(200, 100) {
    Form {
        Color.red.frame(width: 40, height: 10).background(Sonda(id: "fa"))
        Color.red.frame(width: 20, height: 10).background(Sonda(id: "fb"))
    }
}
check(perto(m["fa"], 80, 36, 40, 10) && perto(m["fb"], 80, 54, 20, 10), "Form columns: \(desc(m["fa"])) \(desc(m["fb"]))")

// NSColor(Color) e Color(nsColor:): ida e volta em sRGB
let ns = NSColor(Color(nsColor: NSColor(srgbRed: 0.2, green: 0.4, blue: 0.6, alpha: 0.5)))
check(abs(ns.redComponent - 0.2) < 0.01 && abs(ns.greenComponent - 0.4) < 0.01 && abs(ns.blueComponent - 0.6) < 0.01 && abs(ns.alphaComponent - 0.5) < 0.01,
      "NSColor(Color) ida e volta: \(ns.redComponent) \(ns.greenComponent) \(ns.blueComponent) \(ns.alphaComponent)")

// DisclosureGroup: fechado esconde o conteúdo; aberto (isExpanded) mostra, recuado 16
m = medir(200, 100) {
    DisclosureGroup { Color.red.frame(width: 10, height: 10).background(Sonda(id: "dg")) } label: { EmptyView() }
}
check(m["dg"] == nil, "DisclosureGroup fechado esconde o conteúdo")
m = medir(200, 100) {
    DisclosureGroup(isExpanded: .constant(true)) { Color.red.frame(width: 10, height: 10).background(Sonda(id: "dg")) }
        label: { Color.blue.frame(width: 30, height: 10).background(Sonda(id: "dgl")) }
}
if let c = m["dg"], let l = m["dgl"] {
    check(c.minY > l.minY && abs(c.minX - (l.minX - 16) - 16) < 20, "DisclosureGroup aberto mostra o conteúdo abaixo do rótulo: \(desc(l)) \(desc(c))")
} else { check(false, "DisclosureGroup aberto: \(m.keys.sorted())") }

// GroupBox: o conteúdo fica dentro da caixa, com margem 10
m = medir(200, 100) { GroupBox { Color.red.frame(width: 20, height: 10).background(Sonda(id: "gb")) } }
if let g = m["gb"] { check(g.width == 20 && g.minX >= 10, "GroupBox: \(desc(g))") } else { check(false, "GroupBox: sem conteúdo") }

// NavigationStack(path:): o topo da pilha vem do navigationDestination do tipo; com a pilha vazia, a raiz
m = medir(200, 100) {
    NavigationStack(path: .constant([7])) {
        Color.red.frame(width: 10, height: 10).background(Sonda(id: "raiz"))
            .navigationDestination(for: Int.self) { n in
                Color.blue.frame(width: CGFloat(n * 10), height: 10).background(Sonda(id: "destino"))
            }
    }
}
check(abs((m["destino"]?.width ?? 0) - 70) < 0.5, "NavigationStack mostra o destino do valor empilhado: \(desc(m["destino"]))")
m = medir(200, 100) {
    NavigationStack { Color.red.frame(width: 10, height: 10).background(Sonda(id: "raiz")) }
}
check(m["raiz"] != nil, "NavigationStack vazia mostra a raiz: \(desc(m["raiz"]))")

// NavigationSplitView: barra lateral à esquerda, detalhe ocupando o resto
m = medir(600, 300) {
    NavigationSplitView { Color.clear.background(Sonda(id: "lateral")) } detail: { Color.clear.background(Sonda(id: "detalhe")) }
}
if let l = m["lateral"], let d = m["detalhe"] {
    check(l.minX == 0 && l.width >= 180 && l.width <= 260 && abs(d.maxX - 600) < 0.5 && d.minX > l.maxX,
          "NavigationSplitView: lateral \(desc(l)) detalhe \(desc(d))")
} else { check(false, "NavigationSplitView: \(m.keys.sorted())") }

// sheet: o conteúdo aparece centralizado só enquanto isPresented
m = medir(200, 100) {
    Color.clear.sheet(isPresented: .constant(true)) { Color.red.frame(width: 40, height: 20).background(Sonda(id: "folha")) }
}
check(perto(m["folha"], 80, 40, 40, 20), "sheet apresentada no centro: \(desc(m["folha"]))")
m = medir(200, 100) {
    Color.clear.sheet(isPresented: .constant(false)) { Color.red.frame(width: 40, height: 20).background(Sonda(id: "folha")) }
}
check(m["folha"] == nil, "sheet fechada não aparece")

// toolbar: barra de 38 no topo; navegação à esquerda, o resto à direita; o conteúdo fica embaixo
m = medir(300, 200) {
    Color.clear.background(Sonda(id: "corpo"))
        .toolbar {
            ToolbarItem(placement: .navigation) { Color.red.frame(width: 20, height: 20).background(Sonda(id: "nav")) }
            ToolbarItem { Color.red.frame(width: 30, height: 20).background(Sonda(id: "acao")) }
        }
}
check(perto(m["nav"], 10, 9, 20, 20) && perto(m["acao"], 260, 9, 30, 20) && perto(m["corpo"], 0, 38, 300, 162),
      "toolbar: nav \(desc(m["nav"])) ação \(desc(m["acao"])) corpo \(desc(m["corpo"]))")

// Text tem tamanho de texto, não da proposta (por último: depende do CoreText do libSystem)
m = medir(200, 100) { Text("Olá, mOS").background(Sonda(id: "t")) }
if let t = m["t"] {
    check(t.width > 10 && t.width < 200 && t.height > 5 && t.height < 100 && abs(t.midX - 100) < 1 && abs(t.midY - 50) < 1,
          "Text com tamanho próprio e centralizado: \(desc(t))")
} else { check(false, "Text: sonda não apareceu") }

print("swiftui: \(passou)/\(total) ok")
