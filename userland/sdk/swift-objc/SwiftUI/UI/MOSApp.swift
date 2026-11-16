// mOS — ciclo de vida do app SwiftUI na variante com interop ObjC (substitui, nesta variante, o
// OpenSwiftUIMOS/App/MOSApp.swift da variante nativa). Abre a janela do AppKit do mOS (mos_run_app) e põe nela um
// NSHostingView com a primeira cena: assim um app com @main App tem o mesmo hosting de um app AppKit com
// NSHostingView — redimensionar refaz o layout sem perder estado, NSViewRepresentable vira subview e os eventos e o
// foco vêm do próprio NSHostingView. O @NSApplicationDelegateAdaptor recebe o que o delegate interno não trata.
#if !OPENSWIFTUI_SWIFTUI_RENDERER
public import AppKit
public import Combine
import Foundation
@_spi(ForOpenSwiftUIOnly)
@_spi(StdoutRenderer)
import OpenSwiftUICore
import MOSBridge

final class _MOSAppState {
    var hosting: NSView?
    var start: ((UnsafeMutableRawPointer, CGSize) -> Void)?
}

nonisolated(unsafe) let _mosApp = _MOSAppState()

func runMOSApp<Application>(_ app: Application) -> Never where Application: App {
    _mosApp.start = { content, size in
        var root: AnyView?
        Update.dispatchImmediately(reason: nil) {
            let graph = AppGraph(app: app)
            graph.instantiate()
            AppGraph.shared = graph
            if let item = graph.rootSceneList?.items.first {
                root = AnyView(item.value.view.environment(\.self, item.environment))
            }
        }
        guard let root else { return }
        let hosting = NSHostingView(rootView: root)
        _mosApp.hosting = hosting
        mos_add_subview(content, Unmanaged.passUnretained(hosting).toOpaque())
        MOSFocusManager.shared.onChange = { _MOSHostingState.renderAll() }
    }
    "App".withCString { title in
        mos_run_app(title, 640, 480, { _, content, w, h in
            if _mosApp.hosting == nil, let content { _mosApp.start?(content, CGSize(width: w, height: h)) }
            _mosApp.hosting?.frame = CGRect(x: 0, y: 0, width: w, height: h)
        }, nil)
    }
    exit(0)
}

// MARK: - NSApplicationDelegateAdaptor

/// Cria o delegate do app (uma vez) e o liga ao NSApplication do mOS.
@available(macOS 11.0, *)
@MainActor
@propertyWrapper
public struct NSApplicationDelegateAdaptor<DelegateType>: DynamicProperty where DelegateType: NSObject, DelegateType: NSApplicationDelegate {
    let delegate: DelegateType

    public init(_ delegateType: DelegateType.Type = DelegateType.self) {
        let d = _MOSAppDelegateStore.shared(delegateType)
        delegate = d
    }

    public var wrappedValue: DelegateType { delegate }
}

@available(macOS 11.0, *)
extension NSApplicationDelegateAdaptor where DelegateType: ObservableObject {
    public init(_ delegateType: DelegateType.Type = DelegateType.self) {
        delegate = _MOSAppDelegateStore.shared(delegateType)
    }

    public var projectedValue: ObservedObject<DelegateType>.Wrapper { ObservedObject(wrappedValue: delegate).projectedValue }
}

/// O delegate é um só por processo (o App é recriado pelo grafo, o delegate não).
enum _MOSAppDelegateStore {
    nonisolated(unsafe) static var instance: AnyObject?

    static func shared<D: NSObject & NSApplicationDelegate>(_ type: D.Type) -> D {
        if let d = instance as? D { return d }
        let d = type.init()
        instance = d
        mos_set_app_delegate(Unmanaged.passUnretained(d).toOpaque())
        return d
    }
}
#endif
