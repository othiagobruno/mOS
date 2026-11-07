import SwiftUI

@_silgen_name("mos_log")
func mos_log(_ s: UnsafePointer<CChar>)

struct OlaApp: App {
    var body: some Scene {
        WindowGroup {
            Text("Ola SwiftUI")
        }
    }
}

@main
enum Boot {
    static func main() {
        "entrou no app".withCString { mos_log($0) }
        OlaApp.main()
    }
}
