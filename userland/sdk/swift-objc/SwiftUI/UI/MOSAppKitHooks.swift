// Gancho para o agente do AppKit apresentar um popover nativo.
//
// Este arquivo é do SwiftUI. Não altera appkit*.c. O presenter começa nil:
// o popover é desenhado dentro da árvore (MOSContainers.swift). Se o AppKit
// instalar um presenter, o SwiftUI chama present(_:) e não desenha o cartão.
//
// MOSPopoverRequest.edge: 0 topo, 1 base, 2 início, 3 fim.
// anchor pode vir .zero quando a view ainda não publicou o quadro global;
// o apresentador usa a view hospedeira nesse caso.
// dismiss() tira o popover. O SwiftUI não desenha os dois ao mesmo tempo.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
import CoreGraphics
import Foundation

public struct MOSPopoverRequest {
    public var anchor: CGRect
    public var edge: Int
    public var dismiss: () -> Void

    public init(anchor: CGRect, edge: Int, dismiss: @escaping () -> Void) {
        self.anchor = anchor
        self.edge = edge
        self.dismiss = dismiss
    }
}

public protocol MOSPopoverPresenting: AnyObject {
    func present(_ request: MOSPopoverRequest)
    func dismiss()
}

public enum MOSPopoverHook {
    public static var presenter: MOSPopoverPresenting?
}
#endif
