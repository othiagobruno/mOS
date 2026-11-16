// mOS — teste do SwiftUI.framework (tools/test-swiftui.sh). Liga como um app da Apple contra o SwiftUI.framework do
// mOS (OpenSwiftUI com -module-abi-name SwiftUI) e roda no build/machotest. As sondas medem com GeometryReader onde cada
// view caiu no layout; os valores esperados são os do SwiftUI da Apple para o mesmo código.
import AppKit
import OpenSwiftUI

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
            Color.clear.onAppear { medidas[id] = g.frame(in: .global) }
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

print("swiftui: \(passou)/\(total) ok")
