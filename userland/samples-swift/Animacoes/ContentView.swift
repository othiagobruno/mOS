import SwiftUI

struct ContentView: View {
    @State private var alturas: [Double] = [40, 90, 60, 120, 75]
    @State private var passo = 0
    @State private var mostrarExtra = false
    @State private var largura = 120.0

    let cores: [Color] = [.red, .orange, .yellow, .green, .blue]

    var body: some View {
        VStack(spacing: 16) {
            Text("Animações no SwiftUI").font(.title2)
            HStack(alignment: .bottom, spacing: 12) {
                ForEach(0..<5) { i in
                    cores[i].frame(width: 40, height: alturas[i])
                }
                if mostrarExtra {
                    Color.purple.frame(width: 40, height: 40)
                        .transition(.scale)
                }
            }
            .frame(height: 160)
            .padding(12)
            .background(Color(white: 0, opacity: 0.06), cornerRadius: 8)

            Color.teal.frame(width: largura, height: 10)
                .animation(.easeInOut(duration: 0.6), value: largura)

            HStack {
                Button("Embaralhar") {
                    withAnimation(.easeOut(duration: 0.45)) {
                        passo += 1
                        let base: [Double] = [130, 50, 100, 30, 150, 80, 60]
                        for i in 0..<alturas.count { alturas[i] = base[(i + passo) % base.count] }
                    }
                }
                Button(mostrarExtra ? "Esconder quadrado" : "Mostrar quadrado") {
                    withAnimation(.spring()) { mostrarExtra.toggle() }
                }
                Button("Barra") { largura = largura > 200 ? 120 : 360 }
            }
        }
        .padding(20)
    }
}
