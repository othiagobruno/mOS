// A janela sobe pelo NSHostingView (WindowGroup._start). O autoteste C roda no onAppear
// e escreve HOSTING PASS ou HOSTING FAIL no log. Ele não abre o WhatsApp.
import SwiftUI

struct ContentView: View {
    @State private var tab = 0
    @State private var sel: Int? = nil
    @State private var pop = false

    var body: some View {
        TabView(selection: $tab) {
            List([0, 1, 2, 3], id: { $0 }, selection: $sel) { i in
                Text("Linha \(i)")
            }
            .tabItem { Text("Lista") }

            ScrollView {
                ForEach(0..<12) { i in
                    Text("Item \(i)")
                }
            }
            .tabItem { Text("Rolagem") }

            VStack(alignment: .leading, spacing: 12) {
                Button("Abrir") { pop = true }
                    .popover(isPresented: $pop) { Text("Detalhe") }
                Text("Vidro").padding().glassEffect()
            }
            .tabItem { Text("Mais") }
        }
        .onAppear { _ = mosHostingSelfTest() }
    }
}
