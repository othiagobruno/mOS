// Gancho para apresentar o popover com o NSPopover do libSystem.
//
// Este arquivo é do SwiftUI. Não altera appkit*.c. O presenter padrão é
// MOSAppKitPopoverPresenter: chama showRelativeToRect:ofView:preferredEdge: e close.
// O NSPopover do mOS só registra o modelo (appkit_status.c); não pinta janela.
// replacesCard fica false para o cartão do SwiftUI continuar visível.
//
// MOSPopoverRequest.edge: 0 topo, 1 base, 2 início, 3 fim.
// anchor pode vir .zero quando a view ainda não publicou o quadro global.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
import AppKit
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
    /// true esconde o cartão desenhado na árvore. O presenter do mOS devolve false.
    var replacesCard: Bool { get }
}

/// Instala o NSPopover sem criar NSWindow. O cartão SwiftUI permanece.
public final class MOSAppKitPopoverPresenter: NSObject, MOSPopoverPresenting {
    public var replacesCard: Bool { false }
    private var popover: NSPopover?
    private var anchorView: NSView?

    public override init() { super.init() }

    public func present(_ request: MOSPopoverRequest) {
        dismiss()
        let pop = NSPopover()
        pop.behavior = .transient
        pop.contentView = NSView(frame: NSRect(x: 0, y: 0, width: 160, height: 64))
        let width = request.anchor.width > 1 ? request.anchor.width : 1
        let height = request.anchor.height > 1 ? request.anchor.height : 1
        let anchor = NSView(frame: NSRect(x: request.anchor.origin.x, y: request.anchor.origin.y, width: width, height: height))
        anchorView = anchor
        pop.show(relativeTo: NSRect(x: 0, y: 0, width: width, height: height), of: anchor, preferredEdge: mosPopoverRectEdge(request.edge))
        popover = pop
    }

    public func dismiss() {
        popover?.close()
        popover = nil
        anchorView = nil
    }
}

private func mosPopoverRectEdge(_ edge: Int) -> NSRectEdge {
    switch edge {
    case 1: return .maxY
    case 2: return .minX
    case 3: return .maxX
    default: return .minY
    }
}

public enum MOSPopoverHook {
    public static var presenter: MOSPopoverPresenting? = MOSAppKitPopoverPresenter()
}
#endif
