// mOS — só a interface que o ExtensionKit.swift precisa do SwiftUI para compilar (o protocolo View, com o nome de
// módulo SwiftUI, para o nome mangled ser o da Apple: 7SwiftUI4ViewP). Vira só um .swiftmodule temporário
// (build-frameworks-objc.sh, pasta iface/); nada daqui é ligado: em execução os símbolos vêm do SwiftUI.framework.
public protocol View {
    associatedtype Body: View
    var body: Body { get }
}
