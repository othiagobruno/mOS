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

// Text tem tamanho de texto, não da proposta (por último: depende do CoreText do libSystem)
m = medir(200, 100) { Text("Olá, mOS").background(Sonda(id: "t")) }
if let t = m["t"] {
    check(t.width > 10 && t.width < 200 && t.height > 5 && t.height < 100 && abs(t.midX - 100) < 1 && abs(t.midY - 50) < 1,
          "Text com tamanho próprio e centralizado: \(desc(t))")
} else { check(false, "Text: sonda não apareceu") }

print("swiftui: \(passou)/\(total) ok")
