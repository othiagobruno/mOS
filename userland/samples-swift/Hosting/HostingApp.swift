// Hosting — List, ScrollView e TabView dentro do NSHostingView do runtime embutido.
// A cena de pixels (clique, rolagem, vidro e popover) é a mesma de mos_sui_selftest.
import SwiftUI

@main
struct HostingApp: App {
    var body: some Scene {
        WindowGroup("Hosting") { ContentView() }
            .defaultSize(width: 640, height: 480)
    }
}
